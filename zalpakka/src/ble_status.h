enum ble_status {
	BLE_STATUS_SEARCHING,
	BLE_STATUS_PAIRED,
	BLE_STATUS_CONNECTED
};

enum ble_status ble_status_get(void);
