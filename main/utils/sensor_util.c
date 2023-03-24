#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "mqtt_util.h"

const static char *TAG = "Sensor";

#define SENSOR_CHANNEL          ADC_CHANNEL_1

static int sensor_raw;
static int voltage;
static bool cali_done;
adc_cali_handle_t sensor_cali_handle;
adc_oneshot_unit_handle_t sensor_handle;

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool sensor_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/*static void sensor_calibration_deinit(adc_cali_handle_t handle)
{
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
}*/

/*---------------------------------------------------------------
        ADC Initiation
---------------------------------------------------------------*/

void sensor_init(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &sensor_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, SENSOR_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//
    sensor_cali_handle = NULL;
    cali_done = sensor_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &sensor_cali_handle);
}

void sensor_read(void)
{
    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, SENSOR_CHANNEL, &sensor_raw));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, SENSOR_CHANNEL, sensor_raw);
    if (cali_done) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw, &voltage));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, SENSOR_CHANNEL, voltage);

        char voltage_data[5];
        sprintf(voltage_data, "%d mV", voltage);
        mqtt_send_data("sensor", voltage_data);
    }

    vTaskDelay(pdMS_TO_TICKS(5000));

}