
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx_spi.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_tim.h"
#include "misc.h"
#include "arm_math.h"
#include "stdint.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

#include "main.h"
#include "uart_proto.h"

#include "strip_mic.h"
#include "strip_visu.h"
#include "uart2.h"
#include "uart3.h"


static void GPIO_Configure(void) ;
static void I2S_Configure(void);
static void NVIC_Configure(void);
static void RCC_Configure(void);





#define TOGGLE_LED()	GPIOD->ODR ^= GPIO_Pin_13

void timer_init()
{
	TIM_TimeBaseInitTypeDef TIM_BaseStruct;

	TIM_TimeBaseStructInit(&TIM_BaseStruct);
	TIM_TimeBaseInit(TIM2, &TIM_BaseStruct);

	TIM_Cmd(TIM2, ENABLE);

}


void task_idle(void *p)
{
	while(1)
	{
		vTaskDelay(500/portTICK_RATE_MS);
		TOGGLE_LED();

	//	uart3_dma_send("012345678901234567890123456789", 30);
		//USART2->DR = 'a';
	}

}


float32_t s1[13]= {100000.7,-100.0, -10000.0, 220.0, 100000.0, 1.0, 2.0, 3.0, -1.0, -100000.2, 9999999.0, -999.0, };
float32_t max;

int main()
{
	// enable FPU
	SCB->CPACR |= (0xF << 20);

	RCC_Configure();
	NVIC_Configure();
	GPIO_Configure();
	I2S_Configure();
	RNG_Cmd(ENABLE);

	RCC_ClocksTypeDef rccclk;

	RCC_GetClocksFreq(&rccclk);
	timer_init();


	//strip_visu(s1);

	strip_visu_init();


	USART2_Init();
	USART3_Init();
	DMA_ConfigurationUSART3();
	//

	xTaskCreate(task_idle, ( signed char * ) "IDLE_TASK", 128, NULL, tskIDLE_PRIORITY, ( xTaskHandle * ) NULL );
	xTaskCreate(task_mic, ( signed char * ) "MIC_TASK", 2048, NULL, tskIDLE_PRIORITY+1, ( xTaskHandle * ) NULL );
	//xTaskCreate(task_usart_rx, ( signed char * ) "USART_RX", 256, NULL, tskIDLE_PRIORITY, ( xTaskHandle * ) NULL );
	vTaskStartScheduler();
	for(;;);
}



void vApplicationTickHook()
{

}
void vApplicationStackOverflowHook()
{
	 for( ;; );
}


/* Private functions ---------------------------------------------------------*/
static void GPIO_Configure(void){
  GPIO_InitTypeDef GPIO_InitStructure;

  // Configure MP45DT02's CLK / I2S2_CLK (PB10) line
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  // Configure MP45DT02's DOUT / I2S2_DATA (PC3) line
  GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;	// pull-up bo linia danych z mikrofonu jest aktywna przez okresu zegara
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_SPI2);  // Connect pin 10 of port B to the SPI2 peripheral
  GPIO_PinAFConfig(GPIOC, GPIO_PinSource3, GPIO_AF_SPI2);   // Connect pin 3 of port C to the SPI2 peripheral

  /* Configure the GPIO_LED pin */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOD, &GPIO_InitStructure);

  // USART2
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2|GPIO_Pin_3; // PA2 = TX PA3 = RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);


  // USART3 // led strip
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_9; // PB10 = TX PB11 = RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOD, &GPIO_InitStructure);

  GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3);
  GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3);
}

static void I2S_Configure(void){
  I2S_InitTypeDef I2S_InitStructure;

  SPI_I2S_DeInit(SPI2);
  I2S_InitStructure.I2S_AudioFreq = OUT_FREQ*2;
  I2S_InitStructure.I2S_Standard = I2S_Standard_LSB;
  I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
  I2S_InitStructure.I2S_CPOL = I2S_CPOL_High;
  I2S_InitStructure.I2S_Mode = I2S_Mode_MasterRx;
  I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
  I2S_Init(SPI2, &I2S_InitStructure);

  // zalozona cz. probkowania po decymacji 32kHz, przy decymacji 64 cz. na wyjsciu CLK ma byc: 32kHz*64 = 2048000Hz
  // uklad generuje cz. dla dwoch kanalow 16bit czyli zegar bedzie mial cz. 32*AudioFreq
  // wiec potrzebujemy AudioFreq 2048000Hz/32 = 64000. Czyli fclk = (fs * ovs_ratio)/32 = (32kHz*64)/32 = fs*2

  // Enable the Rx buffer not empty interrupt

}

static void NVIC_Configure(void){
  NVIC_InitTypeDef NVIC_InitStructure;

  // Configure the interrupt priority grouping
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

  // Configure the SPI2 interrupt channel
  NVIC_InitStructure.NVIC_IRQChannel = SPI2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_Init(&NVIC_InitStructure);

  /* USART2 IRQ Channel configuration */
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_Init(&NVIC_InitStructure);


  NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream6_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);


  /* USART3 IRQ Channel configuration */
  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_Init(&NVIC_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

static void RCC_Configure(void){
  // Enable CRC module - required by PDM Library

  /********/
  /* AHB1 */
  /********/
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOD| RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC |
                         RCC_AHB1Periph_CRC, ENABLE);

  /********/
  /* APB1 */
  /********/
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
  /********/
  /* APB2 */
  /********/
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);

  /* random number generator */
  RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
  RCC_AHB2PeriphResetCmd(RCC_AHB2Periph_RNG, ENABLE);
  /* Release RNG from reset state */
  RCC_AHB2PeriphResetCmd(RCC_AHB2Periph_RNG, DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);



  RCC_PLLI2SCmd(ENABLE);
}
