/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_mod_scaler
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <drivers/input_processor.h>
#include <zmk/hid.h>

struct mod_scaler_config {
    uint8_t mods;
    int32_t multiplier;
    int32_t divisor;
};

struct mod_scaler_data {
    int32_t remainder[2];
};

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct mod_scaler_data *data = dev->data;
    const struct mod_scaler_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    int axis = event->code == INPUT_REL_X ? 0 : 1;
    if (!(zmk_hid_get_explicit_mods() & cfg->mods)) {
        data->remainder[axis] = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }
    /* Keep the remainder so slow movement still adds up. */
    int32_t scaled = event->value * cfg->multiplier + data->remainder[axis];
    event->value = scaled / cfg->divisor;
    data->remainder[axis] = scaled % cfg->divisor;
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, divisor) > 0, "divisor must be positive");                        \
    static struct mod_scaler_data data_##n;                                                        \
    static const struct mod_scaler_config config_##n = {                                           \
        .mods = DT_INST_PROP(n, mods),                                                             \
        .multiplier = DT_INST_PROP(n, multiplier),                                                 \
        .divisor = DT_INST_PROP(n, divisor),                                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, POST_KERNEL,                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)

#endif
