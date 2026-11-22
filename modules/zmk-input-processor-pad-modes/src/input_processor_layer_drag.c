/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_layer_drag
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "layer-drag supports a single instance");

/*
 * While `layer` is active, the first real cursor movement presses the drag
 * button (or the layer activation itself, if the finger is already down). The button then stays down, even across finger lifts, until the
 * layer is released, so the finger can be repositioned on a small pad.
 */
struct layer_drag_config {
    const struct device *input_dev;
    int32_t layer;
    uint16_t button_code;
    uint16_t touch_code;
    int32_t move_threshold;
};

struct layer_drag_data {
    struct k_mutex lock;
    int32_t travel;     /* movement since touch-down, before grabbing */
    bool touching;
    bool held;          /* we pressed the button and have not released it */
    bool press_pending; /* injected press still in the input queue */
};

static struct layer_drag_data layer_drag_data_0;
static const struct layer_drag_config layer_drag_config_0 = {
    .input_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, input_device)),
    .layer = DT_INST_PROP(0, layer),
    .button_code = DT_INST_PROP(0, button_code),
    .touch_code = DT_INST_PROP(0, touch_code),
    .move_threshold = DT_INST_PROP(0, move_threshold),
};

/* Caller holds data->lock. Never blocks: the input thread drains this queue. */
static void release_button(struct layer_drag_data *data, const struct layer_drag_config *cfg) {
    if (!data->held) {
        return;
    }
    data->held = false;
    data->press_pending = false;
    input_report_key(cfg->input_dev, cfg->button_code, 0, true, K_NO_WAIT);
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct layer_drag_data *data = dev->data;
    const struct layer_drag_config *cfg = dev->config;

    k_mutex_lock(&data->lock, K_FOREVER);

    if (event->type == INPUT_EV_KEY && event->code == cfg->button_code && event->value &&
        data->press_pending) {
        data->press_pending = false; /* our press has gone through */
    } else if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        data->touching = event->value != 0;
        data->travel = 0;
        /* Lifting keeps the drag while the layer is held; otherwise drop. */
        if (!data->touching && !zmk_keymap_layer_active(cfg->layer)) {
            release_button(data, cfg);
        }
    } else if (event->type == INPUT_EV_REL &&
               (event->code == INPUT_REL_X || event->code == INPUT_REL_Y)) {
        if (event->value != 0 && data->touching && !data->held &&
            zmk_keymap_layer_active(cfg->layer)) {
            /* Ignore jitter (e.g. a two-finger tap) until the finger really
             * moves, then press the button where the cursor still is. */
            data->travel += event->value < 0 ? -event->value : event->value;
            if (data->travel < cfg->move_threshold) {
                event->value = 0;
            } else if (input_report_key(cfg->input_dev, cfg->button_code, 1, true, K_NO_WAIT) ==
                       0) {
                data->held = true;
                data->press_pending = true;
            }
        }
        if (data->press_pending) {
            /* Hold the cursor still until the queued press has been sent. */
            event->value = 0;
        }
    }

    k_mutex_unlock(&data->lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

static int layer_drag_layer_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    struct layer_drag_data *data = &layer_drag_data_0;
    const struct layer_drag_config *cfg = &layer_drag_config_0;

    if (ev == NULL || ev->layer != cfg->layer) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    k_mutex_lock(&data->lock, K_FOREVER);
    if (!ev->state) {
        /* Layer released: drop the drag. */
        release_button(data, cfg);
    } else if (data->touching && !data->held) {
        /* The finger was already on the pad when the layer came on: grab now,
         * before any further movement carries the cursor off the target. */
        if (input_report_key(cfg->input_dev, cfg->button_code, 1, true, K_NO_WAIT) == 0) {
            data->held = true;
            data->press_pending = true;
        }
    }
    k_mutex_unlock(&data->lock);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(layer_drag, layer_drag_layer_listener);
ZMK_SUBSCRIPTION(layer_drag, zmk_layer_state_changed);

static int layer_drag_init(const struct device *dev) {
    struct layer_drag_data *data = dev->data;

    k_mutex_init(&data->lock);
    return 0;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

DEVICE_DT_INST_DEFINE(0, layer_drag_init, NULL, &layer_drag_data_0, &layer_drag_config_0,
                      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY, &api);

#endif
