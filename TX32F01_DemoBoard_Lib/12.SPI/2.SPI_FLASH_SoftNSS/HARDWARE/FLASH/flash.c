#include "TX32F01_periph.h"
#include "systick.h"
#include "FLASH.h"

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

    GPIO_Init(GPIO2,PIN04,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO2,PIN05,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO3,PIN00,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO3,PIN01,GPIO_MODE_AF);//设置为复用模式

    GPIO_PinRemapConfig(GPIO2,PIN04,GPIO_AF_SPI_CS);//复用为SPI_CS
    GPIO_PinRemapConfig(GPIO2,PIN05,GPIO_AF_SPI_CLK);//复用为SPI_CLK
    GPIO_PinRemapConfig(GPIO3,PIN00,GPIO_AF_SPI_MOSI);//复用为SPI_MOSI
    GPIO_PinRemapConfig(GPIO3,PIN01,GPIO_AF_SPI_MISO);//复用为SPI_MISO
	
    SPI_DeInit();//复位SPI
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;//主机模式	
    SPI_InitStructure.SPI_DataWidth = SPI_DataWidth_8b;//8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;//CPOL选择High
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;//CPHA选择2Edge
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;//片选信号选择软件控制
    SPI_InitStructure.SPI_CLK_DIV = SPI_CLK_DIV_8;//SPI时钟选择系统时钟8分频
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;//高位数据优先发送
    SPI_Init(&SPI_InitStructure);//初始化SPI
		
	SPI_CSInternalSelected(SPI_NSSInternalSoft_Low);//SPI NSS 软件控制低电平有效
		
	SPI_Cmd(ENABLE);//开启SPI
	SPI_FLASH_CS(0);//使能器件
}

//SPIx 读写一个字节
//TxData:要写入的字节
//返回值:读取到的字节
u8 SPI_ReadWriteByte(u8 TxData)
{	
	SPI_ClearFlag(SPI_ALL_SR);//清除所有标志位
    SPI_SendData(TxData);//发送数据
    while(!SPI_GetFlagStatus(SPI_TXEM_SR));//等待发送FIF0为空
    while(SPI_GetFlagStatus(SPI_RXEM_SR));//等待接收FIFO不为空
    return SPI_ReceiveData();
}

//读取SPI_FLASH的状态寄存器
//BIT7  6   5   4   3   2   1   0
//SPR   RV  TB BP2 BP1 BP0 WEL BUSY
//SPR:默认0,状态寄存器保护位,配合WP使用
//TB,BP2,BP1,BP0:FLASH区域写保护设置
//WEL:写使能锁定
//BUSY:忙标记位(1,忙;0,空闲)
//默认:0x00
u8 SPI_Flash_ReadSR(void)
{
    u8 byte=0;
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_ReadStatusReg);//发送读取状态寄存器命令
    byte=SPI_ReadWriteByte(0Xff);//读取一个字节
    SPI_FLASH_CS(1);//取消片选
    return byte;
}

//写SPI_FLASH状态寄存器
//只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写!!!
void SPI_FLASH_Write_SR(u8 sr)
{
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_WriteStatusReg);//发送写取状态寄存器命令
    SPI_ReadWriteByte(sr);//写入一个字节
    SPI_FLASH_CS(1);//取消片选
}

//SPI_FLASH写使能
//将WEL置位
void SPI_FLASH_Write_Enable(void)
{
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_WriteEnable);//发送写使能
    SPI_FLASH_CS(1);//取消片选
}

//SPI_FLASH写禁止
//将WEL清零
void SPI_FLASH_Write_Disable(void)
{
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_WriteDisable);//发送写禁止指令
    SPI_FLASH_CS(1);//取消片选
}

//读取芯片ID W25X16的ID:0XEF14
u16 SPI_Flash_ReadID(void)
{
    u16 Temp = 0;
    SPI_FLASH_CS(0);
    SPI_ReadWriteByte(0x90);//发送读取ID命令
    SPI_ReadWriteByte(0x00);
    SPI_ReadWriteByte(0x00);
    SPI_ReadWriteByte(0x00);
    Temp|=SPI_ReadWriteByte(0xFF)<<8;
    Temp|=SPI_ReadWriteByte(0xFF);
    SPI_FLASH_CS(1);
    return Temp;
}

//读取SPI FLASH
//在指定地址开始读取指定长度的数据
//pBuffer:数据存储区
//ReadAddr:开始读取的地址(24bit)
//NumByteToRead:要读取的字节数(最大65535)
void SPI_Flash_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)
{
    u16 i;
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_ReadData);//发送读取命令
    SPI_ReadWriteByte((u8)((ReadAddr)>>16));//发送24bit地址
    SPI_ReadWriteByte((u8)((ReadAddr)>>8));
    SPI_ReadWriteByte((u8)ReadAddr);
    for(i=0; i<NumByteToRead; i++)
    {
        pBuffer[i]=SPI_ReadWriteByte(0XFF);//循环读数
    }
    SPI_FLASH_CS(1);//取消片选
}

//SPI在一页(0~65535)内写入少于256个字节的数据
//在指定地址开始写入最大256字节的数据
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!
void SPI_Flash_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
    u16 i;
    SPI_FLASH_Write_Enable();//SET WEL
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_PageProgram);//发送写页命令 0x2
    SPI_ReadWriteByte((u8)((WriteAddr)>>16));//发送24bit地址
    SPI_ReadWriteByte((u8)((WriteAddr)>>8));
    SPI_ReadWriteByte((u8)WriteAddr);
    for(i=0; i<NumByteToWrite; i++)SPI_ReadWriteByte(pBuffer[i]);//循环写数
    SPI_FLASH_CS(1);//取消片选
    SPI_Flash_Wait_Busy();//等待写入结束
}

//无检验写SPI FLASH
//必须确保所写的地址范围内的数据全部为0XFF,否则在非0XFF处写入的数据将失败!
//具有自动换页功能
//在指定地址开始写入指定长度的数据,但是要确保地址不越界!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
//CHECK OK
void SPI_Flash_Write_NoCheck(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
    u16 pageremain;
    pageremain=256-WriteAddr%256;//单页剩余的字节数
    if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite;//不大于256个字节
    while(1)
    {
        SPI_Flash_Write_Page(pBuffer,WriteAddr,pageremain);
        if(NumByteToWrite==pageremain)break;//写入结束了
        else //NumByteToWrite>pageremain
        {
            pBuffer+=pageremain;
            WriteAddr+=pageremain;

            NumByteToWrite-=pageremain;//减去已经写入了的字节数
            if(NumByteToWrite>256)pageremain=256;//一次可以写入256个字节
            else pageremain=NumByteToWrite;//不够256个字节了
        }
    };
}

//擦除整个芯片
//整片擦除时间:
//W25X16:25s
//W25X32:40s
//W25X64:40s
//等待时间超长...
void SPI_Flash_Erase_Chip(void)
{
    SPI_FLASH_Write_Enable();//SET WEL
    SPI_Flash_Wait_Busy();
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_ChipErase);//发送片擦除命令
    SPI_FLASH_CS(1);//取消片选
    SPI_Flash_Wait_Busy();//等待芯片擦除结束
    delay_ms(100);
}

//擦除一个扇区
//Dst_Addr:扇区地址 0~511 for w25x16
//擦除一个山区的最少时间:150ms
void SPI_Flash_Erase_Sector(u32 Dst_Addr)
{
    Dst_Addr*=4096;
    SPI_FLASH_Write_Enable();//SET WEL
    SPI_Flash_Wait_Busy();
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_SectorErase);//发送扇区擦除指令
    SPI_ReadWriteByte((u8)((Dst_Addr)>>16));//发送24bit地址
    SPI_ReadWriteByte((u8)((Dst_Addr)>>8));
    SPI_ReadWriteByte((u8)Dst_Addr);
    SPI_FLASH_CS(1);//取消片选
    SPI_Flash_Wait_Busy();//等待擦除完成
}

//等待空闲
void SPI_Flash_Wait_Busy(void)
{
    while ((SPI_Flash_ReadSR()&0x01)==0x01);   // 等待BUSY位清空
}

//进入掉电模式
void SPI_Flash_PowerDown(void)
{
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_PowerDown);//发送掉电命令
    SPI_FLASH_CS(1);//取消片选
    delay_us(3);//等待TPD
}

//唤醒
void SPI_Flash_WAKEUP(void)
{
    SPI_FLASH_CS(0);//使能器件
    SPI_ReadWriteByte(W25X_ReleasePowerDown);//  send W25X_PowerDown command 0xAB
    SPI_FLASH_CS(1);//取消片选
    delay_us(3);//等待TRES1
}


























