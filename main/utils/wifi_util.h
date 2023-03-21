
#pragma once

#define WIFI_CONNECTION_MAXIMUM_RETRY 2
#define INVALID_REASON                255
#define INVALID_RSSI                  -128

struct wifi_info 
{
    bool sta_connected;
    bool sta_got_ip;
    bool ble_is_connected;
    uint8_t sta_bssid[6];
    uint8_t sta_ssid[32];
    int sta_ssid_len;
    wifi_sta_list_t sta_list;
    bool sta_is_connecting;
    esp_blufi_extra_info_t sta_conn_info;
};

void record_wifi_conn_info(int rssi, uint8_t reason);
void wifi_connect(void);
bool wifi_reconnect(void);
int softap_get_current_connection_number(void);
void initialise_wifi(void);
esp_err_t wifi_scan(void);