#ifndef _HAL_FlASH_H
#define _HAL_FlASH_H

#include "fpdefine.h"

#define FLASH_SECTOR_SIZE  512 //一个扇区512字节


//FLASH 操作模式配置参数
#define FLASH_MODE_PROGRAM              (u32)0xF0AA1234
#define FLASH_MODE_ERASE_SECTOR         (u32)0xF055ABCD
#define FLASH_MODE_ERASE_CHIP     		  (u32)0xF0CC5678
#define FLASH_MODE_READ_ONLY       			(u32)0x00000000

//FLASH 操作模式标志
#define FLASH_MODE_FLAG_PROGRAM         (u32)0x00000001
#define FLASH_MODE_FLAG_ERASE_SECTOR    (u32)0x00000002
#define FLASH_MODE_FLAG_ERASE_CHIP      (u32)0x00000003
#define FLASH_MODE_FLAG_READ_ONLY       (u32)0x00000000

//FLASH 标志位
#define FLASH_IF_CRC_ERR         			  (u32)0x00004000
#define FLASH_IF_CRC_DONE         		  (u32)0x00002000
#define FLASH_IF_READ_AUTH_ERR     			(u32)0x00000400
#define FLASH_IF_PROG_AUTH_ERR     			(u32)0x00000200
#define FLASH_IF_ERASE_AUTH_ERR    			(u32)0x00000100
#define FLASH_IF_PROG_CHECK_ERR    			(u32)0x00000080
#define FLASH_IF_ERASE_CHECK_ERR   			(u32)0x00000040
#define FLASH_IF_READ_ADDR_INVLD   			(u32)0x00000020
#define FLASH_IF_PROG_ADDR_INVLD   			(u32)0x00000010
#define FLASH_IF_ERASE_ADDR_INVLD  			(u32)0x00000008
#define FLASH_IF_PROGRAM_END       			(u32)0x00000004
#define FLASH_IF_ERASE_SECTOR_END  			(u32)0x00000002
#define FLASH_IF_ERASE_CHIP_END    			(u32)0x00000001

//FLASH 中断标志位
#define FLASH_IE_CRC_ERR           			(u32)0x00004000
#define FLASH_IE_CRC_DONE         		  (u32)0x00002000
#define FLASH_IE_READ_AUTH_ERR     			(u32)0x00000400
#define FLASH_IE_PROG_AUTH_ERR     			(u32)0x00000200
#define FLASH_IE_ERASE_AUTH_ERR    			(u32)0x00000100
#define FLASH_IE_PROG_CHECK_ERR    			(u32)0x00000080
#define FLASH_IE_ERASE_CHECK_ERR   			(u32)0x00000040
#define FLASH_IE_READ_ADDR_INVLD   			(u32)0x00000020
#define FLASH_IE_PROG_ADDR_INVLD   			(u32)0x00000010
#define FLASH_IE_ERASE_ADDR_INVLD  			(u32)0x00000008
#define FLASH_IE_PROGRAM_END       			(u32)0x00000004
#define FLASH_IE_ERASE_SECTOR_END  			(u32)0x00000002
#define FLASH_IE_ERASE_ALLFLASH_END     (u32)0x00000001


#define M8(adr)  (*((volatile uint8_t  *) (adr)))   //以8位方式读FLASH数据
#define M16(adr) (*((volatile uint16_t *) (adr)))	 //以16位方式读FLASH数据（注意字节对齐）
#define M32(adr) (*((volatile uint32_t *) (adr)))	 //以32位方式读FLASH数据（注意字节对齐）



void Flash_Unlock(void);
void Flash_Lock(void);
void Flash_SCU24MHz_ClkCfg(void);
void Flash_ReadSpeed(FunctionalState NewState);
void Flash_Main_WriteEease_Enable(void);
void Flash_NVR_WriteEease_Enable(void);
BOOL FLASH_ProgramWord(uint32_t StartAddr,uint32_t *Data,uint32_t Num);
BOOL FLASH_ProgramOneWord(uint32_t StartAddr,uint32_t Data);
BOOL FLASH_ProgramHalfWord(uint32_t StartAddr,uint16_t *Data,uint32_t Num);
BOOL FLASH_ProgramOneHalfWord(uint32_t StartAddr,uint16_t Data);
BOOL FLASH_ProgramByte(uint32_t StartAddr,uint8_t *Data,uint32_t Num);
BOOL FLASH_ProgramOneByte(uint32_t StartAddr,uint8_t Data);
BOOL Flash_EraseChip(void);
BOOL Flash_EraseSector(uint32_t StartAddr);
BOOL ReadOutProtectEnable(void);
BOOL ReadOutProtectDisable(void);
uint16_t Flash_CRC(u32 StartAddress,u32 EndAddress);

#endif

