#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      CONFIG_BT_DEVICE_APPEARANCE & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
};

#if CONFIG_SHELL

static int cmd_ble_unpair(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	err = bt_unpair(BT_ID_DEFAULT, NULL);
	if (err) {
		shell_error(sh, "Failed to clear pairings (err %d)",
			    err);
		return err;
	}

	shell_print(sh, "Pairing successfully cleared");

	return 0;
}
SHELL_CMD_REGISTER(ble_unpair, NULL, "BLE Unpair", cmd_ble_unpair);

#endif

static void ble_disconnect(struct bt_conn *conn, void *user_data)
{
	int err;

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("bt_conn_disconnect: %d", err);
	}
}


void ble_stop(void)
{
	int err;

	err = bt_le_adv_stop();
	if (err) {
		LOG_ERR("bt_le_adv_stop: %d", err);
	}

	bt_conn_foreach(BT_CONN_TYPE_LE, ble_disconnect, NULL);
}

static int ble_setup(void)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -EIO;
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return -EIO;
	}

	LOG_INF("Advertising successfully started");

	return 0;
}
SYS_INIT(ble_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
