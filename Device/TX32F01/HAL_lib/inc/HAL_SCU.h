#ifndef _HAL_SCU_H
#define _HAL_SCU_H

#include "fpdefine.h"
#include "TX32F01.h"


#define SysClock_24M       0x0
#define SysClock_12M       0x8
#define SysClock_6M        0x9
#define SysClock_4M        0xa
#define SysClock_2M        0xb


#define MCOSEL_NoOut       (0x0<<24)
#define MCOSEL_HRC         (0x1<<24)
#define MCOSEL_HCLK        (0x2<<24)
#define MCOSEL_LRC         (0x3<<24)


#define Periph_TIM0        0x1
#define Periph_TIM1        0x2
#define Periph_TIM2        0x4
#define Periph_UART        0x8
#define Periph_I2C         0x10
#define Periph_ADC         0x20
#define Periph_SPI         0x40
#define Periph_GPIO0       0x100
#define Periph_GPIO1       0x200
#define Periph_GPIO2       0x400
#define Periph_GPIO3       0x800
#define Periph_ALL         0xFFF


#define BOR_2P0V					 0x0
#define BOR_2P25V					 (0x1<<1)
#define BOR_2P5V					 (0x2<<1)
#define BOR_2P75V					 (0x3<<1)
#define BOR_3P0V					 (0x4<<1)
#define BOR_3P3V					 (0x5<<1)
#define BOR_3P6V					 (0x6<<1)
#define BOR_4P0V					 (0x7<<1)

#define PVD_2P0V					 0x0
#define PVD_2P25V					 (0x1<<5)
#define PVD_2P5V					 (0x2<<5)
#define PVD_2P75V					 (0x3<<5)
#define PVD_3P0V					 (0x4<<5)
#define PVD_3P3V					 (0x5<<5)
#define PVD_3P6V					 (0x6<<5)
#define PVD_4P0V					 (0x7<<5)



void SCU_Unlock(void);
void SCU_Lock(void);
void SCU_SetSysClock(u8 sysclock);
void SCU_MCO(u32 mcosel);
void SCU_PeriphClockCmd(u32 Periph,FunctionalState NewState);
void SCU_ResetPeriphClock(u32 Periph);
void SCU_SetBor(u8 BorLevel,FunctionalState NewState);
void SCU_SetPVD(u8 PVDLevel,FunctionalState NewState);
uint32_t 	GetSystemClock(void);
void SCU_ClearPWR_Flag(void);
#endif




