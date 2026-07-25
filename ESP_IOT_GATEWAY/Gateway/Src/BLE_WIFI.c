/**
 * @file BLE_WIFI.c
 * @brief ESP32 WiFi configuration application via Bluetooth Low Energy (BLE)
 * @note Operational flow:
 * 1. Boot -> Check Flash memory (NVS).
 * 2. IF NO WIFI: Enable BLE -> Wait for App to send SSID/Pass -> Save to NVS -> Reset.
 * 3. IF WIFI EXISTS: Release Bluetooth RAM -> Enable WiFi and connect to Router.
 * 4. BOOT button (GPIO0): Press and hold for 3 seconds to erase NVS, returning the board to BLE waiting state.
 */

#include "global.h"

// ==============================================================================
// 1. BASIC CONFIGURATION FOR BLUETOOTH LE (BLE)
// ==============================================================================
#define GATTS_TABLE_TAG "GATTS_TABLE_DEMO"

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SAMPLE_DEVICE_NAME          "ESP_GATTS_DEMO"
#define SVC_INST_ID                 0

// Enum to manage Attributes in the GATT table
enum
{
    IDX_SVC,
    IDX_CHAR_SSID,
    IDX_CHAR_VAL_SSID,
    
    IDX_CHAR_PASS,
    IDX_CHAR_VAL_PASS,

    BLE_WIFI_IDX_NB
};

// Define the maximum length for received characteristic values
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 100
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

uint16_t ble_wifi_table[BLE_WIFI_IDX_NB];

// ==============================================================================
// 2. ADVERTISING PACKET CONFIGURATION
// ==============================================================================
static uint8_t adv_config_done = 0;

// Advertising raw data array
#define CONFIG_SET_RAW_ADV_DATA
static uint8_t raw_adv_data[] = {
    /* Flags */
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
    /* TX Power Level */
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0xEB,
    /* Complete 16-bit Service UUIDs */
    0x03, ESP_BLE_AD_TYPE_16SRV_CMPL, 0x0F, 0x00,
    /* Complete Local Name */
    0x10, ESP_BLE_AD_TYPE_NAME_CMPL,
    'B', 'L', 'E', '_', 'W', 'I', 'F', 'I', '_', 'C', 'O', 'N', 'F', 'I', 'G'
};

// Scan response raw data array
static uint8_t raw_scan_rsp_data[] = {
    /* Flags */
    0x02, ESP_BLE_AD_TYPE_FLAG, 0x06,
    /* TX Power Level */
    0x02, ESP_BLE_AD_TYPE_TX_PWR, 0xEB,
    /* Complete 16-bit Service UUIDs */
    0x03, ESP_BLE_AD_TYPE_16SRV_CMPL, 0x0F, 0x00
};

// Advertising interval and mode parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// ==============================================================================
// 3. GATT TABLE SETUP
// ==============================================================================
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                    esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);


static struct gatts_profile_inst ble_wifi_config_profile_tab[PROFILE_NUM] = {
    [PROFILE_APP_IDX] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,     
    },
};

// UUIDs for specific Attributes
static const uint16_t WIFI_SERVICE_UUID = 0x00FF;
static const uint16_t SSID_CHAR_UUID = 0x0101;
static const uint16_t PASS_CHAR_UUID = 0x0102;
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

// Attribute database configuration
static const esp_gatts_attr_db_t gatt_db[BLE_WIFI_IDX_NB] = {
    [IDX_SVC] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(WIFI_SERVICE_UUID), (uint8_t *)&WIFI_SERVICE_UUID}},
    [IDX_CHAR_SSID] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},
    [IDX_CHAR_VAL_SSID] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&SSID_CHAR_UUID, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}},
    [IDX_CHAR_PASS] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},
    [IDX_CHAR_VAL_PASS] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&PASS_CHAR_UUID,ESP_GATT_PERM_WRITE, GATTS_DEMO_CHAR_VAL_LEN_MAX, 0, NULL}},
};

// ==============================================================================
// 4. BLE GAP (GENERIC ACCESS PROFILE) HANDLER
// ==============================================================================
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising start failed");
            }else{
                ESP_LOGI(GATTS_TABLE_TAG, "Advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TABLE_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "Stop adv successfully");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "update connection params status = %d, conn_int = %d, latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

// ==============================================================================
// 5. BLE GATTS HANDLER - WIFI CREDENTIALS RECEPTION
// ==============================================================================
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:
            esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
            if (raw_adv_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
            if (raw_scan_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;
            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, BLE_WIFI_IDX_NB, SVC_INST_ID);
            if (create_attr_ret){
                ESP_LOGE(GATTS_TABLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
            break;  
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "GATT_WRITE_EVT, handle = %d, len = %x", param->write.handle, param->write.len);
            char value[64] = {0};
            int len = param->write.len;
            if(len >= sizeof(value)){
                len = sizeof(value) - 1;
            }
            memcpy(value, param->write.value, len);
            value[len] = '\0';
            nvs_handle_t my_nvs_handle;
            esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &my_nvs_handle);
            if(err == ESP_OK){
                if(param->write.handle == ble_wifi_table[IDX_CHAR_VAL_SSID]){
                    ESP_LOGI(GATTS_TABLE_TAG, "SSID: %s", value);
                    nvs_set_str(my_nvs_handle, "ssid", value);
                    nvs_commit(my_nvs_handle);
                }
                else if(param->write.handle == ble_wifi_table[IDX_CHAR_VAL_PASS]){
                    ESP_LOGI(GATTS_TABLE_TAG, "PASS: %s", value);
                    nvs_set_str(my_nvs_handle, "pass", value);
                    nvs_commit(my_nvs_handle);
                    char check_ssid[64] = {0};
                    size_t check_size = sizeof(check_ssid);
                    esp_err_t check = nvs_get_str(my_nvs_handle, "ssid", check_ssid, &check_size);
                    nvs_close(my_nvs_handle);
                    if(check == ESP_OK && strlen(check_ssid) > 0){
                        nvs_close(my_nvs_handle);
                        ESP_LOGI(GATTS_TABLE_TAG, "SSID and PASS received, restarting...");
                        if (param->write.need_rsp){
                            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                                        param->write.trans_id, ESP_GATT_OK, NULL);
                        }
                        vTaskDelay(1000 / portTICK_PERIOD_MS); // Wait 1 second for NVS to write safely
                        esp_restart();
                    }
                }
                else{
                    nvs_close(my_nvs_handle);
                }
            }
            else{
                ESP_LOGI(GATTS_TABLE_TAG, "NVS ERROR");
            }
            if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
            ESP_LOG_BUFFER_HEX(GATTS_TABLE_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;  
            conn_params.min_int = 0x10;
            conn_params.timeout = 400;
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TABLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(GATTS_TABLE_TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != BLE_WIFI_IDX_NB){
                ESP_LOGE(GATTS_TABLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to BLE_WIFI_IDX_NB(%d)", param->add_attr_tab.num_handle, BLE_WIFI_IDX_NB);
            }
            else {
                ESP_LOGI(GATTS_TABLE_TAG, "create attribute table successfully, the number handle = %d",param->add_attr_tab.num_handle);
                memcpy(ble_wifi_table, param->add_attr_tab.handles, sizeof(ble_wifi_table));
                esp_ble_gatts_start_service(ble_wifi_table[IDX_SVC]);
            }
            break;
        }
        default:
            break;
    }
}

// GATT event distribution handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            ble_wifi_config_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
        } else {
            ESP_LOGE(GATTS_TABLE_TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == ble_wifi_config_profile_tab[idx].gatts_if) {
                if (ble_wifi_config_profile_tab[idx].gatts_cb) {
                    ble_wifi_config_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

// ==============================================================================
// 6. WIFI STATION MODULE
// ==============================================================================
#define WIFI_TAG "WIFI_STATION"
static int s_retry_num = 0;
static bool mqtt_mbedtls_started = false;

// Network state event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_TAG, "Retrying connection attempt %d...", s_retry_num);
        } else {
            ESP_LOGE(WIFI_TAG, "Cannot connect to this WiFi!");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "CONNECTION SUCCESSFUL! ESP32 IP is: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        if(!mqtt_mbedtls_started){
            xTaskCreate(mqtt_mbedtls_client_task, "mqtt_mbedtls_client_task", 10240, NULL, 5, NULL);
            mqtt_mbedtls_started = true;
        }
    }
}

// Function to configure and start WiFi
void wifi_init_sta(const char* ssid, const char* pass) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "WiFi initialized!");
}


// ==============================================================================
// 7. RESET BUTTON TASK
// ==============================================================================
#define BUTTON_PIN GPIO_NUM_0
void button_reset_task(void *arg)
{
    // Configure button pin
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_down_en = 0, 
        .pull_up_en = 1
    };
    gpio_config(&io_conf);

    int press_count = 0;

    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) {
            press_count++; 
            if (press_count >= 30) {
                ESP_LOGW("BUTTON", "3-SECOND LONG PRESS DETECTED!");
                ESP_LOGW("BUTTON", "Erasing all WiFi credentials (NVS)...");

                nvs_flash_erase(); 
                nvs_flash_init();  
                
                ESP_LOGW("BUTTON", "Erase complete! Chip will restart in 1 second...");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart(); 
            }
        } else {
            press_count = 0;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// ==============================================================================
// 8. BLUETOOTH INITIALIZATION FUNCTION (CALLED FROM MAIN)
// ==============================================================================
void ble_config_start(void) {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_IDX));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}