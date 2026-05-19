#ifndef __MYIIC_H
#define __MYIIC_H
#include "TX32F01_periph.h"

//IO方向设置
#define SDA_IN()      GPIO_Init(GPIO1,PIN00,GPIO_MODE_INPUT_PU);

#define SDA_OUT()     GPIO_Init(GPIO1,PIN00,GPIO_MODE_OUTPUT_PP);


//IO操作函数
#define IIC_SCL(x)    (x==0)?(GPIO_ResetBits(GPIO1,PIN01)):(GPIO_SetBits(GPIO1,PIN01)) //SCL
#define IIC_SDA(x)    (x==0)?(GPIO_ResetBits(GPIO1,PIN00)):(GPIO_SetBits(GPIO1,PIN00)) //SDA	 
#define READ_SDA      GPIO_ReadInputDataBit(GPIO1,PIN00)  //输入SDA 

//IIC所有操作函数
void IIC_Init(void);                //初始化IIC的IO口
void IIC_Start(void);				//发送IIC开始信号
void IIC_Stop(void);	  			//发送IIC停止信号
void IIC_Send_Byte(u8 txd);			//IIC发送一个字节
u8 IIC_Read_Byte(unsigned char ack);//IIC读取一个字节
u8 IIC_Wait_Ack(void); 				//IIC等待ACK信号
void IIC_Ack(void);					//IIC发送ACK信号
void IIC_NAck(void);				//IIC不发送ACK信号

void IIC_Write_One_Byte(u8 daddr,u8 addr,u8 data);
u8 IIC_Read_One_Byte(u8 daddr,u8 addr);


#endif
















