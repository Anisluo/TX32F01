# APP_FOC — TX32F01 sensored FOC demo

A minimal but complete sensored field-oriented control demo for the TX32F01
(Cortex-M0, 24 MHz, 4 KB SRAM, 32 KB Flash). Designed to drop into the same
Bootloader + APP layout as the other `APP_*` projects in this repo.

## Architecture

```
                              ┌─────────────────────────────┐
                              │       TIM0 (master)         │
                              │ PWM phase U + Update event  │
                              └────────────┬────────────────┘
                                           │ trigger
                                           ▼
        Phase Currents Ia/Ib  ─────▶ ┌──────────┐
        Bus voltage Vbus      ─────▶ │   ADC    │ 3-rank sequence
                                     └─────┬────┘
                                           │ EOC IRQ
                                           ▼
                              ┌─────────────────────────────┐
                              │  FOC loop @ 10 kHz          │
                              │  Clarke → Park → PI → IPark │
                              │  → SVPWM → CCR0/CCR1/CCR2   │
                              └─────────────────────────────┘
                                           ▲
                              ┌─────────────────────────────┐
                              │  Hall A/B/C → EXTI          │
                              │  Sector edge → angle/ω      │
                              └─────────────────────────────┘
```

## Pin mapping

The library only exposes one OCx + OCxN pair per timer, so we use all three
timers, one per half-bridge.

| Phase | TIM | High side (OCx) | Low side (OCxN) | AF              |
|-------|-----|-----------------|------------------|-----------------|
| U     | TIM0| GPIO2.PIN02     | GPIO0.PIN03      | T0CH / T0CHN    |
| V     | TIM1| GPIO0.PIN00     | GPIO0.PIN01      | T1CH / T1CHN    |
| W     | TIM2| GPIO1.PIN05     | GPIO1.PIN04      | T2CH / T2CHN    |

| Signal | Pin                       | Note                              |
|--------|---------------------------|-----------------------------------|
| Ia     | GPIO1.PIN00 (ADC AN5)     | Phase U current shunt amp out     |
| Ib     | GPIO1.PIN01 (ADC AN6)     | Phase V current shunt amp out     |
| Vbus   | GPIO1.PIN02 (ADC AN7)     | DC bus divider                    |
| Hall A | GPIO2.PIN00 → EXTI0       | Both edges                        |
| Hall B | GPIO2.PIN01 → EXTI1       | Both edges                        |
| Hall C | GPIO2.PIN02 → EXTI2       | Conflicts with T0CH — see below   |
| Brake  | GPIO1.PIN02 (TIMER_BRK_IN)| Optional, shared with Vbus pin    |
| UART   | GPIO3.PIN06/PIN07         | Shell at 115200 8N1               |

> ⚠️ **Pin conflict warning.** The TX32F01 AF table is small. The pins above
> are picked to match `HAL_GPIO.h` + the existing PWM examples but **you
> must verify against your actual demo board schematic**. Edit
> `foc_config.h` to relocate. In particular, Hall C and T0CH both want
> GPIO2.PIN02 — pick one or move Hall C onto EXTI6/7 on GPIO3.

## Build modes

Three shell commands (over UART) drive the demo:

- `idle`   — all PWM at 50%, no current command. Safe state.
- `align` — apply Id=I_align at θ=0 for 200 ms to lock the rotor.
- `vf <Hz>`  — open-loop V/F ramp at `Hz` electrical Hz.
- `foc <rpm>` — closed-loop sensored FOC with id*=0, iq* from speed PI.
- `stop`   — disable PWM (BDTR break).

Telemetry is printed every 100 ms with `id, iq, vbus, hall, rpm`.

## Files

```
APP_FOC/
├── FOC/
│   ├── foc_config.h     // all tunables and pin assignments
│   ├── foc_math.h/.c    // Q15 helpers, sin/cos table
│   ├── foc_pwm.h/.c     // 3× TIM init + SVPWM commit
│   ├── foc_adc.h/.c     // sequence + EOC ISR
│   ├── foc_hall.h/.c    // EXTI-based sector + speed
│   ├── foc_loop.h/.c    // Clarke/Park/PI, mode machine
│   └── foc_shell.h/.c   // tiny UART REPL
├── USER/
│   └── main.c
├── TX32F01_FOC.uvprojx  // Keil µVision 5 project (ARMCC v5.06)
├── TX32F01_FOC.sct      // scatter: links @ 0x01002000, size 0x5800
└── README.md
```

## 用 Keil 打开 + 编译

1. **打开工程**：双击 `TX32F01_FOC.uvprojx`。Keil µVision 5 / 设备包 `Keil.TX32F01.1.0.3` 已被
   工程文件指定，跟仓库里其它 `APP_*` 工程一致。
2. **检查 include 路径**（Project → Options → C/C++ → Include Paths，已在 uvprojx 里设置好）：

   ```
   ..\Device\CMSIS\KEIL_CORE
   ..\Device\TX32F01\Include
   ..\Device\TX32F01\HAL_lib\inc
   ..\APP_PATCH
   .\FOC
   ```
3. **Build**：F7。预期输出 `Objects/firmware.bin` 和 `Objects/TX32F01_FOC.axf`，
   AfterMake 脚本会自动 `fromelf` 出 bin。
4. **烧录**：和其它 APP 一样有两种方式：
   - 用 J-Link 直接烧 `Objects/firmware.bin` 到地址 `0x01002000`（保留 BL 不动）；
   - 进入 Bootloader 后通过 YMODEM 串口升级，发送同一个 `firmware.bin`（用仓库根目录
     `ymodem_send.ps1` 脚本）。

### 工程构成

| Group       | 内容 |
|-------------|---|
| startup     | `..\Device\TX32F01\Source\ARM\startup_TX32F01.s` |
| USER        | `.\USER\main.c` |
| FOC         | `foc_math.c` / `foc_pwm.c` / `foc_adc.c` / `foc_hall.c` / `foc_loop.c` / `foc_shell.c` |
| BOOT_GLUE   | `..\APP_PATCH\app_softvec.c`（中断软向量转发，BL trampoline 需要） |
| HAL         | `HAL_SCU.c` / `HAL_GPIO.c` / `HAL_UART.c` / `HAL_TIM.c` / `HAL_ADC.c` |

注意 HAL 组里 **只包含实际用到的 5 个** 模块，没把 SPI/I2C/IWDT/Flash 拉进来，
省 Flash 容量。如果你给 demo 加了 SPI/I2C 外设，记得手动把对应 `HAL_*.c` 加入工程。

### 常见编译报错

| 报错 | 处理 |
|------|------|
| `cannot open source input file "TX32F01_periph.h"` | Include 路径丢了 `..\Device\TX32F01\HAL_lib\inc`，检查 Options → C/C++。|
| `undefined symbol app_softvec_register_irq` | BOOT_GLUE 组没编进去，或者 `..\APP_PATCH` 不在 include 路径里。|
| `Error: L6406E: No space in execution regions` | FOC 代码超 22 KB，需要降 `Optim` 到 `-Os` 或者去掉 telemetry。 |
| `warning: function declared implicitly` | 缺 include；FOC 模块之间互相 include 时只用对应的 `.h`。|

## Safety checklist before powering an inverter

1. **Verify dead-time.** `FOC_DEADTIME_TCK` in `foc_config.h` must be ≥ the
   gate driver's measured propagation delay. Default = 20 timer ticks
   ≈ 2.5 µs at 8 MHz timer clock — adjust to your board.
2. **Verify polarity.** OCx (high side) starts HIGH at CNT=0 and goes LOW
   at CCR match. OCxN is the complement. If your gate driver is
   active-low, flip with `TIM_SetOCx_Polarity` / `TIM_SetOCxN_Polarity`.
3. **Hardware overcurrent.** Tie a comparator output to TIMER_BRK_IN
   (GPIO1.PIN02). Default polarity is falling-edge → BDTR forces both
   outputs low. We enable auto-restore so the loop can recover after a
   transient. Disable if you want a latched fault instead.
4. **Bench-test open loop first.** `vf 1` at low Vbus current limit before
   you trust the closed loop.

## Why this is small

| Item                | Bytes |
|---------------------|------:|
| sin table (256×Q15) | 512   |
| Static state        | ~120  |
| Code                | ~6 kB |

Plenty of room left for FreeRTOS or a CAN stack if you want one.
