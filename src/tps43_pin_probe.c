/*
 * Test-only diagnostic for the Nickey44A-PAD TPS43 wiring.
 *
 * Built only when CMake is configured with -DNICKEY_PIN_PROBE=ON. The TPS43
 * driver and I2C0 are disabled in that build, so this code can drive the four
 * candidate lines (P0.09, P0.10, P0.16, P1.10) directly:
 *   1. Report each line's level with pull-up / pull-down (floating, driven).
 *   2. Count transitions on each line for a while (RDY toggles when touched).
 *   3. Bit-bang an I2C address probe to 0x74 on every ordered (SDA, SCL) pair,
 *      then scan all 7-bit addresses on any pair that answered.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pin_probe, LOG_LEVEL_INF);

struct probe_pin {
    const struct device *port;
    gpio_pin_t pin;
    const char *name;
};

static struct probe_pin pins[] = {
    {DEVICE_DT_GET(DT_NODELABEL(gpio0)), 9, "P0.09(SDA pad)"},
    {DEVICE_DT_GET(DT_NODELABEL(gpio0)), 10, "P0.10(SCL pad)"},
    {DEVICE_DT_GET(DT_NODELABEL(gpio0)), 16, "P0.16(white)"},
    {DEVICE_DT_GET(DT_NODELABEL(gpio1)), 10, "P1.10(yellow)"},
};

#define NPINS ARRAY_SIZE(pins)
#define HALF_US 5
#define STRETCH_US 20000

static void pin_cfg(const struct probe_pin *p, gpio_flags_t flags) {
    gpio_pin_configure(p->port, p->pin, flags);
}

static int pin_get(const struct probe_pin *p) { return gpio_pin_get_raw(p->port, p->pin); }

/* Open-drain emulation: release = input with pull-up, low = drive 0. */
static void od_release(const struct probe_pin *p) { pin_cfg(p, GPIO_INPUT | GPIO_PULL_UP); }
static void od_low(const struct probe_pin *p) { pin_cfg(p, GPIO_OUTPUT_LOW); }

static bool scl_release_wait(const struct probe_pin *scl) {
    od_release(scl);
    for (int t = 0; t < STRETCH_US; t++) {
        if (pin_get(scl)) {
            return true;
        }
        k_busy_wait(1);
    }
    return false;
}

/* Returns 1 = ACK, 0 = NACK, -1 = SCL held low, -2 = bus not idle. */
static int i2c_probe_addr(const struct probe_pin *sda, const struct probe_pin *scl, uint8_t addr) {
    od_release(sda);
    od_release(scl);
    k_busy_wait(HALF_US * 2);
    if (!pin_get(sda) || !pin_get(scl)) {
        return -2;
    }
    /* START */
    od_low(sda);
    k_busy_wait(HALF_US);
    od_low(scl);
    k_busy_wait(HALF_US);

    uint8_t byte = (uint8_t)(addr << 1); /* write */
    for (int i = 7; i >= 0; i--) {
        if (byte & BIT(i)) {
            od_release(sda);
        } else {
            od_low(sda);
        }
        k_busy_wait(HALF_US);
        if (!scl_release_wait(scl)) {
            return -1;
        }
        k_busy_wait(HALF_US);
        od_low(scl);
    }
    /* ACK bit */
    od_release(sda);
    k_busy_wait(HALF_US);
    if (!scl_release_wait(scl)) {
        return -1;
    }
    int ack = !pin_get(sda);
    k_busy_wait(HALF_US);
    od_low(scl);
    /* STOP */
    od_low(sda);
    k_busy_wait(HALF_US);
    od_release(scl);
    k_busy_wait(HALF_US);
    od_release(sda);
    k_busy_wait(HALF_US * 4);
    return ack;
}

static void report_levels(void) {
    for (size_t i = 0; i < NPINS; i++) {
        pin_cfg(&pins[i], GPIO_INPUT | GPIO_PULL_UP);
        k_busy_wait(200);
        int pu = pin_get(&pins[i]);
        pin_cfg(&pins[i], GPIO_INPUT | GPIO_PULL_DOWN);
        k_busy_wait(200);
        int pd = pin_get(&pins[i]);
        const char *v = (pu && !pd)   ? "floating (nothing driving it)"
                        : (pu && pd)  ? "HIGH (driven high or strong pull-up)"
                        : (!pu && !pd) ? "LOW (driven low / shorted to GND)"
                                       : "odd (PU=0 PD=1)";
        LOG_INF("level %-15s PU=%d PD=%d -> %s", pins[i].name, pu, pd, v);
    }
}

static void count_toggles(int ms) {
    int last[NPINS], cnt[NPINS] = {0}, highs[NPINS] = {0}, samples = 0;
    for (size_t i = 0; i < NPINS; i++) {
        pin_cfg(&pins[i], GPIO_INPUT | GPIO_PULL_DOWN);
    }
    k_busy_wait(200);
    for (size_t i = 0; i < NPINS; i++) {
        last[i] = pin_get(&pins[i]);
    }
    int64_t end = k_uptime_get() + ms;
    while (k_uptime_get() < end) {
        for (size_t i = 0; i < NPINS; i++) {
            int v = pin_get(&pins[i]);
            if (v != last[i]) {
                cnt[i]++;
                last[i] = v;
            }
            highs[i] += v;
        }
        samples++;
        k_busy_wait(50);
    }
    for (size_t i = 0; i < NPINS; i++) {
        LOG_INF("toggle %-15s transitions=%d high=%d%%", pins[i].name, cnt[i],
                samples ? highs[i] * 100 / samples : 0);
    }
}

static void release_all(gpio_flags_t flags) {
    for (size_t i = 0; i < NPINS; i++) {
        pin_cfg(&pins[i], flags);
    }
}

static void scan_pairs(const char *label, gpio_flags_t others) {
    LOG_INF("--- I2C probe 0x74, other lines %s ---", label);
    for (size_t s = 0; s < NPINS; s++) {
        for (size_t c = 0; c < NPINS; c++) {
            if (s == c) {
                continue;
            }
            release_all(others);
            int ack = 0, nack = 0, held = 0, busy = 0;
            for (int n = 0; n < 150; n++) {
                int r = i2c_probe_addr(&pins[s], &pins[c], 0x74);
                if (r == 1) {
                    ack++;
                } else if (r == 0) {
                    nack++;
                } else if (r == -1) {
                    held++;
                } else {
                    busy++;
                }
                k_msleep(7);
            }
            LOG_INF("SDA=%-15s SCL=%-15s ACK=%d NACK=%d SCLheld=%d busy=%d", pins[s].name,
                    pins[c].name, ack, nack, held, busy);
            if (ack > 0) {
                for (uint8_t a = 0x08; a < 0x78; a++) {
                    if (i2c_probe_addr(&pins[s], &pins[c], a) == 1) {
                        LOG_INF("    device answers at 0x%02x", a);
                    }
                }
            }
            k_msleep(20);
        }
    }
}

static void probe_thread(void) {
    k_msleep(3000);
    for (int round = 1; round <= 3; round++) {
        LOG_INF("===== TPS43 pin probe round %d (touch the pad during 'toggle') =====", round);
        report_levels();
        count_toggles(4000);
        scan_pairs("floating", GPIO_INPUT);
        scan_pairs("pulled up", GPIO_INPUT | GPIO_PULL_UP);
        release_all(GPIO_INPUT);
        LOG_INF("===== round %d done =====", round);
        k_msleep(5000);
    }
}

K_THREAD_DEFINE(tps43_pin_probe, 2048, probe_thread, NULL, NULL, NULL, 7, 0, 0);
