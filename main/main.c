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

extern struct wifi_info wifi_inf;

//Put this inside the sensor_config struct??
static RTC_DATA_ATTR struct timeval sleep_enter_time;

static const char *TAG = "MAIN";

const static char* ALARM_TOPIC = "sensor/alarm";
const static char* LOG_TOPIC = "sensor/log";

extern bool config_done;

void app_main(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int sleep_time_ms = (now.tv_sec - sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - sleep_enter_time.tv_usec) / 1000;

    //TODO get MAC adress from the microcontroller
    uint8_t mac_addr[6] = {0};
    if(esp_read_mac(mac_addr, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get base MAC address");
    }

    struct sensor_config sensor_conf;
    get_saved_config(&sensor_conf);

    bool perform_reading=false;

    initialise_wifi();
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    mqtt_client_init();//Change this
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    switch(esp_sleep_get_wakeup_cause()) 
    {
        case ESP_SLEEP_WAKEUP_TIMER:
            printf("Wake up from timer. Time spent in deep sleep: %dms\n", sleep_time_ms);
            if(++sensor_conf.current_wb_readings == sensor_conf.wb_reading)
            {
                sensor_conf.current_wb_readings = 0;
                perform_reading = true;
            }
            
            bool humidity_detected = hum_sensor_read();
            if(humidity_detected)
            {
                printf("Humidity Detected\n");
                char alarm_message[20];
                sprintf(alarm_message, "%x:%x:%x:%x:%x:%x 0",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                mqtt_send_data(ALARM_TOPIC, alarm_message);
            }
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            printf("Not a deep sleep reset\n");
            
            sensor_conf.wb_reading=2;
            sensor_conf.current_wb_readings=0;
            sensor_conf.readings=4;
            sensor_conf.current_readings=0;
            sensor_conf.temp = (int*)calloc(sensor_conf.readings,sizeof(int));
            sensor_conf.ph = (float*)calloc(sensor_conf.readings,sizeof(float));

            esp_err_t err = esp_blufi_host_and_cb_init();
            if (err) 
            {
                BLUFI_ERROR("%s initialise failed: %s\n", __func__, esp_err_to_name(err));
                return;
            }
            BLUFI_INFO("BLUFI VERSION %04x\n", esp_blufi_get_version());
            while(config_done == false)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            //esp_blufi_host_deinit(); //Da erro
    }

    const int wakeup_time_sec = 10;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000);


    if(perform_reading)
    {
        

        sensors_read(&sensor_conf.temp[sensor_conf.current_readings], &sensor_conf.ph[sensor_conf.current_readings]);

        if(++sensor_conf.current_readings == sensor_conf.readings)
        {
            char log_message[60]; //52 chars I think
            sprintf(log_message, "%x:%x:%x:%x:%x:%x %d %d %.1f %d %.1f %d %.1f %d %.1f", 
                mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], 
                sensor_conf.readings, 
                sensor_conf.temp[0], sensor_conf.ph[0],
                sensor_conf.temp[1], sensor_conf.ph[1],
                sensor_conf.temp[2], sensor_conf.ph[2],
                sensor_conf.temp[3], sensor_conf.ph[3]);

            mqtt_send_data(LOG_TOPIC, log_message);
            sensor_conf.current_readings = 0;
        }
    }

    printf("%d\n", sensor_conf.current_readings);
    printf("%d\n", sensor_conf.current_wb_readings);

    set_saved_config(&sensor_conf);

    printf("Entering deep sleep\n");
    gettimeofday(&sleep_enter_time, NULL);

    vTaskDelay(10 / portTICK_PERIOD_MS);

    esp_deep_sleep_start();
    
}