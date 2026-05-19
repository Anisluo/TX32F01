#ifndef _HAL_SPI_H
#define _HAL_SPI_H

#include "fpdefine.h"
#include "TX32F01.h"

typedef struct {
    uint16_t SPI_Mode;//主从模式选择
    uint16_t SPI_DataWidth;//数据长度选择
    uint16_t SPI_CPOL;//CPOL选择
    uint16_t SPI_CPHA;//CPHA选择
    uint16_t SPI_NSS;//片选信号软硬件控制选择
    uint16_t SPI_CLK_DIV;//分频系数选择
    uint16_t SPI_FirstBit;//优先发送数据选择
} SPI_InitTypeDef;


/* SPI_CR	*/
//SPI主从模式选择
#define SPI_Mode_Slave    (0)
#define SPI_Mode_Master   (1<<1)

//SPI数据长度选择
#define SPI_DataWidth_8b  (0)
#define SPI_DataWidth_16b (1<<6)

//SPI CPOL选择
#define SPI_CPOL_Low  		(0)
#define SPI_CPOL_High 		(1<<3)

//SPI CPHA选择
#define SPI_CPHA_1Edge    (0)
#define SPI_CPHA_2Edge    (1<<2)

//SPI 片选信号软硬件控制选择
#define SPI_NSS_Hard      (0)
#define SPI_NSS_Soft      (1<<7)

//SPI 片选信号硬件有效值选择
#define SPI_NSS_HardEffective_Low      (0)
#define SPI_NSS_HardEffective_High     (1<<8)

//SPI 优先发送数据选择
#define SPI_FirstBit_MSB  (0)
#define SPI_FirstBit_LSB  (1<<4)

//SPI 软件控制输出High/Low
#define SPI_NSSInternalSoft_Reset  (0)
#define SPI_NSSInternalSoft_Set    (1<<9)

#define SPI_NSSInternalSoft_Low    (0)
#define SPI_NSSInternalSoft_High   (1<<8)


/* SPI_DIVR*/
//SPI 分频系数选择
#define SPI_CLK_DIV_2   (0x01)
#define SPI_CLK_DIV_4   (0x02)
#define SPI_CLK_DIV_8   (0x04)
#define SPI_CLK_DIV_16  (0x08)
#define SPI_CLK_DIV_32  (0x10)
#define SPI_CLK_DIV_64  (0x20)
#define SPI_CLK_DIV_128 (0x40)
#define SPI_CLK_DIV_256 (0x80)
#define SPI_CLK_DIV_510 (0xFF)


/* SPI_IER	*/
#define	SPI_SPIF_IE				1
#define	SPI_SPDF_IE				(1<<1)
#define	SPI_OVFL_IE				(1<<2)
#define	SPI_UDRU_IE				(1<<3)
#define	SPI_TOUT_IE				(1<<4)
#define	SPI_TXF1_IE				(1<<7)
#define	SPI_TXF2_IE				(1<<8)
#define	SPI_TXF3_IE				(1<<9)
#define	SPI_RXF1_IE				(1<<10)
#define	SPI_RXF2_IE				(1<<11)
#define	SPI_RXF3_IE				(1<<12)
#define	SPI_TXEM_IE				(1<<13)
#define	SPI_RXFU_IE		  	(1<<14)


/* SPI_SR*/
#define	SPI_SPIF_SR				1
#define	SPI_SPDF_SR				(1<<1)
#define	SPI_OVFL_SR				(1<<2)
#define	SPI_UDRU_SR				(1<<3)
#define	SPI_TOUT_SR		    (1<<4)
#define	SPI_TXF1_SR				(1<<7)
#define	SPI_TXF2_SR				(1<<8)
#define	SPI_TXF3_SR				(1<<9)
#define	SPI_RXF1_SR				(1<<10)
#define	SPI_RXF2_SR				(1<<11)
#define	SPI_RXF3_SR				(1<<12)
#define	SPI_TXEM_SR				(1<<13)
#define	SPI_RXFU_SR		    (1<<14)
#define	SPI_RXEM_SR		    (1<<15)
#define	SPI_TXFU_SR		    (1<<16)
#define	SPI_SRMT_SR		    (1<<17)
#define	SPI_BUSY_SR		    (1<<18)
#define	SPI_ALL_SR		    (0x7FFFF)


void SPI_DeInit(void);
void SPI_Init(SPI_InitTypeDef *SPI_InitStruct);
void SPI_Cmd(FunctionalState NewState);
void SPI_ITConfig(uint16_t SPI_IT, FunctionalState NewState);
BOOL SPI_GetFlagStatus(uint32_t SPI_FLAG);
void SPI_ClearFlag(uint32_t SPI_FLAG);
void SPI_SendData(uint16_t Data);
uint16_t SPI_ReceiveData(void);
void SPI_CSInternalSelected(uint16_t SPI_CSInternalSelected);
void SPI_NSSInternalSoftwareConfig(uint16_t SPI_NSSInternalSoft);


#endif




