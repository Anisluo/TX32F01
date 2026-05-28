# APP_TEST — minimal BL+APP verification harness

The cheapest possible APP that proves the bootloader → APP handoff is alive.
Zero external hardware: no SPI NOR, no motor, no shunt, no Hall sensors,
no I²C devices. Only the LED on `GPIO0.PIN03` and UART on
`GPIO3.PIN06/PIN07` (115200 8N1).

If this APP runs, the entire BL infrastructure works. Any breakage in
APP_FOC / APP_LOG / APP_BENCH is **in their own logic**, not in the
shared BL+softvec+fault_dump chain.

## What runs

```
main()
  ├── scu_init_24mhz()                  — clock to 24 MHz, BOR on
  ├── led_init()                         — GPIO0.PIN03 output
  ├── uart_init_115200()                 — UART RX/TX on GPIO3.PIN06/07
  ├── print banner + reset reason
  ├── register HardFault soft-vector → fault_dump
  ├── fault_dump_init() + drain prev    — if previous boot HardFaulted
  ├── register SysTick soft-vector
  ├── SysTick_Config(24000)              — 1 ms tick
  └── loop:
        every 500 ms: toggle LED
        every 1000 ms: print "[TEST] alive #N up=...ms"
        on UART RX byte:
          ?/h → help
          s   → status (uptime / boot_count / fault_count)
          c   → deliberate HardFault to validate fault_dump path
          *   → echo
```

## Files

```
APP_TEST/
├── USER/main.c           ← entry point (200 lines, single file)
├── TX32F01_TEST.sct      ← scatter: link @ 0x01002000, reserves fault region
├── TX32F01_TEST.uvprojx  ← Keil project, pre-wired
└── README.md             ← this file
```

Dependencies pulled from sibling dirs:
- `APP_PATCH/app_softvec.c/.h`     — soft-vector registration
- `APP_PATCH/fault_dump.c/.h/_asm.s` — HardFault capture
- `Device/TX32F01/HAL_lib/src/HAL_{SCU,GPIO,UART,Flash}.c`

Total firmware size: ~3 KB Flash, ~200 B SRAM. Comfortable headroom.

## Expected UART output

### First boot (no previous fault)

```
============================
  TX32F01 Bootloader v1.0
============================
[BL] alive ×N
                              ← BL jumps here after the wait window
[TEST] APP_TEST start @ 0x01002000
[TEST] SCU_RSR=0x00000022 POR SOFT
[TEST] no pending fault. boot_count=1 total_faults=0
[TEST] BL+APP handoff OK. heartbeat starting...
[TEST] alive #1 up=1000ms
[TEST] alive #2 up=2000ms
[TEST] alive #3 up=3000ms
...
```

### After typing `c` (intentional crash test)

```
[TEST] crashing intentionally...

============================
  TX32F01 Bootloader v1.0       ← chip reset
============================
[BL] alive ×N

[TEST] APP_TEST start @ 0x01002000
[TEST] SCU_RSR=0x00000020 SOFT  ← SOFT reset = NVIC_SystemReset()
[TEST] previous boot crashed — dumping:

========== HARDFAULT CAPTURED ==========
boot_count  = 2
fault_count = 1
module      = "test"
reset_rsr   = 0x00000020 SOFT
flags       = 0x00000000
exc_return  = 0xFFFFFFF9 (thread-mode, MSP)
...
R0  = 0xFFFFFFFE                  ← we jumped to 0xFFFFFFFE
PC  = 0xFFFFFFFE
LR  = 0x0100xxxx                  ← return addr inside do_crash()
...
========================================

[TEST] BL+APP handoff OK. heartbeat starting...
```

If this works end-to-end, your BL + softvec + fault_dump + APP infra is
production-quality and you can confidently start debugging APP_FOC,
APP_LOG, etc. without doubting the foundation.

## Build / flash

### One-time: flash BL (if not already)

1. Keil → open `BOOTLOADER/TX32F01_BL.uvprojx`
2. Project → Rebuild All
3. Flash → Download
   - **Flash Download settings: `Erase Sectors`, range `0x01000000 / 0x2000`**

### Flash APP_TEST

1. Keil → open `APP_TEST/TX32F01_TEST.uvprojx`
2. Project → Rebuild All
3. Flash → Download
   - **Flash Download settings: `Erase Sectors`, range `0x01002000 / 0x5800`**
   - This is already preset in the .uvprojx but verify in
     `Options → Debug → Settings → Flash Download`

### Reset and watch UART

```
serial: 115200 8N1
expect: banner from BL → wait window → [TEST] APP_TEST start
```

## Why not just patch APP_FOC instead

APP_FOC depends on:
- TIM0/1/2 (PWM)
- ADC + shunt amp (current sensing)
- Hall sensors on EXTI
- Motor on the inverter output

If any of those isn't wired correctly, the FOC ISR can hang waiting for
events that never come — masking infra bugs. APP_TEST has none of those
dependencies, so a successful run unambiguously confirms the infra.

Use APP_TEST first; promote to APP_FOC once you trust the foundation.
