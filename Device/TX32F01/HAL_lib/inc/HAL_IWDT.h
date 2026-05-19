#ifndef _HAL_IWDT_H
#define _HAL_IWDT_H

#include "fpdefine.h"
#include "TX32F01.h"


/* IWDT_PR	*/
#define	IWDT_PR_4 			0
#define	IWDT_PR_8 			1
#define IWDT_PR_16 			2
#define	IWDT_PR_32 			3
#define IWDT_PR_64 			4
#define	IWDT_PR_128 		5
#define	IWDT_PR_256 		6




void  IWDT_Init(uint8_t Prescaler,uint16_t ReloadValue);
void IWDT_CmdEnable(void);
void IWDT_ITConfig(FunctionalState NewState);
BOOL IWDT_GetFlagStatus(void);
void IWDT_ClearFlag(void);
void IWDT_ReloadCounter(void);


#endif




