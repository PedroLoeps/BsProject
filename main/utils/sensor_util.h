#pragma once


void sensors_init(void);
void sensors_read(int* temp, float* ph);
bool hum_sensor_read();