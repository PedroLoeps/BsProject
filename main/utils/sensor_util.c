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

#include "dht11.h"

const static char *TAG = "SENSORS";

#define PH_SENSOR_ENTRY_CHANNEL         ADC_CHANNEL_1
#define PH_SENSOR_EXIT_CHANNEL          ADC_CHANNEL_9

#define BATTERY_CHANNEL                 ADC_CHANNEL_8
#define BATTERY_POWER                   GPIO_NUM_3

#define HT_SENSOR_POWER_GPIO            GPIO_NUM_6
#define HUM_TEMP_SENSOR_GPIO            GPIO_NUM_5

#define INFILTRATION_SENSOR_CHANNEL     ADC_CHANNEL_3

#define WATER_LEVEL_GPIO                GPIO_NUM_7


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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, PH_SENSOR_ENTRY_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, PH_SENSOR_EXIT_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(sensor_handle, INFILTRATION_SENSOR_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//
    sensor_cali_handle = NULL;
    cali_done = sensors_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &sensor_cali_handle);


}

int infiltration_read(void) 
{
    gpio_set_pull_mode(INFILTRATION_SENSOR_CHANNEL, GPIO_PULLUP_ONLY);

    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, INFILTRATION_SENSOR_CHANNEL, &sensor_raw[0]));

    if (cali_done) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[0], &voltage[0]));
    }

    gpio_set_pull_mode(INFILTRATION_SENSOR_CHANNEL, GPIO_FLOATING);
    
    return (voltage[0]);
}

void ph_sensors_read(float* ph_entry, float* ph_exit)
{
    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, PH_SENSOR_ENTRY_CHANNEL, &sensor_raw[0]));
    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, PH_SENSOR_EXIT_CHANNEL, &sensor_raw[1]));
    
    if (cali_done) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[0], &voltage[0]));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[1], &voltage[1]));
      
        float m = (8.8 - 4.01) / (1315 - 1805);
        *ph_entry = 7 - (1500 - voltage[0]) * m;
        *ph_entry = 7 - (1500 - voltage[1]) * m;
    }
}

void hum_temp_sensor_read(int* temp, int* hum)
{
    gpio_set_direction(HUM_TEMP_SENSOR_GPIO, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_pull_mode(HUM_TEMP_SENSOR_GPIO, GPIO_PULLUP_ONLY);

    DHT11_init(HUM_TEMP_SENSOR_GPIO);

    *temp = DHT11_read().temperature;
    *hum = DHT11_read().humidity;

    gpio_set_pull_mode(HUM_TEMP_SENSOR_GPIO, GPIO_FLOATING);

}

bool water_level_read(void)
{
    bool detected;

    gpio_set_direction(WATER_LEVEL_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(WATER_LEVEL_GPIO, GPIO_PULLUP_ONLY);
    
    if(gpio_get_level(WATER_LEVEL_GPIO)) detected = false;
    else detected = true;
    
    gpio_set_pull_mode(WATER_LEVEL_GPIO, GPIO_FLOATING);

    return detected;
}

int battery_read(void)
{
    gpio_set_direction(BATTERY_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(BATTERY_POWER, 1);

    ESP_ERROR_CHECK(adc_oneshot_read(sensor_handle, BATTERY_CHANNEL, &sensor_raw[0]));
    
    if (cali_done) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(sensor_cali_handle, sensor_raw[0], &voltage[0]));
    }

    gpio_set_level(BATTERY_POWER, 0);

    return voltage[0];
}