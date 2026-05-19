# 把现有 App 工程改造成 BL 兼容版本（5 步）

> 假设 App 工程是 `TX32F01_DemoBoard_Lib/1.GPIO/1.Toggle`，BL 已经烧到 `0x01000000`。

### 1. 替换 scatter file

把工程里 `Objects/TX32F01.sct` 的内容换成 `APP_PATCH/TX32F01_APP.sct` 的内容
（基址改成 `0x01002000`、大小 `0x5800`）。Keil 里：
`Options for Target → Linker` 勾掉 "Use Memory Layout"，
"Scatter File" 填上 `..\..\..\..\APP_PATCH\TX32F01_APP.sct`。

### 2. 调试下载范围也要改

`Options for Target → Debug → Settings (Flash Download)`：
Add 一个 Flash Algorithm，下载起始地址改成 `0x01002000`，
大小 `0x5800` —— 这样下载 App 时不会动到 BL。

### 3. 把 `APP_PATCH/app_softvec.c/.h` 加进 App 工程

放到 USER 或新建的 `BOOT_GLUE` 分组都行。

### 4. 把 App 里的中断函数改成"注册式"

原来：
```c
void TIMER0_Handler(void) {  ...  }
```
改成：
```c
#include "app_softvec.h"

static void my_tim0_isr(void) { ... }

void hw_init(void) {
    ...
    app_softvec_register_irq(TIM0_IRQn, my_tim0_isr);
    NVIC_EnableIRQ(TIM0_IRQn);
}
```

`systick.c` 里的 `SysTick_Handler` 同理：
```c
static void app_systick_isr(void) { TimingDelay_Decrement(); }

void delay_init(void) {
    u32 SYSCLK = GetSystemClock();
    app_softvec_register_systick(app_systick_isr);
    SysTick_Config(SYSCLK / 1000);
    NVIC_SetPriority(SysTick_IRQn, 0xF);
}
/* 注意：原来文件里的 `void SysTick_Handler(void)` 必须删掉，
   否则会覆盖 BL 的强符号定义 —— 但实际上 App 的代码根本不会被中断
   入口跳到（中断走的是 BL 的向量表），所以删掉即可。*/
```

### 5. 生成 firmware.bin 并通过 YMODEM 升级

Keil 命令行加：
```
fromelf --bin --output ./Objects/firmware.bin ./Objects/TX32F01.axf
```
然后在 PC 用任意 YMODEM 工具发：
- **SecureCRT**：Transfer → Send YModem → 选 firmware.bin
- **Linux**：`sb firmware.bin < /dev/ttyUSB0 > /dev/ttyUSB0`（lrzsz 包）
- **Tera Term**：File → Transfer → YMODEM → Send

BL 收完会算 CRC、写 meta、软复位，下一次就进 App 了。

---

## App 主动进升级模式

```c
#include "app_softvec.h"
...
if (button_pressed_3s) app_request_bootloader_update();
```
