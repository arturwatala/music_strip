#include "stdint.h"
#include "arm_math.h"
#include "stm32f4xx_rng.h"


#include "strip_cfg.h"

#include "helper.h"
#include "uart_proto.h"
#include "stm32f4xx.h"

#include "mel_filter.h"
#include "strip_dsp.h"

#include "uart3.h"

#define BANDS_NO	12


typedef struct
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
}_pixel_color;

#define N_PIXELS_SIDE	(N_PIXELS/2)
#define N_RGB_OUT_DATA	(N_PIXELS*3)

float32_t r_pixel[N_PIXELS_SIDE];
float32_t g_pixel[N_PIXELS_SIDE];
float32_t b_pixel[N_PIXELS_SIDE];

static int8_t uart_data[256];


#define BUFF_LEN FRAMES_PER_BUFFER*2

float32_t y_roll[BUFF_LEN];
float32_t y_data[BUFF_LEN];
float32_t Y_data[BUFF_LEN];
float32_t YS[BUFF_LEN/2];
float32_t hamming_w[BUFF_LEN]; // z tego mozna zrobic stala pozniej

static arm_rfft_fast_instance_f32 S;
static uart_out_data* d;


static float32_t mel_out[16];
static float32_t mel_tmp[32];

static float32_t mel_lacc = 0.0f;
static float32_t mel_racc = 0.0f;
static int32_t n,k,l;

float32_t gain_val = 1.0f;

volatile static int32_t  act_val;


static float32_t mel_out_smoothed[12]  = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static float32_t visu_out[12]  = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
static float32_t tmp_out[12]  = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};

static float32_t r;
static float32_t g;
static float32_t b;

volatile static int32_t cnt;

static f32_conv_struct conv_struct;
static float32_t gain = 1.0f;

float32_t rand()
{

	while(RNG_GetFlagStatus(RNG_FLAG_DRDY)== RESET)
	{
	}
	return ((float32_t)RNG_GetRandomNumber())/((float32_t)((uint32_t)(0xFFFFFFFF)));
}


void strip_visu_init()
{
	int32_t i;

	arm_rfft_fast_init_f32(&S, BUFF_LEN);

	for(i=0; i<BUFF_LEN; i++)
	{
		hamming_w[i] = 0.54f + 0.46f*arm_cos_f32( ( (2*PI)/((float32_t)BUFF_LEN) )*(i- 512.0f) );
	}
}



float32_t exp_filter(float32_t val, const float32_t a_rise, const float32_t a_decay)
{
	static float32_t prev_val = 1.0f;

	static float32_t alpha;

	if(val > prev_val)
		alpha = a_rise; // alpha rise
	else
		alpha = a_decay;

	prev_val = prev_val + alpha * (val - prev_val);

	return prev_val;
}

// exponentially weighted moving average filter (first order IIR)
void exp_filter_n(float32_t* in, float32_t* out, int32_t n, const float32_t a_rise, const float32_t a_decay)
{
	static float32_t alpha;

	for(int32_t i = 0; i < n; i++)
	{
		if(in[i] > out[i])
			alpha = a_rise; // alpha rise
		else
			alpha =  a_decay;

		out[i] = out[i] + alpha*(in[i] - out[i]);
	}
}


void strip_visu(float32_t* samples)
{
	static int32_t i;

	cnt = (int32_t)(TIM2->CNT);

	// przesuwanie buforow H <- L <- nowe probki
	w2cpy((void*)y_roll, (void*)(&y_roll[FRAMES_PER_BUFFER]), samples, FRAMES_PER_BUFFER);

	// normalizacja i przemnozenie przez okno hamminga
	for(i=0; i<BUFF_LEN;i++)
		y_data[i] = (y_roll[i]/32767.0f) * hamming_w[i];

	arm_rfft_fast_f32(&S, y_data, Y_data, 0);
	arm_cmplx_mag_squared_f32(Y_data, YS, BUFF_LEN/2); // squared magnitude

	mel_lacc = 0.0f;
	mel_racc = 0.0f;
	n = 0;
	k = 0;
	l = 0;

	// bank filtrow mell dla parzystych i nieparzystych cz.
	for(int32_t i = 0; i<FRAMES_PER_BUFFER; i++)
	{
		mel_lacc += mel_l[i] * YS[i];
		mel_racc += mel_r[i] * YS[i];

		if(i == mel_il[k])
		{
			k++;
			mel_out[n++] = mel_lacc * mel_lacc * gain_val; //
			mel_lacc = 0.0f;
		}

		if(i == mel_ir[l])
		{
			l++;
			mel_out[n++] = mel_racc * mel_racc * gain_val; //
			mel_racc = 0.0f;
		}
	}

	for(int32_t i = 0; i < 32; i++)
		mel_tmp[i] = 0.0f;

	// filtr gaussa i wybor maksimum do skalowania

	conv_struct.in_x = mel_out;
	conv_struct.in_h = gauss;
	conv_struct.len_x = 12;
	conv_struct.len_h = 12;
	conv_struct.out_y = mel_tmp;

	f32_conv(&conv_struct);

	// scaling factor with time constant
	f32absmax(mel_tmp, &gain, 24);
	gain = exp_filter(gain, 0.99f, 0.01f);
	for(int32_t i = 0; i < 12; i++)
		mel_out[i] = mel_out[i]/gain;

	// frequency bands smoothed by time constant
	exp_filter_n(mel_out, mel_out_smoothed, 12, 0.6f, 0.4f);


	exp_filter_n(mel_out_smoothed, tmp_out, 12, 0.99f, 0.01f);


	r = 0.0f;
	g = 0.0f;
	b = 0.0f;

	// wyszukanie maksymalnej wartosci dla kazdej barwy
	for (i = 0; i < 12; i++)
	{
		visu_out[i] = mel_out_smoothed[i] / tmp_out[i];

		if (i < 4) {
			if (r < visu_out[i])
				r = visu_out[i];
		} else if (i < 8) {
			if (g < visu_out[i])
				g = visu_out[i];
		} else {
			if (b < visu_out[i])
				b = visu_out[i];
		}
	}

	// przesuwanie paska w prawo
	for (i = N_PIXELS_SIDE - 1; i >= 1 ; i--)
	{
		r_pixel[i] = r_pixel[i-1] * 0.98f;
		g_pixel[i] = g_pixel[i-1] * 0.98f;
		b_pixel[i] = b_pixel[i-1] * 0.98f;
	}


	r_pixel[0] = r;
	g_pixel[0] = g;
	b_pixel[0] = b;



	// przygotowanie danych wyjsciowych
	d = (uart_out_data*)uart_data;
	d->v1 = 0xFFEEAABB;



	// przepisanie danych z lustrzanym odbiciem
	static int8_t px;
	for(i=0; i < N_PIXELS_SIDE ; i++)
	{
		//prawa strona
		px = (int8_t)(b_pixel[i]*255.0f);
		d->data[(N_RGB_OUT_DATA/2) + (i*3)] = px;
		d->data[(N_RGB_OUT_DATA/2-1) - (i*3+2)] = px;

		px = (int8_t)(r_pixel[i]*255.0f);
		d->data[(N_RGB_OUT_DATA/2) + (i*3+1)] = px;
		d->data[(N_RGB_OUT_DATA/2-1) - (i*3+1)] = px;

		px = (int8_t)(g_pixel[i]*255.0f);
		d->data[(N_RGB_OUT_DATA/2) + (i*3+2)] = px;
		d->data[(N_RGB_OUT_DATA/2-1) - (i*3)] = px;
	}

	// czas wykonania w us
	act_val = ((int32_t)(TIM2->CNT) - cnt)/84;
	d->v2 = act_val;

	uart3_dma_send(uart_data, 216);

}
