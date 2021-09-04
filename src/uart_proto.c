#include "stdint.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "helper.h"

#include "arm_math.h"
#include "uart_proto.h"
#include "uart2.h"


extern TaskHandle_t xTaskUart2Notify;

extern  int8_t* p_uart_tx_free;
extern  int8_t* p_uart_tx_curr;
extern  int8_t* p_uart_tx_user;

float32_t mic_vol;
float32_t gain_attack;

void task_usart_rx()
{
	static int8_t* buff;
	static int32_t len;
	static uart_out_data* dout;
	static uart_in_data* din;

	xTaskUart2Notify = xTaskGetCurrentTaskHandle();

	DMA_Configuration();

	while(1)
	{
		len = uart2_read(&buff);
		if(len > 4)
		{

			din = (uart_in_data*)buff;


			if(din->v0 == 0xAABBCCDD)
			{
				mic_vol = din->v1;
				gain_attack = din->v2;
				//toggle_buff2(&p_uart_tx_curr, &p_uart_tx_free);
				dout = (uart_out_data*)p_uart_tx_curr;
				dout->timestamp ++;

				//uart2_dma_send(p_uart_tx_curr, 216/*140*/);
			}
		}

	}
}
