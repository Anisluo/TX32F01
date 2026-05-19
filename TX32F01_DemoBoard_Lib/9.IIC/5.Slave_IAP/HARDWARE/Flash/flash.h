#ifndef _FLASH_H
#define _FLASH_H
#include "TX32F01_periph.h"
#include "fpdefine.h"

#define appAddr    0x01001000 //应用程序跳转地址

#define Address_EnterAPPFlag    0x1000E00	//更新地址标志位存放地址
#define Address_Checksum        0x1000E04	//checksum存放地址


#define  APPUpdate_Pass_Flag      0x5555 //程序更新成功标志位
#define  APPUpdate_Fail_Flag      0x1234//程序更新失败标志位


//对flash操作需要先调用函数blFlashUnlock
extern booltyped blFlashUnlock(void);
extern booltyped blFlashLock(void);
extern void blFlashEraseWriteClkCfg(void);

extern booltyped appCodeWrite_AutoErase(u32 addr,u32 len,u8* p);
extern booltyped appCode_ALLErase(void);
extern u16 CRC16_Check(void);
extern booltyped  Flash_WriteAPPUpdate_Flag(u32 flagaddr,u16 data);
extern booltyped  Flash_Erase_APPUpdate_Flag(u32 flagaddr);

#endif



