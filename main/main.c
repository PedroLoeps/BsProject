#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "nvs_flash.h"
#include "wifi_util.h"
#include "blufi_util.h"
#include "sensor_util.h"
#include "nvs_util.h"
#include "mqtt_util.h"
#include "esp_log.h"

#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_sleep.h"
#include "esp_mac.h"

#include "driver/gpio.h"

struct sensor_config
{
    //Number of readings on each day
    int readings;
    //Current number of readings done on this day
    int current_readings;
    //Number of times the sensor wakes up to check alarms before 
    //registering a reading
    int wb_reading;
    //Current number of times the sensor has woken up before reading
    int current_wb_readings;
};

static RTC_DATA_ATTR long int sleep_time_sec;
static RTC_DATA_ATTR struct sensor_config sensorConfig;
//It's not possible to dynamically set the size of an array in RTC_SLOW_MEMORY, so we decided
//on a maximum value of reads per day
static RTC_DATA_ATTR int temp[8];
static RTC_DATA_ATTR float ph_entry[8];
static RTC_DATA_ATTR float ph_exit[8];

//Variable that informs if there is any stored log that was not possible to send
static RTC_DATA_ATTR int backedup_data;

static RTC_DATA_ATTR uint8_t mac_addr[6];

static const char *TAG = "MAIN";

const static char* ALARM_TOPIC = "sensor/alarm";
const static char* LOG_TOPIC = "sensor/log";

const static int array_size = 16;

static void sort_float_array(float *array, int size)
{
    float temp;
    int i, j;

    for(i = 0; i < size - 1; i++ )
        {
            for(j = i + 1; j < size; j++)
            {
                if(array[i] > array[j])
                {
                    temp = array[i];
                    array[i] = array[j];
                    array[j] = temp;
                }
            }
        }
}


void app_main(void)
{
    //Informs if on this wakeup the microcontroller will read the sensors
    bool perform_reading=false;

    //Informs if the client has already been initiated
    int client_initiated = 0;
    

    switch(esp_sleep_get_wakeup_cause()) 
    {
        case ESP_SLEEP_WAKEUP_TIMER:
            printf("Wake up from timer.\n");
            if(++(sensorConfig.current_wb_readings) >= sensorConfig.wb_reading)
            {
                sensorConfig.current_wb_readings = 0;
                perform_reading = true;
            }
            
            bool water_detected = water_level_read();
            if(water_detected)
            {
                nvs_init();

                initialise_wifi();
                client_initiated = 1;
                //Wait a while so the can finish initiating
                vTaskDelay(5000 / portTICK_PERIOD_MS);

                mqtt_client_init();
                printf("High Water Level Detected\n");
                char alarm_wl_message[20];
                sprintf(alarm_wl_message, "%x:%x:%x:%x:%x:%x 1",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                mqtt_send_data(ALARM_TOPIC, alarm_wl_message);
            }
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");

            backedup_data = 0;

            int done = 0;

            if(esp_read_mac(mac_addr, ESP_MAC_WIFI_STA) != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get base MAC address");
            }
            
            //Default configuration(in case the user does not provide a configuration)
            sensorConfig.wb_reading=2;
            sensorConfig.current_wb_readings=0;
            sensorConfig.readings=4;
            sensorConfig.current_readings=0;

            //Calculate the time between wakeups in minutes
            sleep_time_sec = (24 * 60 * 60) / (sensorConfig.readings * sensorConfig.wb_reading);

            nvs_init();
            
            initialise_wifi();
            vTaskDelay(5000 / portTICK_PERIOD_MS);

            esp_err_t err = esp_blufi_host_and_cb_init();
            if (err) 
            {
                BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(err));
                return;
            }
            
            while(done == 0)
            {
                //Get configuration received from the user and end configuration
                done = get_sensor_config(&sensorConfig.wb_reading, &sensorConfig.readings);
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
    }

    if(perform_reading)
    {
        int hum;

        sensors_init();

        gpio_set_direction(POWER_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(POWER_GPIO, 1);
        gpio_hold_en(POWER_GPIO);

        esp_sleep_enable_timer_wakeup(180 * 1000000);
        esp_light_sleep_start();

        float *ph1, *ph2;
        ph1 = (float*)calloc(16, sizeof(float));
        ph2 = (float*)calloc(16, sizeof(float));

        int i;
        //Read the ph sensor a number of times, making it possible to remove any outlier
        for(i = 0; i < array_size; i++)
        {
            ph_sensors_read(&ph1[i], &ph2[i]);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        gpio_hold_dis(POWER_GPIO);
        gpio_set_level(POWER_GPIO, 0);

        sort_float_array(ph1, array_size);
        sort_float_array(ph2, array_size);

        ph_entry[sensorConfig.current_readings] = (ph1[array_size/2] + ph1[(array_size/2) + 1])/2;
        ph_exit[sensorConfig.current_readings] = (ph2[array_size/2] + ph2[(array_size/2) + 1])/2;

        hum_temp_sensor_read(&temp[sensorConfig.current_readings], &hum);

        if(hum >= 70)
        {
            if(client_initiated == 0)
            {
                nvs_init();

                initialise_wifi();
                vTaskDelay(5000 / portTICK_PERIOD_MS);

                mqtt_client_init();

                client_initiated = 1;
            }

            printf("High Humidity Detected\n");
            char alarm_h_message[20];
            sprintf(alarm_h_message, "%x:%x:%x:%x:%x:%x 2",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            mqtt_send_data(ALARM_TOPIC, alarm_h_message);
        }

        if(battery_read()<=1200)
        {
            if(client_initiated == 0)
            {
                nvs_init();

                initialise_wifi();
                vTaskDelay(5000 / portTICK_PERIOD_MS);

                mqtt_client_init();

                client_initiated = 1;
            }

            printf("Low Battery Detected\n");
            char alarm_b_message[20];
            sprintf(alarm_b_message, "%x:%x:%x:%x:%x:%x 3",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            mqtt_send_data(ALARM_TOPIC, alarm_b_message);
        }
        
        if(infiltration_read()<=3000)
        {
            if(client_initiated == 0)
            {
                nvs_init();

                initialise_wifi();
                vTaskDelay(5000 / portTICK_PERIOD_MS);

                mqtt_client_init();

                client_initiated = 1;
            }

            printf("Water Leak Detected\n");
            char alarm_l_message[20];
            sprintf(alarm_l_message, "%x:%x:%x:%x:%x:%x 4",
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            mqtt_send_data(ALARM_TOPIC, alarm_l_message);
        }

        if(++(sensorConfig.current_readings) >= sensorConfig.readings)
        {
            if(client_initiated == 0)
            {
                nvs_init();

                initialise_wifi();
                vTaskDelay(5000 / portTICK_PERIOD_MS);

                mqtt_client_init();
            }

            char log_message[80];
            sprintf(log_message, "%x:%x:%x:%x:%x:%x %d %d %.1f %.1f %d %.1f %.1f %d %.1f %.1f %d %.1f %.1f", 
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], 
                sensorConfig.readings, 
                temp[0], ph_entry[0], ph_exit[0],
                temp[1], ph_entry[1], ph_exit[1],
                temp[2], ph_entry[2], ph_exit[2],
                temp[3], ph_entry[3], ph_exit[3]);

            mqtt_send_data(LOG_TOPIC, log_message);

            if(check_mqtt() && backedup_data == 0)
            {
                printf("Saving data to send on another time");
                set_saved_readings(temp, ph_entry, ph_exit, sensorConfig.readings);
                backedup_data = 1;
            }
            else if(backedup_data == 1)
            {
                get_saved_readings(temp, ph_entry, ph_exit);

                sprintf(log_message, "%x:%x:%x:%x:%x:%x %d %d %.1f %.1f %d %.1f %.1f %d %.1f %.1f %d %.1f %.1f", 
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], 
                    sensorConfig.readings, 
                    temp[0], ph_entry[0], ph_exit[0],
                    temp[1], ph_entry[1], ph_exit[1],
                    temp[2], ph_entry[2], ph_exit[2],
                    temp[3], ph_entry[3], ph_exit[3]);

                mqtt_send_data(LOG_TOPIC, log_message);

                vTaskDelay(1000 / portTICK_PERIOD_MS);
                
                backedup_data = 0;

            }
            sensorConfig.current_readings = 0;
        }
    }

    esp_sleep_enable_timer_wakeup(sleep_time_sec * 1000000);

    vTaskDelay(10 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Entering Deep Sleep");

    esp_deep_sleep_start();
}