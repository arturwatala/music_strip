void task_usart_rx();

typedef struct
{
	uint32_t timestamp;
	uint32_t v1;
	uint32_t v2;
	int8_t	data[204];
//	float data[32];
}uart_out_data;

typedef struct
{
	uint32_t v0;
	float32_t v1;
	float32_t v2;
	float32_t v3;
}uart_in_data;
