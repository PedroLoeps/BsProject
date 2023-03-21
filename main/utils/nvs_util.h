
#pragma once

#include "nvs_flash.h"

esp_err_t nvs_init();
esp_err_t open_nvs(const char* namespace, nvs_handle_t* my_handle);
esp_err_t set_saved_wifi(wifi_config_t* wifi_config);
esp_err_t get_saved_wifi(wifi_config_t* wifi_config);