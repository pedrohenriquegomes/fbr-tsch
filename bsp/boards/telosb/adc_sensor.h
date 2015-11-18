#ifndef __ADC_SENSOR_H__
#define __ADC_SENSOR_H__

#include "board_info.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== module variables ================================

//=========================== prototypes ======================================

void adc_sensor_init(void);
uint16_t adc_sens_read_solar(void);
float adc_sens_convert_solar(uint16_t cputemp);
uint16_t adc_sens_read_light(void);
float adc_sens_convert_light(uint16_t cputemp);

#endif // __ADC_SENSOR_H__
