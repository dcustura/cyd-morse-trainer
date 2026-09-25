#include "ble_hid_output.h"

#include "esp_bt.h"
#include "esp_err.h"
#include "esp_hidd.h"
#include "esp_log.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "ble_hid_output";

#define DEVICE_NAME "Morse Trainer"
#define HID_SERVICE_UUID 0x1812
#define HID_REPORT_ID_KEYBOARD 1
#define KEY_RELEASE_DELAY_MS 20
#define CHAR_QUEUE_DEPTH 32
#define TASK_STACK_SIZE 3072
#define TASK_PRIORITY 4

/* USB HID Usage Tables (stable spec, not ESP-IDF-version-sensitive): 0x04-0x1D
 * are the letter keys, 0x1E-0x27 the digit keys, and this modifier bit is the
 * left-Shift bit of a boot-keyboard report's first byte. */
#define KC_MOD_NONE 0x00
#define KC_MOD_SHIFT 0x02

/* Standard 8-byte boot-keyboard report descriptor: Report ID 1, 1 modifier
 * byte + 1 reserved byte + 5 output LED bits (unused by this device) + 6
 * input keycode bytes. */
static const uint8_t s_keyboard_report_map[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop Ctrls) */
    0x09, 0x06, /* Usage (Keyboard) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, 0x01, /*   Report ID (1) */
    0x05, 0x07, /*   Usage Page (Kbrd/Keypad) */
    0x19, 0xE0, /*   Usage Minimum (0xE0) */
    0x29, 0xE7, /*   Usage Maximum (0xE7) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x01, /*   Logical Maximum (1) */
    0x75, 0x01, /*   Report Size (1) */
    0x95, 0x08, /*   Report Count (8) */
    0x81, 0x02, /*   Input (Data,Var,Abs) -- modifier byte */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x08, /*   Report Size (8) */
    0x81, 0x03, /*   Input (Const,Var,Abs) -- reserved byte */
    0x95, 0x05, /*   Report Count (5) */
    0x75, 0x01, /*   Report Size (1) */
    0x05, 0x08, /*   Usage Page (LEDs) */
    0x19, 0x01, /*   Usage Minimum (Num Lock) */
    0x29, 0x05, /*   Usage Maximum (Kana) */
    0x91, 0x02, /*   Output (Data,Var,Abs) -- LED state, ignored */
    0x95, 0x01, /*   Report Count (1) */
    0x75, 0x03, /*   Report Size (3) */
    0x91, 0x03, /*   Output (Const,Var,Abs) -- LED padding */
    0x95, 0x06, /*   Report Count (6) */
    0x75, 0x08, /*   Report Size (8) */
    0x15, 0x00, /*   Logical Minimum (0) */
    0x25, 0x65, /*   Logical Maximum (101) */
    0x05, 0x07, /*   Usage Page (Kbrd/Keypad) */
    0x19, 0x00, /*   Usage Minimum (0x00) */
    0x29, 0x65, /*   Usage Maximum (0x65) */
    0x81, 0x00, /*   Input (Data,Array,Abs) -- up to 6 simultaneous keys */
    0xC0,       /* End Collection */
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    {.data = s_keyboard_report_map, .len = sizeof(s_keyboard_report_map)},
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id = 0x16C0, /* obdev.at's shared free-for-use VID, as used by many hobby USB/BLE HID devices */
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = DEVICE_NAME,
    .manufacturer_name = DEVICE_NAME,
    .serial_number = "1",
    .report_maps = s_report_maps,
    .report_maps_len = 1,
};

static esp_hidd_dev_t *s_hid_dev;
static QueueHandle_t s_char_queue;
static volatile bool s_enabled;
static volatile bool s_connected;
/* True only right after a real, non-Backspace keystroke was actually
 * queued. A word-gap MORSE_CODEC_EVENT_SPACE is forwarded only while this
 * is true, so the pause after something that sent nothing at all (an
 * unmapped prosign, an undecodable sequence) or sent Backspace (HH) never
 * produces a stray space of its own. Only ever touched from paddle_input's
 * keyer task via ble_hid_output_notify_char(). */
static bool s_last_send_was_char;

static struct ble_hs_adv_fields s_adv_fields;
static ble_uuid16_t s_hid_svc_uuid = BLE_UUID16_INIT(HID_SERVICE_UUID);

/* Maps a decoded Morse character to a boot-keyboard (modifier, keycode)
 * pair. Only covers the exact character set morse_codec.c's decode table
 * can produce (see components/morse_codec/morse_codec.c): uppercase
 * letters, digits, space, and its supported punctuation. Letters are sent
 * with Shift held so the paired device echoes uppercase, matching what the
 * Practice screen shows. Returns false for anything with no sensible
 * keystroke (the MORSE_CODEC_UNKNOWN_CHAR marker, a prosign sentinel, or
 * any other unexpected value), which the caller then drops. */
static bool char_to_hid_keycode(char c, uint8_t *modifier, uint8_t *keycode)
{
    if (c >= 'A' && c <= 'Z') {
        *modifier = KC_MOD_SHIFT;
        *keycode = (uint8_t)(0x04 + (c - 'A'));
        return true;
    }
    if (c >= '0' && c <= '9') {
        *modifier = KC_MOD_NONE;
        *keycode = (c == '0') ? 0x27 : (uint8_t)(0x1E + (c - '1'));
        return true;
    }

    switch (c) {
    case ' ':
        *modifier = KC_MOD_NONE;
        *keycode = 0x2C;
        return true;
    case '.':
        *modifier = KC_MOD_NONE;
        *keycode = 0x37;
        return true;
    case ',':
        *modifier = KC_MOD_NONE;
        *keycode = 0x36;
        return true;
    case '/':
        *modifier = KC_MOD_NONE;
        *keycode = 0x38;
        return true;
    case '?':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x38;
        return true;
    case '\'':
        *modifier = KC_MOD_NONE;
        *keycode = 0x34;
        return true;
    case '"':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x34;
        return true;
    case ';':
        *modifier = KC_MOD_NONE;
        *keycode = 0x33;
        return true;
    case ':':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x33;
        return true;
    case '-':
        *modifier = KC_MOD_NONE;
        *keycode = 0x2D;
        return true;
    case '_':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x2D;
        return true;
    case '=':
        *modifier = KC_MOD_NONE;
        *keycode = 0x2E;
        return true;
    case '+':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x2E;
        return true;
    case '(':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x26;
        return true;
    case ')':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x27;
        return true;
    case '&':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x24;
        return true;
    case '!':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x1E;
        return true;
    case '@':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x1F;
        return true;
    case '$':
        *modifier = KC_MOD_SHIFT;
        *keycode = 0x21;
        return true;
    case MORSE_CODEC_PROSIGN_HH:
        /* HH conventionally means "error, keyed over" -- Backspace is the
         * one prosign with an obvious single-keystroke equivalent. */
        *modifier = KC_MOD_NONE;
        *keycode = 0x2A;
        return true;
    default:
        return false;
    }
}

static void send_key_report(uint8_t modifier, uint8_t keycode)
{
    uint8_t report[8] = {modifier, 0, keycode, 0, 0, 0, 0, 0};
    esp_hidd_dev_input_set(s_hid_dev, 0, HID_REPORT_ID_KEYBOARD, report, sizeof(report));
    vTaskDelay(pdMS_TO_TICKS(KEY_RELEASE_DELAY_MS));
    memset(report, 0, sizeof(report));
    esp_hidd_dev_input_set(s_hid_dev, 0, HID_REPORT_ID_KEYBOARD, report, sizeof(report));
}

/* Drains s_char_queue and sends each character as a key-down/key-up HID
 * report, off the caller's task so ble_hid_output_notify_char() (called
 * from paddle_input's real-time keyer task) never blocks on the
 * KEY_RELEASE_DELAY_MS release delay. */
static void ble_hid_output_task(void *arg)
{
    (void)arg;
    char c;
    while (1) {
        if (xQueueReceive(s_char_queue, &c, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!s_connected) {
            continue;
        }
        uint8_t modifier, keycode;
        if (char_to_hid_keycode(c, &modifier, &keycode)) {
            send_key_report(modifier, keycode);
        }
    }
}

static void ble_hid_adv_init(void)
{
    memset(&s_adv_fields, 0, sizeof(s_adv_fields));
    s_adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    s_adv_fields.appearance = ESP_HID_APPEARANCE_KEYBOARD;
    s_adv_fields.appearance_is_present = 1;
    s_adv_fields.tx_pwr_lvl_is_present = 1;
    s_adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    s_adv_fields.name = (const uint8_t *)DEVICE_NAME;
    s_adv_fields.name_len = strlen(DEVICE_NAME);
    s_adv_fields.name_is_complete = 1;
    s_adv_fields.uuids16 = &s_hid_svc_uuid;
    s_adv_fields.num_uuids16 = 1;
    s_adv_fields.uuids16_is_complete = 1;

    /* "Just Works" bonding (no MITM protection): this board has no pairing
     * UI (no numeric-passkey screen) yet, and a keyboard has no keypad of
     * its own to enter one on either -- the same trust model most
     * consumer BLE keyboards use. */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
}

static int ble_hid_gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d", event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* Already bonded, but the peer wants to pair again -- drop the old
         * bond and let the new one proceed rather than reject it. */
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc) == 0) {
            ble_store_util_delete_peer(&desc.peer_id_addr);
        }
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    default:
        return 0;
    }
}

static void ble_hid_adv_start(void)
{
    int rc = ble_gap_adv_set_fields(&s_adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_hid_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    }
}

static void ble_hid_dev_event_cb(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "advertising");
        ble_hid_adv_start();
        break;
    case ESP_HIDD_CONNECT_EVENT:
        s_connected = true;
        ESP_LOGI(TAG, "connected");
        break;
    case ESP_HIDD_DISCONNECT_EVENT:
        s_connected = false;
        ESP_LOGI(TAG, "disconnected: %s",
                 esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev),
                                                param->disconnect.reason));
        ble_hid_adv_start();
        break;
    default:
        break;
    }
}

/* Declared (not in a public header in this IDF version) by the NimBLE
 * "store/config" module linked in via CONFIG_BT_NIMBLE_NVS_PERSIST; it wires
 * up ble_store_config_{read,write,delete} as the active bond store. */
void ble_store_config_init(void);

static void ble_hid_host_task(void *param)
{
    (void)param;
    nimble_port_run(); /* returns only once nimble_port_stop() is called */
    nimble_port_freertos_deinit();
}

static esp_err_t ble_controller_and_host_init(void)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    /* This device only ever advertises BLE, never classic BT -- releasing
     * classic BT's controller memory up front frees a meaningful chunk of
     * this board's already-tight SRAM. */
    esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "controller_mem_release failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "controller_init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "controller_enable failed: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_nimble_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_nimble_init failed: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

/* Releases everything ble_controller_and_host_init() claimed. Called when a
 * later bring-up step (esp_hidd_dev_init()) fails, so a half-started BT/
 * NimBLE stack doesn't sit there permanently holding RAM that nothing else
 * on this device can spare -- e.g. cli_start()'s own, much smaller
 * allocations failed this way during hardware bring-up of this feature. */
static void ble_controller_and_host_deinit(void)
{
    esp_nimble_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
}

void ble_hid_output_init(bool enabled)
{
    s_enabled = enabled;
    if (!enabled) {
        return;
    }

    s_char_queue = xQueueCreate(CHAR_QUEUE_DEPTH, sizeof(char));
    if (s_char_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate failed");
        s_enabled = false;
        return;
    }

    if (ble_controller_and_host_init() != ESP_OK) {
        s_enabled = false;
        return;
    }

    ble_hid_adv_init();

    esp_err_t err = esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE, ble_hid_dev_event_cb, &s_hid_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_hidd_dev_init failed: %s", esp_err_to_name(err));
        ble_controller_and_host_deinit();
        s_enabled = false;
        return;
    }

    /* esp_hidd_dev_init() (esp_hid's NimBLE backend) is what creates the GAP
     * service this sets the name on; NimBLE's own compiled-in default for
     * it is literally "nimble", which is what a peer reads and displays
     * once paired -- the advertised name set in ble_hid_adv_init() only
     * matters pre-connection, during scanning. */
    ble_svc_gap_device_name_set(DEVICE_NAME);

    ble_store_config_init();
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    xTaskCreate(ble_hid_output_task, "ble_hid_out", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    nimble_port_freertos_init(ble_hid_host_task);
}

bool ble_hid_output_is_connected(void)
{
    return s_connected;
}

void ble_hid_output_notify_char(morse_codec_event_t event, char decoded_char)
{
    if (!s_enabled || s_char_queue == NULL) {
        return;
    }

    if (event == MORSE_CODEC_EVENT_SPACE) {
        if (!s_last_send_was_char) {
            return;
        }
        s_last_send_was_char = false;
        xQueueSend(s_char_queue, &decoded_char, 0);
        return;
    }

    if (event != MORSE_CODEC_EVENT_CHAR) {
        /* MORSE_CODEC_EVENT_UNKNOWN: nothing sendable, and the pause that
         * follows shouldn't produce a space either. */
        s_last_send_was_char = false;
        return;
    }

    uint8_t modifier, keycode;
    if (!char_to_hid_keycode(decoded_char, &modifier, &keycode)) {
        /* An unmapped prosign (SK/VE/CT): nothing sendable either. */
        s_last_send_was_char = false;
        return;
    }

    s_last_send_was_char = (decoded_char != MORSE_CODEC_PROSIGN_HH);
    xQueueSend(s_char_queue, &decoded_char, 0);
}
