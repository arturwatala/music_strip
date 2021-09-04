void USART2_Init(void);
void DMA_Configuration();
int32_t uart2_read(int8_t** buff);
int32_t uart_dma_send(int8_t* buff, int32_t len);
