// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Jitao Shi <jitao.shi@mediatek.com>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define HW_NT51021_VND_MIPI 0x8f
#define HW_NT51021_VND_INDEX0 0x83
#define HW_NT51021_VND_INDEX1 0x84

#define HW_NT51021_SET_DISPLAY_BRIGHTNESS 0x9f
#define HW_NT51021_CABC_MODE 0x90 // 0xc0 off | 0x00 on
#define HW_NT51021_CABC_DIMMING 0x95 // 0xB0 off | 0x60 on

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
};

struct boe_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	const struct panel_desc *desc;

	enum drm_panel_orientation orientation;

	struct regulator *vddio;
	struct regulator *vsp;
	struct regulator *vsn;
	struct gpio_desc *vdd_gpio;
	struct gpio_desc *bl_gpio;
	struct gpio_desc *vled_gpio;
	struct gpio_desc *reset_gpio;

    int hw_led_en_flag;

	bool prepared;
	bool enabled;
};

enum dsi_cmd_type {
	//INIT_GEN_CMD,
	INIT_DCS_CMD,
	DELAY_CMD,
};

struct panel_init_cmd {
	enum dsi_cmd_type type;
	size_t len;
	const char *data;
};

#define _INIT_DCS_CMD(...) { \
	.type = INIT_DCS_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#define _INIT_DELAY_CMD(...) { \
	.type = DELAY_CMD,\
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

static const struct panel_init_cmd boe_nt51021_10_init_cmd[] = {
	_INIT_DCS_CMD(0x8f, 0xa5),
	_INIT_DELAY_CMD(2),
	_INIT_DCS_CMD(0x83, 0xbb),
	_INIT_DCS_CMD(0x84, 0x22),
	_INIT_DCS_CMD(0x90, 0x00),
	_INIT_DCS_CMD(0xa1, 0xff),
	_INIT_DCS_CMD(0xa2, 0xfe),
	_INIT_DCS_CMD(0xa3, 0xf7),
	_INIT_DCS_CMD(0xa4, 0xf1),
	_INIT_DCS_CMD(0xa5, 0xe9),
	_INIT_DCS_CMD(0xa6, 0xe2),
	_INIT_DCS_CMD(0xa7, 0xdc),
	_INIT_DCS_CMD(0xa8, 0xd7),
	_INIT_DCS_CMD(0xa9, 0xd1),
	_INIT_DCS_CMD(0xaa, 0xcc),
	_INIT_DCS_CMD(0xb4, 0x42),
	_INIT_DCS_CMD(0xb5, 0x5b),
	_INIT_DCS_CMD(0xb6, 0x4f),
	_INIT_DCS_CMD(0xAD, 0x03),
	_INIT_DCS_CMD(0x9a, 0x10),
	_INIT_DCS_CMD(0x94, 0xb8),
	_INIT_DCS_CMD(0x95, 0x00),
	_INIT_DCS_CMD(0x99, 0x00),
	_INIT_DCS_CMD(0x96, 0x00),
	_INIT_DCS_CMD(0x83, 0x00),
	_INIT_DCS_CMD(0x84, 0x00),
	_INIT_DCS_CMD(0x8C, 0x80),
	_INIT_DCS_CMD(0xCD, 0x6C),
	_INIT_DCS_CMD(0xC0, 0x8B),
	_INIT_DCS_CMD(0xC8, 0xF0),
	_INIT_DCS_CMD(0x8B, 0x10),
	_INIT_DCS_CMD(0xA9, 0x0A),
	_INIT_DCS_CMD(0x97, 0x00),
	//_INIT_DCS_CMD(0x9f, 0x66),
	_INIT_DCS_CMD(0x83, 0xaa),
	_INIT_DCS_CMD(0x84, 0x11),
	_INIT_DCS_CMD(0xa9, 0x4b),
	_INIT_DCS_CMD(0x85, 0x04),
	_INIT_DCS_CMD(0x86, 0x08),
	_INIT_DCS_CMD(0x9c, 0x10),
	//_INIT_DCS_CMD(0x11),
	//_INIT_DELAY_CMD(120),
	{},
};

static const struct panel_init_cmd cmi_nt51021_10_init_cmd[] = {
	//_INIT_DCS_CMD(0x21),
	_INIT_DCS_CMD(HW_NT51021_VND_MIPI, 0xa5),
	_INIT_DELAY_CMD(20),
	_INIT_DCS_CMD(0x01),
	_INIT_DELAY_CMD(20),
	_INIT_DCS_CMD(HW_NT51021_VND_MIPI, 0xa5),
	_INIT_DELAY_CMD(20),
	_INIT_DCS_CMD(0x83, 0xBB),
	_INIT_DCS_CMD(0x84, 0x22),
	_INIT_DCS_CMD(0x96, 0x00),
	_INIT_DCS_CMD(0x90, 0xC0),
	_INIT_DCS_CMD(0x91, 0xA0),
	_INIT_DCS_CMD(0x9A, 0x10),
	_INIT_DCS_CMD(0x94, 0x78),
	_INIT_DCS_CMD(0x95, 0xB0),
	_INIT_DCS_CMD(0xA1, 0xFF),
	_INIT_DCS_CMD(0xA2, 0xFA),
	_INIT_DCS_CMD(0xA3, 0xF3),
	_INIT_DCS_CMD(0xA4, 0xED),
	_INIT_DCS_CMD(0xA5, 0xE7),
	_INIT_DCS_CMD(0xA6, 0xE2),
	_INIT_DCS_CMD(0xA7, 0xDC),
	_INIT_DCS_CMD(0xA8, 0xD7),
	_INIT_DCS_CMD(0xA9, 0xD1),
	_INIT_DCS_CMD(0xAA, 0xCC),
	_INIT_DCS_CMD(0xB4, 0x1C),
	_INIT_DCS_CMD(0xB5, 0x38),
	_INIT_DCS_CMD(0xB6, 0x30),
	_INIT_DCS_CMD(0x83, 0x00),
	_INIT_DCS_CMD(0x84, 0x00),
	_INIT_DCS_CMD(0x8C, 0x80),
	_INIT_DCS_CMD(0xC7, 0x50),
	_INIT_DCS_CMD(0xC5, 0x50),
	_INIT_DCS_CMD(0x85, 0x04),
	_INIT_DCS_CMD(0x86, 0x08),
	_INIT_DCS_CMD(0xA9, 0x2A),
	_INIT_DCS_CMD(0x83, 0xAA),
	_INIT_DCS_CMD(0x84, 0x11),
	_INIT_DCS_CMD(0x9C, 0x10),
	_INIT_DCS_CMD(0xA9, 0x4B),
	{},
};


static inline struct boe_panel *to_boe_panel(struct drm_panel *panel)
{
	return container_of(panel, struct boe_panel, base);
}

static int boe_panel_init_cmd(struct boe_panel *boe)
{
	struct mipi_dsi_device *dsi = boe->dsi;
	struct drm_panel *panel = &boe->base;
	int i, err = 0;

	if (boe->desc->init_cmds) {
		const struct panel_init_cmd *init_cmds = boe->desc->init_cmds;

        //dsi->mode_flags |= MIPI_DSI_MODE_LPM;

		for (i = 0; init_cmds[i].len != 0; i++) {
			const struct panel_init_cmd *cmd = &init_cmds[i];

			switch (cmd->type) {
			case DELAY_CMD:
				msleep(cmd->data[0]);
				err = 0;
				break;

			case INIT_DCS_CMD:
				err = mipi_dsi_dcs_write(dsi, cmd->data[0],
							 cmd->len <= 1 ? NULL :
							 &cmd->data[1],
							 cmd->len - 1);
				break;

			default:
				err = -EINVAL;
			}

			if (err < 0) {
				dev_err(panel->dev,
					"failed to write command %u\n", i);
				return err;
			}
		}
	}
	return 0;
}

static void hw_nt51021_bias_set(struct boe_panel *boe, int enabled)
{
	//struct boe_panel *boe = to_boe_panel(panel);

	gpiod_set_value(boe->vled_gpio, enabled);

    if (enabled) {
        boe->hw_led_en_flag = 1;
    }
    else {
        boe->hw_led_en_flag = 0;
    }
}

static int boe_panel_enter_sleep_mode(struct boe_panel *boe)
{
	struct mipi_dsi_device *dsi = boe->dsi;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;

	return 0;
}

static int boe_panel_disable(struct drm_panel *panel)
{
	struct boe_panel *boe = to_boe_panel(panel);
	int ret;

	if (!boe->enabled)
		return 0;

	ret = boe_panel_enter_sleep_mode(boe);
	if (ret < 0) {
		dev_err(panel->dev, "failed to set panel off: %d\n", ret);
		return ret;
	}

	msleep(120);

	boe->enabled = false;
	return 0;
}

static int boe_panel_unprepare(struct drm_panel *panel)
{
	struct boe_panel *boe = to_boe_panel(panel);

	if (!boe->prepared)
		return 0;

	/*gpiod_set_value(boe->bl_gpio, 0);
	usleep_range(5000, 7000);*/

	regulator_disable(boe->vsp);
	regulator_disable(boe->vsn);
	usleep_range(3000, 4000);

	regulator_disable(boe->vddio);
	gpiod_set_value(boe->vdd_gpio, 0);
	msleep(300);

	gpiod_set_value(boe->reset_gpio, 1);

	boe->prepared = false;

	return 0;
}

static int boe_panel_prepare(struct drm_panel *panel)
{
	struct boe_panel *boe = to_boe_panel(panel);
	struct mipi_dsi_device *dsi = boe->dsi;
	int ret;

	if (boe->prepared)
		return 0;

	ret = regulator_enable(boe->vddio);
	if (ret < 0)
		return ret;
	usleep_range(3000, 5000);

	ret = regulator_enable(boe->vsp);
	if (ret < 0)
		//return ret;
		goto poweroff1v8;

	//gpiod_set_value_cansleep(boe->bl_gpio, 1);
	gpiod_set_value_cansleep(boe->vdd_gpio, 1);
	//usleep_range(2000, 4000);
	msleep(500);
	//usleep_range(10000, 11000);

	gpiod_set_value_cansleep(boe->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(boe->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(boe->reset_gpio, 0);
	msleep(30);

	ret = regulator_enable(boe->vsn);
	if (ret < 0)
		goto poweroffvsp;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret) {
		dev_err(panel->dev, "failed to turn display on (%d)\n", ret);
		return ret;
	}
	usleep_range(20000, 22000);

    dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = boe_panel_init_cmd(boe);
	if (ret < 0) {
		dev_err(panel->dev, "failed to init panel: %d\n", ret);
		goto poweroff;
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
    }

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	/*ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret) {
		dev_err(panel->dev, "failed to turn display on (%d)\n", ret);
		return ret;
	}
	usleep_range(80000, 90000);*/

	boe->prepared = true;

	return 0;

poweroff:
	regulator_disable(boe->vsn);
poweroffvsp:
	regulator_disable(boe->vsp);
	gpiod_set_value_cansleep(boe->bl_gpio, 0);
	gpiod_set_value_cansleep(boe->vdd_gpio, 0);
	usleep_range(2000, 4000);
poweroff1v8:
//	usleep_range(5000, 7000);
	regulator_disable(boe->vddio);
	gpiod_set_value(boe->reset_gpio, 1);

	return ret;
}

static int boe_panel_enable(struct drm_panel *panel)
{
	struct boe_panel *boe = to_boe_panel(panel);
	struct mipi_dsi_device *dsi = boe->dsi;
	int ret;

	if (boe->enabled)
		return 0;

	usleep_range(20000, 30000);

	hw_nt51021_bias_set(boe, 1);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
    }

	boe->enabled = true;
	return 0;
}

static const struct drm_display_mode boe_nt51021_10_default_mode = {
	.clock = (1200 + 64 + 4 + 36) * (1920 + 104 + 2 + 24) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 64,
	.hsync_end = 1200 + 64 + 4,
	.htotal = 1200 + 64 + 4 + 36,
	.vdisplay = 1920,
	.vsync_start = 1920 + 104,
	.vsync_end = 1920 + 104 + 2,
	.vtotal = 1920 + 104 + 2 + 24,
};

static const struct panel_desc boe_nt51021_10_desc = {
	.modes = &boe_nt51021_10_default_mode,
	.bpc = 8,
	.size = {
		.width_mm = 135,
		.height_mm = 217,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags =  MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
		      MIPI_DSI_MODE_LPM,
	.init_cmds = boe_nt51021_10_init_cmd,
};

static const struct drm_display_mode cmi_nt51021_10_default_mode = {
	.clock = (1200 + 64 + 4 + 36) * (1920 + 104 + 2 + 24) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 64,
	.hsync_end = 1200 + 64 + 4,
	.htotal = 1200 + 64 + 4 + 36,
	.vdisplay = 1920,
	.vsync_start = 1920 + 104,
	.vsync_end = 1920 + 104 + 2,
	.vtotal = 1920 + 104 + 2 + 24,
};

static const struct panel_desc cmi_nt51021_10_desc = {
	.modes = &cmi_nt51021_10_default_mode,
	.bpc = 8,
	.size = {
		.width_mm = 135,
		.height_mm = 217,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO
			| MIPI_DSI_MODE_VIDEO_BURST
			| MIPI_DSI_MODE_VIDEO_HSE
			| MIPI_DSI_MODE_NO_EOT_PACKET
			| MIPI_DSI_CLOCK_NON_CONTINUOUS,
	.init_cmds = cmi_nt51021_10_init_cmd,
};

static int boe_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct boe_panel *boe = to_boe_panel(panel);
	const struct drm_display_mode *m = boe->desc->modes;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, m);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			m->hdisplay, m->vdisplay, drm_mode_vrefresh(m));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = boe->desc->size.width_mm;
	connector->display_info.height_mm = boe->desc->size.height_mm;
	connector->display_info.bpc = boe->desc->bpc;
	/*
	 * TODO: Remove once all drm drivers call
	 * drm_connector_set_orientation_from_panel()
	 */
	drm_connector_set_panel_orientation(connector, boe->orientation);

	return 1;
}

static enum drm_panel_orientation boe_panel_get_orientation(struct drm_panel *panel)
{
	struct boe_panel *boe = to_boe_panel(panel);

	return boe->orientation;
}

static const struct drm_panel_funcs boe_panel_funcs = {
	.disable = boe_panel_disable,
	.unprepare = boe_panel_unprepare,
	.prepare = boe_panel_prepare,
	.enable = boe_panel_enable,
	.get_modes = boe_panel_get_modes,
	.get_orientation = boe_panel_get_orientation,
};

static int hw_nt51021_bl(struct mipi_dsi_device *dsi,
					u16 brightness)
{
	const u8 vnd_mipi[2] = {HW_NT51021_VND_MIPI, 0xa5};
	const u8 chang_page0_index0[2] = {HW_NT51021_VND_INDEX0, 0x00};
	const u8 chang_page0_index1[2] = {HW_NT51021_VND_INDEX1, 0x00};
	//const u8 brightness[2] = {HW_NT51021_SET_DISPLAY_BRIGHTNESS, 0x0};
	u8 payload[2] = { brightness & 0xff, brightness >> 8 };
	
	int ret;

	mipi_dsi_dcs_write_buffer(dsi, vnd_mipi,
					ARRAY_SIZE(vnd_mipi));

	mipi_dsi_dcs_write_buffer(dsi, chang_page0_index0,
					ARRAY_SIZE(chang_page0_index0));

	mipi_dsi_dcs_write_buffer(dsi, chang_page0_index1,
					ARRAY_SIZE(chang_page0_index1));
	usleep_range(1000, 2000);

	ret = mipi_dsi_dcs_write(dsi, HW_NT51021_SET_DISPLAY_BRIGHTNESS,
				 payload, sizeof(payload));
	if (ret < 0)
		return ret;

	return 0;
}

static int hw_nt51021_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
    //struct drm_panel *base;
	struct boe_panel *boe = dev_get_drvdata(&bl->dev);
	//struct hw_nt51021 *ctx = mipi_dsi_get_drvdata(dsi);
	u16 brightness = bl->props.brightness;
	int ret;

//	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	gpiod_set_value_cansleep(ctx->bl_gpio, !!brightness);

	usleep_range(20000, 25000);
	ret = hw_nt51021_bl(dsi, brightness);
	if (ret < 0)
		return ret;

	//dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static int hw_nt51021_bl_get_intensity(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops hw_nt51021_bl_ops = {
	.update_status = hw_nt51021_bl_update_status,
	.get_brightness = hw_nt51021_bl_get_intensity,
};

static struct backlight_device *
hw_nt51021_create_backlight(struct mipi_dsi_device *dsi)
{
	//struct boe_panel *boe = to_boe_panel(panel);
	struct device *dev = &dsi->dev;
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.power = FB_BLANK_UNBLANK,
	props.type = BACKLIGHT_RAW;
	props.brightness = 255;
	props.max_brightness = 255;

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &hw_nt51021_bl_ops, &props);
}

static int boe_panel_add(struct boe_panel *boe)
{
	struct device *dev = &boe->dsi->dev;
	int err;

	boe->vsp = devm_regulator_get(dev, "vsp");
	if (IS_ERR(boe->vsp))
		return PTR_ERR(boe->vsp);

	boe->vsn = devm_regulator_get(dev, "vsn");
	if (IS_ERR(boe->vsn))
		return PTR_ERR(boe->vsn);

	boe->vddio = devm_regulator_get(dev, "vddio");
	if (IS_ERR(boe->vddio))
		return PTR_ERR(boe->vddio);

	boe->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(boe->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(boe->reset_gpio));
		return PTR_ERR(boe->reset_gpio);
	}

	boe->bl_gpio = devm_gpiod_get(dev, "backlight", GPIOD_OUT_HIGH);
	if (IS_ERR(boe->bl_gpio)) {
		dev_err(dev, "cannot get bl-gpios %ld\n",
			PTR_ERR(boe->bl_gpio));
		return PTR_ERR(boe->bl_gpio);
	}

	boe->vdd_gpio = devm_gpiod_get(dev, "vdd", GPIOD_OUT_HIGH);
	if (IS_ERR(boe->vdd_gpio)) {
		dev_err(dev, "cannot get vdd-gpios %ld\n",
			PTR_ERR(boe->vdd_gpio));
		return PTR_ERR(boe->vdd_gpio);
	}

	boe->vled_gpio = devm_gpiod_get(dev, "vled", GPIOD_OUT_HIGH);
	if (IS_ERR(boe->vled_gpio)) {
		dev_err(dev, "cannot get vled-gpios %ld\n",
			PTR_ERR(boe->vled_gpio));
		return PTR_ERR(boe->vled_gpio);
	}

	drm_panel_init(&boe->base, dev, &boe_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	err = of_drm_get_panel_orientation(dev->of_node, &boe->orientation);
	if (err < 0) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, err);
		return err;
	}

	boe->base.backlight = hw_nt51021_create_backlight(boe->dsi);
	if (IS_ERR(boe->base.backlight))
		return dev_err_probe(dev, PTR_ERR(boe->base.backlight),
				     "Failed to create backlight\n");

	boe->base.funcs = &boe_panel_funcs;
	boe->base.dev = &boe->dsi->dev;

	drm_panel_add(&boe->base);

	return 0;
}

static int boe_panel_probe(struct mipi_dsi_device *dsi)
{
	struct boe_panel *boe;
	int ret;
	const struct panel_desc *desc;

	boe = devm_kzalloc(&dsi->dev, sizeof(*boe), GFP_KERNEL);
	if (!boe)
		return -ENOMEM;

	desc = of_device_get_match_data(&dsi->dev);
	dsi->lanes = desc->lanes;
	dsi->format = desc->format;
	dsi->mode_flags = desc->mode_flags;
	boe->desc = desc;
	boe->dsi = dsi;
	ret = boe_panel_add(boe);
	if (ret < 0)
		return ret;

	mipi_dsi_set_drvdata(dsi, boe);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&boe->base);

	return ret;
}

static void boe_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct boe_panel *boe = mipi_dsi_get_drvdata(dsi);

	drm_panel_disable(&boe->base);
	drm_panel_unprepare(&boe->base);
}

static void boe_panel_remove(struct mipi_dsi_device *dsi)
{
	struct boe_panel *boe = mipi_dsi_get_drvdata(dsi);
	int ret;

	boe_panel_shutdown(dsi);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	if (boe->base.dev)
		drm_panel_remove(&boe->base);
}

static const struct of_device_id boe_of_match[] = {
	{ .compatible = "huawei,boe-nt51021",
	  .data = &boe_nt51021_10_desc
	},
	{ .compatible = "huawei,cmi-nt51021",
	  .data = &cmi_nt51021_10_desc
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, boe_of_match);

static struct mipi_dsi_driver boe_panel_driver = {
	.driver = {
		.name = "panel-huawei-nt51021",
		.of_match_table = boe_of_match,
	},
	.probe = boe_panel_probe,
	.remove = boe_panel_remove,
	.shutdown = boe_panel_shutdown,
};
module_mipi_dsi_driver(boe_panel_driver);

MODULE_AUTHOR("Jitao Shi <jitao.shi@mediatek.com>");
MODULE_DESCRIPTION("Huawei nt51021 1200x1920 video mode panel driver");
MODULE_LICENSE("GPL v2");
