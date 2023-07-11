#pragma once

//Pin use to turn the pH sensor boards
#define POWER_GPIO          GPIO_NUM_8

//Initiate, configure and calibrate ADC channel
void sensors_init(void);

/*
    Reads voltage on the infiltration ADC channel
    Returns: Voltage in the pin
*/
int infiltration_read(void);

/*
    Reads voltage on both pH sensors ADC channels and
    saves those values in *ph_entry and *ph_exit
*/
void ph_sensors_read(float* ph_entry, float* ph_exit);

/*
    Gets temperature and humidity values from the 
    DHT11 sensor and stores them in *temp and *hum
*/
void hum_temp_sensor_read(int* temp, int* hum);

/*
    Checks if the water level sensor has been activated
    Returns: true if detected, false if not
*/
bool water_level_read(void);

/*
    Reads voltage on the battery ADC channel
    Returns: Voltage in the pin
*/
int battery_read(void);