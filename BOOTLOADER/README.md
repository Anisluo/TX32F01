# TX32F01 Bootloader

8KB 大小、UART (115200, GPIO3-PIN07 TX / PIN06 RX) YMODEM-1K 升级、片上 Flash CRC 校验。
基于本 SDK 的 HAL 实现。Cortex-M0 无 VTOR，通过 SRAM 软向量表把中断从 BL 转发给 APP。

## 编译

Keil μVision 新建工程，Device: 任意 CM0 通用即可。把这些文件加入工程：

源文件：
- `BOOTLOADER/USER/main.c`
- `BOOTLOADER/BOOT/boot_flash.c`
- `BOOTLOADER/BOOT/boot_uart.c`
- `BOOTLOADER/BOOT/boot_ymodem.c`
- `BOOTLOADER/BOOT/boot_jump.c`
- `Device/TX32F01/HAL_lib/src/HAL_SCU.c`
- `Device/TX32F01/HAL_lib/src/HAL_GPIO.c`
- `Device/TX32F01/HAL_lib/src/HAL_UART.c`
- `Device/TX32F01/HAL_lib/src/HAL_Flash.c`
- `Device/TX32F01/Source/ARM/startup_TX32F01.s`（**直接复用原版**——所有 *_Handler 都是 weak，boot_jump.c 里给出强符号会自动接管）

Include 路径：
- `Device/CMSIS/KEIL_CORE`
- `Device/TX32F01/Include`
- `Device/TX32F01/HAL_lib/inc`
- `BOOTLOADER/BOOT`

Linker：
- 取消"Use Memory Layout"，指定 scatter `BOOTLOADER/TX32F01_BL.sct`
- Code/Const 起始 `0x01000000`，大小 `0x2000`

下载范围：
- Flash Download 算法的 Range：`0x01000000` ~ `0x01001FFF`（**只动 BL 这 8KB**）

C 选项：
- Optimization: -O3 推荐（要塞进 8KB 通常需要 -O2 以上 + 移除浮点支持）
- 不要勾 MicroLIB 也行；BL 不依赖标准库

## 内存分区

| 区域 | 地址 | 大小 | 用途 |
|---|---|---|---|
| BL  | `0x01000000` | 8 KB | 本程序 |
| APP | `0x01002000` | 22 KB | 用户应用 |
| META | `0x01007800` | 1 KB | App magic/size/crc |
| BOOTFLAG | `0x01007C00` | 1 KB | 0xA55AF00D = 留在 BL |
| 软向量表 | `0x20000000` | 96 B | BL 与 APP 共享的中断转发表 |

## 启动流程

```
                              ┌───────────────┐
                              │  reset → BL   │
                              └───────┬───────┘
                                      ▼
                         读 BOOTFLAG / 校验 APP META
                                      │
                ┌─────────────────────┼─────────────────────┐
                ▼                     ▼                     ▼
        flag = 0xA55AF00D     meta ok 且 flag 空         meta 坏
                │                     │                     │
                ▼                     ▼                     ▼
         强制 YMODEM         3 秒内有字节进来?       强制 YMODEM
                │                     │
                │           是 ───────┘
                ▼           否
         接收→烧写→校验    跳转 APP (设 SP/PC)
         成功→软复位
```

## 错误码 (LED)

| 闪烁规律 | 含义 |
|---|---|
| 250 ms 慢闪 | 等待升级中 |
| 长亮 1 s 后短闪 2 次 循环 | 升级流程结束（成功跑到这里说明 reset 失败异常） |
| 长亮 1 s 后短闪 3 次 循环 | YMODEM 接收 / Flash 写 / Meta 写失败 |

## 已知限制

- 暂未实现读保护 (`ReadOutProtectEnable`)。生产前可以在 BL 第一次跑时开。
- BL 不喂 IWDT；推荐 BL 阶段关闭 IWDT，跳到 APP 后再开。
- YMODEM 包大小 1024 B，正好两个 Flash sector，每包先擦后写 ~6 ms。
