#include "global.h"

void app_main(void){
    init_hardware();
    //Khởi tạo phân vùng bộ nhớ Flash (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Hàm khởi tạo Task đọc button
    xTaskCreate(button_reset_task, "button_reset_task", 2048, NULL, 2, NULL);

    // Đọc NVS để kiểm tra thông tin WIFI
    nvs_handle_t my_nvs_handle;
    char ssid[64] = {0};
    char pass[64] = {0};
    size_t required_size = sizeof(ssid);
    bool has_wifi_config = false;

    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &my_nvs_handle);
    if (err == ESP_OK){
        err = nvs_get_str(my_nvs_handle, "ssid", ssid, &required_size);
        if (err == ESP_OK && strlen(ssid) > 0) {
            has_wifi_config = true;
            required_size = 64;
            nvs_get_str(my_nvs_handle, "pass", pass, &required_size);
            ESP_LOGI("MAIN", "Da tim thay WiFi trong bo nho: %s", ssid);
        }
        nvs_close(my_nvs_handle);
    }

    // Điều khiển luồng hoạt động của chương trình 
    if (has_wifi_config == true) {
        ESP_LOGI("MAIN", "Dang khoi dong WiFi");
        
        ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)); 
        
        wifi_init_sta(ssid, pass);
        
    } 
    else {
        ESP_LOGI("MAIN", "Chua co thong tin WiFi. Dang bat Bluetooth...");
        ble_config_start();
    }
}
