/* SPDX-License-Identifier: MIT */
#define DT_DRV_COMPAT zmk_input_processor_report_throttle
#include <zephyr/device.h>
#include <zephyr/kernel.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#include <zephyr/input/input.h>
#include <drivers/input_processor.h>

/*
 * Limit how often relative movement is sent to the host. The input listener
 * accumulates REL values until an event with sync set, so clearing sync on
 * events that arrive too soon merges them into the next report. A delayed
 * zero-movement event flushes whatever is left once the interval has passed.
 * This keeps BLE mouse reports from queueing up (and lagging) when the host's
 * connection interval is longer than the trackpad's report rate.
 */
struct report_throttle_config {
    const struct device *input_dev;
    int32_t min_interval_ms;
};

struct report_throttle_data {
    const struct device *dev;
    struct k_work_delayable flush_work;
    struct k_mutex lock;
    int64_t last_sync;
    bool pending;
};

static void flush_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct report_throttle_data *data =
        CONTAINER_OF(dwork, struct report_throttle_data, flush_work);
    const struct report_throttle_config *cfg = data->dev->config;

    k_mutex_lock(&data->lock, K_FOREVER);
    bool pending = data->pending;
    k_mutex_unlock(&data->lock);

    if (pending) {
        /* Passes through the chain as an empty move and carries the sync. */
        input_report_rel(cfg->input_dev, INPUT_REL_X, 0, true, K_FOREVER);
    }
}

static int handle_event(const struct device *dev, struct input_event *event, uint32_t param1,
                        uint32_t param2, struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    struct report_throttle_data *data = dev->data;
    const struct report_throttle_config *cfg = dev->config;

    if (!event->sync) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_mutex_lock(&data->lock, K_FOREVER);
    int64_t now = k_uptime_get();
    int64_t wait = data->last_sync + cfg->min_interval_ms - now;

    if (event->type == INPUT_EV_REL && wait > 0) {
        /* Too soon: let the listener keep accumulating, flush later. */
        event->sync = false;
        if (!data->pending) {
            data->pending = true;
            k_work_reschedule(&data->flush_work, K_MSEC(wait));
        }
    } else {
        /* Buttons are never delayed; any sync sends the accumulated movement. */
        data->last_sync = now;
        data->pending = false;
        k_work_cancel_delayable(&data->flush_work);
    }
    k_mutex_unlock(&data->lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

static int report_throttle_init(const struct device *dev) {
    struct report_throttle_data *data = dev->data;

    data->dev = dev;
    k_mutex_init(&data->lock);
    k_work_init_delayable(&data->flush_work, flush_work_handler);
    return 0;
}

static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};

#define CREATE_INSTANCE(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP(n, min_interval_ms) > 0, "min-interval-ms must be positive");        \
    static struct report_throttle_data data_##n;                                                   \
    static const struct report_throttle_config config_##n = {                                      \
        .input_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, input_device)),                              \
        .min_interval_ms = DT_INST_PROP(n, min_interval_ms),                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, report_throttle_init, NULL, &data_##n, &config_##n, APPLICATION,      \
                          CONFIG_APPLICATION_INIT_PRIORITY, &api);

DT_INST_FOREACH_STATUS_OKAY(CREATE_INSTANCE)

#endif
