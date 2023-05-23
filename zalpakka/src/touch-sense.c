#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "system.h"

#define DT_DRV_COMPAT touch_sense

LOG_MODULE_REGISTER(touch_sense, LOG_LEVEL_INF);

static K_TIMER_DEFINE(touch_sense_sync, NULL, NULL);

static const struct {
	const struct gpio_dt_spec drive_gpio;
	const struct gpio_dt_spec sense_gpio;
} cfg = {
	.drive_gpio = GPIO_DT_SPEC_INST_GET(0, drive_gpios),
	.sense_gpio = GPIO_DT_SPEC_INST_GET(0, sense_gpios),
};

static struct {
	bool touch;
} data;

#define TOUCH_THRESHOLD 15

static void touch_sense_loop(void)
{
	int i;
	bool touch;
	unsigned int key;

	key = irq_lock();

	gpio_pin_set_dt(&cfg.drive_gpio, 1);

	for (i = 0; i < 50; i++) {
		if (gpio_pin_get_dt(&cfg.sense_gpio)) {
			break;
		}
		k_busy_wait(1);
	}

	irq_unlock(key);

	gpio_pin_set_dt(&cfg.drive_gpio, 0);

	LOG_DBG("sense: %d", i);

	touch = i > TOUCH_THRESHOLD;
	if (touch != data.touch) {
		input_report_key(NULL, INPUT_BTN_TOUCH, touch, true, K_FOREVER);
	}
	data.touch = touch;
}

static void touch_sense_thread(void)
{
	int ret;

	ret = gpio_pin_configure_dt(&cfg.drive_gpio, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("gpio_pin_configure_dt failed: %d", ret);
		return;
	}

	ret = gpio_pin_configure_dt(&cfg.sense_gpio, GPIO_INPUT);
	if (ret) {
		LOG_ERR("gpio_pin_configure_dt failed: %d", ret);
		return;
	}

	k_timer_start(&touch_sense_sync, K_MSEC(50), K_MSEC(50));

	while (system_is_running()) {
		touch_sense_loop();
		k_timer_status_sync(&touch_sense_sync);
	}
}

K_THREAD_DEFINE(touch_sense, 512, touch_sense_thread, NULL, NULL, NULL, 7, 0, 0);
