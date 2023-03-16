
#pragma once

#define EXAMPLE_WIFI_CONNECTION_MAXIMUM_RETRY 2
#define EXAMPLE_INVALID_REASON                255
#define EXAMPLE_INVALID_RSSI                  -128

extern bool gl_sta_connected;
extern bool gl_sta_got_ip;
extern bool ble_is_connected;
extern uint8_t gl_sta_bssid[6];
extern uint8_t gl_sta_ssid[32];
extern int gl_sta_ssid_len;
extern wifi_sta_list_t gl_sta_list;
extern bool gl_sta_is_connecting;
extern esp_blufi_extra_info_t gl_sta_conn_info;

void example_record_wifi_conn_info(int rssi, uint8_t reason);
void example_wifi_connect(void);
bool example_wifi_reconnect(void);
int softap_get_current_connection_number(void);
void initialise_wifi(void);