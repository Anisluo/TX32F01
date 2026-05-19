#include "app_softvec.h"

/* 让 App 主动跳回 bootloader：写魔数到 BOOTFLAG_BASE，然后软复位 */
#define APP_BOOTFLAG_BASE    0x01007C00UL
#define APP_BOOT_REQ_UPDATE  0xA55AF00DUL

void app_request_bootloader_update(void)
{
    /* 用 HAL 的 Flash 接口 */
    SCU_Unlock();
    Flash_SCU24MHz_ClkCfg();
    Flash_Unlock();
    Flash_Main_WriteEease_Enable();

    (void)Flash_EraseSector(APP_BOOTFLAG_BASE);
    (void)FLASH_ProgramOneWord(APP_BOOTFLAG_BASE, APP_BOOT_REQ_UPDATE);

    __disable_irq();
    NVIC_SystemReset();
    for (;;) { }
}

/*
 *  App 的 _所有_ ISR 都应通过 app_softvec_register_*() 注册。
 *  App 自己的 startup_TX32F01.s 里那些 *_Handler 弱符号"是死循环"，
 *  正常情况下永远不会被进到 —— 因为中断已经被 BL 接住了。
 *  这里保留它们，万一升级出问题时也至少有 hardfault 不被静默。
 */
