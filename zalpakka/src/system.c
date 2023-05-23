#include <hal/nrf_gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include "ble.h"
#include "ble_led.h"

LOG_MODULE_REGISTER(system, LOG_LEVEL_INF);

#define WKUP_PIN DT_GPIO_PIN(DT_NODELABEL(wkup), gpios)

static K_SEM_DEFINE(system_sem, 0, 1);

enum {
	FLAGS_UNPAIR,
	FLAGS_SHUTDOWN,
};

static struct {
	atomic_t flags;
} data;

#define SOC_UPDATE_MS (10 * 1000)

static const struct device *pwm_leds = DEVICE_DT_GET_ONE(pwm_leds);
static const struct device *analog_pwr = DEVICE_DT_GET(DT_NODELABEL(analog_pwr));

#define VBATT_NODE DT_NODELABEL(vbatt)
#define VBATT_FULL_MV 4200
#define VBATT_EMPTY_MV 3000
static const struct adc_dt_spec vbatt = ADC_DT_SPEC_GET(VBATT_NODE);

#define SHUTDOWN_DELAY (20 * 60 * 1000)
static int64_t last_event_ts;

static void system_unpair(void)
{
	if (atomic_test_and_set_bit(&data.flags, FLAGS_UNPAIR)) {
		return;
	}
	k_sem_give(&system_sem);
}

static void system_shutdown(void)
{
	if (atomic_test_and_set_bit(&data.flags, FLAGS_SHUTDOWN)) {
		return;
	}
	k_sem_give(&system_sem);
}

#define SUSPEND_GPIOS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(suspend_gpios)
static const struct gpio_dt_spec suspend_gpio_specs[] = {
	DT_FOREACH_PROP_ELEM_SEP(SUSPEND_GPIOS_NODE, gpios,
				 GPIO_DT_SPEC_GET_BY_IDX, (,))
};

static void suspend_gpios(void)
{
	int ret;

	for (int i = 0; i < ARRAY_SIZE(suspend_gpio_specs); i++) {
		ret = gpio_pin_configure_dt(&suspend_gpio_specs[i], GPIO_INPUT);
		if (ret != 0) {
			LOG_ERR("Pin %d configuration failed: %d", i, ret);
			return;
		}
	}
}

static void system_cb(struct input_event *evt)
{
	static bool mode;
	static bool unpair;
	static bool shutdown;

	if (evt->type != INPUT_EV_KEY) {
		return;
	}

	switch (evt->code) {
	case INPUT_BTN_MODE:
		mode = evt->value;
		break;
	case INPUT_BTN_START:
		unpair = evt->value;
		break;
	case INPUT_BTN_SELECT:
		shutdown = evt->value;
		break;
	}

	if (mode && unpair) {
		system_unpair();
	} else if (mode && shutdown) {
		system_shutdown();
	}

	last_event_ts = k_uptime_get();
}
INPUT_LISTENER_CB_DEFINE(NULL, system_cb);

bool system_is_running(void)
{
	if (atomic_test_bit(&data.flags, FLAGS_SHUTDOWN)) {
		return false;
	}
	return true;
}

static void init_soc(void)
{
	int err;

	if (!device_is_ready(vbatt.dev)) {
		LOG_ERR("ADC controller device not ready");
		return;
	}

	err = adc_channel_setup_dt(&vbatt);
	if (err < 0) {
		LOG_ERR("Could not setup ADC channel (%d)", err);
		return;
	}
}

static void update_soc(void)
{
	int16_t buf;
	struct adc_sequence sequence = {
		.buffer = &buf,
		.buffer_size = sizeof(buf),
	};
	int32_t vbatt_mv;
	int soc;
	int err;

	adc_sequence_init_dt(&vbatt, &sequence);

	err = adc_read(vbatt.dev, &sequence);
	if (err < 0) {
		LOG_ERR("Could not read (%d)", err);
		return;
	}

	vbatt_mv = buf;
	adc_raw_to_millivolts_dt(&vbatt, &vbatt_mv);

	vbatt_mv = vbatt_mv *
		DT_PROP(VBATT_NODE, full_ohms) /
		DT_PROP(VBATT_NODE, output_ohms);

	soc = (vbatt_mv - VBATT_EMPTY_MV) * 100 /
		(VBATT_FULL_MV - VBATT_EMPTY_MV);
	soc = CLAMP(soc, 0, 100);

	LOG_INF("update_soc: vbatt_mv=%d soc=%d", vbatt_mv, soc);

	bt_bas_set_battery_level(soc);
}

static void system_loop(void)
{
	static int64_t soc_update_ts = -SOC_UPDATE_MS;
	int err;

	if (k_uptime_get() - soc_update_ts > SOC_UPDATE_MS) {
		update_soc();
		soc_update_ts = k_uptime_get();
	}

	if (k_uptime_get() - last_event_ts > SHUTDOWN_DELAY) {
		system_shutdown();
	}

	if (atomic_test_bit(&data.flags, FLAGS_UNPAIR)) {
		ble_led_stop();

		err = bt_unpair(BT_ID_DEFAULT, NULL);
		if (err) {
			LOG_ERR("Failed to clear pairings (err %d)", err);
		}

		led_on(pwm_leds, 0);
		k_sleep(K_SECONDS(1));
		led_off(pwm_leds, 0);

		atomic_clear_bit(&data.flags, FLAGS_UNPAIR);

		ble_led_restart();
	}

	if (atomic_test_bit(&data.flags, FLAGS_SHUTDOWN)) {
		struct pm_state_info pm_off = {PM_STATE_SOFT_OFF, 0, 0};

		ble_led_stop();
		ble_stop();

		led_on(pwm_leds, 0);
		k_sleep(K_SECONDS(1));
		led_off(pwm_leds, 0);
		k_sleep(K_MSEC(200));
		led_on(pwm_leds, 0);
		k_sleep(K_MSEC(50));
		led_off(pwm_leds, 0);
		k_sleep(K_MSEC(200));
		led_on(pwm_leds, 0);
		k_sleep(K_MSEC(50));
		led_off(pwm_leds, 0);

		suspend_gpios();

		regulator_disable(analog_pwr);

		printk("about to shutdown...\n");

		nrf_gpio_cfg_sense_set(WKUP_PIN, NRF_GPIO_PIN_SENSE_LOW);

		pm_state_force(0, &pm_off);
	}
}

static void system_thread(void)
{
	if (!device_is_ready(pwm_leds)) {
		LOG_ERR("pwm_leds device is not ready");
		return;
	}

	if (!device_is_ready(analog_pwr)) {
		LOG_ERR("analog pwr device is not ready");
		return;
	}

	init_soc();

	led_on(pwm_leds, 0);
	k_sleep(K_MSEC(50));
	led_off(pwm_leds, 0);

	for (;;) {
		system_loop();
		k_sem_take(&system_sem, K_SECONDS(1));
	}
}

K_THREAD_DEFINE(system, 1024, system_thread, NULL, NULL, NULL, 7, 0, 0);

static void input_cb(struct input_event *evt)
{
#if 0
	LOG_INF("input event: dev=%-16s %3s type=%2x code=%3d value=%d",
		evt->dev ? evt->dev->name : "NULL",
		evt->sync ? "SYN" : "",
		evt->type,
		evt->code,
		evt->value);
#endif

	if (evt->code == INPUT_BTN_TOUCH) {
		if (evt->value) {
			led_on(pwm_leds, 2);
		} else {
			led_off(pwm_leds, 2);
		}
	}
}
INPUT_LISTENER_CB_DEFINE(NULL, input_cb);
