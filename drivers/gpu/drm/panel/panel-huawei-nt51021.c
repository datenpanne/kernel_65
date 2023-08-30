// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct hw_nt51021 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *vdd; //3v3
	struct regulator *vsp;
	struct regulator *vsn;
	struct gpio_desc *reset_gpio;

	struct mutex mutex;

	bool prepared;
	bool enabled;
};

static inline
struct hw_nt51021 *to_hw_nt51021(struct drm_panel *panel)
{
	return container_of(panel, struct hw_nt51021, panel);
}

static void hw_nt51021_reset(struct hw_nt51021 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(200);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(200);
}

static int hw_nt51021_init(struct hw_nt51021 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	//dsi->mode_flags = 0;

	mipi_dsi_generic_write_seq(dsi, 0xAD, 0x90);//STV pulse width selection/OE1 width selection
	mipi_dsi_generic_write_seq(dsi, 0xAE, 0x54);//OE2 width selection
	mipi_dsi_generic_write_seq(dsi, 0xAF, 0x70);//TCKV_OE timing adjustment
	//mipi_dsi_generic_write_seq(dsi, 0xF0, 0xC8);//VCOM OP Control Register
	//mipi_dsi_generic_write_seq(dsi, 0xFA, 0x0A);//HAOP OP Control Register
	//mipi_dsi_generic_write_seq(dsi, 0xFD, 0x0F);//V1/V14 Control Register
	mipi_dsi_generic_write_seq(dsi, 0x97, 0x00);//terminal resistor selection
	mipi_dsi_generic_write_seq(dsi, 0x93, 0xFF);//PORT2B/ZIGZAG/ZTYPE/NBW/RES
	mipi_dsi_generic_write_seq(dsi, 0x91, 0x07);//BIST clock;Normal Operation/BIST pattern
	mipi_dsi_generic_write_seq(dsi, 0x92, 0xCF);//Black insertion control
	mipi_dsi_generic_write_seq(dsi, 0x90, 0xFF);//display device top to bottom; display device left to right;
	//mipi_dsi_generic_write_seq(dsi, 0x8c, 0x00);//Control source:V1_V14_SET;GAM_SET;HAOP_SET;VCOM_SET;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}

	return 0;
}

static int hw_nt51021_pwr(struct hw_nt51021 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int err, ret;

	err = regulator_enable(ctx->vdd);
	if (err < 0)
		return err;
	usleep_range(1000, 1100);		
	err = regulator_enable(ctx->vsp);
	if (err < 0)
		return err;
	err = regulator_enable(ctx->vsn);
	if (err < 0)
		return err;
	usleep_range(5000, 6000);

	hw_nt51021_reset(ctx);

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
	//dsi->mode_flags = 0;

	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to do Software Reset: %d\n", ret);
		return ret;
	}
	msleep(20);

	mipi_dsi_generic_write_seq(dsi, 0x8c, 0x00);//Control source:V1_V14_SET;GAM_SET;HAOP_SET;VCOM_SET;
	mipi_dsi_generic_write_seq(dsi, 0xF0, 0xC8);//VCOM OP Control Register
	mipi_dsi_generic_write_seq(dsi, 0xFA, 0x0A);//HAOP OP Control Register
	mipi_dsi_generic_write_seq(dsi, 0xFD, 0x0F);//V1/V14 Control Register
	usleep_range(1000, 2000);

	return 0;
}

static int hw_nt51021_on(struct hw_nt51021 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	//mutex_lock(&ctx->mutex);
	ret = hw_nt51021_pwr(ctx);
	if (ret)
		return ret;

	hw_nt51021_init(ctx);
	if (ret)
		return ret;
	//mutex_unlock(&ctx->mutex);

	return 0;
}

static int hw_nt51021_off(struct hw_nt51021 *ctx)
{
	//struct mipi_dsi_device *dsi = ctx->dsi;

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	regulator_disable(ctx->vsn);
	regulator_disable(ctx->vsp);
	regulator_disable(ctx->vdd);
	usleep_range(5000, 6000);
	return 0;
}

static int hw_nt51021_prepare(struct drm_panel *panel)
{
	struct hw_nt51021 *ctx = to_hw_nt51021(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = hw_nt51021_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		//regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	/*ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}*/

	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret) {
		dev_err(dev, "failed to exit sleep mode (%d)\n", ret);
		return ret;
	}
	/* Up to 120 ms */
	usleep_range(120000, 150000);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret) {
		dev_err(dev, "failed to turn display on (%d)\n", ret);
		return ret;
	}
	/* Some 10 ms */
	usleep_range(10000, 20000);

		ret = mipi_dsi_turn_on_peripheral(dsi);
		if (ret) {
			dev_err(dev, "failed to turn on peripheral(%d)\n", ret);
		return ret;
		}

	ret = mipi_dsi_dcs_nop(dsi);
	if (ret < 0) {
		dev_err(dev, "failed to send DCS nop: %d\n", ret);
		return ret;
	}
	usleep_range(100, 200);

	ctx->prepared = true;
	return 0;
}

static int hw_nt51021_enable(struct drm_panel *panel)
{
	struct hw_nt51021 *ctx = to_hw_nt51021(panel);

	if (ctx->enabled)
		return 0;

	ctx->enabled = true;
	return 0;
}

static int hw_nt51021_unprepare(struct drm_panel *panel)
{
	struct hw_nt51021 *ctx = to_hw_nt51021(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret) {
		dev_err(dev, "failed to turn display off (%d)\n", ret);
		return ret;
	}
	usleep_range(10000, 20000);

	/* Enter sleep mode */
	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret) {
		dev_err(dev, "failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	/* Wait 4 frames, how much is that 5ms in the vendor driver */
	usleep_range(5000, 10000);

	ret = hw_nt51021_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	ctx->prepared = false;
	return 0;
}

static int hw_nt51021_disable(struct drm_panel *panel)
{
	struct hw_nt51021 *ctx = to_hw_nt51021(panel);

	if (!ctx->enabled)
		return 0;


	ctx->enabled = false;
	return 0;
}

static const struct drm_display_mode hw_nt51021_mode = {
	.clock = (1200 + 60 + 24 + 80) * (1920 + 14 + 2 + 10) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 60,
	.hsync_end = 1200 + 60 + 24,
	.htotal = 1200 + 60 + 24 + 80,
	.vdisplay = 1920,
	.vsync_start = 1920 + 14,
	.vsync_end = 1920 + 14 + 2,
	.vtotal = 1920 + 14 + 2 + 10,
	.width_mm = 135,
	.height_mm = 217,
};

static int hw_nt51021_get_modes(struct drm_panel *panel,
					  struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &hw_nt51021_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs hw_nt51021_panel_funcs = {
	.prepare = hw_nt51021_prepare,
	.enable = hw_nt51021_enable,
	.unprepare = hw_nt51021_unprepare,
	.disable = hw_nt51021_disable,
	.get_modes = hw_nt51021_get_modes,
};

static int hw_nt51021_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0)
		return ret;

	//dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct backlight_ops hw_nt51021_bl_ops = {
	.update_status = hw_nt51021_bl_update_status,
};

static struct backlight_device *
hw_nt51021_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 255,
		.max_brightness = 255,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &hw_nt51021_bl_ops, &props);
}

static int hw_nt51021_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct hw_nt51021 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->vsp = devm_regulator_get(dev, "vsp");
	if (IS_ERR(ctx->vsp))
		return PTR_ERR(ctx->vsp);

	ctx->vsn = devm_regulator_get(dev, "vsn");
	if (IS_ERR(ctx->vsn))
		return PTR_ERR(ctx->vsn);

	ctx->vdd = devm_regulator_get(dev, "pp3300");
	if (IS_ERR(ctx->vdd))

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			MIPI_DSI_MODE_NO_EOT_PACKET |  MIPI_DSI_MODE_VIDEO_HSE;// |
			 // MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &hw_nt51021_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ctx->panel.backlight = hw_nt51021_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void hw_nt51021_remove(struct mipi_dsi_device *dsi)
{
	struct hw_nt51021 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id hw_nt51021_of_match[] = {
	{ .compatible = "huawei,nt51021" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hw_nt51021_of_match);

static struct mipi_dsi_driver hw_nt51021_driver = {
	.probe = hw_nt51021_probe,
	.remove = hw_nt51021_remove,
	.driver = {
		.name = "panel-huawei-nt51021",
		.of_match_table = hw_nt51021_of_match,
	},
};
module_mipi_dsi_driver(hw_nt51021_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for HUAWEI_NT51021");
MODULE_LICENSE("GPL v2");
