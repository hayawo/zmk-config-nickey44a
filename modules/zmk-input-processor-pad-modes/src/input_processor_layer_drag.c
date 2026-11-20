/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_layer_drag
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct layer_drag_config {
    const struct device *input_dev;
    int32_t layer;
    uint16_t button_code;
    uint16_t touch_code;
    int32_t move_threshold;
};

struct layer_drag_data {
    int32_t travel; /* movement since touch-down, before grabbing */
    bool touching;
    bool held;
    bool press_pending; /* injected press still in the queue */
};

/* Runs on the input thread only, so no locking is needed. Injected events are
 * queued with K_NO_WAIT: this thread drains the queue, so it must not block. */
static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct layer_drag_data *data = dev->data;
    const struct layer_drag_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY && event->code == cfg->button_code && event->value &&
        data->press_pending) {
        data->press_pending = false; /* our press has reached the host side */
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        data->touching = event->value != 0;
        data->travel = 0;
        if (!data->touching && data->held) {
            data->held = false;
            data->press_pending = false;
            input_report_key(cfg->input_dev, cfg->button_code, 0, true, K_NO_WAIT);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL || (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->value != 0 && data->touching && !data->held &&
        zmk_keymap_layer_active(cfg->layer)) {
        /* Ignore jitter (e.g. a two-finger tap) until the finger really moves,
         * then press the button where the cursor still is. */
        data->travel += event->value < 0 ? -event->value : event->value;
        if (data->travel < cfg->move_threshold) {
            event->value = 0;
            return ZMK_INPUT_PROC_CONTINUE;
        }
        if (input_report_key(cfg->input_dev, cfg->button_code, 1, true, K_NO_WAIT) == 0) {
            data->held = true;
            data->press_pending = true;
        }
    }
    if (data->press_pending) {
        /* Hold the cursor still until the queued press has been sent. */
        event->value = 0;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    static struct layer_drag_data data_##n;                                                        \
    static const struct layer_drag_config config_##n = {                                           \
        .input_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, input_device)),                              \
        .layer = DT_INST_PROP(n, layer),                                                           \
        .button_code = DT_INST_PROP(n, button_code),                                               \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .move_threshold = DT_INST_PROP(n, move_threshold),                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &data_##n, &config_##n, APPLICATION,                      \
                          CONFIG_APPLICATION_INIT_PRIORITY, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)

#endif
