#ifndef _HAL_GPIO_H
#define _HAL_GPIO_H

#include "fpdefine.h"
#include "TX32F01.h"



//函数GPIO_Init的参数
#define GPIO_MODE_INPUT_FLOAT  		 1    /*!< 浮空输入 >*/
#define GPIO_MODE_INPUT_PU     		 2    /*!< 上拉输入 >*/
#define GPIO_MODE_INPUT_PD      	 3    /*!< 下拉输入 >*/
#define GPIO_MODE_OUTPUT_PP    		 4    /*!< 推挽输出 >*/
#define GPIO_MODE_OUTPUT_OD    		 5    /*!< 开漏输出 >*/
#define GPIO_MODE_AF     					 6   	/*!< 复用功能 >*/
#define GPIO_MODE_ANALOG     			 7   	/*!< 模拟功能 >*/


#define GPIO_Speed_High     			 0   	/*!< GPIO高驱动 >*/
#define GPIO_Speed_Low     			   1   	/*!< GPIO低驱动 >*/

#define GPIO_AF_System     			   0x0   	/*!< 端口复用功能 >*/
#define GPIO_AF_UART_RX     			 0x1   	/*!< 端口复用功能 >*/
#define GPIO_AF_UART_TX     			 0x2   	/*!< 端口复用功能 >*/
#define GPIO_AF_TIMER_BRK_IN    	 0x3   	/*!< 端口复用功能 >*/
#define GPIO_AF_T0CH            	 0x4   	/*!< 端口复用功能 >*/
#define GPIO_AF_T0CHN            	 0x5   	/*!< 端口复用功能 >*/
#define GPIO_AF_T1CH            	 0x6   	/*!< 端口复用功能 >*/
#define GPIO_AF_T1CHN            	 0x7   	/*!< 端口复用功能 >*/
#define GPIO_AF_T2CHN            	 0x8   	/*!< 端口复用功能 >*/
#define GPIO_AF_T2CH            	 0x9   	/*!< 端口复用功能 >*/
#define GPIO_AF_SPI_CLK            0xa   	/*!< 端口复用功能 >*/
#define GPIO_AF_SPI_MISO           0xb   	/*!< 端口复用功能 >*/
#define GPIO_AF_SPI_MOSI		       0xc   	/*!< 端口复用功能 >*/
#define GPIO_AF_SPI_CS		         0xd   	/*!< 端口复用功能 >*/
#define GPIO_AF_SCL		             0xe   	/*!< 端口复用功能 >*/
#define GPIO_AF_SDA		             0xf   	/*!< 端口复用功能 >*/



#define PIN00  0x00
#define PIN01  0x01
#define PIN02  0x02
#define PIN03  0x03
#define PIN04  0x04
#define PIN05  0x05
#define PIN06  0x06
#define PIN07  0x07

void GPIO_DeInit(GPIO_Type* GPIOx);
void GPIO_Init(GPIO_Type* GPIOx,u8 pin,u32 mode);
void GPIO_SetSpeed(GPIO_Type* GPIOx, u8 GPIO_Pin,u8 Speed);
void GPIO_PinRemapConfig(GPIO_Type* GPIOx, u8 GPIO_Pin, u8 GPIO_AFReg);

void GPIO_SetBits(GPIO_Type* GPIOx, u8 GPIO_Pin);
void GPIO_ResetBits(GPIO_Type* GPIOx, u8 GPIO_Pin);
void GPIO_Toggle(GPIO_Type* GPIOx, u8 GPIO_Pin);
void GPIO_Write(GPIO_Type* GPIOx, u8 PortVal);
uint8_t GPIO_ReadOutputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin);
uint8_t GPIO_ReadInputDataBit(GPIO_Type* GPIOx, u8 GPIO_Pin);
uint8_t GPIO_ReadInputData(GPIO_Type* GPIOx);
void GPIO_PullUpConfig(GPIO_Type* GPIOx, u8 GPIO_Pin);
void GPIO_PullDownConfig(GPIO_Type* GPIOx, u8 GPIO_Pin);


#endif




