#include "i2c.h"
#include "flash.h"
#include "TX32F01_periph.h"

/*
********************************************************************
*地址0~0x7F模拟EE,地址A0和A2的大小都是128个字节
*地址80~0xCE，只读，返回地址低0x80|3bit|Y，其中如果器件地址是A0,Y=0x50,A2时，Y=0x48
*地址CF~0xFE，只读，返回地址低0x80|3bit|Y，其中如果器件地址是A0,Y=0x30,A2时，Y=0x28
*    其中低3bit为增加中断处理过程，没有直接取值，而是采用switch case方式
********************************************************************
*/

#define i2cSlaveAddr 0x60

#define iap_i2c_version 0x06

booltyped gotoBootloaderFlag = falsefp;

static u8 memBuf[300];
static u16 memBufIdx = 0;

#define sendbufSize 300
static u8 sendbuf[sendbufSize];
static u16 sendbufIdx = 0;
static u16 sendbufLen = 0;


void i2cSlaveCfg(void)
{
    /*
    P15    ------->      I2C SCL
    P16    ------->      I2C SDA
    */
    GPIO_Init(GPIO1,PIN05,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO1,PIN06,GPIO_MODE_AF);//设置为复用模式
    GPIO_PinRemapConfig(GPIO1,PIN05,GPIO_AF_SCL);//复用为SCL
    GPIO_PinRemapConfig(GPIO1,PIN06,GPIO_AF_SDA);//复用为SDA
	
    bl_I2C_DISABLE();//开启IIC前先关闭

    I2C->CR |= (1<<2);//产生ACK信号

    I2C->AR |= i2cSlaveAddr;//或上地址，有地址屏蔽位

    bl_I2C_ENABLE();//开启IIC
}

u8 checksum8bit(u8 *p,u16 len)
{
    u16 i;
    u8 sum = 0;
    for(i=0; i<len; i++)
    {
        sum += p[i];
    }
    return sum;
}

u16  ALLCode_Checksum=0x1111;
u16  Hex_Checksum=0x2222;//赋予初始值，ALLCode_Checksum和Hex_Checksum初始值不能相同

void i2cCmdDecode(void)
{
    u8 stu = 0x55;
    u8  OwnAPPCode=0xAA;//默认不存在APP程序
    u32 addr;

    if( (memBuf[0] != 0x7c)|| (memBuf[memBufIdx-1] != 0x7e))//包头包尾校验
    {
        return;
    }

    if( memBuf[1] != (memBuf[2]^0xFF) )//命令互为反码，校验
    {
        return;
    }
    if(memBuf[memBufIdx-2]!=checksum8bit(&memBuf[1],memBufIdx-3))//整个包做checksum校验（去掉包头和包尾）
    {
        return;
    }

    sendbufIdx = 0;
    sendbufLen = 0;

    sendbuf[sendbufLen++] = 0x7C;//包头
    sendbuf[sendbufLen++] = memBuf[1];//命令
    sendbuf[sendbufLen++] = ~memBuf[1];//命令反码

    switch (memBuf[1])
    {
    case 0x01://获取Bootloader版本号
        gotoBootloaderFlag = truefp;//升级程序，不可进入APP
        if (((*(__IO u32*)appAddr) & 0x2FFE0000 ) == 0x20000000)
        {
            OwnAPPCode=0x55;//APP程序区有程序
        }

        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = OwnAPPCode;//反馈是否存在APP程序
        sendbuf[sendbufLen++] = (*(u16*)Address_Checksum)>>8;
        sendbuf[sendbufLen++] = (*(u16*)Address_Checksum)&0xff;
        sendbuf[sendbufLen++] = iap_i2c_version;//版本号
        break;

    case 0x02://关闭FLASH写保护
        if(truefp != blFlashUnlock())
        {
            stu = 0xAA;
        }
        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;//传输信息
        break;

    case 0x03://打开FLASH写保护
        if(truefp != blFlashLock())
        {
            stu = 0xAA;
        }
        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;//传输信息
        break;

    case 0x04://APP程序区全擦除
        if(truefp != appCode_ALLErase())
        {
            stu = 0xAA;
        }
        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
					sendbuf[sendbufLen++] = 0;//传输信息
        break;

    case 0x05://烧写FLASH
        addr = memBuf[3];
        addr <<= 8;
        addr |= memBuf[4];
        addr <<= 8;
        addr |= memBuf[5];
        addr <<= 8;
        addr |= memBuf[6];//烧写程序地址（升级设备发来地址格式是从0地址开始）

        if (memBuf[7] != checksum8bit(&memBuf[3],4))//数据地址checksum
        {
            stu = 0xAA;
        }
        else if(truefp != appCodeWrite_AutoErase(addr,memBuf[8]+1,&memBuf[9]))
        {
            stu = 0xAA;
        }
        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;//传输信息
        break;

    case 0x06://校验整个下载程序的checksum，一定要在程序下载完成后执行，以扇区为单位

        Hex_Checksum= (memBuf[5]<< 8);
        Hex_Checksum|= memBuf[6];
        ALLCode_Checksum=CRC16_Check();//APP程序校验

        Flash_Erase_APPUpdate_Flag(Address_EnterAPPFlag);//先擦除标志位

        if(ALLCode_Checksum!=Hex_Checksum)
        {
            Flash_WriteAPPUpdate_Flag(Address_EnterAPPFlag,APPUpdate_Fail_Flag);//写入0x1234代表升级失败
            stu = 0xAA;
        }
        else
        {
            Flash_WriteAPPUpdate_Flag(Address_EnterAPPFlag,APPUpdate_Pass_Flag);//写入0x5555代表升级成功
            Flash_WriteAPPUpdate_Flag(Address_Checksum,ALLCode_Checksum);//写入checksum
        }

        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = (ALLCode_Checksum>>8)&0xff;//checksum 高8位
        sendbuf[sendbufLen++] = (ALLCode_Checksum&0xff);//checksum 低8位
        break;

    case 0x07://升级成功，并且跳转到APP（上位机需勾选该选项）
        if(ALLCode_Checksum==Hex_Checksum)//判断checksum是否正确
        {
            gotoBootloaderFlag = falsefp;//只有当checksum正确跳转到APP
        }
        else
        {
            stu = 0xAA;//checksum不正确，不能跳转到APP
        }

        sendbuf[sendbufLen++] = stu;//状态，0x55成功，0xAA失败
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;//传输信息
        break;

    default:
        sendbuf[sendbufLen++] = 0xAA;//错误命令
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;
        sendbuf[sendbufLen++] = 0;//传输信息
        break;
    }
    sendbuf[sendbufLen++] = checksum8bit(&sendbuf[1],7);//checksum
    sendbuf[sendbufLen++] = 0x7E;//包尾

}

__attribute__((always_inline)) __STATIC_INLINE void i2cSlaveSendData(void)
{
    if(sendbufIdx >= sendbufLen)
    {
        bl_I2C_CLEAR_SI();
        return;
    }
    bl_I2C_SLAVE_SEND_DR(sendbuf[sendbufIdx++]);
}

void I2C_Bootloader(void)
{
    switch (I2C->SR)
    {
    case 0xA8://从机已收到地址+R，并返回ACK
        i2cSlaveSendData();
        memBufIdx = 0;
        break;
    case 0xB8://从机已发送数据，并收到ACK
        i2cSlaveSendData();
        break;
    case 0xC0://从机已发送数据，并收到NACK
    case 0xC8://从机已发送最后一个数据，并收到NACK
        bl_I2C_CLEAR_SI();

        break;
    case 0x60://从机已收到地址+W，并返回ACK
        memBufIdx = 0;
        bl_I2C_CLEAR_SI();
        break;
    case 0x80://从机已收到数据，并返回ACK
        memBuf[memBufIdx++] = I2C->DR;
        bl_I2C_CLEAR_SI();
        break;
    case 0x88://从机已收到数据，并返回NACK
        memBuf[memBufIdx++] = I2C->DR;
        bl_I2C_CLEAR_SI();
        break;

    case 0xD0://从机已收到STOP
        bl_I2C_CLEAR_SI();
        i2cCmdDecode();
        memBufIdx = 0;
        break;

    default:
        bl_I2C_CLEAR_SI();
        break;
    }
}


