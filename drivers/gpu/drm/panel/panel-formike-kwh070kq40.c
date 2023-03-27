// SPDX-License-Identifier: GPL-2.0+

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>

#include <linux/gpio/consumer.h>

#include <drm/drm_connector.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct kwh070kq40_panel
{
    struct drm_panel       panel;
    struct mipi_dsi_device *dsi;

    const struct drm_display_mode *mode;

    struct
    {
        struct gpio_desc *power;
        struct gpio_desc *reset;
        struct gpio_desc *updn;
        struct gpio_desc *shlr;
    }                             gpios;

    struct
    {
        bool test_pattern;
    }                             config;
};

static inline struct kwh070kq40_panel *
panel_to_kwh070kq40_panel(struct drm_panel *panel)
{
    return container_of(panel,
    struct kwh070kq40_panel, panel);
}

static int kwh070kq40_panel_prepare(struct drm_panel *panel)
{
    struct kwh070kq40_panel *pnl = panel_to_kwh070kq40_panel(panel);
    struct mipi_dsi_device  *dsi = pnl->dsi;
    struct device           *dev = &pnl->dsi->dev;
    ssize_t                 ret;

    printk(KERN_INFO
    "kwh070kq40_panel_prepare\n");

    msleep(20); //20ms

    gpiod_set_value(pnl->gpios.power, 1);
    msleep(20); //20ms
    gpiod_set_value(pnl->gpios.reset, 1);
    msleep(10); //10ms
    gpiod_set_value(pnl->gpios.reset, 0);
    msleep(10); //10ms
    gpiod_set_value(pnl->gpios.reset, 1);

    msleep(120); //120ms

    if (pnl->config.test_pattern)
    {
        ret = mipi_dsi_dcs_write(dsi, 0x87, (u8[]) {0x5A}, 1); //enable commands
        if (ret < 0)
        {
            dev_err(dev, "failed to enable commands: %zd\n", ret);
            return ret;
        }

        ret = mipi_dsi_dcs_write(dsi, 0xB1, (u8[]) {0x08}, 1); //set to test
        printk(KERN_INFO
        "kwh070kq40_panel_prepare satting test mode\n");
        if (ret < 0)
        {
            dev_err(dev, "failed to set test mode: %zd\n", ret);
            return ret;
        }
    }

    /*ret = mipi_dsi_dcs_write(dsi, 0x87, (u8[]){ 0x5A }, 1); //enable commands
    if (ret < 0) {
        dev_err(dev, "failed to enable commands: %d\n", ret);
        return ret;
    }

    ret = mipi_dsi_dcs_write(dsi, 0xB2, (u8[]){ 0x70 }, 1); //set to 4 lane
    if (ret < 0) {
        dev_err(dev, "failed to set 2lane: %d\n", ret);
        return ret;
    }*/

    /*ret = mipi_dsi_dcs_write(dsi, 0xB1, (u8[]){ 0x08 }, 1); //set to test
    if (ret < 0) {
        dev_err(dev, "failed to set test mode: %d\n", ret);
        return ret;
    }*/

    return 0;
}

static int kwh070kq40_panel_enable(struct drm_panel *panel)
{
    struct kwh070kq40_panel *pnl = panel_to_kwh070kq40_panel(panel);
    int                     ret;

    printk(KERN_INFO
    "kwh070kq40_panel_enable\n");

    pnl->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

    ret = mipi_dsi_dcs_set_tear_on(pnl->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
    if (ret)
        return ret;

    ret = mipi_dsi_dcs_exit_sleep_mode(pnl->dsi);
    if (ret)
        return ret;

    msleep(120);

    mipi_dsi_dcs_set_display_on(pnl->dsi);
    printk("kwh070kq40_panel_enable2");
    return 0;
}

static int kwh070kq40_panel_disable(struct drm_panel *panel)
{
    struct kwh070kq40_panel *pnl = panel_to_kwh070kq40_panel(panel);

    printk(KERN_INFO
    "kwh070kq40_panel_disable\n");

    return mipi_dsi_dcs_enter_sleep_mode(pnl->dsi);
}

static int kwh070kq40_panel_unprepare(struct drm_panel *panel)
{
    struct kwh070kq40_panel *pnl = panel_to_kwh070kq40_panel(panel);

    printk(KERN_INFO
    "kwh070kq40_panel_unprepare\n");

    gpiod_set_value(pnl->gpios.reset, 0);
    gpiod_set_value(pnl->gpios.power, 0);

    return 0;
}

static int of_parse_display_timing(const struct device_node *np,
                                   struct drm_display_mode *dt)
{
    u32 val = 0;
    int ret = 0;

    //	memset(dt, 0, sizeof(*dt));

    ret |= of_property_read_u32(np, "clock-frequency", &val);
    dt->clock = val;
    ret |= of_property_read_u32(np, "hdisplay", &val);
    dt->hsync_start = val;
    ret |= of_property_read_u32(np, "hsync_start", &val);
    dt->hsync_start = val;
    ret |= of_property_read_u32(np, "hsync_end", &val);
    dt->hsync_end = val;
    ret |= of_property_read_u32(np, "htotal", &val);
    dt->htotal = val;
    ret |= of_property_read_u32(np, "vdisplay", &val);
    dt->vdisplay = val;
    ret |= of_property_read_u32(np, "vsync_start", &val);
    dt->vsync_start = val;
    ret |= of_property_read_u32(np, "vsync_end", &val);
    dt->vsync_end = val;
    ret |= of_property_read_u32(np, "vtotal", &val);
    dt->vtotal = val;

    if (ret)
    {
        pr_err("%pOF: error reading timing properties\n", np);
        return -EINVAL;
    }

    return 0;
}

int of_get_display_timing(const struct device_node *np, const char *name,
                          struct drm_display_mode *dt)
{
    struct device_node *timing_np;
    int                ret;

    if (!np)
        return -EINVAL;

    timing_np = of_get_child_by_name(np, name);
    if (!timing_np)
        return -ENOENT;

    ret = of_parse_display_timing(timing_np, dt);

    of_node_put(timing_np);

    return ret;
}

static const struct drm_display_mode default_mode = {
        .clock = 55555,
        .hdisplay = 1024,
        .hsync_start = 1024 + 160,
        .hsync_end = 1024 + 160 + 80,
        .htotal = 1024 + 160 + 80 + 80,
        .vdisplay = 600,
        .vsync_start = 600 + 12,
        .vsync_end = 600 + 12 + 10,
        .vtotal = 600 + 12 + 10 + 13,

        .width_mm = 154,
        .height_mm = 86,
};

static int kwh070kq40_panel_get_modes(struct drm_panel *panel,
                                      struct drm_connector *connector)
{
    struct kwh070kq40_panel *pnl       = panel_to_kwh070kq40_panel(panel);
    struct drm_display_mode *mode;
    static const u32        bus_format = MEDIA_BUS_FMT_RGB888_1X24;
    int                     ret;

    printk(KERN_INFO
    "kwh070kq40_panel_get_modes\n");

    //	pnl->mode = drm_mode_create(connector->dev);

    mode = drm_mode_duplicate(connector->dev, &default_mode);
    if (!mode)
    {
        dev_err(&pnl->dsi->dev, "Failed to add mode "
        DRM_MODE_FMT
        "\n", DRM_MODE_ARG(&default_mode));
        return -EINVAL;
    }

    ret = of_get_display_timing(panel->dev->of_node, "panel-timing", mode);
    if (ret < 0)
    {
        dev_err(panel->dev, "%pOF: no panel-timing node found for \"panel-dsi\" binding\n", panel->dev->of_node);
        return ret;
    }

    drm_mode_set_name(mode);

    mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    drm_mode_probed_add(connector, mode);

    connector->display_info.bpc       = 8;
    connector->display_info.width_mm  = mode->width_mm;
    connector->display_info.height_mm = mode->height_mm;
    drm_display_info_set_bus_formats(&connector->display_info, &bus_format,
                                     1);

    printk(KERN_ERR
    "Requested mode: "
    DRM_MODE_FMT
    "\n",
            DRM_MODE_ARG(mode));

    return 1;
}

static const struct drm_panel_funcs kwh070kq40_panel_funcs = {
        .prepare = kwh070kq40_panel_prepare,
        .enable = kwh070kq40_panel_enable,
        .disable = kwh070kq40_panel_disable,
        .unprepare = kwh070kq40_panel_unprepare,
        .get_modes = kwh070kq40_panel_get_modes,
};

static int kwh070kq40_panel_dsi_probe(struct mipi_dsi_device *dsi)
{
    struct kwh070kq40_panel *pnl;
    struct device_node      *config_np;
    //	struct drm_display_mode *mode;
    int                     ret;
    u32                     val = 0;

    printk(KERN_INFO
    "kwh070kq40_panel_dsi_probe\n");

    pnl = devm_kzalloc(&dsi->dev, sizeof(*pnl), GFP_KERNEL);
    if (!pnl)
        return -ENOMEM;

    mipi_dsi_set_drvdata(dsi, pnl);
    pnl->dsi = dsi;

    drm_panel_init(&pnl->panel, &dsi->dev, &kwh070kq40_panel_funcs, DRM_MODE_CONNECTOR_DSI);

    pnl->panel.prepare_upstream_first = true;

    //	ret = of_get_display_timing(dsi->dev.of_node, "panel-timing", pnl->mode);
    //	if (ret < 0) {
    //		dev_err(&dsi->dev, "%pOF: no panel-timing node found for \"panel-dsi\" binding\n",
    //			dsi->dev.of_node);
    //		return ret;
    //	}

    pnl->gpios.reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(pnl->gpios.reset))
    {
        dev_err(&dsi->dev, "Couldn't get our reset GPIO\n");
        return PTR_ERR(pnl->gpios.reset);
    }

    pnl->gpios.power = devm_gpiod_get(&dsi->dev, "power", GPIOD_OUT_LOW);
    if (IS_ERR(pnl->gpios.power))
    {
        dev_err(&dsi->dev, "Couldn't get our power GPIO\n");
        return PTR_ERR(pnl->gpios.power);
    }

    pnl->gpios.updn = devm_gpiod_get(&dsi->dev, "updn", GPIOD_OUT_LOW);
    if (IS_ERR(pnl->gpios.updn))
    {
        dev_err(&dsi->dev, "Couldn't get our updn GPIO\n");
        return PTR_ERR(pnl->gpios.updn);
    }
    pnl->gpios.shlr = devm_gpiod_get(&dsi->dev, "shlr", GPIOD_OUT_LOW);
    if (IS_ERR(pnl->gpios.shlr))
    {
        dev_err(&dsi->dev, "Couldn't get our shlr GPIO\n");
        return PTR_ERR(pnl->gpios.shlr);
    }

    config_np = of_get_child_by_name(pnl->panel.dev->of_node, "panel-config");
    if (config_np)
    {
        ret |= of_property_read_u32(config_np, "test-pattern", &val);
        if (!ret)
        {
            pnl->config.test_pattern = val;
            printk(KERN_INFO
            "kwh070kq40_panel_dsi_probe test pattern\n");
        }
    }
    else
    {
        printk(KERN_INFO
        "kwh070kq40_panel_dsi_probe failed loading panel config from device tree\n");
    }

    /*ret = drm_panel_of_backlight(&pnl->panel);
    if (ret)
        return ret;*/

    drm_panel_add(&pnl->panel);

    dsi->mode_flags = MIPI_DSI_MODE_VIDEO_HSE |
                      MIPI_DSI_MODE_VIDEO |
                      MIPI_DSI_MODE_VIDEO_BURST |
                      MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
    dsi->format     = MIPI_DSI_FMT_RGB888;
    dsi->lanes      = 4;

    ret = mipi_dsi_attach(dsi);
    if (ret < 0)
    {
        dev_err(&dsi->dev, "Couldn't attach DSI panel\n");
        drm_panel_remove(&pnl->panel);
        return ret;
    }

    return 0;
}

static int kwh070kq40_panel_dsi_remove(struct mipi_dsi_device *dsi)
{
    struct kwh070kq40_panel *pnl = mipi_dsi_get_drvdata(dsi);

    printk(KERN_INFO
    "kwh070kq40_panel_dsi_remove\n");

    mipi_dsi_detach(dsi);
    drm_panel_remove(&pnl->panel);

    return 0;
}

static const struct of_device_id kwh070kq40_panel_of_match[] = {
        {.compatible = "formike,kwh070kq40"},
        {},
};

MODULE_DEVICE_TABLE(of, kwh070kq40_panel_of_match
);

static struct mipi_dsi_driver kwh070kq40_panel_driver = {
        .probe = kwh070kq40_panel_dsi_probe,
        .remove = kwh070kq40_panel_dsi_remove,
        .driver = {
                .name = "panel-formike-kwh070kq40",
                .of_match_table    = kwh070kq40_panel_of_match,
        },
};

module_mipi_dsi_driver(kwh070kq40_panel_driver);

MODULE_AUTHOR("BootsyZ ");
MODULE_DESCRIPTION("Formike KWH070KQ40 panel driver");
MODULE_LICENSE("GPL v2");
