

#include <stdio.h>
#include "esp_err.h"
#include "esp_blufi_api.h"
#include "esp_log.h"
#include "esp_blufi.h"
#include "blufi.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "console/console.h"


void ble_store_config_init(void);

static void blufi_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blufi_on_sync(void)
{
  esp_blufi_profile_init();
}

void bleprph_host_task(void *param)
{
    ESP_LOGI("BLUFI_EXAMPLE", "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t esp_blufi_host_init(void)
{
    esp_err_t err;
    err = esp_nimble_init();
    if (err) {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

/* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = blufi_on_reset;
    ble_hs_cfg.sync_cb = blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = 4;
    /*
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
#endif
#ifdef CONFIG_EXAMPLE_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0; Need to define?
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;
#endif
#endif */

    int rc;
    rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set(BLUFI_DEVICE_NAME);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    esp_blufi_btc_init();

    err = esp_nimble_enable(bleprph_host_task);
    if (err) {
        BLUFI_ERROR("%s failed: %s\n", __func__, esp_err_to_name(err));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_blufi_host_deinit(void)
{
    esp_err_t ret = ESP_OK;

    ret = esp_blufi_profile_deinit();
    if(ret != ESP_OK) {
        return ret;
    }

    esp_blufi_btc_deinit();

    ret = nimble_port_stop();
    if (ret == 0) {
        nimble_port_deinit();
    }

    return ret;
}

esp_err_t esp_blufi_gap_register_callback(void)
{
    return ESP_OK;
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *example_callbacks)
{
    esp_err_t ret = ESP_OK;

    ret = esp_blufi_register_callbacks(example_callbacks);
    if(ret){
        BLUFI_ERROR("%s blufi register failed, error code = %x\n", __func__, ret);
        return ret;
    }


    //Find more about GAP API(Might not be needed)
    ret = esp_blufi_gap_register_callback();
    if(ret){
        BLUFI_ERROR("%s gap register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_host_init();
    if (ret) {
        BLUFI_ERROR("%s initialise host failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    return ret;
}