/* SPDX-License-Identifier: MIT */
/*
 * Logging builds only: report BLE connection parameters at INF level, so a
 * cursor-lag episode can be matched against the host's connection interval
 * (ZMK itself logs these at DBG only).
 */
#include <zephyr/kernel.h>

#if IS_ENABLED(CONFIG_ZMK_USB_LOGGING) && IS_ENABLED(CONFIG_BT)

#include <zephyr/bluetooth/conn.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const char *role_str(struct bt_conn *conn) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) < 0) {
        return "?";
    }
    return info.role == BT_CONN_ROLE_CENTRAL ? "to-split-peripheral" : "to-host";
}

static void log_params(struct bt_conn *conn, const char *what) {
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) < 0) {
        return;
    }
    /* interval is in 1.25 ms units; print in units of 0.01 ms. */
    LOG_INF("BLE %s %s: interval %u.%02u ms latency %u timeout %u ms", role_str(conn), what,
            info.le.interval * 125 / 100, info.le.interval * 125 % 100, info.le.latency,
            info.le.timeout * 10);
}

static void connected(struct bt_conn *conn, uint8_t err) {
    if (!err) {
        log_params(conn, "connected");
    }
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval, uint16_t latency,
                             uint16_t timeout) {
    ARG_UNUSED(interval);
    ARG_UNUSED(latency);
    ARG_UNUSED(timeout);
    log_params(conn, "params updated");
}

BT_CONN_CB_DEFINE(pad_modes_conn_log) = {
    .connected = connected,
    .le_param_updated = le_param_updated,
};

#endif
