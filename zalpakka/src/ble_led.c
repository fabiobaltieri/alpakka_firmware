#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_status.h"

LOG_MODULE_REGISTER(ble_led, LOG_LEVEL_INF);

static const struct device *pwm_leds = DEVICE_DT_GET_ONE(pwm_leds);

static int ble_led_start(void)
{
	if (!device_is_ready(pwm_leds)) {
		LOG_ERR("leds device is not ready");
		return 0;
	}
	return 0;
}
SYS_INIT(ble_led_start, APPLICATION, 91);

static void ble_led_searching(void *arg1, void *arg2, void *arg3)
{
	for (;;) {
		led_set_brightness(pwm_leds, 3, 50);
		k_sleep(K_MSEC(30));
		led_set_brightness(pwm_leds, 3, 0);
		k_sleep(K_SECONDS(1));
	}
}

static void ble_led_paired(void *arg1, void *arg2, void *arg3)
{
	int i;

	for (;;) {
		for (i = 0; i <= 40; i++) {
			led_set_brightness(pwm_leds, 3, i);
			k_sleep(K_MSEC(5));
		}
		for (i = 40; i >= 0; i--) {
			led_set_brightness(pwm_leds, 3, i);
			k_sleep(K_MSEC(5));
		}
		k_sleep(K_SECONDS(2));
	}
}

static void ble_led_connected(void *arg1, void *arg2, void *arg3)
{
	int i;

	for (;;) {
		for (i = 0; i <= 40; i++) {
			led_set_brightness(pwm_leds, 3, 40 - i);
			k_sleep(K_MSEC(5));
		}
		for (i = 40; i >= 0; i--) {
			led_set_brightness(pwm_leds, 3, 40 - i);
			k_sleep(K_MSEC(5));
		}
		k_sleep(K_SECONDS(2));
	}
}

K_THREAD_STACK_DEFINE(ble_thread_stack, 256);

static struct k_thread ble_led;

void ble_led_restart(void)
{
	k_thread_entry_t entry;

	k_thread_abort(&ble_led);

	switch (ble_status_get()) {
	case BLE_STATUS_SEARCHING:
		entry = ble_led_searching;
		break;
	case BLE_STATUS_CONNECTED:
		entry = ble_led_connected;
		break;
	case BLE_STATUS_PAIRED:
		entry = ble_led_paired;
		break;
	default:
		LOG_ERR("unknown bt state");
		return;
	}

	k_thread_create(&ble_led, ble_thread_stack,
			K_THREAD_STACK_SIZEOF(ble_thread_stack),
			entry, NULL, NULL, NULL,
			7, 0, K_NO_WAIT);

	k_thread_name_set(&ble_led, "ble_led");
}

void ble_led_stop(void)
{
	led_set_brightness(pwm_leds, 3, 0);

	k_thread_abort(&ble_led);
}
