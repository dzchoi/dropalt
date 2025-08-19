// Rewrite of riot/cpu/sam0_common/periph/adc.c to support:
//   - ADC0 and ADC1 as well
//   - Resolution of adc_sample() hang when starting during the ADC startup period
//   - Averaging multiple samples (when ADC_SAMPLENUM is given)
//   - Non-blocking as well as blocking execution

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "periph/adc.h"

/**
 * @brief   Initialize the given ADC line
 *
 * The ADC line is initialized in synchronous, blocking mode.
 *
 * @param[in] line      line to initialize
 *
 * @return              0 on success
 * @return              -1 on invalid ADC line
 */
int adc_init(adc_t line);

/**
 * @brief   Initialize the ADC
 *
 * The ADC is powered on and configured with the given resolution. 
 * sampling continuously and resulting in the average.
 *
 * @param[in] dev       ADC0 or ADC1
 * @param[in] res       resolution to use for conversion
 *
 * @return              0 on success
 * @return              -1 on invalid ADC line
 */
int adc_configure(Adc* dev, adc_res_t res);

/**
 * @brief   Sample a value from the given ADC line
 *
 * This function can measure the input in non-blocking as well as in blocking execution,
 * replacing the existing adc_sample().
 *
 * @param[in] line      line to sample
 * @param[in] callback  NULL for blocking execution, or callback function to grab the
 *                      result for non-blocking execution. Beware that callback is called
 *                      in interrupt context.
 * @param[in] arg       argument to pass to callback function
 *
 * @return              the sampled value on success
 * @return              -1 if resolution is not applicable, or 0 on non-blocking
 *                      execution
 *
 * @pre
 *   The adc_configure() needs to be performed prior to this function call.
 */
int32_t adc_get(adc_t line, void (*callback)(void* arg, uint16_t result), void* arg);

#ifdef __cplusplus
}
#endif
