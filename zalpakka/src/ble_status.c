#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include "ble_status.h"
#include "ble_led.h"

LOG_MODULE_REGISTER(ble_state, LOG_LEVEL_INF);

static struct {
	bool ble_bonded;
	bool ble_connected;
} data;

K_MUTEX_DEFINE(data_lock);

enum ble_status ble_status_get(void)
{
	enum ble_status ret;

	k_mutex_lock(&data_lock, K_FOREVER);
	if (!data.ble_bonded) {
		ret = BLE_STATUS_SEARCHING;
	} else if (data.ble_connected) {
		ret = BLE_STATUS_CONNECTED;
	} else {
		ret = BLE_STATUS_PAIRED;
	}
	k_mutex_unlock(&data_lock);

	return ret;
}

static void bond_count_cb(const struct bt_bond_info *info, void *user_data)
{
	int *bond_count = user_data;
	(*bond_count)++;
}

static void update_ble_status(void)
{
	int bond_count = 0;

	bt_foreach_bond(BT_ID_DEFAULT, bond_count_cb, &bond_count);

	k_mutex_lock(&data_lock, K_FOREVER);
	data.ble_bonded = bond_count > 0;
	k_mutex_unlock(&data_lock);

	ble_led_restart();
}

static void auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
	update_ble_status();
}

static void auth_bond_deleted(uint8_t id, const bt_addr_le_t *peer)
{
	update_ble_status();
}

static struct bt_conn_auth_info_cb auth_callbacks = {
	.pairing_complete = auth_pairing_complete,
	.bond_deleted = auth_bond_deleted
};

static int ble_setup(void)
{
	bt_conn_auth_info_cb_register(&auth_callbacks);

	update_ble_status();

	return 0;
}
SYS_INIT(ble_setup, APPLICATION, 91);

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int ret;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, err);
		return;
	}

	LOG_INF("Connected %s", addr);

	ret = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (ret) {
		LOG_ERR("Failed to set security");
	}

	k_mutex_lock(&data_lock, K_FOREVER);
	data.ble_connected = true;
	k_mutex_unlock(&data_lock);

	ble_led_restart();
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected from %s (reason 0x%02x)", addr, reason);

	k_mutex_lock(&data_lock, K_FOREVER);
	data.ble_connected = false;
	k_mutex_unlock(&data_lock);

	ble_led_restart();
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("LE conn param updated: %s int 0x%04x lat %d to %d",
		addr, interval, latency, timeout);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_updated = le_param_updated,
};
