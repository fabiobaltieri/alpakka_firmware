#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static const struct device *wdt = DEVICE_DT_GET(DT_NODELABEL(wdt));

int main(void)
{
	int wdt_channel_id;
	int err;

	printk("Z-Alpakka started\n");

	if (!device_is_ready(wdt)) {
		LOG_ERR("wdt device is not ready");
		return 0;
	}

	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0,
		.window.max = 5000,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install error");
		return 0;
	}

	err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (err < 0) {
		LOG_ERR("Watchdog setup error");
		return 0;
	}

	for (;;) {
		wdt_feed(wdt, wdt_channel_id);
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
