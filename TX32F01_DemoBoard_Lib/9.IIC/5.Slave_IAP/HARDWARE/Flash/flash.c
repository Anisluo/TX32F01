#include "flash.h"
#include "TX32F01_periph.h"
#include "i2c.h"

static u32 EndAddress=appAddr;

/******************************************************************************************
*函 数 名  blFlashRegisterOpreEnable
*功能描述  解除flash相关寄存器写入保护
*参    数
*返    回
******************************************************************************************/
static booltyped blFlashRegisterOpreEnable(void)
{
    u32 temp;
    FLASH->PKEYR = 0xBADBEE;
    temp = FLASH->PKEYR;
    if(1 != temp)
    {
        return falsefp;
    }
    return truefp;
}

/******************************************************************************************
*函 数 名  blFlashRegisterOpreDisable
*功能描述  打开flash相关寄存器写入保护
*参    数
*返    回
******************************************************************************************/
static booltyped blFlashRegisterOpreDisable(void)
{
    u32 temp;
    FLASH->PKEYR = 0;
    temp = FLASH->PKEYR;
    if(0 != temp)
    {
        return falsefp;
    }
    return truefp;
}

/******************************************************************************************
*函 数 名  blFlashRegisterOpreIsEnable
*功能描述  查询flash相关寄存器写入保护是否关闭
*参    数
*返    回  truefp，写保护关闭；falsefp，写保护打开
******************************************************************************************/
booltyped blFlashRegisterOpreIsEnable(void)
{
    if(0 != FLASH->PKEYR)
    {
        return truefp;
    }
    return falsefp;
}

/******************************************************************************************
*函 数 名  blFlashUnlock
*功能描述  解锁程序区写入、擦除保护
*参    数
*返    回
******************************************************************************************/
booltyped blFlashUnlock(void)
{
    if(truefp != blFlashRegisterOpreEnable()) {
        return falsefp;
    }
    FLASH->PRGKEYR = 0x22BADBEE;
    return truefp;
}

/******************************************************************************************
*函 数 名  blFlashLock
*功能描述  打开程序区写入、擦除保护
*参    数
*返    回
******************************************************************************************/
booltyped blFlashLock(void)
{
    FLASH->PRGKEYR = 0;
    if(truefp != blFlashRegisterOpreDisable()) {
        return falsefp;
    }
    return truefp;
}

/******************************************************************************************
*函 数 名  appCodeWrite_AutoErase
*功能描述  写片上flash
*参    数  addr相对地址，len数据长度（byte为单位），p数据缓存
*返    回
******************************************************************************************/
booltyped appCodeWrite_AutoErase(u32 addr,u32 len,u8* p)
{
    u32 i;
    u8 *a;

    if((addr+appAddr)>=0x1008000)//防止超过Flash总容量32K
    {
        return falsefp;
    }

    if(0 == (addr&0x1FF))//如果是扇区起始地址，自动擦除
    {
        CLEAR_WATCH_DOG();
        FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
        FLASH->PEKEYR = FLASH_MODE_ERASE_SECTOR;
        FLASH->IFR = 0;
        *((u32*)(addr+appAddr)) = 0;//APP起始地址+命令地址（从0开始）
        i=0;
        while((FLASH->IFR & FLASH_IF_ERASE_SECTOR_END)==0)
        {
            i++;
            if(i>=32*6000) return falsefp;//如果超过6ms还没擦掉，认为擦写失败
        }
        EndAddress=addr+appAddr+0x1ff;
    }

    FLASH->IFR = 0;
    FLASH->PEKEYR = FLASH_MODE_PROGRAM;

    a = (u8*)(addr+appAddr);//APP起始地址+命令地址（从0开始）

    for(i=0; i<len; i++)
    {
        *a = *p;
        a++;
        p++;
			  CLEAR_WATCH_DOG();
			
        if (FLASH_IF_PROGRAM_END != FLASH->IFR)
        {
            return falsefp;
        }
        FLASH->IFR = 0;
    }
    return truefp;

}

booltyped Flash_Erase_APPUpdate_Flag(u32 flagaddr)
{
    u32 i;
    CLEAR_WATCH_DOG();
    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR = FLASH_MODE_ERASE_SECTOR;
    FLASH->IFR = 0;
    *((u32*)(flagaddr)) = 0;
    i=0;
    while((FLASH->IFR & FLASH_IF_ERASE_SECTOR_END)==0)
    {
        i++;
        if(i>=32*6000) return falsefp;//如果超过6ms还没擦掉，认为擦写失败
    }
    CLEAR_WATCH_DOG();
    FLASH->IFR = 0;
    return truefp;
}

booltyped  Flash_WriteAPPUpdate_Flag(u32 flagaddr,u16 data)
{
    CLEAR_WATCH_DOG();

    FLASH->IFR = 0;
    FLASH->PEKEYR = FLASH_MODE_PROGRAM;

    *((u32*)(flagaddr)) = data;//写入标志位

    if (FLASH_IF_PROGRAM_END != FLASH->IFR)
    {
        return falsefp;
    }
    CLEAR_WATCH_DOG();
    FLASH->IFR = 0;

    return truefp;
}

u8 InvertUint8(u8 x)
{
    u8 y, i;
    y = 0;
    for (i = 0; i < 8; i++)
    {
        if (0 != (x & (1 << i)))
        {
            y |= (u8)(1 << (7 - i));
        }
    }
    return y;
}

u16 InvertUint16(u16 x)
{
    u16 y, i;
    y = 0;
    for (i = 0; i < 16; i++)
    {
        if (0 != (x & (1 << i)))
        {
            y |= (u16)(1 << (15 - i));
        }
    }
    return y;
}

u16 CRC16_Check(void)
{
    u16 wCRCin = 0;// 0xFFFF;
    u16 wCPoly = 0x1021;
    u32 i, j;
    u16 wChar = 0;
    u32 pidx;
    for (i = appAddr; i <= EndAddress; i++)
    {
        pidx = (i & 0xFFFFFFFC)|(3-(i&3));
        wChar = InvertUint8(*((u8*)(pidx)));
        wCRCin ^= (wChar << 8);

        for (j = 0; j < 8; j++)
        {
            if (0x8000 == (wCRCin & 0x8000))
            {
                wCRCin = ((wCRCin << 1) ^ wCPoly);
            }
            else
            {
                wCRCin <<= 1;
            }
        }
    }
    return InvertUint16(wCRCin);
}

booltyped appCode_ALLErase(void)
{
    u32 i;
    u32 BaseAddr=appAddr;
    u32 EndAddr=0x1007fff;

    u32 EraseAddr=BaseAddr;
	
    FLASH->PKEYR = 0xBADBEE;
    FLASH->PRGKEYR=0x22badbee;//使能main区擦写操作
    FLASH->PEKEYR = FLASH_MODE_ERASE_SECTOR;
    FLASH->IFR = 0;

    while(EraseAddr<=EndAddr)
    {
        CLEAR_WATCH_DOG();
				M32(EraseAddr)=0x1;//往扇区地址写数据，擦除对应扇区数据
        i=0;
				while((FLASH->IFR & 0x2)==0) 
        {
            i++;
            if(i>=32*6000) 
							
						return falsefp;//如果超过6ms还没擦掉，认为擦写失败
        }
        FLASH->IFR = 0;
        EraseAddr+=0x100;
    }
    return truefp;
}



