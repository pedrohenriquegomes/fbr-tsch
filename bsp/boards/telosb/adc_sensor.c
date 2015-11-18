#include "adc_sensor.h"
//#include "adc.h"
//#include <headers/hw_cctest.h>
//#include <headers/hw_rfcore_xreg.h>

//=========================== defines =========================================

//=========================== variables =======================================

//=========================== prototype =======================================

//=========================== public ==========================================

/**
   \brief Initialize the sensor
*/
void adc_sensor_init(void) {
}

/**
   \brief Read rough data from sensor
   \param[out] ui16Dummy rough data.
*/
uint16_t adc_sens_read_solar(void) {
   return 0;
}

/**
   \brief Convert rough data to human understandable
   \param[in] cputemp rough data.
   \param[out] the number of registered OpenSensors.
*/
float adc_sens_convert_solar(uint16_t cputemp) {
   return 0;
}

/**
   \brief Read rough data from sensor
   \param[out] ui16Dummy rough data.
*/
uint16_t adc_sens_read_light(void) {
   return 0;
}

/**
   \brief Convert rough data to human understandable
   \param[in] cputemp rough data.
   \param[out] the number of registered OpenSensors.
*/
float adc_sens_convert_light(uint16_t cputemp) {
   return 0;
}

//=========================== private =========================================
