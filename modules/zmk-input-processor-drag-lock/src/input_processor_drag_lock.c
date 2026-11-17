/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_drag_lock
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

enum drag_state {
    DRAG_IDLE,      /* button up */
    DRAG_PRESSED,   /* button down, finger holding (press-and-hold active) */
    DRAG_LOCKED,    /* hold ended, button kept down by us */
    DRAG_RELEASING, /* our injected release is on its way */
};

struct drag_lock_config {
    const struct device *input_dev;
    uint16_t button_code;
    uint16_t touch_code;
    int32_t tap_ms;
    int32_t lock_ms;
};

struct drag_lock_data {
    const struct device *dev;
    struct k_work_delayable release_work;
    struct k_mutex lock;
    enum drag_state state;
    int64_t press_time;
    bool locked_press; /* a new press arrived while locked */
};

static void release_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct drag_lock_data *data = CONTAINER_OF(dwork, struct drag_lock_data, release_work);
    const struct drag_lock_config *cfg = data->dev->config;

    k_mutex_lock(&data->lock, K_FOREVER);
    bool release = data->state == DRAG_LOCKED && !data->locked_press;
    if (release) {
        data->state = DRAG_RELEASING;
    }
    k_mutex_unlock(&data->lock);

    if (release) {
        LOG_DBG("drag lock timeout: drop");
        input_report_key(cfg->input_dev, cfg->button_code, 0, true, K_FOREVER);
    }
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct drag_lock_data *data = dev->data;
    const struct drag_lock_config *cfg = dev->config;
    int ret = ZMK_INPUT_PROC_CONTINUE;

    if (event->type != INPUT_EV_KEY) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_mutex_lock(&data->lock, K_FOREVER);
    int64_t now = k_uptime_get();

    if (event->code == cfg->touch_code) {
        if (data->state == DRAG_LOCKED) {
            if (event->value) {
                /* Finger is back: keep the button down while it moves. */
                k_work_cancel_delayable(&data->release_work);
            } else if (!data->locked_press) {
                k_work_reschedule(&data->release_work, K_MSEC(cfg->lock_ms));
            }
        }
    } else if (event->code == cfg->button_code) {
        switch (data->state) {
        case DRAG_IDLE:
            if (event->value) {
                data->state = DRAG_PRESSED;
                data->press_time = now;
            }
            break;
        case DRAG_PRESSED:
            if (!event->value) {
                if (now - data->press_time < cfg->tap_ms) {
                    data->state = DRAG_IDLE; /* plain tap/click */
                } else {
                    /* Long press ended: hold the button and wait for a re-touch. */
                    data->state = DRAG_LOCKED;
                    data->locked_press = false;
                    k_work_reschedule(&data->release_work, K_MSEC(cfg->lock_ms));
                    ret = ZMK_INPUT_PROC_STOP;
                }
            }
            break;
        case DRAG_LOCKED:
            if (event->value) {
                /* Button is already down on the host; swallow the new press. */
                data->locked_press = true;
                data->press_time = now;
                k_work_cancel_delayable(&data->release_work);
                ret = ZMK_INPUT_PROC_STOP;
            } else if (data->locked_press && now - data->press_time < cfg->tap_ms) {
                /* Tap while locked: drop now (let this release through). */
                data->state = DRAG_IDLE;
                data->locked_press = false;
            } else {
                /* Another long press ended: stay locked. */
                data->locked_press = false;
                k_work_reschedule(&data->release_work, K_MSEC(cfg->lock_ms));
                ret = ZMK_INPUT_PROC_STOP;
            }
            break;
        case DRAG_RELEASING:
            if (!event->value) {
                data->state = DRAG_IDLE; /* our injected release */
            } else {
                data->state = DRAG_PRESSED;
                data->press_time = now;
            }
            break;
        }
    }

    k_mutex_unlock(&data->lock);
    return ret;
}

static int drag_lock_init(const struct device *dev) {
    struct drag_lock_data *data = dev->data;

    data->dev = dev;
    data->state = DRAG_IDLE;
    k_mutex_init(&data->lock);
    k_work_init_delayable(&data->release_work, release_work_handler);
    return 0;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, tap_ms) > 0, "tap-ms must be positive");                          \
    BUILD_ASSERT(DT_INST_PROP(n, lock_ms) > 0, "lock-ms must be positive");                        \
    static struct drag_lock_data data_##n;                                                         \
    static const struct drag_lock_config config_##n = {                                            \
        .input_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, input_device)),                              \
        .button_code = DT_INST_PROP(n, button_code),                                               \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .tap_ms = DT_INST_PROP(n, tap_ms),                                                         \
        .lock_ms = DT_INST_PROP(n, lock_ms),                                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, drag_lock_init, NULL, &data_##n, &config_##n, APPLICATION,            \
                          CONFIG_APPLICATION_INIT_PRIORITY, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)

#endif
