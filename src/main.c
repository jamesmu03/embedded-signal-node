#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <stdint.h>
#include <stdbool.h>

#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Stats updated in timer callback (ISR context) */
struct cycle_stats {
    uint32_t prev_cycles;
    uint64_t sum_cycles;
    uint32_t min_cycles;
    uint32_t max_cycles;
    uint32_t count;
};

static volatile struct cycle_stats stats = {
    .prev_cycles = 0,
    .sum_cycles = 0,
    .min_cycles = UINT32_MAX,
    .max_cycles = 0,
    .count = 0
};

/* Flag set by ISR when a report is ready; cleared in main */
static volatile bool report_ready = false;

/* Timer callback at 1 kHz. Keep ISR minimal. */
void timer_callback(struct k_timer *t)
{
    uint32_t now = k_cycle_get_32();

    if (stats.prev_cycles != 0) {
        uint32_t delta = now - stats.prev_cycles; /* unsigned wrap OK */
        stats.sum_cycles += delta;
        if (delta < stats.min_cycles) stats.min_cycles = delta;
        if (delta > stats.max_cycles) stats.max_cycles = delta;
        stats.count++;
    }

    stats.prev_cycles = now;

    gpio_pin_toggle_dt(&led);

    if (stats.count >= 1000) {
        report_ready = true;
    }
}

K_TIMER_DEFINE(sample_timer, timer_callback, NULL);

void main(void)
{
    if (!device_is_ready(led.port)) {
        printk("LED device not ready\n");
        return;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    k_timer_start(&sample_timer, K_MSEC(1), K_MSEC(1));

    while (1) {
        if (report_ready) {
            /* Snapshot */
            uint64_t sum_cycles = stats.sum_cycles;
            uint32_t min_c = stats.min_cycles;
            uint32_t max_c = stats.max_cycles;
            uint32_t cnt = stats.count;

            /* Reset for next window */
            stats.sum_cycles = 0;
            stats.min_cycles = UINT32_MAX;
            stats.max_cycles = 0;
            stats.count = 0;
            stats.prev_cycles = k_cycle_get_32();
            report_ready = false;

            if (cnt > 0) {
                uint32_t avg_cycles = (uint32_t)(sum_cycles / cnt);
                /* Print raw cycles and also microsecond conversions for convenience */
                /* Note: conversion uses k_cyc_to_ns_floor32 / 1000 for microseconds if available */
                uint32_t avg_us = k_cyc_to_us_floor32(avg_cycles);
                uint32_t min_us = k_cyc_to_us_floor32(min_c);
                uint32_t max_us = k_cyc_to_us_floor32(max_c);
                printk("period_cycles: avg=%u min=%u max=%u count=%u | us: avg=%u min=%u max=%u\n",
                       avg_cycles, min_c, max_c, cnt, avg_us, min_us, max_us);
            } else {
                printk("period_cycles: no samples\n");
            }
        }
        k_sleep(K_MSEC(10));
    }
}
