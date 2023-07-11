
#pragma once

#include "nvs_flash.h"
#include "esp_wifi_types.h"

//Initiate Non Volatile Storage
esp_err_t nvs_init();

//Open Non Volatile Storage
esp_err_t open_nvs(const char* namespace, nvs_handle_t* my_handle);

//Save WI-FI information
esp_err_t set_saved_wifi(wifi_config_t* wifi_config);

//Get WI-FI information
esp_err_t get_saved_wifi(wifi_config_t* wifi_config);

//Save pH and temperature readings
esp_err_t set_saved_readings(int* temp, float* ph_entry, float* ph_exit, int size);

//Get pH and temperature readings
esp_err_t get_saved_readings(int* temp, float* ph_entry, float* ph_exit);