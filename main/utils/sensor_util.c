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
#include "driver/gpio.h"

const static char *TAG = "SENSOR";

#define PH_SENSOR_CHANNEL           ADC_CHANNEL_1
#define TEMP_SENSOR_CHANNEL         ADC_CHANNEL_3
#define PULLUP_GPIO                 GPIO_NUM_5
#define HUMIDITY_SENSOR_GPIO        GPIO_NUM_6


static int sensor_raw[2];
static int voltage[2];
static bool cali_done;
adc_cali_handle_t sensor_cali_handle;
adc_oneshot_unit_handle_t sensor_handle;

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool sensors_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
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

void sensors_init(void)
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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, PH_SENSOR_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, TEMP_SENSOR_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//
    sensor_cali_handle = NULL;
    cali_done = sensors_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &sensor_cali_handle);
}

static float voltage_to_ph(int voltage)
{
    return (((float)voltage*14)/3300);
}

static int voltage_to_temp(int voltage)
{
    return ((voltage*60)/3300);
}

void sensors_read(int* temp, float* ph)
{
    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, PH_SENSOR_CHANNEL, &sensor_raw[0]));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, PH_SENSOR_CHANNEL, sensor_raw[0]);
    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, TEMP_SENSOR_CHANNEL, &sensor_raw[1]));
    ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, TEMP_SENSOR_CHANNEL, sensor_raw[1]);
    if (cali_done) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[0], &voltage[0]));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, PH_SENSOR_CHANNEL, voltage[0]);
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[1], &voltage[1]));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, TEMP_SENSOR_CHANNEL, voltage[1]);
        printf("Test 1\n");

        *ph = voltage_to_ph(voltage[0]);
        printf("Test 2\n");
        *temp = voltage_to_temp(voltage[1]);
        printf("Test 3\n");
    }

    //Try not using this
    vTaskDelay(pdMS_TO_TICKS(5000));

}

bool hum_sensor_read()
{
    //Pode ser usar gpio_isolate em vez de ter um GPIO para o pullup?

    gpio_set_direction(PULLUP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUMIDITY_SENSOR_GPIO, GPIO_MODE_INPUT);

    gpio_set_level(PULLUP_GPIO, 1);

    bool detected = gpio_get_level(HUMIDITY_SENSOR_GPIO) ? false : true;

    gpio_set_level(PULLUP_GPIO, 0);

    return detected;
}