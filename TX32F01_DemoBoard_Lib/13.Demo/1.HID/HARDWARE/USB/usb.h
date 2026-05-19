#ifndef usb_h_
#define usb_h_
#include<stdint.h>
#include<stdbool.h>
extern unsigned char CH375CONFLAG;     // USB配置标志
extern unsigned char IsBusying;        // USB忙碌标志
extern u8 battery_capacity;            // 电池电量百分比
/**
 * CH375初始化
 */
void CH375_Init(void);

/**
 * 设置电池电量（用于测试）
 * @param level: 电池电量 0-100%
 */
void UPS_SetBatteryLevel(u8 level);

/**
 * 获取当前电池电量
 * @return: 电池电量 0-100%
 */
u8 UPS_GetBatteryLevel(void);


/**
 * USB协议处理函数
 */
void mCh375Ep0Up(void);
void mCh375DesUp(void);
void Handle_Cmd_Setup(void);
void Handle_Cmd_Ep0IN(unsigned char request_type);
void EXTI5_Handler(void);

#endif

