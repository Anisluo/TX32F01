#ifndef _I2C_H
#define _I2C_H

#include "TX32F01_periph.h"
#include "fpdefine.h"

#define CLEAR_WATCH_DOG()    IWDT->KR = 0xAAAA; IWDT->KR = 0xAAAA


////////////////////////////////////////////////////////////////////

/* I2C->CR		*/
#define I2C_EN					(1UL<<6)
#define	I2C_START				(1UL<<5)
#define	I2C_STOP				(1UL<<4)
#define	I2C_CMEN				(1UL<<3)
#define	I2C_ACK					(1UL<<2)
#define	I2C_CRF					(1UL<<1)
#define	I2C_CTF					(1UL<<0)


//USER DEFINE
#define I2C_SLA_W 0xA0//0xEA
#define I2C_SLA_R 0xA1//0xEB

#define bl_I2C_CR_START  ((u32)(1<<5))
#define bl_I2C_CR_STOP   ((u32)(1<<4))

//STA
#define bl_I2C_STA_STARTBIT_SENDED    (0x08)      //已发送起始位
#define bl_I2C_STA_RESTARTBIT_SENDED  (0x10)      //已发送重复起始位
#define bl_I2C_STA_SLAW_ACKED         (0x18)      //已发送从机地址加W，并收到ACK
#define bl_I2C_STA_DATW_ACKED         (0x28)      //已发送数据，并收到ACK
#define bl_I2C_STA_DATW_NACKED        (0x30)      //已发送数据，并收到NACK
#define bl_I2C_STA_SLAR_ACKED         (0x40)      //已发送从机地址加R，并收到ACK
#define bl_I2C_STA_SLAR_NACKED        (0x48)      //已发送从机地址加R，并收到NACK
#define bl_I2C_STA_DATARECVED_ACK     (0x50)      //已收到数据，并返回ACK
#define bl_I2C_STA_DATARECVED_NACK    (0x58)      //已收到数据，并返回NACK

#define bl_I2C_DISABLE() (I2C->CR &= (~(1<<6)))
#define bl_I2C_ENABLE()  (I2C->CR |= (1<<6))
//位操作语句
#define bl_I2C_SEND_START()       (I2C->CR |= bl_I2C_CR_START)
#define bl_I2C_SEND_STOP()        {I2C->CR |= bl_I2C_CR_STOP;\
                                    I2C->CR &= ~((u32)1<<3);}
#define bl_I2C_SEND_RESTART()     {bl_I2C_SEND_START(); \
                                	I2C->CR &= ~((u32)1<<3);}
#define bl_I2C_SEND_ACK()         {I2C->CR |= (1<<2);\
                                    I2C->CR &= ~((u32)1<<3);}
#define bl_I2C_SEND_NACK()        {I2C->CR &= ~((u32)1<<2);\
                                    I2C->CR &= ~((u32)1<<3);}
#define bl_I2C_CLEAR_START()      (I2C->CR &= ~bl_I2C_CR_START)
#define bl_I2C_CLEAR_SI()         (I2C->CR &= ~((u32)1<<3))
#define bl_I2C_DR_SEND_ENABLE()   (I2C->CR &= ~((u32)1<<3))

#define bl_I2C_DR_SEND(x)         {I2C->DR = x; bl_I2C_DR_SEND_ENABLE();}
#define bl_I2C_SLAVE_SEND_DR(x)   {I2C->DR = x;I2C->CR |= (1<<2);bl_I2C_DR_SEND_ENABLE();}

extern vu32 gotoAppCnt;
extern booltyped gotoBootloaderFlag;

extern u8 i2cTimeout;

extern void i2cSlaveCfg(void);
void I2C_Bootloader(void);

#endif
