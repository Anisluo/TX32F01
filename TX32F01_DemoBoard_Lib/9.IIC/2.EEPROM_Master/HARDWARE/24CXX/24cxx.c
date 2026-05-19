#include "24cxx.h"
#include "systick.h"
#include "HAL_i2c.h"

//定义IIC从机地址,用户自己定义
#define I2C_SLA_W 0xA0
#define I2C_SLA_R 0xA1


//初始化IIC接口
void AT24CXX_Init(u32 Clk)
{
    SCU_Unlock();// 解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO1,ENABLE);//打开GPIO1外设时钟
    SCU_PeriphClockCmd(Periph_I2C,ENABLE);//打开IIC外设时钟
    SCU_Lock();// 上锁SCU，不可对其余SCU寄存器操作。

    // GPIO初始化
    GPIO_Init(GPIO1,PIN00,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO1,PIN01,GPIO_MODE_AF);//设置为复用模式
    GPIO_PinRemapConfig(GPIO1,PIN00,GPIO_AF_SDA);//复用为SDA
    GPIO_PinRemapConfig(GPIO1,PIN01,GPIO_AF_SCL);//复用为SCL

    GPIO_PullUpConfig(GPIO1,PIN00);//内部上拉
    GPIO_PullUpConfig(GPIO1,PIN01);//内部上拉

    I2C_DeInit();//I2C寄存器恢复默认值
    I2C_Master_Init(Clk);//配置IIC主机时钟
    I2C_Cmd(ENABLE);//IIC使能
}

//在AT24CXX指定地址读出一个数据
//ReadAddr:开始读数的地址
//返回值  :读到的数据
u8 AT24CXX_ReadOneByte(u16 ReadAddr)
{
    u8 temp=0;

    I2C_GenerateSTART();//产生START
    if(truefp != I2C_Sta_Check(I2C_STA_STARTBIT_SENDED)) 
    {   
        //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }
    I2C_ClearSTART();//清除 START

    I2C_SendData(I2C_SLA_W);//发送从机写地址
    if(truefp != I2C_Sta_Check(I2C_STA_SLAW_ACKED))
    {
         //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_SendData(ReadAddr%256);//发送读出数据的地址
    if(truefp != I2C_Sta_Check(I2C_STA_DATW_ACKED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_GenerateRESTART();//重新产生START
    if(truefp != I2C_Sta_Check(I2C_STA_RESTARTBIT_SENDED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }
    I2C_ClearSTART();//清除 START

    I2C_SendData(I2C_SLA_R);//发送从机读地址
    if(truefp != I2C_Sta_Check(I2C_STA_SLAR_ACKED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_SEND_NACK();//发送NACK
    if(truefp != I2C_Sta_Check(I2C_STA_DATARECVED_NACK)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();//检查状态失败，发送STOP后返回
        return falsefp;
    }
    temp = I2C_ReceiveData();//接收数据

    I2C_GenerateSTOP();//产生STOP
    return temp;
}

//在AT24CXX指定地址写入一个数据
//WriteAddr  :写入数据的目的地址
//DataToWrite:要写入的数据
booltyped AT24CXX_WriteOneByte(u16 WriteAddr,u8 DataToWrite)
{
    I2C_GenerateSTART();//产生START
    if(truefp != I2C_Sta_Check(I2C_STA_STARTBIT_SENDED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();    //检查状态失败，发送STOP后返回
        return falsefp;
    }
    I2C_ClearSTART();//CLEAR START

    I2C_SendData(I2C_SLA_W);//发送从机写地址
    if(truefp != I2C_Sta_Check(I2C_STA_SLAW_ACKED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();    //检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_SendData(WriteAddr);//写入数据的目的地址
    if(truefp != I2C_Sta_Check(I2C_STA_DATW_ACKED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();    //检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_SendData(DataToWrite);//发送要写入数据
    if(truefp != I2C_Sta_Check(I2C_STA_DATW_ACKED)) 
    {
        //等待SI跳转
        I2C_GenerateSTOP();    //检查状态失败，发送STOP后返回
        return falsefp;
    }

    I2C_GenerateSTOP();//产生STOP

    delay_ms(10);//	必要的延时，防止EEPROM还未写完就开始读
    return truefp;
}

//在AT24CXX里面的指定地址开始写入长度为Len的数据
//该函数用于写入16bit或者32bit的数据.
//WriteAddr  :开始写入的地址
//DataToWrite:数据数组首地址
//Len        :要写入数据的长度2,4
void AT24CXX_WriteLenByte(u16 WriteAddr,u32 DataToWrite,u8 Len)
{
    u8 t;
    for(t=0; t<Len; t++)
    {
        AT24CXX_WriteOneByte(WriteAddr+t,(DataToWrite>>(8*t))&0xff);
    }
}

//在AT24CXX里面的指定地址开始读出长度为Len的数据
//该函数用于读出16bit或者32bit的数据.
//ReadAddr   :开始读出的地址
//返回值     :数据
//Len        :要读出数据的长度2,4
u32 AT24CXX_ReadLenByte(u16 ReadAddr,u8 Len)
{
    u8 t;
    u32 temp=0;
    for(t=0; t<Len; t++)
    {
        temp<<=8;
        temp+=AT24CXX_ReadOneByte(ReadAddr+Len-t-1);
    }
    return temp;
}

//检查AT24CXX是否正常
//这里用了24XX的最后一个地址(255)来存储标志字.
//如果用其他24C系列,这个地址要修改
//返回1:检测失败
//返回0:检测成功
u8 AT24CXX_Check(void)
{
    u8 temp;
    temp=AT24CXX_ReadOneByte(255);//避免每次开机都写AT24CXX
    if(temp==0X55)return 0;
    else//排除第一次初始化的情况
    {
        AT24CXX_WriteOneByte(255,0X55);
        temp=AT24CXX_ReadOneByte(255);
        if(temp==0X55)return 0;
    }
    return 1;
}

//在AT24CXX里面的指定地址开始读出指定个数的数据
//ReadAddr :开始读出的地址 对24c02为0~255
//pBuffer  :数据数组首地址
//NumToRead:要读出数据的个数
void AT24CXX_Read(u16 ReadAddr,u8 *pBuffer,u16 NumToRead)
{
    while(NumToRead)
    {
        *pBuffer++=AT24CXX_ReadOneByte(ReadAddr++);
        NumToRead--;
    }
}

//在AT24CXX里面的指定地址开始写入指定个数的数据
//WriteAddr :开始写入的地址 对24c02为0~255
//pBuffer   :数据数组首地址
//NumToWrite:要写入数据的个数
void AT24CXX_Write(u16 WriteAddr,u8 *pBuffer,u16 NumToWrite)
{
    while(NumToWrite--)
    {
        AT24CXX_WriteOneByte(WriteAddr,*pBuffer);
        WriteAddr++;
        pBuffer++;
    }
}












