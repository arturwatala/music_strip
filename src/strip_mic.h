#ifndef STRIP_MIC_H_
#define STRIP_MIC_H_

#define DECIMATION_FACTOR       64
#define OUT_FREQ                32000	// cz. probkowania

// przyjmujemy ze dzialamy na 1ms probkach czyli (dla 1s mamy 32000 * 64 = 2048000b = 256000B)
// dla 1ms zatam 2048b = 256B
#define PDM_Input_Buffer_SIZE   ((OUT_FREQ/1000)*DECIMATION_FACTOR/16)// (ilosc probek na 1ms)*DECIMATION_FACTOR/8
// po decymacji 2048b/64 = 32 slowa 16bit
#define PCM_Output_Buffer_SIZE  (OUT_FREQ/1000)


void task_mic(void* p);



#endif /* STRIP_MIC_H_ */
