#include "ble_manager.h"
#include "driver_motor.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static const char *TAG = "BLE_MANAGER";

/* ─── UUID ─────────────────────────────────────────────── */
#define SVC_WIFI_UUID       0xAA01
#define CHAR_SSID_UUID      0xAA11
#define CHAR_PASSWORD_UUID  0xAA12
#define CHAR_STATUS_UUID    0xAA13

#define SVC_MOTOR_UUID      0xAA02
#define CHAR_CMD_UUID       0xAA21

/* ─── GATT profile index ────────────────────────────────── */
#define PROFILE_WIFI        0
#define PROFILE_MOTOR       1
#define PROFILE_NUM         2

/* ─── Attribute table index (WiFi service) ──────────────── */
enum {
    IDX_SVC_WIFI = 0,

    IDX_CHAR_SSID,
    IDX_CHAR_SSID_VAL,

    IDX_CHAR_PASSWORD,
    IDX_CHAR_PASSWORD_VAL,

    IDX_CHAR_STATUS,
    IDX_CHAR_STATUS_VAL,
    IDX_CHAR_STATUS_CFG,    /* CCCD cho notify */

    IDX_WIFI_NB,
};

/* ─── Attribute table index (Motor service) ─────────────── */
enum {
    IDX_SVC_MOTOR = 0,

    IDX_CHAR_CMD,
    IDX_CHAR_CMD_VAL,

    IDX_MOTOR_NB,
};

/* ─── Internal state ────────────────────────────────────── */
static uint16_t s_wifi_handle_table[IDX_WIFI_NB];
static uint16_t s_motor_handle_table[IDX_MOTOR_NB];
static uint16_t s_conn_id        = 0xFFFF;
static uint16_t s_gatts_if_wifi  = ESP_GATT_IF_NONE;
static uint16_t s_gatts_if_motor = ESP_GATT_IF_NONE;

static char s_ssid[64]     = {0};
static char s_password[64] = {0};

/* ─── Advertise config ──────────────────────────────────── */
static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ─── GATT attribute definitions — WiFi Service ─────────── */
static const uint16_t primary_service_uuid   = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_decl_uuid    = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_cfg_uuid     = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_write         = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify   = ESP_GATT_CHAR_PROP_BIT_READ |
                                               ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint16_t svc_wifi_uuid          = SVC_WIFI_UUID;
static const uint16_t char_ssid_uuid         = CHAR_SSID_UUID;
static const uint16_t char_password_uuid     = CHAR_PASSWORD_UUID;
static const uint16_t char_status_uuid       = CHAR_STATUS_UUID;

static uint8_t s_status_val                  = WIFI_STATUS_IDLE;
static uint8_t s_cccd_val[2]                 = {0x00, 0x00};

static const esp_gatts_attr_db_t s_wifi_attr_db[IDX_WIFI_NB] = {
    /* WiFi Service Declaration */
    [IDX_SVC_WIFI] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
         ESP_GATT_PERM_READ, sizeof(svc_wifi_uuid),
         sizeof(svc_wifi_uuid), (uint8_t *)&svc_wifi_uuid}
    },
    /* SSID Characteristic Declaration */
    [IDX_CHAR_SSID] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_decl_uuid,
         ESP_GATT_PERM_READ, sizeof(char_prop_write),
         sizeof(char_prop_write), (uint8_t *)&char_prop_write}
    },
    /* SSID Characteristic Value */
    [IDX_CHAR_SSID_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_ssid_uuid,
         ESP_GATT_PERM_WRITE, 64, 0, NULL}
    },
    /* Password Characteristic Declaration */
    [IDX_CHAR_PASSWORD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_decl_uuid,
         ESP_GATT_PERM_READ, sizeof(char_prop_write),
         sizeof(char_prop_write), (uint8_t *)&char_prop_write}
    },
    /* Password Characteristic Value */
    [IDX_CHAR_PASSWORD_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_password_uuid,
         ESP_GATT_PERM_WRITE, 64, 0, NULL}
    },
    /* Status Characteristic Declaration */
    [IDX_CHAR_STATUS] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_decl_uuid,
         ESP_GATT_PERM_READ, sizeof(char_prop_read_notify),
         sizeof(char_prop_read_notify), (uint8_t *)&char_prop_read_notify}
    },
    /* Status Characteristic Value */
    [IDX_CHAR_STATUS_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_status_uuid,
         ESP_GATT_PERM_READ, sizeof(s_status_val),
         sizeof(s_status_val), &s_status_val}
    },
    /* Status CCCD */
    [IDX_CHAR_STATUS_CFG] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_cfg_uuid,
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(s_cccd_val),
         sizeof(s_cccd_val), s_cccd_val}
    },
};

/* ─── GATT attribute definitions — Motor Service ────────── */
static const uint16_t svc_motor_uuid = SVC_MOTOR_UUID;
static const uint16_t char_cmd_uuid  = CHAR_CMD_UUID;
static uint8_t s_cmd_val             = 0x00;

static const esp_gatts_attr_db_t s_motor_attr_db[IDX_MOTOR_NB] = {
    /* Motor Service Declaration */
    [IDX_SVC_MOTOR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
         ESP_GATT_PERM_READ, sizeof(svc_motor_uuid),
         sizeof(svc_motor_uuid), (uint8_t *)&svc_motor_uuid}
    },
    /* Command Characteristic Declaration */
    [IDX_CHAR_CMD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&character_decl_uuid,
         ESP_GATT_PERM_READ, sizeof(char_prop_write),
         sizeof(char_prop_write), (uint8_t *)&char_prop_write}
    },
    /* Command Characteristic Value */
    [IDX_CHAR_CMD_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_cmd_uuid,
         ESP_GATT_PERM_WRITE, sizeof(s_cmd_val), 0, NULL}
    },
};

/* ─── WiFi event handler ────────────────────────────────── */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ble_manager_notify_wifi_status(WIFI_STATUS_CONNECTING);

    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
        ble_manager_notify_wifi_status(WIFI_STATUS_FAILED);

    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ble_manager_notify_wifi_status(WIFI_STATUS_CONNECTED);
    }
}

/* ─── Kết nối WiFi với SSID/pass nhận được qua BLE ──────── */
static void wifi_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,  &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid,     ssid,     sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

/* ─── GATTS event handler — WiFi Service ────────────────── */
static void gatts_wifi_event_handler(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gatts_if,
                                     esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        esp_ble_gatts_create_attr_tab(s_wifi_attr_db, gatts_if,
                                      IDX_WIFI_NB, 0);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK &&
            param->add_attr_tab.num_handle == IDX_WIFI_NB) {
            memcpy(s_wifi_handle_table, param->add_attr_tab.handles,
                   sizeof(s_wifi_handle_table));
            esp_ble_gatts_start_service(s_wifi_handle_table[IDX_SVC_WIFI]);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        s_conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "BLE client connected, conn_id=%d", s_conn_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        s_conn_id = 0xFFFF;
        ESP_LOGI(TAG, "BLE client disconnected, restart advertising");
        esp_ble_gap_start_advertising(&s_adv_params);
        break;

    case ESP_GATTS_WRITE_EVT: {
        uint16_t handle = param->write.handle;
        uint16_t len    = param->write.len;
        uint8_t *data   = param->write.value;

        if (handle == s_wifi_handle_table[IDX_CHAR_SSID_VAL]) {
            memset(s_ssid, 0, sizeof(s_ssid));
            memcpy(s_ssid, data, len < sizeof(s_ssid) - 1 ? len : sizeof(s_ssid) - 1);
            ESP_LOGI(TAG, "Received SSID: %s", s_ssid);

        } else if (handle == s_wifi_handle_table[IDX_CHAR_PASSWORD_VAL]) {
            memset(s_password, 0, sizeof(s_password));
            memcpy(s_password, data, len < sizeof(s_password) - 1 ? len : sizeof(s_password) - 1);
            ESP_LOGI(TAG, "Received Password (len=%d)", len);

            /* Đã có đủ SSID + Password → bắt đầu kết nối WiFi */
            if (strlen(s_ssid) > 0) {
                wifi_connect(s_ssid, s_password);
            }
        }
        break;
    }

    default:
        break;
    }
}

/* ─── GATTS event handler — Motor Service ───────────────── */
static void gatts_motor_event_handler(esp_gatts_cb_event_t event,
                                      esp_gatt_if_t gatts_if,
                                      esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        esp_ble_gatts_create_attr_tab(s_motor_attr_db, gatts_if,
                                      IDX_MOTOR_NB, 0);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK &&
            param->add_attr_tab.num_handle == IDX_MOTOR_NB) {
            memcpy(s_motor_handle_table, param->add_attr_tab.handles,
                   sizeof(s_motor_handle_table));
            esp_ble_gatts_start_service(s_motor_handle_table[IDX_SVC_MOTOR]);
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == s_motor_handle_table[IDX_CHAR_CMD_VAL] &&
            param->write.len == 1) {
            motor_cmd_t cmd = (motor_cmd_t)param->write.value[0];
            ESP_LOGI(TAG, "Motor command received: 0x%02X", cmd);
            driver_motor_handle_cmd(cmd);
        }
        break;

    default:
        break;
    }
}

/* ─── GATTS dispatch — phân luồng theo gatts_if ─────────── */
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                 esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == PROFILE_WIFI) {
            s_gatts_if_wifi = gatts_if;
        } else if (param->reg.app_id == PROFILE_MOTOR) {
            s_gatts_if_motor = gatts_if;
        }
    }

    if (gatts_if == s_gatts_if_wifi || gatts_if == ESP_GATT_IF_NONE) {
        gatts_wifi_event_handler(event, gatts_if, param);
    }
    if (gatts_if == s_gatts_if_motor || gatts_if == ESP_GATT_IF_NONE) {
        gatts_motor_event_handler(event, gatts_if, param);
    }
}

/* ─── GAP event handler ─────────────────────────────────── */
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                               esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT) {
        esp_ble_gap_start_advertising(&s_adv_params);
    }
}

/* ─── Public API ─────────────────────────────────────────── */
esp_err_t ble_manager_init(const char *device_name)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "bt controller init failed"); return ret; }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "bt controller enable failed"); return ret; }

    ret = esp_bluedroid_init();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "bluedroid init failed"); return ret; }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) { ESP_LOGE(TAG, "bluedroid enable failed"); return ret; }

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);

    /* Đăng ký 2 GATT profile */
    esp_ble_gatts_app_register(PROFILE_WIFI);
    esp_ble_gatts_app_register(PROFILE_MOTOR);

    /* Advertise data */
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp        = false,
        .include_name        = true,
        .include_txpower     = true,
        .min_interval        = 0x0006,
        .max_interval        = 0x0010,
        .appearance          = 0x00,
        .manufacturer_len    = 0,
        .p_manufacturer_data = NULL,
        .service_data_len    = 0,
        .p_service_data      = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
    };

    esp_ble_gap_set_device_name(device_name);
    esp_ble_gap_config_adv_data(&adv_data);

    ESP_LOGI(TAG, "BLE initialized, advertising as \"%s\"", device_name);
    return ESP_OK;
}

void ble_manager_notify_wifi_status(ble_wifi_status_t status)
{
    if (s_conn_id == 0xFFFF || s_gatts_if_wifi == ESP_GATT_IF_NONE) return;

    uint8_t val = (uint8_t)status;
    esp_ble_gatts_send_indicate(s_gatts_if_wifi, s_conn_id,
                                s_wifi_handle_table[IDX_CHAR_STATUS_VAL],
                                sizeof(val), &val, false);
}

void ble_manager_deinit(void)
{
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    ESP_LOGI(TAG, "BLE deinitialized");
}
