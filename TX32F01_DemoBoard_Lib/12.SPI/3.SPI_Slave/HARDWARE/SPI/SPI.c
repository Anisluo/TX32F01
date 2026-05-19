#include "TX32F01_periph.h"
#include "systick.h"
#include "SPI.h"

#define	FLASH_SIZE   8*1024*1024 	//SPI FLASH 大小为8M字节

//初始化SPI FLASH的IO口
void SPI_Flash_Init(void)
{
    SPI_InitTypeDef SPI_InitStructure;

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO2,ENABLE);//打开GPIO2外设时钟
    SCU_PeriphClockCmd(Periph_GPIO3,ENABLE);//打开GPIO3外设时钟
    SCU_PeriphClockCmd(Periph_SPI,ENABLE);//打开SPI外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。


    GPIO_Init(GPIO2,PIN03,GPIO_MODE_AF);//配置为复用模式
    GPIO_Init(GPIO3,PIN03,GPIO_MODE_AF);//配置为复用模式
    GPIO_Init(GPIO3,PIN04,GPIO_MODE_AF);//配置为复用模式
    GPIO_Init(GPIO3,PIN05,GPIO_MODE_AF);//配置为复用模式

    GPIO_PinRemapConfig(GPIO2,PIN03,GPIO_AF_SPI_CS);//复用为SPI_CS	，需要注意，SPI从机，必须把CS配置为硬件
    GPIO_PinRemapConfig(GPIO3,PIN03,GPIO_AF_SPI_CLK);//复用为SPI_CLK
    GPIO_PinRemapConfig(GPIO3,PIN04,GPIO_AF_SPI_MOSI);//复用为SPI_MOSI
    GPIO_PinRemapConfig(GPIO3,PIN05,GPIO_AF_SPI_MISO);//复用为SPI_MISO

		GPIO_PullUpConfig(GPIO2,PIN03);
		GPIO_PullUpConfig(GPIO3,PIN03);
		GPIO_PullUpConfig(GPIO3,PIN04);
		GPIO_PullUpConfig(GPIO3,PIN05);
	
    SPI_DeInit();//复位SPI
    SPI_InitStructure.SPI_Mode = SPI_Mode_Slave;//从机模式
    SPI_InitStructure.SPI_DataWidth = SPI_DataWidth_8b;//8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;//CPOL选择High
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;//CPHA选择2Edge
    SPI_InitStructure.SPI_NSS = SPI_NSS_Hard;//片选信号选择硬件控制
    SPI_InitStructure.SPI_CLK_DIV = SPI_CLK_DIV_2;//从机要选择2分频
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;//高位数据优先发送
    SPI_Init(&SPI_InitStructure);//初始化SPI

    SPI_Cmd(ENABLE);//开启SPI
}

//SPIx 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节
u8 SPI_ReadWriteByte(u8 TxData)
{
		u8 Receive;
    SPI_ClearFlag(SPI_ALL_SR);//清除所有标志位
    SPI_SendData(TxData);//发送数据
    while(!SPI_GetFlagStatus(SPI_TXEM_SR));//等待发送FIF0为空
    while(SPI_GetFlagStatus(SPI_RXEM_SR));//等待接收FIFO不为空
		Receive=SPI_ReceiveData();

    return Receive;
}



















