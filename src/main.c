/*
 * Copyright (c) 2025 Embeint Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/nus.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define M_PI 3.141592653589793f

LOG_MODULE_REGISTER(biosignal, LOG_LEVEL_INF);

/* Multi-channel configuration */
#define NUM_CHANNELS 8
#define SAMPLE_RATE_HZ 1000
#define RING_BUFFER_SIZE 1024
#define UART_TX_BUF_SIZE 256
#define UART_TX_RATE_HZ 100

/* BLE State */
static struct bt_conn *current_conn;
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, "BioSignal"),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

/* CRC-16 CCITT implementation */
static uint16_t crc16_ccitt(const char *data, size_t len)
{
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < len; i++) {
		crc ^= (uint8_t)data[i] << 8;
		for (int j = 0; j < 8; j++) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}
	LOG_INF("Connected");
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	/* Echo back or handle commands if needed */
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

/* Per-channel ring buffers for sample storage */
static int16_t ring_buffers[NUM_CHANNELS][RING_BUFFER_SIZE];
static int heads[NUM_CHANNELS] = {0};

/* Signal generation state */
static float phases[NUM_CHANNELS];
static const float signal_freqs[NUM_CHANNELS] = {
	10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f
};

/* Timer handles */
static struct k_timer sample_timer;
static struct k_timer uart_tx_timer;

/* UART device (kept for debugging if needed, but primary stream is BLE) */
static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart0));
static char uart_tx_buf[UART_TX_BUF_SIZE];

/**
 * @brief Sample timer ISR - generates simulated electrode signals
 * 
 * Runs at 1 kHz (per channel), generating sine/cosine waveforms
 * with varying frequencies to simulate multi-electrode recordings.
 */
static void sample_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	for (int ch = 0; ch < NUM_CHANNELS; ch++) {
		/* Alternate between sine and cosine for channel variety */
		float val = (ch % 2 == 0) ? sinf(phases[ch]) : cosf(phases[ch]);
		
		/* Convert to 16-bit signed integer */
		ring_buffers[ch][heads[ch]] = (int16_t)(val * 32767.0f);
		
		/* Advance ring buffer */
		heads[ch] = (heads[ch] + 1) % RING_BUFFER_SIZE;
		
		/* Update phase for next sample */
		phases[ch] += 2.0f * M_PI * signal_freqs[ch] / SAMPLE_RATE_HZ;
		if (phases[ch] > 2.0f * M_PI) {
			phases[ch] -= 2.0f * M_PI;
		}
	}
}

/**
 * @brief UART transmission timer - sends latest samples over BLE NUS
 * 
 * Runs at 100 Hz, transmitting CSV-formatted data packets containing
 * timestamp, all 8 channel values, and CRC-16.
 */
static void uart_tx_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	if (!current_conn) {
		return;
	}

	/* Get latest sample from each channel */
	/* Format: timestamp,ch0,ch1,ch2,ch3,ch4,ch5,ch6,ch7 */
	int len = snprintf(uart_tx_buf, UART_TX_BUF_SIZE,
		"%lld,%d,%d,%d,%d,%d,%d,%d,%d",
		(long long)k_uptime_get(),
		ring_buffers[0][(heads[0] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[1][(heads[1] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[2][(heads[2] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[3][(heads[3] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[4][(heads[4] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[5][(heads[5] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[6][(heads[6] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE],
		ring_buffers[7][(heads[7] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE]);
	
	if (len > 0 && len < UART_TX_BUF_SIZE - 6) { /* Ensure space for ,CRC\n */
		/* Calculate CRC on the payload so far */
		uint16_t crc = crc16_ccitt(uart_tx_buf, len);
		
		/* Append CRC and newline */
		len += snprintf(uart_tx_buf + len, UART_TX_BUF_SIZE - len, ",%04X\n", crc);
		
		/* Transmit via BLE NUS */
		int err = bt_nus_send(NULL, uart_tx_buf, len);
		if (err) {
			LOG_WRN("BLE send failed (err %d)", err);
		}
	}
}

int main(void)
{
	LOG_INF("Bio-Signal Telemetry Node starting...");
	LOG_INF("Simulating %d channels at %d Hz", NUM_CHANNELS, SAMPLE_RATE_HZ);

	/* Initialize signal phases with offsets for visual variety */
	for (int ch = 0; ch < NUM_CHANNELS; ch++) {
		phases[ch] = ch * M_PI / 4.0f;
	}

	/* Initialize BLE */
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}
	LOG_INF("Bluetooth initialized");

	err = bt_nus_init(&nus_cb);
	if (err) {
		LOG_ERR("NUS init failed (err %d)", err);
		return -1;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return -1;
	}
	LOG_INF("Advertising successfully started");

	/* Start sampling timer (1 kHz per channel = 8 kHz total) */
	k_timer_init(&sample_timer, sample_timer_handler, NULL);
	k_timer_start(&sample_timer, K_MSEC(1), K_MSEC(1));
	
	/* Start UART transmission timer (100 Hz packet rate) */
	k_timer_init(&uart_tx_timer, uart_tx_timer_handler, NULL);
	k_timer_start(&uart_tx_timer, K_MSEC(10), K_MSEC(10));

	LOG_INF("Streaming started - 8 kHz aggregate throughput");

	/* Periodic status logging */
	while (1) {
		k_sleep(K_SECONDS(1));
		int latest = (heads[0] - 1 + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
		LOG_INF("Ch0:%d Ch1:%d Ch2:%d Ch3:%d Ch4:%d Ch5:%d Ch6:%d Ch7:%d",
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