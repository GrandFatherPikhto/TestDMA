/*
 * pulse.c
 *
 *  Created on: Nov 23, 2024
 *      Author: denis
 */
#include "tim.h"
#include "pulse.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_tim.h"

#define DATA_SIZE 0x2000

static uint32_t s_data[DATA_SIZE] = {0};

static void s_data_init(void);
void s_start_dma_transfer (void);

void Data_Init(void)
{
	s_data_init();
}

void Start_DMA_Transfer(void)
{
	s_start_dma_transfer();
}


static void s_data_init(void)
{
	uint8_t flag = 0;
	for (uint32_t i = 0; i < DATA_SIZE; i++)
	{
		s_data[i] = (1ULL << (11 + 0x10)) | (1ULL << (12 + 0x10));
		if (i % 50 == 0) flag ^= 1;

		if (i >= 0 && i < DATA_SIZE)
		{
			s_data[i] = flag ? (1ULL << (11 + 0x10)) | (1ULL << (12)) : (1ULL << (11)) | (1ULL << (12 + 0x10));
		}
	}
	s_data[DATA_SIZE - 1] = (1ULL << (11 + 0x10)) | (1ULL << (12 + 0x10));
}

void s_start_dma_transfer (void)
{
	if (HAL_DMA_GetState(&hdma_tim1_ch4_trig_com) != HAL_DMA_STATE_READY)
	{
		if (HAL_DMA_Abort(&hdma_tim1_ch4_trig_com) != HAL_OK)
		{
			Error_Handler ();
		}
	}

	if (HAL_DMA_Start(&hdma_tim1_ch4_trig_com, (uint32_t)s_data, (uint32_t) &GPIOE->BSRR, DATA_SIZE) != HAL_OK)
	{
		Error_Handler();
	}

	__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_CC4);

	if (!(htim1.Instance->CR1 & TIM_CR1_CEN))
	{
		__HAL_TIM_ENABLE(&htim1);
	}
}
