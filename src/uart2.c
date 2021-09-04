

#include "misc.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_dma.h"
#include "stdint.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "helper.h"

#include "uart2.h"



TaskHandle_t xTaskUart2Notify = NULL;

uint32_t f_rx_err;

#define RX_BUFF_LEN     16
static int8_t uart_rx_buff0[RX_BUFF_LEN];
static int8_t uart_rx_buff1[RX_BUFF_LEN];
static int8_t uart_rx_buff2[RX_BUFF_LEN];

#define TX_BUFF_LEN     256
static int8_t uart_tx_buff0[TX_BUFF_LEN];
static int8_t uart_tx_buff1[TX_BUFF_LEN];
static int8_t uart_tx_buff2[TX_BUFF_LEN];


static int8_t* p_uart_rx_free;
static int8_t* p_uart_rx_curr;
static int8_t* p_uart_rx_user;

int8_t* p_uart_tx_free;
int8_t* p_uart_tx_curr;
int8_t* p_uart_tx_user;

static uint32_t uart_rx_len_curr;



int32_t uart2_read(int8_t** buff)
{

	if (ulTaskNotifyTake( pdTRUE, 10000 / portTICK_RATE_MS) < 1)
		return -1;

	portENTER_CRITICAL();
	toggle_buff2(&p_uart_rx_user, &p_uart_rx_free);
	portEXIT_CRITICAL();
	*buff = p_uart_rx_user;

	f_rx_err = 0;

	return uart_rx_len_curr;
}

int32_t uart2_dma_send(int8_t* buff, int32_t len)
{
	DMA1_Stream6->NDTR = len;
	DMA1_Stream6->M0AR = (uint32_t)buff;

	DMA_Cmd(DMA1_Stream6, ENABLE);

	if (ulTaskNotifyTake( pdTRUE, 1000/portTICK_RATE_MS) < 1)
		return -1;



	return 0;
}



void USART2_IRQHandler()
{
	static int32_t rx_len;
	static int8_t tmp;
	static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if (USART2->SR & (USART_FLAG_PE | USART_FLAG_FE | USART_FLAG_ORE ))
	{
		f_rx_err = 1;
	}

	while (USART2->SR & USART_FLAG_RXNE)
	{
		if (rx_len < RX_BUFF_LEN)
			p_uart_rx_curr[rx_len++] = (int8_t)USART2->DR;
		else
			tmp = (int8_t)USART2->DR;

	}

	if ((USART2->SR & USART_FLAG_IDLE))
	{
		tmp = (int8_t)USART2->DR;

		if (!f_rx_err)
			uart_rx_len_curr = rx_len;
		else
			uart_rx_len_curr = -2;

		toggle_buff2(&p_uart_rx_curr, &p_uart_rx_free);

		rx_len = 0;
		f_rx_err = 0;
		// odczyt tylko po to zeby skasowac flage IDLE

		vTaskNotifyGiveFromISR(xTaskUart2Notify, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


void USART2_Init(void)
{
	USART_InitTypeDef USART_InitStructure;

	p_uart_rx_free = uart_rx_buff0; /* bufor wolny */
	p_uart_rx_curr = uart_rx_buff1; /* bufor aktualnie zapisywany w przerwaniu*/
	p_uart_rx_user = uart_rx_buff2; /* bufor zawierajacy odebrane dane */

	p_uart_tx_free = uart_tx_buff0; /* bufor do ktorego zapisywane moga byc dane */
	p_uart_tx_curr = uart_tx_buff1; /* bufor aktualnie wysylany w przerwaniu */
	p_uart_tx_user = uart_tx_buff2; /* bufor z danymi do wyslania danymi */

	USART_DeInit(USART2);
	USART_InitStructure.USART_BaudRate = 115200*2;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART2, &USART_InitStructure);


	//USART_DECmd(USART2, ENABLE);
	//USART_DEPolarityConfig(USART2, USART_DEPolarity_High);
	//USART_SetDEAssertionTime(USART2, 16);
	//USART_SetDEDeassertionTime(USART2, 16);

	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART2, USART_IT_IDLE, ENABLE);
	//USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
	USART_Cmd(USART2, ENABLE);
}




void DMA1_Stream6_IRQHandler(void)
{
	static portBASE_TYPE xHigherPriorityTaskWoken;
	xHigherPriorityTaskWoken = pdFALSE;

	/* Test on DMA Stream Transfer Complete interrupt */
	if (DMA_GetITStatus(DMA1_Stream6, DMA_IT_TCIF6))
	{
		DMA_Cmd(DMA1_Stream6, DISABLE);
		DMA_ClearITPendingBit(DMA1_Stream6, DMA_IT_TCIF6);
		vTaskNotifyGiveFromISR(xTaskUart2Notify, &xHigherPriorityTaskWoken);
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void DMA_Configuration(void) {
	DMA_InitTypeDef DMA_InitStructure;

	DMA_DeInit(DMA1_Stream6);
	DMA_InitStructure.DMA_Channel = DMA_Channel_4;
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral; // Transmit
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)"";
	DMA_InitStructure.DMA_BufferSize = 0x0000;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &USART2->DR;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA1_Stream6, &DMA_InitStructure);
	/* Enable the USART Tx DMA request */
	USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
	/* Enable DMA Stream Transfer Complete interrupt */
	DMA_ITConfig(DMA1_Stream6, DMA_IT_TC, ENABLE);
	/* Enable the DMA Tx Stream */

}
