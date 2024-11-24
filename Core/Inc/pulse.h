/*
 * pulse.h
 *
 *  Created on: Nov 23, 2024
 *      Author: denis
 */

#ifndef INC_PULSE_H_
#define INC_PULSE_H_

void Data_Init(void);
void Start_DMA_Transfer (void);

extern DMA_HandleTypeDef hdma_tim1_ch4_trig_com;

#endif /* INC_PULSE_H_ */
