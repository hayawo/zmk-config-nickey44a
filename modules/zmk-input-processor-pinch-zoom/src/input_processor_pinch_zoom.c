/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_pinch_zoom
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "pinch-zoom requires a central-side devicetree instance"
#endif

#include <drivers/input_processor.h>
#include <zmk/events/keycode_state_changed.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct pinch_zoom_config {
    uint16_t zoom_code;
    uint16_t wheel_code;
    uint16_t touch_code;
    int32_t divisor;
    bool invert;
    uint32_t modifier;
    int32_t release_ms;
};

struct pinch_zoom_data {
    const struct device *dev;
    struct k_work_delayable release_work;
    struct k_mutex lock;
    int32_t accum;
    bool modifier_held;
};

/* Caller holds data->lock. */
static void release_modifier(const struct device *dev) {
    struct pinch_zoom_data *data = dev->data;
    const struct pinch_zoom_config *cfg = dev->config;

    data->accum = 0;
    if (!data->modifier_held) {
        return;
    }
    data->modifier_held = false;
    int ret = raise_zmk_keycode_state_changed_from_encoded(cfg->modifier, false, k_uptime_get());
    if (ret < 0) {
        LOG_WRN("pinch zoom modifier release failed: %d", ret);
    }
}

static void release_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct pinch_zoom_data *data = CONTAINER_OF(dwork, struct pinch_zoom_data, release_work);

    k_mutex_lock(&data->lock, K_FOREVER);
    release_modifier(data->dev);
    k_mutex_unlock(&data->lock);
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct pinch_zoom_data *data = dev->data;
    const struct pinch_zoom_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY && event->code == cfg->touch_code) {
        if (!event->value) {
            k_work_cancel_delayable(&data->release_work);
            k_mutex_lock(&data->lock, K_FOREVER);
            release_modifier(dev);
            k_mutex_unlock(&data->lock);
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL || event->code != cfg->zoom_code) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_mutex_lock(&data->lock, K_FOREVER);
    int32_t value = cfg->invert ? -event->value : event->value;
    data->accum = CLAMP((int64_t)data->accum + value, -(int64_t)cfg->divisor * 100,
                        (int64_t)cfg->divisor * 100);
    int32_t steps = data->accum / cfg->divisor;
    data->accum -= steps * cfg->divisor;

    if (steps != 0 && !data->modifier_held) {
        int ret = raise_zmk_keycode_state_changed_from_encoded(cfg->modifier, true, k_uptime_get());
        if (ret < 0) {
            LOG_WRN("pinch zoom modifier press failed: %d", ret);
        } else {
            data->modifier_held = true;
        }
    }
    bool held = data->modifier_held;
    k_mutex_unlock(&data->lock);

    k_work_reschedule(&data->release_work, K_MSEC(cfg->release_ms));

    if (steps == 0 || !held) {
        /* Swallow the raw zoom axis; it means nothing to the host. */
        event->code = cfg->wheel_code;
        event->value = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    LOG_DBG("pinch zoom %d step(s)", steps);
    event->code = cfg->wheel_code;
    event->value = steps;
    return ZMK_INPUT_PROC_CONTINUE;
}

static int pinch_zoom_init(const struct device *dev) {
    struct pinch_zoom_data *data = dev->data;

    data->dev = dev;
    k_mutex_init(&data->lock);
    k_work_init_delayable(&data->release_work, release_work_handler);
    return 0;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, divisor) > 0, "divisor must be positive");                        \
    BUILD_ASSERT(DT_INST_PROP(n, release_ms) > 0, "release-ms must be positive");                  \
    BUILD_ASSERT(DT_INST_PROP(n, zoom_code) != DT_INST_PROP(n, wheel_code),                        \
                 "zoom and wheel codes must differ");                                              \
    static struct pinch_zoom_data data_##n;                                                        \
    static const struct pinch_zoom_config config_##n = {                                           \
        .zoom_code = DT_INST_PROP(n, zoom_code),                                                   \
        .wheel_code = DT_INST_PROP(n, wheel_code),                                                 \
        .touch_code = DT_INST_PROP(n, touch_code),                                                 \
        .divisor = DT_INST_PROP(n, divisor),                                                       \
        .invert = DT_INST_PROP(n, invert),                                                         \
        .modifier = DT_INST_PROP(n, modifier),                                                     \
        .release_ms = DT_INST_PROP(n, release_ms),                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, pinch_zoom_init, NULL, &data_##n, &config_##n, POST_KERNEL,           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)

#endif
