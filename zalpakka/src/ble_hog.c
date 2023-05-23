#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "system.h"

LOG_MODULE_REGISTER(ble_hog, LOG_LEVEL_INF);

enum {
	HIDS_REMOTE_WAKE = BIT(0),
	HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
	uint16_t version; /* version number of base USB HID Specification */
	uint8_t code; /* country HID Device hardware is localized for. */
	uint8_t flags;
} __packed;

struct hids_report {
	uint8_t id; /* report id */
	uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
	.version = 0x0000,
	.code = 0x00,
	.flags = HIDS_NORMALLY_CONNECTABLE,
};

enum {
	HIDS_INPUT = 0x01,
	HIDS_OUTPUT = 0x02,
	HIDS_FEATURE = 0x03,
};

static struct hids_report input_gamepad = {
	.id = 0x01,
	.type = HIDS_INPUT,
};

static struct hids_report input_mouse = {
	.id = 0x02,
	.type = HIDS_INPUT,
};

static struct hids_report input_keyboard = {
	.id = 0x03,
	.type = HIDS_INPUT,
};

static bool notify_enabled;
static uint8_t ctrl_point;
static uint8_t report_map[] = {
	/* Gamepad */
	0x05, 0x01,                    // Usage Page (Generic Desktop)
	0x09, 0x05,                    // Usage (Game Pad)
	0xa1, 0x01,                    // Collection (Application)
	0x85, 0x01,                    //  Report ID (1)

	0x05, 0x01,                    //  Usage Page (Generic Desktop)
	0x75, 0x04,                    //  Report Size (4)
	0x95, 0x01,                    //  Report Count (1)
	0x25, 0x07,                    //  Logical Maximum (7)
	0x46, 0x3b, 0x01,              //  Physical Maximum (315)
	0x65, 0x14,                    //  Unit (Degrees,EngRotation)
	0x09, 0x39,                    //  Usage (Hat switch)
	0x81, 0x42,                    //  Input (Data,Var,Abs,Null)
	0x45, 0x00,                    //  Physical Maximum (0)
	0x65, 0x00,                    //  Unit (None)

	0x75, 0x01,                    //  Report Size (1)
	0x95, 0x04,                    //  Report Count (4)
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)

	0x05, 0x09,                    //  Usage Page (Button)
	0x15, 0x00,                    //  Logical Minimum (0)
	0x25, 0x01,                    //  Logical Maximum (1)
	0x75, 0x01,                    //  Report Size (1)
	0x95, 0x0d,                    //  Report Count (13)
	0x09, 0x01,                    //  Usage (BTN_SOUTH)
	0x09, 0x02,                    //  Usage (BTN_EAST)
	0x09, 0x04,                    //  Usage (BTN_NORTH)
	0x09, 0x05,                    //  Usage (BTN_WEST)
	0x09, 0x07,                    //  Usage (BTN_TL)
	0x09, 0x08,                    //  Usage (BTN_TR)
	0x09, 0x0b,                    //  Usage (BTN_SELECT)
	0x09, 0x0c,                    //  Usage (BTN_START)
	0x09, 0x0d,                    //  Usage (BTN_MODE)
	0x09, 0x0e,                    //  Usage (BTN_THUMBL)
	0x09, 0x0f,                    //  Usage (BTN_THUMBR)
	0x09, 0x11,                    //  Usage (BTN_TRIGGER_HAPPY1)
	0x09, 0x12,                    //  Usage (BTN_TRIGGER_HAPPY2)
	0x81, 0x02,                    //  Input (Data,Var,Abs)

	0x75, 0x01,                    //  Report Size (1)
	0x95, 0x03,                    //  Report Count (3)
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)

	0x05, 0x01,                    //  Usage Page (Generic Desktop)
	0x15, 0x01,                    //  Logical Minimum (1)
	0x26, 0xff, 0x00,              //  Logical Maximum (255)
	0x09, 0x01,                    //  Usage (Pointer)
	0xa1, 0x00,                    //  Collection (Physical)
	0x09, 0x30,                    //   Usage (X)
	0x09, 0x31,                    //   Usage (Y)
	0x75, 0x08,                    //   Report Size (8)
	0x95, 0x02,                    //   Report Count (2)
	0x81, 0x02,                    //   Input (Data,Var,Abs)
	0xc0,                          //  End Collection

	0x09, 0x01,                    //  Usage (Pointer)
	0xa1, 0x00,                    //  Collection (Physical)
	0x09, 0x33,                    //   Usage (Rx)
	0x09, 0x34,                    //   Usage (Ry)
	0x75, 0x08,                    //   Report Size (8)
	0x95, 0x02,                    //   Report Count (2)
	0x81, 0x02,                    //   Input (Data,Var,Abs)

	0xc0,                          //  End Collection
	0xc0,                          // End Collection

	/* Mouse */
	0x05, 0x01,                    // Usage Page (Generic Desktop)
	0x09, 0x02,                    // Usage (Mouse)
	0xa1, 0x01,                    // Collection (Application)
	0x85, 0x02,                    //  Report ID (2)
	0x09, 0x01,                    //  Usage (Pointer)
	0xa1, 0x00,                    //  Collection (Physical)

	0x05, 0x09,                    //   Usage Page (Button)
	0x19, 0x01,                    //   Usage Minimum (1)
	0x29, 0x03,                    //   Usage Maximum (3)
	0x15, 0x00,                    //   Logical Minimum (0)
	0x25, 0x01,                    //   Logical Maximum (1)
	0x95, 0x03,                    //   Report Count (3)
	0x75, 0x01,                    //   Report Size (1)
	0x81, 0x02,                    //   Input (Data,Var,Abs)

	0x95, 0x01,                    //   Report Count (1)
	0x75, 0x05,                    //   Report Size (5)
	0x81, 0x03,                    //   Input (Cnst,Var,Abs)

	0x05, 0x01,                    //   Usage Page (Generic Desktop)
	0x16, 0x01, 0x80,              //   Logical Minimum (-32767)
	0x26, 0xff, 0x7f,              //   Logical Maximum (32767)
	0x75, 0x10,                    //   Report Size (16)
	0x95, 0x02,                    //   Report Count (2)
	0x09, 0x30,                    //   Usage (X)
	0x09, 0x31,                    //   Usage (Y)
	0x81, 0x06,                    //   Input (Data,Var,Rel)

	0x15, 0x81,                    //   Logical Minimum (-127)
	0x25, 0x7f,                    //   Logical Maximum (127)
	0x75, 0x08,                    //   Report Size (8)
	0x95, 0x01,                    //   Report Count (1)
	0x09, 0x38,                    //   Usage (Wheel)
	0x81, 0x06,                    //   Input (Data,Var,Rel)

	0xc0,                          //  End Collection
	0xc0,                          // End Collection

	/* Keyboard */
	0x05, 0x01,                    // Usage Page (Generic Desktop)
	0x09, 0x06,                    // Usage (Keyboard)
	0xa1, 0x01,                    // Collection (Application)
	0x85, 0x03,                    //  Report ID (3)
	0x05, 0x07,                    //  Usage Page (Keyboard)
	0x19, 0xe0,                    //  Usage Minimum (224)
	0x29, 0xe7,                    //  Usage Maximum (231)
	0x15, 0x00,                    //  Logical Minimum (0)
	0x25, 0x01,                    //  Logical Maximum (1)
	0x95, 0x08,                    //  Report Count (8)
	0x75, 0x01,                    //  Report Size (1)
	0x81, 0x02,                    //  Input (Data,Var,Abs)
	0x95, 0x01,                    //  Report Count (1)
	0x75, 0x08,                    //  Report Size (8)
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)
	0x05, 0x07,                    //  Usage Page (Keyboard)
	0x19, 0x00,                    //  Usage Minimum (0)
	0x2a, 0xff, 0x00,              //  Usage Maximum (255)
	0x15, 0x00,                    //  Logical Minimum (0)
	0x26, 0xff, 0x00,              //  Logical Maximum (255)
	0x95, 0x06,                    //  Report Count (6)
	0x75, 0x08,                    //  Report Size (8)
	0x81, 0x00,                    //  Input (Data,Arr,Abs)
	0xc0,                          // End Collection

};

static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, report_map,
				 sizeof(report_map));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("notify_enabled: %d", notify_enabled);
}

static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_info, NULL, &info),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),

	/* Report 1: gamepad */
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input_gamepad),

	/* Report 2: mouse */
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input_mouse),

	/* Report 3: keyboard */
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ_ENCRYPT,
			       read_input_report, NULL, NULL),
	BT_GATT_CCC(input_ccc_changed,
		    BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_report, NULL, &input_keyboard),

	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, write_ctrl_point, &ctrl_point),
);

struct code_to_bit_map {
	uint16_t code;
	uint8_t bit;
};

static int code_to_bit(const struct code_to_bit_map *map, int map_size,
		       uint16_t code)
{
	int i;

	for (i = 0; i < map_size; i++) {
		if (map[i].code == code) {
			return map[i].bit;
		}
	}
	return -1;
}

struct {
	uint8_t hat;
	uint16_t buttons;
	uint8_t x;
	uint8_t y;
	uint8_t rx;
	uint8_t ry;
} __packed report_gamepad, report_gamepad_last;

struct {
	uint8_t buttons;
	int16_t x;
	int16_t y;
	int8_t wheel;
} __packed report_mouse, report_mouse_last;

#define KEYS_REPORT_SIZE 6
struct {
	uint8_t modifiers;
	uint8_t _reserved;
	uint8_t keys[KEYS_REPORT_SIZE];
} __packed report_keyboard, report_keyboard_last;

#define HAT_UP 0
#define HAT_DOWN 1
#define HAT_LEFT 2
#define HAT_RIGHT 3

static const struct code_to_bit_map hat_map[] = {
	{INPUT_BTN_DPAD_UP, HAT_UP},
	{INPUT_BTN_DPAD_DOWN, HAT_DOWN},
	{INPUT_BTN_DPAD_LEFT, HAT_LEFT},
	{INPUT_BTN_DPAD_RIGHT, HAT_RIGHT},
};

static void ble_hog_set_hat(uint16_t code, uint32_t value)
{
	static uint8_t hat_bits;
	int bit;

	bit = code_to_bit(hat_map, ARRAY_SIZE(hat_map), code);
	if (bit < 0) {
		return;
	}

	WRITE_BIT(hat_bits, bit, value);

	switch (hat_bits) {
	case BIT(HAT_UP):
		report_gamepad.hat = 0;
		break;
	case BIT(HAT_UP) | BIT(HAT_RIGHT):
		report_gamepad.hat = 1;
		break;
	case BIT(HAT_RIGHT):
		report_gamepad.hat = 2;
		break;
	case BIT(HAT_RIGHT) | BIT(HAT_DOWN):
		report_gamepad.hat = 3;
		break;
	case BIT(HAT_DOWN):
		report_gamepad.hat = 4;
		break;
	case BIT(HAT_DOWN) | BIT(HAT_LEFT):
		report_gamepad.hat = 5;
		break;
	case BIT(HAT_LEFT):
		report_gamepad.hat = 6;
		break;
	case BIT(HAT_LEFT) | BIT(HAT_UP):
		report_gamepad.hat = 7;
		break;
	default:
		report_gamepad.hat = 8;
	}
}

static const struct code_to_bit_map button_map[] = {
	{INPUT_BTN_MODE, 8},
};

static void ble_hog_set_key_gamepad(uint16_t code, uint32_t value)
{
	int bit;

	bit = code_to_bit(button_map, ARRAY_SIZE(button_map), code);
	if (bit < 0) {
		return;
	}

	WRITE_BIT(report_gamepad.buttons, bit, value);
}

static void ble_hog_set_key_mouse(uint16_t code, uint32_t value)
{
	uint16_t bit;

	switch (code) {
	case INPUT_BTN_TL2:
		bit = 1;
		break;
	case INPUT_BTN_TR2:
		bit = 0;
		break;
	default:
		return;
	}

	WRITE_BIT(report_mouse.buttons, bit, value);
}

static void ble_hog_set_abs(uint16_t code, uint32_t value)
{
	switch (code) {
	case INPUT_ABS_X:
		report_gamepad.x = CLAMP(value, 0, UINT8_MAX);
		break;
	case INPUT_ABS_Y:
		report_gamepad.y = CLAMP(value, 0, UINT8_MAX);
		break;
	default:
		return;
	}
}

static void ble_hog_set_rel(uint16_t code, int32_t value)
{
	switch (code) {
	case INPUT_REL_X:
		report_mouse.x = CLAMP(value, INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_Y:
		report_mouse.y = CLAMP(value, INT16_MIN, INT16_MAX);
		break;
	case INPUT_REL_Z:
		report_mouse.wheel = CLAMP(value, INT8_MIN, INT8_MAX);;
		break;
	default:
		return;
	}
}

static const struct code_to_bit_map button_map_keyboard[] = {
	{INPUT_KEY_0, 0x52}, /* dpad up */
	{INPUT_KEY_1, 0x51}, /* dpad down */
	{INPUT_KEY_2, 0x50}, /* dpad left */
	{INPUT_KEY_3, 0x4f}, /* dpad right */

	{INPUT_BTN_NORTH, 0x17},
	{INPUT_BTN_SOUTH, 0x09},
	{INPUT_BTN_WEST, 0x15},
	{INPUT_BTN_EAST, 0x19},

	{INPUT_BTN_SELECT, 0x2b},
	{INPUT_BTN_START, 0x29},
	{INPUT_KEY_A, 0x10}, /* select 2 */
	{INPUT_KEY_B, 0x11}, /* start 2 */

	{INPUT_BTN_TL, 0x14},
	{INPUT_BTN_TR, 0x08},

	{INPUT_KEY_C, 0x2c}, /* L4 */
};

static void ble_hog_set_key_keyboard(uint16_t code, uint32_t value)
{
	int i;
	int hid_code;

	/* modifiers */
	if (code == INPUT_BTN_THUMBL) {
		WRITE_BIT(report_keyboard.modifiers, 1, value);
		return;
	} else if (code == INPUT_KEY_D) {
		WRITE_BIT(report_keyboard.modifiers, 0, value);
		return;
	}

	/* normal keys */
	hid_code = code_to_bit(button_map_keyboard,
			       ARRAY_SIZE(button_map_keyboard),
			       code);
	if (hid_code < 0) {
		return;
	}

	if (value) {
		for (i = 0; i < KEYS_REPORT_SIZE; i++) {
			if (report_keyboard.keys[i] == 0x00) {
				report_keyboard.keys[i] = hid_code;
				return;
			}
		}
	} else {
		for (i = 0; i < KEYS_REPORT_SIZE; i++) {
			if (report_keyboard.keys[i] == hid_code) {
				report_keyboard.keys[i] = 0x00;
				return;
			}
		}
	}
}

static void input_cb(struct input_event *evt)
{
	if (evt->type == INPUT_EV_KEY) {
		ble_hog_set_hat(evt->code, evt->value);
		ble_hog_set_key_gamepad(evt->code, evt->value);
		ble_hog_set_key_mouse(evt->code, evt->value);
		ble_hog_set_key_keyboard(evt->code, evt->value);
	} else if (evt->type == INPUT_EV_ABS) {
		ble_hog_set_abs(evt->code, evt->value);
	} else if (evt->type == INPUT_EV_REL) {
		ble_hog_set_rel(evt->code, evt->value);
	} else {
		LOG_ERR("unrecognized event type: %x", evt->type);
	}

	if (!notify_enabled) {
		return;
	}

	if (!input_queue_empty()) {
		return;
	}

	if (memcmp(&report_gamepad_last, &report_gamepad, sizeof(report_gamepad))) {
		bt_gatt_notify(NULL, &hog_svc.attrs[5],
			       &report_gamepad, sizeof(report_gamepad));
		memcpy(&report_gamepad_last, &report_gamepad, sizeof(report_gamepad));
	}

	if (memcmp(&report_mouse_last, &report_mouse, sizeof(report_mouse))) {
		bt_gatt_notify(NULL, &hog_svc.attrs[9],
			       &report_mouse, sizeof(report_mouse));
		report_mouse.x = 0;
		report_mouse.y = 0;
		report_mouse.wheel = 0;
		memcpy(&report_mouse_last, &report_mouse, sizeof(report_mouse));
	}

	if (memcmp(&report_keyboard_last, &report_keyboard, sizeof(report_keyboard))) {
		bt_gatt_notify(NULL, &hog_svc.attrs[13],
			       &report_keyboard, sizeof(report_keyboard));
		memcpy(&report_keyboard_last, &report_keyboard, sizeof(report_keyboard));
	}
}
INPUT_LISTENER_CB_DEFINE(NULL, input_cb);
