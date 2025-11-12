#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/devicetree.h>
#include <zephyr/arch/cpu.h>
#include <stdint.h>

#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Statistics structure for timer measurements */
struct timer_stats {
    uint32_t prev_cycles;
    uint32_t sum_us;
    uint32_t min_us;
    uint32_t max_us;
    uint32_t count;
};

static struct timer_stats stats = {
    .prev_cycles = 0,
    .sum_us = 0,
    .min_us = UINT32_MAX,
    .max_us = 0,
    .count = 0
};

/* Timer callback runs at 1 kHz */
void timer_callback(struct k_timer *t)
{
    uint32_t now = k_cycle_get_32();
    
    if (stats.prev_cycles != 0) {
        uint32_t delta_us = k_cyc_to_us_floor32(now - stats.prev_cycles);
        stats.sum_us += delta_us;
        if (delta_us < stats.min_us) stats.min_us = delta_us;
        if (delta_us > stats.max_us) stats.max_us = delta_us;
        stats.count++;
    }
    stats.prev_cycles = now;

    gpio_pin_toggle_dt(&led);

    /* Print and reset stats once per second (1000 callbacks) */
    if (stats.count >= 1000) {
        uint32_t avg = stats.sum_us / stats.count;
        printk("period_us: avg=%u min=%u max=%u count=%u\n", avg, stats.min_us, stats.max_us, stats.count);
        
        /* Reset statistics */
        stats.sum_us = 0;
        stats.min_us = UINT32_MAX;
        stats.max_us = 0;
        stats.count = 0;
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

    while (1) k_sleep(K_SECONDS(1));
}
