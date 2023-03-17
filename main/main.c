#include <stdio.h>
#include "nvs_flash.h"


#include "esp_blufi_api.h"
#include "blufi.h"
#include "wifi.h"

#include "esp_blufi.h"

void app_main(void)
{
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err); //Understand what this does better but it is usefull--

    initialise_wifi();


    err = esp_blufi_host_and_cb_init();
    if (err) {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
}