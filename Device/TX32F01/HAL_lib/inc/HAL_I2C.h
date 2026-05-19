#ifndef _HAL_I2C_H
#define _HAL_I2C_H
#include "fpdefine.h"

//IIC主机状态位
#define I2C_STA_STARTBIT_SENDED    (0x08)      //已发送起始位
#define I2C_STA_RESTARTBIT_SENDED  (0x10)      //已发送重复起始位
#define I2C_STA_SLAW_ACKED         (0x18)      //已发送从机地址加W，并收到ACK
#define I2C_STA_DATW_ACKED         (0x28)      //已发送数据，并收到ACK
#define I2C_STA_DATW_NACKED        (0x30)      //已发送数据，并收到NACK
#define I2C_STA_SLAR_ACKED         (0x40)      //已发送从机地址加R，并收到ACK
#define I2C_STA_SLAR_NACKED        (0x48)      //已发送从机地址加R，并收到NACK
#define I2C_STA_DATARECVED_ACK     (0x50)      //已收到数据，并返回ACK
#define I2C_STA_DATARECVED_NACK    (0x58)      //已收到数据，并返回NACK


void I2C_DeInit(void);
void I2C_Master_Init(uint32_t Clk);
void I2C_Cmd(FunctionalState NewState);
void I2C_GenerateSTART(void);
void I2C_GenerateSTOP(void);
void I2C_GenerateRESTART(void);
void I2C_ClearSTART(void);
booltyped I2C_Sta_Check(u8 Sta);
void I2C_SEND_ACK(void);
void I2C_SEND_NACK(void);
void I2C_SendData(uint8_t Data);
uint8_t I2C_ReceiveData(void);


#endif










