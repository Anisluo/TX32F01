//#include "include.h"

#include "CH372.h"
#include "CH375INC.h"
#include "TX32F01_periph.h"
#include "LED.h"

//sbit CH375_INT=P3^2;  //CH375中断信号输出端，低电平输出
//sbit CH375_CS=P3^3;   //CH375片选控制端，低电平有效，芯片内置上拉电阻
//sbit CH375_RD=P3^4;   //CH375读控制端，低电平有效，芯片内置上拉电阻
//sbit CH375_WR=P3^5;   //CH375写控制端，低电平有效，芯片内置上拉电阻
//sbit CH375_A0=P3^7;   //CH375命令/数据选择控制端，低电平时写数据，高电平时写命令

/***********************************************************************************
****函数名称：不准确的延时2US的函数
****函数作用：
****函数描述：
************************************************************************************/
void delay2us(void)
{
	unsigned char i;
	for(i=0;i<4;i++)__NOP();
}
/***********************************************************************************
****函数名称：不准确的延时50mS的函数
****函数作用：
****函数描述：
************************************************************************************/
void delay50ms(void)
{
	unsigned char m,n;
	for(m=0;m<200;m++)
		for(n=0;n<250;n++);
}

/**
 * 设置数据总线为输出模式
 */

 void DataBus_SetOutput(void)
{
    //设置GPIO1高4bit为推挽输出
    DATAPORTH->MDR = (DATAPORTH->MDR & 0x00FF) | 0x5500;
    
    //设置GPIO2低4bit为推挽输出
    DATAPORTL->MDR = (DATAPORTL->MDR & 0xFF00) | 0x0055;
    
    //禁用输入
    DATAPORTH->IER &= 0x0F;
    DATAPORTL->IER &= 0xF0;
}


/**
 * 设置数据总线为输入模式
 */
void DataBus_SetInput(void)
{
    // 设置为输入模式
    DATAPORTH->MDR &= 0x00FF;   // GPIO1 高4bit输入模式
    DATAPORTL->MDR &= 0xFF00;   // GPIO2 低4bit输入模式
    
    //使能上拉和输入
    DATAPORTH->PUR |= 0xF0;     // H_4 上拉使能
    DATAPORTL->PUR |= 0x0F;     // L_4 上拉使能
    DATAPORTH->IER |= 0xF0;     // H_4 输入使能
    DATAPORTL->IER |= 0x0F;     // L_4 输入使能
}

/**
 * 设置数据总线为空闲状态
 */

void DataBus_SetIdle(void){
	DataBus_SetOutput();
	DATAPORTH->ODR |= 0xF0;     // H_4 set High
	DATAPORTL->ODR |= 0x0F;     // L_4 set High

}

void CH375_GPIOInit(void)
{
    SCU_Unlock();
    SCU_PeriphClockCmd(Periph_GPIO1, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO2, ENABLE);
    SCU_PeriphClockCmd(Periph_GPIO3, ENABLE);
    SCU_Lock();
    
    GPIO_DeInit(GPIO1);
    GPIO_DeInit(GPIO2);
    GPIO_DeInit(GPIO3);
    
    // 数据总线配置
    // GPIO1[7:4] - 数据总线H_4 D[7:4]
    GPIO_Init(GPIO1, PIN04, GPIO_MODE_OUTPUT_PP);  // D4
    GPIO_Init(GPIO1, PIN05, GPIO_MODE_OUTPUT_PP);  // D5
    GPIO_Init(GPIO1, PIN06, GPIO_MODE_OUTPUT_PP);  // D6
    GPIO_Init(GPIO1, PIN07, GPIO_MODE_OUTPUT_PP);  // D7
    
    // GPIO2[3:0] - 数据总线L_4 D[3:0]
    GPIO_Init(GPIO2, PIN00, GPIO_MODE_OUTPUT_PP);  // D0
    GPIO_Init(GPIO2, PIN01, GPIO_MODE_OUTPUT_PP);  // D1
    GPIO_Init(GPIO2, PIN02, GPIO_MODE_OUTPUT_PP);  // D2
    GPIO_Init(GPIO2, PIN03, GPIO_MODE_OUTPUT_PP);  // D3
    
    // CH375控制信号配置
    GPIO_Init(GPIO2, PIN04, GPIO_MODE_OUTPUT_PP);  // CS 
    GPIO_Init(GPIO2, PIN05, GPIO_MODE_INPUT_PU);   // INT
    GPIO_Init(GPIO3, PIN00, GPIO_MODE_OUTPUT_PP);  // WR
    GPIO_Init(GPIO3, PIN01, GPIO_MODE_OUTPUT_PP);  // RD
    GPIO_Init(GPIO3, PIN03, GPIO_MODE_OUTPUT_PP);  // A0
    
    // 配置上拉/下拉（只针对数据总线）
    GPIO1->PUR &= 0x0F;         // 
    GPIO2->PUR &= 0xF0;         // 
    GPIO1->PDR &= 0x0F;         // 
    GPIO2->PDR &= 0xF0;         // 
    
    // 初始化控制信号状态
    CH375_A0(0);    // 数据模式
    CH375_RD(1);    // 读无效
    CH375_CS(1);    // 片选无效
    CH375_WR(1);    // 写无效
    
    // 数据总线初始化为空闲状态
    DataBus_SetIdle();
}


/**
 * CH375写命令函数
 * @param cmd: 要写入的命令
 */
void CH375_WRITE_CMD(uint8_t cmd)
{
    // 1. 准备数据总线
    DataBus_SetOutput();
    
    // 2. 设置信号初始状态
    CH375_RD(1);        // 读无效
    CH375_WR(1);        // 写无效
    CH375_CS(1);        // 片选无效
    CH375_A0(1);        // 
    
    DELAY_SETUP();
    
    // 3. 输出命令数据
    DATA_WRITE(cmd);
    DELAY_SETUP();
    
    // 4. 命令写时序
   // CH375_A0(1);        // 命令模式
    CH375_CS(0);        // 选中芯片
    DELAY_SETUP();
    
    CH375_WR(0);        // 写有效
    DELAY_ACCESS();     // 写脉冲宽度
    CH375_WR(1);        // 写无效
    
    DELAY_HOLD();
    
    // 5. 恢复控制信号
    CH375_CS(1);        // 取消选中
    CH375_A0(0);        // 数据模式
    
    // 6. 数据总线空闲
    DataBus_SetIdle();
    DELAY_HOLD();
}



/**
 * CH375写数据
 * @param data: 要写入的数据
 */
void CH375_WRITE_DATA(uint8_t data)
{
    // 1. 准备数据总线
    DataBus_SetOutput();
    
    // 2. 设置控制信号初始状态
    CH375_RD(1);        // 读无效
    CH375_WR(1);        // 写无效
    CH375_CS(1);        // 片选无效
    CH375_A0(0);        // 数据模式
    
	
		DATA_WRITE(data);   // 输出数据

    DELAY_SETUP();
    
    // 3. 数据写时序
    CH375_CS(0);        // 选中芯片
    DELAY_SETUP();
    
    CH375_WR(0);        // 写有效
    DELAY_ACCESS();     // 数据setup time
    
    CH375_WR(1);        // 写无效
    DELAY_HOLD();
    
    // 4. 恢复控制信号
    CH375_CS(1);        // 取消选中
    
    // 5. 数据总线空闲发
    DataBus_SetIdle();
    DELAY_HOLD();
}


/**
 * CH375读数据函数
 * @return: 读取的数据
 */
uint8_t CH375_READ_DATA(void)
{
    uint8_t data;
    
    // 1. 设置数据总线为输入
    DataBus_SetInput();
    
    // 2. 设置控制信号初始状态
    CH375_WR(1);        // 写无效
    CH375_RD(1);        // 读无效
    CH375_CS(1);        // 片选无效
    CH375_A0(0);        // 数据模式
    
    DELAY_SETUP();
    
    // 3. 数据读时序
    CH375_CS(0);        // 选中芯片
    DELAY_SETUP();
    
    CH375_RD(0);        // 读有效
    DELAY_ACCESS();     // 等待数据稳定
    
    data = DATA_READ(); // 读取数据
    
    CH375_RD(1);        // 读无效
    DELAY_HOLD();
    
    // 4. 恢复控制信号
    CH375_CS(1);        // 取消选中
    
    // 5. 恢复数据总线为输出模式
    DataBus_SetIdle();
    DELAY_HOLD();
    
    return data;
}



