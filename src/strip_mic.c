/*
 * strip_mic.c
 *
 *  Created on: 20.09.2020
 *      Author: Artur
 */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx_spi.h"

#include "stdint.h"
#include "arm_math.h"

#include "pdm_filter.h"
#include "helper.h"

#include "strip_cfg.h"
#include "strip_mic.h"



TaskHandle_t xTasI2Snotify;

uint16_t PDM_InBuff1[PDM_Input_Buffer_SIZE]; // 256 bajtow na 1ms przy oversamplingu 64 i fs=32khz
uint16_t PDM_InBuff2[PDM_Input_Buffer_SIZE];

uint16_t* PDM_InBuff_RD;
uint16_t* PDM_InBuff_WR;


// moze lepiej odrazu gromadzic wiecej
int16_t PCM_Output_Buffer[PCM_Output_Buffer_SIZE]; // (2048bitow)/1ms po decymacji daje 32 slowa 16bit/1ms

uint32_t InternalBufferSize = 0;
uint32_t Data_Status = 0;

static PDMFilter_InitStruct Filter;

static float32_t PCM_Buff[512];



void task_mic(void* p)
{
	static uint16_t volume ;
	static uint32_t i, z;

	Filter.Fs = OUT_FREQ;
	Filter.HP_HZ = 20;
	Filter.LP_HZ = 16000;
	Filter.In_MicChannels = 1;
	Filter.Out_MicChannels = 1;
	PDM_Filter_Init(&Filter);

	PDM_InBuff_RD =  PDM_InBuff1;
	PDM_InBuff_WR =  PDM_InBuff2;




	xTasI2Snotify = xTaskGetCurrentTaskHandle();
	I2S_Cmd(SPI2, ENABLE);
	SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
	portTASK_USES_FLOATING_POINT();

	while (1)
	{
		// swieze dane z mikrofonu
		if (pdTRUE == ulTaskNotifyTake( pdTRUE, portMAX_DELAY)) {
			volume = 80.0f;
			PDM_Filter_64_LSB((uint8_t*) PDM_InBuff_RD, PCM_Output_Buffer,
					volume, &Filter);

			// przepisanie danych do dluzszego bufora i normalizacja na zakres -1..1 czy jest potrzebna?
			for (i = 0; i < PCM_Output_Buffer_SIZE; i++) {

				//buffer_input[i + PCM_Output_Buffer_SIZE * z] = arm_sin_f32(2*PI*15000/32000*i) + arm_sin_f32(2*PI*1000/32000*i);

				PCM_Buff[i + PCM_Output_Buffer_SIZE * z] =
						(float32_t) (PCM_Output_Buffer[i]);
				//rand(); ((float32_t) ((int16_t) (PCM_Output_Buffer[i]))) / 32767.0f;
			}

			if (++z >= (512 / PCM_Output_Buffer_SIZE))
			{


				strip_visu(PCM_Buff); // zrobic tutaj podwojne buforoanie?

				z = 0;
			}
		}
	}
}


void SPI2_IRQHandler(void)
{

	static portBASE_TYPE xHigherPriorityTaskWoken;

	u16 app;

	xHigherPriorityTaskWoken = pdFALSE;

	// Check if new data are available in SPI data register
	if (SPI_GetITStatus(SPI2, SPI_I2S_IT_RXNE) != RESET)
	{
		app = SPI_I2S_ReceiveData(SPI2);

		// SPI |LSB0|MSB0|..
		// zmienic na I2S_InitStructure.I2S_Standard = I2S_Standard_LSB;
	//   PDM_InBuff_WR[InternalBufferSize++] = (uint8_t) app;
		PDM_InBuff_WR[InternalBufferSize++] = HTONS(app);

		//SPI |MSB0|LSB0|MBS1|LSB1|
		//*((uint16_t*) (&PDM_InBuff_WR[InternalBufferSize])) = app;
		//InternalBufferSize += 2;

		// odebrane 256B
		if (InternalBufferSize >= PDM_Input_Buffer_SIZE)
		{


			toggle_buff2(&PDM_InBuff_RD, &PDM_InBuff_WR);
			InternalBufferSize = 0;

			vTaskNotifyGiveFromISR(xTasI2Snotify, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}

//int16_t	sin_short[512];

//float32_t buffer_output[512];
//float32_t buffer_output_mag[256];
//float32_t maxvalue;
//uint32_t  maxvalueindex;

//arm_rfft_fast_instance_f32 S;
//arm_cfft_radix4_instance_f32 S_CFFT;

//	arm_rfft_fast_init_f32(&S, 512);
// Calculate Real FFT (512 probek real traktowane jest jako 256 probek zespolonych
// wykonywana jest zespolona transformata dla 256 probek
// otrzymujemy 256 probek zespolonych sygnalu (bez cz. ujemnych, ktore sa symetrycznym odbiciem)
//arm_rfft_fast_f32(&S, buffer_input, buffer_output, 0);

// Calculate magnitude
//arm_cmplx_mag_f32(buffer_output, buffer_output_mag, 256);

// Get maximum value of magnitude
//arm_max_f32(&(buffer_output_mag[1]), 256, &maxvalue, &maxvalueindex);

//d = (uart_out_data*)p_uart_tx_user;

//d->v1 = 0xFFEEAABB;

// przeskalowanie na int16 (w sumie to moglbybyc uint bo probki sa zawsze dodatnie?)
//for(int i=0; i<256; i++)
//	d->data[i] = (int16_t)((buffer_output_mag[i]/maxvalue)*32767.0f);

//toggle_buff2(&p_uart_tx_user, &p_uart_tx_free);

/*
 for(i=0; i<512; i++)
 {
 sin_short[i] = (int16_t)((arm_sin_f32(2*PI*160/32000*i)*32767.0));
 }
 */
