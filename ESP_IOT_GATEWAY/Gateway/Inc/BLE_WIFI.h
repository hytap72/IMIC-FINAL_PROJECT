#ifndef BLE_WIFI_H
#define BLE_WIFI_H

void wifi_init_sta(const char* ssid, const char* pass);
void button_reset_task(void *arg);
void ble_config_start(void);

#endif