#include <stdlib.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "system.h"

#define DT_DRV_COMPAT analog_axis

LOG_MODULE_REGISTER(analog, LOG_LEVEL_INF);

static K_TIMER_DEFINE(analog_sync, NULL, NULL);

#define AXIS_INIT(node) { \
	.adc = ADC_DT_SPEC_GET(node), \
	.offset = DT_PROP(node, offset), \
	.dead_zone = DT_PROP(node, dead_zone), \
	.limit = DT_PROP(node, limit), \
	.code = DT_PROP(node, code), \
	.invert = DT_PROP(node, invert), \
},

static const struct {
	struct adc_dt_spec adc;
	int offset;
	int dead_zone;
	int limit;
	uint16_t code;
	bool invert;
} axis[] = {
	DT_INST_FOREACH_CHILD(0, AXIS_INIT)
};

#define CHANNELS ARRAY_SIZE(axis)

static struct {
	uint8_t out[CHANNELS];
} data;

BUILD_ASSERT(CHANNELS == 2);

static void analog_loop(void)
{
	int err;
	int i;
	int16_t bufs[CHANNELS];
	int32_t out;
	struct adc_sequence sequence = {
		.buffer = bufs,
		.buffer_size = sizeof(bufs),
	};

	adc_sequence_init_dt(&axis[0].adc, &sequence);

	for (i = 0; i < CHANNELS; i++) {
		sequence.channels |= BIT(axis[i].adc.channel_id);
	}

	err = adc_read(axis[0].adc.dev, &sequence);
	if (err < 0) {
		LOG_ERR("Could not read (%d)", err);
		return;
	}

	LOG_DBG("analog: %4d %4d", bufs[0], bufs[1]);

	for (i = 0; i < CHANNELS; i++) {
		bufs[i] += axis[i].offset;

		if (axis[i].invert) {
			bufs[i] *= -1;
		}

		if (abs(bufs[i]) < axis[i].dead_zone) {
			out = 128;
		} else {
			out = CLAMP(bufs[i] * INT8_MAX / axis[i].limit + 128,
				       0, UINT8_MAX);
		}

		if (data.out[i] != out) {
			input_report_abs(NULL, axis[i].code, out, true, K_FOREVER);
		}
		data.out[i] = out;
	}
}

static void analog_thread(void)
{
	int err;
	int i;

	for (i = 0; i < CHANNELS; i++) {
		if (!device_is_ready(axis[i].adc.dev)) {
			LOG_ERR("ADC controller device not ready");
			return;
		}

		err = adc_channel_setup_dt(&axis[i].adc);
		if (err < 0) {
			LOG_ERR("Could not setup channel #%d (%d)", i, err);
			return;
		}
	}

	k_timer_start(&analog_sync, K_MSEC(15), K_MSEC(15));

	while (system_is_running()) {
		analog_loop();
		k_timer_status_sync(&analog_sync);
	}
}

K_THREAD_DEFINE(analog, 512, analog_thread, NULL, NULL, NULL, 7, 0, 0);
