#ifndef _HAL_UART_H
#define _HAL_UART_H

#include "fpdefine.h"
#include "TX32F01.h"


typedef struct {
    uint32_t UART_BaudRate;//波特率
    uint16_t UART_WordLength;//数据字长
    uint16_t UART_StopBits;//停止位长
    uint16_t UART_Parity;//奇偶校验
    uint16_t UART_Mode;//收发模式选择
} UART_InitTypeDef;



/* UART_CR	*/
#define	UART_RXP_HIGH 			0
#define UART_RXP_LOW				(1<<11)
#define	UART_TXP_HIGH	  		0
#define UART_TXP_LOW				(1<<10)

#define UART_1STOPBIT			  0
#define UART_2STOPBIT			  (1<<7)

#define UART_7DATABIT			  0
#define UART_8DATABIT			  (1<<5)
#define UART_9DATABIT			  (2<<5)

#define	UART_Pority_ODD			 0x18
#define UART_Pority_EVEN		 0x10
#define UART_Pority_None		 0

/* UART_BRR	*/
#define	BAUDRATE_DIV(CLK,BD)		    (((CLK/BD/16)<<4)+CLK/BD%16)


/* UART_IER	*/
#define	UART_PEIE				1
#define UART_TCIE				(1<<4)
#define UART_TDEIE			(1<<5)
#define UART_RDNEIE			(1<<7)
#define UART_RDOVIE			(1<<9)
#define UART_FEIE				(1<<10)


/* UART_IFR	*/
#define	UART_PEIF				1
#define UART_TCIF				(1<<4)
#define UART_TDEIF			(1<<5)
#define UART_RDNEIF			(1<<7)
#define UART_RDOVIF			(1<<9)
#define UART_FEIF				(1<<10)
#define UART_ALLIF			(0xFFF)


/* UART_CER	*/
#define	UART_Mode_Tx				(1<<2)
#define	UART_Mode_Rx				(1<<1)

#define	UART_UE				     	1



void UART_DeInit(void);
void UART_Init(UART_InitTypeDef *UART_InitStruct);
void UART_ITConfig(uint16_t UART_IT, FunctionalState NewState);
void UART_Cmd(FunctionalState NewState);
BOOL UART_GetFlagStatus(uint16_t UART_FLAG);
void UART_ClearFlag(uint16_t UART_FLAG);
void UART_SendData(uint16_t Data);
uint16_t UART_ReceiveData(void);



#endif




