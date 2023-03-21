#include <stdio.h>
#include "nvs_flash.h"


#include "esp_blufi_api.h"
#include "blufi_util.h"
#include "wifi_util.h"
#include "nvs_util.h"

#include "esp_blufi.h"

void app_main(void)
{
    esp_err_t err;

    ESP_ERROR_CHECK(nvs_init());

    initialise_wifi();


    err = esp_blufi_host_and_cb_init();
    if (err) {
        BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
}