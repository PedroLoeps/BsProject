#pragma once

#define WIFI_CONNECTION_MAXIMUM_RETRY 2
#define INVALID_REASON                255
#define INVALID_RSSI                  -128

//Record WI_FI connection information
void record_wifi_conn_info(int rssi, uint8_t reason);

//Connect to WI-FI
void wifi_connect(void);

//Reconnect to WI-FI
bool wifi_reconnect(void);

//
int softap_get_current_connection_number(void);

//Initialize WI-FI
void initialise_wifi(void);

//Scan available AP's
esp_err_t wifi_scan(void);