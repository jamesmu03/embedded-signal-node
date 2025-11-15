/*
 * Copyright (c) 2025 Embeint Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>

#define M_PI 3.141592653589793f

LOG_MODULE_REGISTER(sample, LOG_LEVEL_INF);

#define NUM_CHANNELS 8
#define SAMPLE_RATE 1000
#define RING_SIZE 1024

int16_t ring_buffers[NUM_CHANNELS][RING_SIZE];
int heads[NUM_CHANNELS] = {0};
float phases[NUM_CHANNELS];
float freqs[NUM_CHANNELS] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};

struct k_timer sample_timer;

void sample_timer_handler(struct k_timer *timer_id) {
    for(int ch = 0; ch < NUM_CHANNELS; ch++) {
        float val = (ch % 2 == 0) ? sinf(phases[ch]) : cosf(phases[ch]);
        ring_buffers[ch][heads[ch]] = (int16_t)(val * 32767.0f);
        heads[ch] = (heads[ch] + 1) % RING_SIZE;
        phases[ch] += 2.0f * (float)M_PI * freqs[ch] / SAMPLE_RATE;
        if(phases[ch] > 2.0f * (float)M_PI) phases[ch] -= 2.0f * (float)M_PI;
    }
}

int main(void) {
    for(int ch = 0; ch < NUM_CHANNELS; ch++) {
        phases[ch] = ch * (float)M_PI / 4.0f; // different phases
    }

    k_timer_init(&sample_timer, sample_timer_handler, NULL);
    k_timer_start(&sample_timer, K_MSEC(1), K_MSEC(1));

    while (1) {
        k_sleep(K_SECONDS(1));
        int latest = (heads[0] - 1 + RING_SIZE) % RING_SIZE;
        LOG_INF("Ch0: %d, Ch1: %d, Ch2: %d, Ch3: %d, Ch4: %d, Ch5: %d, Ch6: %d, Ch7: %d",
            ring_buffers[0][latest],
            ring_buffers[1][latest],
            ring_buffers[2][latest],
            ring_buffers[3][latest],
            ring_buffers[4][latest],
            ring_buffers[5][latest],
            ring_buffers[6][latest],
            ring_buffers[7][latest]);
    }
}