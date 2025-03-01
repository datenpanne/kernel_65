// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Raffaele Tranquillini <raffaele.tranquillini@gmail.com>
 *
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct jdi_r69429_1200p {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct regulator_bulk_data supplies[4];
	struct gpio_desc *reset_gpio;
	//struct gpio_desc *vsn_gpio;
	//struct gpio_desc *vsp_gpio;
	struct gpio_desc *vled_gpio;
	struct gpio_desc *blen_gpio;
	struct gpio_desc *vcc_gpio;
	//struct gpio_desc *tp_vci_gpio;
	//struct backlight_device *backlight;
	
	bool prepared;
        bool enabled;
};

static inline struct jdi_r69429_1200p *to_jdi_r69429_1200p(struct drm_panel *panel)
{
	return container_of(panel, struct jdi_r69429_1200p, panel);
}

static void jdi_r69429_1200p_reset(struct jdi_r69429_1200p *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
}

static void jdi_r69429_1200p_power_on(struct jdi_r69429_1200p *ctx)
{
	gpiod_set_value_cansleep(ctx->vcc_gpio, 1);
	usleep_range(1000, 2000);
	//gpiod_set_value_cansleep(ctx->blen_gpio, 1);
	//usleep_range(5000, 6000);
	/*gpiod_set_value_cansleep(ctx->vsp_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->vsn_gpio, 1);
	usleep_range(5000, 6000);*/
	gpiod_set_value_cansleep(ctx->vled_gpio, 1);
	usleep_range(5000, 6000);
}

static void jdi_r69429_1200p_power_off(struct jdi_r69429_1200p *ctx)
{
	gpiod_set_value_cansleep(ctx->vcc_gpio, 0);
	usleep_range(5000, 6000);
	//gpiod_set_value_cansleep(ctx->blen_gpio, 0);
	//usleep_range(5000, 6000);
	/*gpiod_set_value_cansleep(ctx->vsp_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->vsn_gpio, 0);
	usleep_range(5000, 6000);*/
	gpiod_set_value_cansleep(ctx->vled_gpio, 0);
	usleep_range(5000, 6000);
}

static int jdi_r69429_1200p_on(struct jdi_r69429_1200p *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xB3, 0x04, 0x08, 0x00, 0x22, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xB6, 0x3A, 0xD3);
	mipi_dsi_generic_write_seq(dsi, 0xB8, 0x07, 0x90, 0x1E, 0x00, 0x1E, 0x32);
	mipi_dsi_generic_write_seq(dsi, 0xB9, 0x07, 0x82, 0x3c, 0x00, 0x3C, 0x87);
	mipi_dsi_generic_write_seq(dsi, 0xBA, 0x07, 0x9E, 0x20, 0x00, 0x20, 0x8F);
	mipi_dsi_generic_write_seq(dsi, 0xCE, 0x7D, 0x40, 0x43, 0x49, 0x55, 0x62, 0x71, 0x82, 0x94, 0xA8, 0xB9, 0xCB, 0xDB, 0xE9, 0xF5, 0xFC, 0xFF, 0x01, 0x38, 0x02, 0x02, 0x44, 0x24);
	mipi_dsi_generic_write_seq(dsi, 0xD6, 0x01);
	mipi_dsi_generic_write_seq(dsi, 0xC6, 0x78, 0x01, 0x45, 0x05, 0x67, 0x67, 0x0A, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0A, 0x19, 0x05);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0x77);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_column_address(dsi, 0x0000, 0x04AF);
	if (ret < 0) {
		dev_err(dev, "Failed to set column address: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_page_address(dsi, 0x0000, 0x077F);
	if (ret < 0) {
		dev_err(dev, "Failed to set page address: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_tear_scanline(dsi, 0x077F);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear scanline: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_MEMORY_START, 0x00);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x00);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0x5E, 0x06);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x01);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(80);

	return 0;
}

static int jdi_r69429_1200p_off(struct jdi_r69429_1200p *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(15000, 20000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	return 0;
}

static int jdi_r69429_1200p_prepare(struct drm_panel *panel)
{
	struct jdi_r69429_1200p *ctx = to_jdi_r69429_1200p(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	gpiod_set_value_cansleep(ctx->vcc_gpio, 1);
	usleep_range(5000, 6000);

	gpiod_set_value_cansleep(ctx->vled_gpio, 1);
	usleep_range(5000, 6000);

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "regulator enable failed, %d\n", ret);
		return ret;
	}
	msleep(80);
	//usleep_range(2000, 5000);

	jdi_r69429_1200p_reset(ctx);
	//msleep(80);

	/*gpiod_set_value_cansleep(ctx->vsp_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->vsn_gpio, 1);
	usleep_range(5000, 6000);*/

      //jdi_r69429_1200p_power_on(ctx);

	ret = jdi_r69429_1200p_on(ctx);
	usleep_range(10000, 11000);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int jdi_r69429_1200p_enable(struct drm_panel *panel)
{
	struct jdi_r69429_1200p *ctx = to_jdi_r69429_1200p(panel);

	if (ctx->enabled)
		return 0;

	gpiod_set_value_cansleep(ctx->blen_gpio, 1);
	usleep_range(5000, 6000);
	//backlight_enable(ctx->backlight);

	ctx->enabled = true;

	return 0;
}

static int jdi_r69429_1200p_unprepare(struct drm_panel *panel)
{
	struct jdi_r69429_1200p *ctx = to_jdi_r69429_1200p(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = jdi_r69429_1200p_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

        jdi_r69429_1200p_power_off(ctx);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static int jdi_r69429_1200p_disable(struct drm_panel *panel)
{
	struct jdi_r69429_1200p *ctx = to_jdi_r69429_1200p(panel);

	if (!ctx->enabled)
		return 0;
	gpiod_set_value_cansleep(ctx->blen_gpio, 0);
	usleep_range(5000, 6000);
	//gpiod_set_value_cansleep(ctx->tp_vci_gpio, 0);
	//backlight_disable(ctx->backlight);

	ctx->enabled = false;

	return 0;
}

static const struct drm_display_mode jdi_r69429_1200p_mode = {
	.clock = (1200 + 80 + 16 + 64) * (1920 + 32 + 16 + 32) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 80,
	.hsync_end = 1200 + 80 + 16,
	.htotal = 1200 + 80 + 16 + 64,
	.vdisplay = 1920,
	.vsync_start = 1920 + 32,
	.vsync_end = 1920 + 32 + 16,
	.vtotal = 1920 + 32 + 16 + 32,
	.width_mm = 95,
	.height_mm = 151,
};

static int jdi_r69429_1200p_get_modes(struct drm_panel *panel,
				    struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &jdi_r69429_1200p_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs jdi_r69429_1200p_panel_funcs = {
	.prepare = jdi_r69429_1200p_prepare,
        .enable = jdi_r69429_1200p_enable,
	.unprepare = jdi_r69429_1200p_unprepare,
	.disable = jdi_r69429_1200p_disable,
	.get_modes = jdi_r69429_1200p_get_modes,
};

static int dsi_dcs_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	int ret;
	u16 brightness = bl->props.brightness;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness & 0x04;
}

static int dsi_dcs_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct backlight_ops dsi_bl_ops = {
	.update_status = dsi_dcs_bl_update_status,
	.get_brightness = dsi_dcs_bl_get_brightness,
};

static struct backlight_device *
drm_panel_create_dsi_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = 255;
	props.max_brightness = 255;

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &dsi_bl_ops, &props);
}

static int jdi_r69429_1200p_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct jdi_r69429_1200p *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vddio";
	ctx->supplies[1].supply = "vddio-incell";
	ctx->supplies[2].supply = "vsp";
	ctx->supplies[3].supply = "vsn";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->vcc_gpio = devm_gpiod_get(dev, "vcc", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vcc_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->vcc_gpio),
				     "Failed to get vcc-gpios\n");

	/*ctx->tp_vci_gpio = devm_gpiod_get(dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->tp_vci_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->tp_vci_gpio),
				     "Failed to get vci-gpios\n");*/

	ctx->blen_gpio = devm_gpiod_get(dev, "backlight", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->blen_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->blen_gpio),
				     "Failed to get backlight-gpios\n");

	/*ctx->vsp_gpio = devm_gpiod_get(dev, "vsp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vsp_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->vsp_gpio),
				     "Failed to get vsp-gpios\n");

	ctx->vsn_gpio = devm_gpiod_get(dev, "vsn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vsn_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->vsn_gpio),
				     "Failed to get vsn-gpios\n");*/

	ctx->vled_gpio = devm_gpiod_get(dev, "vled", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vled_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->vled_gpio),
				     "Failed to get vled-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_VIDEO_HSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &jdi_r69429_1200p_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = drm_panel_create_dsi_backlight(ctx->dsi);
	if (IS_ERR(ctx->panel.backlight)) {
		ret = PTR_ERR(ctx->panel.backlight);
		dev_err(dev, "failed to register backlight %d\n", ret);
		return ret;
	}

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		return ret;
	}

	return 0;
}

static void jdi_r69429_1200p_remove(struct mipi_dsi_device *dsi)
{
	struct jdi_r69429_1200p *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id jdi_r69429_1200p_of_match[] = {
	{ .compatible = "pele,jdi-r69429" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jdi_r69429_1200p_of_match);

static struct mipi_dsi_driver jdi_r69429_1200p_driver = {
	.probe = jdi_r69429_1200p_probe,
	.remove = jdi_r69429_1200p_remove,
	.driver = {
		.name = "panel-pele-jdi--r69429",
		.of_match_table = jdi_r69429_1200p_of_match,
	},
};
module_mipi_dsi_driver(jdi_r69429_1200p_driver);

MODULE_AUTHOR("Raffaele Tranquillini <raffaele.tranquillini@gmail.com>");
MODULE_DESCRIPTION("DRM driver for JDI WUXGA r69429 DSI panel, command mode");
MODULE_LICENSE("GPL v2");
