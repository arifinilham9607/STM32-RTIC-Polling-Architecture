/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Bulletproof Proteus Architecture (Pure Polling ADC, NO DMA)
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"

/* USER CODE BEGIN Includes */
#include "arm_math.h"
#include <stdio.h> 
/* USER CODE END Includes */

#define STAGES 1

arm_biquad_casd_df1_inst_f32 LPF_Sensor1;
arm_biquad_casd_df1_inst_f32 LPF_Sensor2;
float32_t biquadStateSensor1[4 * STAGES];
float32_t biquadStateSensor2[4 * STAGES];
float32_t biquadCoeffs[5 * STAGES] = { 0.05f, 0.1f, 0.05f, 0.3f, -0.1f };

float32_t g_last_sent_s1 = 0.0f;
float32_t g_last_sent_s2 = 0.0f;
float32_t g_threshold_delta = 0.05f;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);

/* Fungsi Pembacaan ADC Langsung (Tanpa DMA) */
uint16_t Read_ADC_Channel(uint32_t channel)
{
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, channel);
  LL_ADC_REG_StartConversionSWStart(ADC1);
  
  // Tunggu hingga konversi 1 saluran selesai
  while (LL_ADC_IsActiveFlag_EOS(ADC1) == 0) {}
  LL_ADC_ClearFlag_EOS(ADC1);
  
  return LL_ADC_REG_ReadConversionData12(ADC1);
}

/* USER CODE BEGIN 0 */
void Process_Sensor_Data(uint16_t raw_s1, uint16_t raw_s2)
{
  static uint32_t simulated_time_ms = 0; 

  // 1. Eksekusi DSP Multi-Sensor
  float32_t in_s1 = (float32_t)raw_s1;
  float32_t out_s1;
  arm_biquad_cascade_df1_f32(&LPF_Sensor1, &in_s1, &out_s1, 1);
  int32_t filter_s1 = (int32_t)out_s1;

  float32_t in_s2 = (float32_t)raw_s2;
  float32_t out_s2;
  arm_biquad_cascade_df1_f32(&LPF_Sensor2, &in_s2, &out_s2, 1);
  int32_t filter_s2 = (int32_t)out_s2;

  // 2. Evaluasi Event-Triggered (Threshold disesuaikan ke 800)
  uint8_t state_s1 = (filter_s1 > 800) ? 1 : 0;
  uint8_t state_s2 = (filter_s2 > 800) ? 1 : 0;
  
  // Kalkulasi Delta Absolut untuk Grafik B
  int32_t delta_absolut = filter_s1 - filter_s2;
  if(delta_absolut < 0) delta_absolut = -delta_absolut;

  // Status Sistem Gabungan (0, 1, 2, atau 3)
  uint8_t critical_system_state = state_s1 + (state_s2 * 2); 

  // 3. Logika Aktuator Multi-Sensor
  if (state_s1 == 1) LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_5); // LED 1 ON
  else LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_5);

  if (state_s2 == 1) LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_6); // LED 2 ON (Tambahkan PA6 di inisialisasi!)
  else LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_6);

  // 4. Format CSV Super-Lengkap (8 Kolom)
  // Format: Waktu, Raw1, DSP1, Raw2, DSP2, Delta, StateKritis, AktuatorGabungan
  char tx_buffer[100];
  sprintf(tx_buffer, "%lu,%u,%ld,%u,%ld,%ld,%d,%d\r\n", 
          simulated_time_ms, raw_s1, filter_s1, raw_s2, filter_s2, delta_absolut, critical_system_state, state_s1);

  // 5. Transmisi
  for(uint8_t i = 0; tx_buffer[i] != '\0'; i++) {
    while (!LL_USART_IsActiveFlag_TXE(USART1)) {}
    LL_USART_TransmitData8(USART1, tx_buffer[i]);
  }

  simulated_time_ms += 10; 
}
/* USER CODE END 0 */

int main(void)
{
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_AFIO);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();

  arm_biquad_cascade_df1_init_f32(&LPF_Sensor1, STAGES, biquadCoeffs, biquadStateSensor1);
  arm_biquad_cascade_df1_init_f32(&LPF_Sensor2, STAGES, biquadCoeffs, biquadStateSensor2);

  LL_ADC_Enable(ADC1);
  LL_ADC_StartCalibration(ADC1);
  while ((ADC1->CR2 & ADC_CR2_CAL) != 0) {}

  uint16_t adc_val1 = 0;
  uint16_t adc_val2 = 0;

  while (1)
  {
    // Baca Sensor secara manual (Bypass DMA total)
    adc_val1 = Read_ADC_Channel(LL_ADC_CHANNEL_1); // Baca PA1
    adc_val2 = Read_ADC_Channel(LL_ADC_CHANNEL_2); // Baca PA2

    // Proses Data
    Process_Sensor_Data(adc_val1, adc_val2);
    
    // Jeda menggunakan perulangan aman
    for(volatile uint32_t i = 0; i < 200000; i++);
  }
}

void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_2){}
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1){}
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2, LL_RCC_PLL_MUL_16);
  LL_RCC_PLL_Enable();
  while(LL_RCC_PLL_IsReady() != 1){}
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL){}
  LL_Init1msTick(64000000);
  LL_SetSystemCoreClock(64000000);
  LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSRC_PCLK2_DIV_6);
}

static void MX_ADC1_Init(void)
{
  LL_ADC_InitTypeDef ADC_InitStruct = {0};
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct = {0};
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
  
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1|LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
  ADC_InitStruct.SequencersScanMode = LL_ADC_SEQ_SCAN_DISABLE; // Dimatikan agar bisa manual
  LL_ADC_Init(ADC1, &ADC_InitStruct);
  
  ADC_CommonInitStruct.Multimode = LL_ADC_MULTI_INDEPENDENT;
  LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &ADC_CommonInitStruct);
  
  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_DISABLE; 
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
  ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_NONE; // DMA diputus total
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);
  
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_239CYCLES_5);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_2, LL_ADC_SAMPLINGTIME_239CYCLES_5);
}

static void MX_USART1_UART_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
  
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_FLOATING;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  LL_USART_Init(USART1, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(USART1);
  LL_USART_Enable(USART1);
}

static void MX_GPIO_Init(void)
{
  // Mengaktifkan clock untuk GPIOD dan GPIOA
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOD);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Konfigurasi PA5 dan PA6 sekaligus sebagai Output
  // Menggunakan operator | (OR) untuk menggabungkan dua pin dalam satu register
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5 | LL_GPIO_PIN_6;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
void Error_Handler(void) { while(1) {} }