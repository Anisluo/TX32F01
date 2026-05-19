#include "HAL_Flash.h"
#include "TX32F01.h"

#define Flash_START_ADDRESS  0x1000000
#define Reg_ReadOutProrect	 0x10085fc

/********************************************************************************************************
**函数信息 ：Flash_Unlock()
**功能描述 ：使能Flash寄存器写操作
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/
void Flash_Unlock(void)
{
    FLASH->PKEYR=0xbadbee;//使能Flash寄存器写操作。
}


/********************************************************************************************************
**函数信息 ：Flash_Lock()
**功能描述 ：失能Flash寄存器写操作
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/
void Flash_Lock(void)
{
    FLASH->PKEYR=0;//禁止Flash寄存器写操作
}


/********************************************************************************************************
**函数信息 ：Flash_Main_WriteEease_Enable()
**功能描述 ：使能main区擦写操作
**输入参数 ：无
**输出参数 ：无
********************************************************************************************************/
void Flash_Main_WriteEease_Enable(void)
{
    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
}

/********************************************************************************************************
**函数信息 ：Flash_SCU24MHz_ClkCfg()
**功能描述 ：当系统时钟为24MHz时，如果想要擦写FLASH数据，必须要调用本函数
**输入参数 ：无
**输出参数 ：无
**注：本芯片只有在系统时钟4MHz和24MHz下才能擦写FLASH。当系统时钟为24MHz时，必须要调用本函数；当系统时钟为4MHz时，
			不需要调用本函数。
********************************************************************************************************/
void Flash_SCU24MHz_ClkCfg(void)
{
    FLASH->PKEYR = 0xBADBEE;
    *((unsigned int*)0x40008030) = 0x11badbee;
    *((unsigned int*)0x40008034) = 0x9C;
    *((unsigned int*)0x40008038) = 0x9C;
    *((unsigned int*)0x4000803C) = 0x1;
    *((unsigned int*)0x40008040) = 0x1;
    *((unsigned int*)0x40008044) = 0x84;
    *((unsigned int*)0x40008048) = 0x6;
    *((unsigned int*)0x4000804C) = 0x84;
    *((unsigned int*)0x40008050) = 0x528;
    *((unsigned int*)0x40008054) = 0x14C4;
    *((unsigned int*)0x40008058) = 0x1A5DA;
    *((unsigned int*)0x4000805C) = 0xAFC7A;
    *((unsigned int*)0x40008060) = 0x3;
}

/********************************************************************************************************
**函数信息 ：Flash_ReadSpeed(FunctionalState NewState)
**功能描述 ：开启/关闭Flash读加速
**输入参数 ：NewState：ENABLE(开启读加速)/DISABLE(关闭读加速)
**输出参数 ：无
********************************************************************************************************/
void Flash_ReadSpeed(FunctionalState NewState)
{
    if(NewState==ENABLE)
    {
        FLASH->ACR|=0x1;//开启读加速
    }
    else
    {
        FLASH->ACR&=~0x1;//关闭读加速
    }
}


/********************************************************************************************************
**函数信息 ：FLASH_ProgramWord(uint32_t StartAddr,uint32_t *Data,uint32_t Num)
**功能描述 ：按字（32位）为单位写入Flash
**输入参数 ：StartAddr：要写入数据的Flash首地址
						*Data：指向要写入Flash数据数组的首地址
						Num：写入数据长度
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramWord(uint32_t StartAddr,uint32_t *Data,uint32_t Num)
{
    uint32_t i;
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    for(i=0; i<Num; i++)
    {
        FLASH->IFR=0;//清标志位
        M32(StartAddr)=*Data;
        while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
		{
            j++;
            if(j>=2000)
                return FALSE;
        }
        StartAddr+=4;//地址+4
        Data+=1;//数据递增
        j=0;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}

/********************************************************************************************************
**函数信息 ：FLASH_ProgramOneWord(uint32_t StartAddr,uint32_t Data)
**功能描述 ：按字（32位）往某一地址写入数据
**输入参数 ：StartAddr：要写入数据的Flash地址
						Data：要写入的数据
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramOneWord(uint32_t StartAddr,uint32_t Data)
{
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    FLASH->IFR=0;//清标志位
    M32(StartAddr)=Data;
    while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
	{
        j++;
        if(j>=2000)
            return FALSE;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}


/********************************************************************************************************
**函数信息 ：FLASH_ProgramHalfWord(uint32_t StartAddr,uint16_t *Data,uint32_t Num)
**功能描述 ：按半字（16位）为单位写入Flash
**输入参数 ：StartAddr：要写入数据的Flash首地址
						*Data：指向要写入Flash数据数组的首地址
						Num：写入数据长度
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramHalfWord(uint32_t StartAddr,uint16_t *Data,uint32_t Num)
{
    uint32_t i;
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    for(i=0; i<Num; i++)
    {
        FLASH->IFR=0;//清标志位
        M16(StartAddr)=*Data;
        while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
		{
            j++;
            if(j>=2000)
                return FALSE;
        }

        StartAddr+=2;//地址+2
        Data+=1;//数据递增
        j=0;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}

/********************************************************************************************************
**函数信息 ：FLASH_ProgramOneHalfWord(uint32_t StartAddr,uint16_t Data)
**功能描述 ：按半字（16位）往某一地址写入数据
**输入参数 ：StartAddr：要写入数据的Flash地址
						Data：要写入的数据
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramOneHalfWord(uint32_t StartAddr,uint16_t Data)
{
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    FLASH->IFR=0;//清标志位
    M16(StartAddr)=Data;
    while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
	{
        j++;
        if(j>=2000)
            return FALSE;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}

/********************************************************************************************************
**函数信息 ：FLASH_ProgramByte(uint32_t StartAddr,uint8_t *Data,uint32_t Num)
**功能描述 ：按字节（8位）为单位写入Flash
**输入参数 ：StartAddr：要写入数据的Flash首地址
						*Data：指向要写入Flash数据数组的首地址
						Num：写入数据长度
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramByte(uint32_t StartAddr,uint8_t *Data,uint32_t Num)
{
    uint32_t i;
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    for(i=0; i<Num; i++)
    {
        FLASH->IFR=0;//清标志位
        M8(StartAddr)=*Data;
        while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
		{
            j++;
            if(j>=2000)
                return FALSE;
        }

        StartAddr+=1;//地址+1
        Data+=1;//数据递增
        j=0;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}

/********************************************************************************************************
**函数信息 ：FLASH_ProgramOneHalfWord(uint32_t StartAddr,uint8_t Data)
**功能描述 ：按字节（8位）往某一地址写入数据
**输入参数 ：StartAddr：要写入数据的Flash地址
						Data：要写入的数据
**输出参数 ：FALSE：Flash写失败
						TRUE：Flash写成功
********************************************************************************************************/
BOOL FLASH_ProgramOneByte(uint32_t StartAddr,uint8_t Data)
{
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    FLASH->IFR=0;//清标志位
    M8(StartAddr)=Data;
    while(FLASH->IFR != FLASH_IE_PROGRAM_END) 
	{
        j++;
        if(j>=2000)
            return FALSE;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}

/********************************************************************************************************
**函数信息 ：Flash_EraseChip()
**功能描述 ：Flash main区全擦除
**输入参数 ：无
**输出参数 ：FALSE：Flash全擦除失败
						TRUE：Flash全擦除成功
********************************************************************************************************/
BOOL Flash_EraseChip(void)
{
    uint32_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_ERASE_CHIP;//开启Flash擦扇区模式
    FLASH->IFR=0;//清标志位

    M32(0x1000000)=0x0;//往扇区地址写数据，擦除对应扇区数据

    //全片擦除后，以下程序也运行不到了
    while((FLASH->IFR &FLASH_IE_ERASE_ALLFLASH_END)==0 )
	{
        j++;
        if(j>=32*40000)//Flash全片擦除最大等待40ms
            return FALSE;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}


/********************************************************************************************************
**函数信息 ：Flash_EraseSector(uint32_t StartAddr)
**功能描述 ：Flash main扇区擦除
**输入参数 ：无
**输出参数 ：FALSE：Flash全擦除失败
						TRUE：Flash全擦除成功
********************************************************************************************************/
BOOL Flash_EraseSector(uint32_t StartAddr)
{
    uint32_t j=0;

    FLASH->ACHKR=0x1;//使能擦除完成自动校验功能
    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_ERASE_SECTOR;//开启Flash擦扇区模式
    FLASH->IFR=0;//清标志位

    M32(StartAddr)=0x0;//往扇区地址写数据，擦除对应扇区数据

    while(FLASH->IFR != FLASH_IE_ERASE_SECTOR_END) 
	{
        j++;
        if(j>=32*5000)//Flash扇区擦除最大等待5ms
            return FALSE;
    }

    FLASH->IFR=0;//清标志位
    return  TRUE;
}


/********************************************************************************************************
**函数信息 ：ReadOutProtectEnable()
**功能描述 ：使能读保护
**输入参数 ：无
**输出参数 ：FALSE：加密功能未打开，设置读保护失败
						TRUE：0：加密功能已打开，设置读保护成功
********************************************************************************************************/
BOOL ReadOutProtectEnable(void)
{
    uint16_t j=0;

    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_PROGRAM;//开启Flash写模式

    M32(Reg_ReadOutProrect)=~0x1;//使能读保护

    while((FLASH->ASR&0x1)==0) 
	{
        j++;
        if(j>=2000)
            return FALSE;
    }

    return TRUE;
}


/********************************************************************************************************
**函数信息 ：ReadOutProtectDisable()
**功能描述 ：失能读保护
**输入参数 ：无
**输出参数 ：FALSE：加密功能未关闭，解除读保护失败
						TRUE：加密功能已关闭，解除读保护成功
注意：不管有没有设置读保护，只要解除读保护，就会导致程序区数据全擦除，慎重调用
********************************************************************************************************/
BOOL ReadOutProtectDisable(void)
{
    uint32_t j=0;

    FLASH->ACHKR=0x1;//使能擦除完成自动校验功能
    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR=FLASH_MODE_ERASE_SECTOR;//开启Flash擦扇区模式

    //注意，不管有没有设置读保护，只要擦除读保护扇区，就会导致FLASH全片擦除，慎重调用
    M32(Reg_ReadOutProrect)=0x0;//擦除该地址数据，即解除读保护

    while((FLASH->ASR&0x1)==0x1) 
	{
        j++;
        if(j>=32*5000)
            return FALSE;
    }

    return TRUE;
}

/********************************************************************************************************
**函数信息 ：Flash_CRC(u32 StartAddress,u32 EndAddress)
**功能描述 ：CRC计算
**输入参数 ：StartAddress：CRC校验起始扇区地址,必须填入扇区的起始地址
						EndAddress：CRC校验结束扇区地址，比如填入结束扇区的结束地址
**输出参数 ：CRC校验码
注意：填入参数必须为扇区起始地址和扇区结束地址
********************************************************************************************************/
uint16_t Flash_CRC(u32 StartAddress,u32 EndAddress)
{
    uint32_t j=0;

    FLASH->CRCARL=StartAddress;//开始地址
    FLASH->CRCARH=EndAddress;//结束地址

    FLASH->IFR=0;//清除标志位

    FLASH->CRCCR=1<<4;//开始计算CRC

    while(FLASH->IFR != FLASH_IF_CRC_DONE) 
	{
        j++;
        if(j>=32*5000)
            return FALSE;
    }

    if(FLASH->IFR&FLASH_IF_CRC_ERR)
    {
        return FALSE;
    }

    FLASH->IFR=0;//清除标志位
    return FLASH->CRCVR;
}



