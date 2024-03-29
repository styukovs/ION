#include "measure.h"

measures get_measure(void)
{
    static bool update_calibr_coef = 0;
    const float coef_U = (U_ref/MAX_VAL_ADC) * ((R_div_fb+R_U_fb) / R_U_fb);     
    const float coef_I = ((U_ref/MAX_VAL_ADC) / R_I_fb) * 1000000.0;

    float U_coef_calibr, U_bias_calibr, I_above_100_coef_calibr, I_above_100_bias_calibr, I_below_100_coef_calibr, R_bias_calibr;
    //eeprom_write_object(0x01, &coef_U_calibr, sizeof(coef_U_calibr));
    
    eeprom_read_object(0x01, &U_coef_calibr, sizeof(float));
    eeprom_read_object(0x05, &U_bias_calibr, sizeof(float));
    eeprom_read_object(0x09, &I_above_100_coef_calibr, sizeof(float));
    eeprom_read_object(0x0D, &I_above_100_bias_calibr, sizeof(float));
    eeprom_read_object(0x11, &I_below_100_coef_calibr, sizeof(float));
    eeprom_read_object(0x15, &R_bias_calibr, sizeof(float));
    
    uint32_t i, j;
    uint32_t sum_adres_U = 0, sum_adres_I = 0;
    adc_result_t I_array[NUM_OF_SAMPLES];
    adc_result_t U_array[NUM_OF_SAMPLES];
    
    measures measure = {0, 0, 0};
    
    if (!update_calibr_coef)
    {
        
        update_calibr_coef = true;
    }
    
    for (j = 0; j < 128; j++)
    {
        for (i = 0; i < NUM_OF_SAMPLES; i++)
        {
            U_array[i] = ADC_GetConversion(FB_U);            
        }
        __delay_us(10);
        for (i = 0; i < NUM_OF_SAMPLES; i++)
        {
            I_array[i] = ADC_GetConversion(FB_I);
        }
        
        bubble_sort(I_array, NUM_OF_SAMPLES);//qsort(I_array, NUM_OF_SAMPLES, sizeof(adc_result_t), (adc_result_t(*) (const void *, const void *)) comp);
        bubble_sort(U_array, NUM_OF_SAMPLES);//qsort(U_array, NUM_OF_SAMPLES, sizeof(adc_result_t), (adc_result_t(*) (const void *, const void *)) comp);

        sum_adres_I += I_array[MEDIAN];
        sum_adres_U += U_array[MEDIAN];
        
        /*sum_adres_U += ADC_GetConversion(FB_U);
        __delay_us(10);
        sum_adres_I += ADC_GetConversion(FB_I);
        __delay_us(10);*/
    }
    
    measure.voltage = (sum_adres_U/128.0) * coef_U * U_coef_calibr + U_bias_calibr;
    
    measure.current = (sum_adres_I/128.0) * coef_I;
    
    if (measure.current < 100)
    {
        measure.current = measure.current * I_below_100_coef_calibr;
    }
    else
    {
        measure.current = measure.current * I_above_100_coef_calibr + I_above_100_bias_calibr;
    }
    
    if (measure.voltage < 0)
    {
        measure.voltage = 0;
    }
    if (measure.current < 0)
    {
        measure.current = 0;
    }
    measure.resistance = (measure.voltage / measure.current) - 1 + R_bias_calibr;
    
    if (measure.resistance < 0)
    {
        measure.resistance = 0;
    }
        
    return measure;
}

adc_result_t comp (const adc_result_t *i, const adc_result_t *j)
{
    return *i - *j;
}

void bubble_sort(adc_result_t list[], int8_t n)
{
  int8_t c, d;
  adc_result_t t;
 
  for (c = 0 ; c < n - 1; c++)
  {
    for (d = 0 ; d < n - c - 1; d++)
    {
      if (list[d] > list[d+1])
      {
        /* Swapping */
 
        t         = list[d];
        list[d]   = list[d+1];
        list[d+1] = t;
      }
    }
  }
}