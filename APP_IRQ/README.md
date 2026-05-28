# APP_IRQ — Cortex-M0 IRQ latency lab

In-target measurement of interrupt entry latency on the TX32F01 (Cortex-M0, 24 MHz),
with the goal of producing **defensible numbers you can plug into a real-time
schedulability analysis** — e.g. picking the safe deadline between an ADC
sample-ready IRQ and the PWM update window in a FOC current loop.

## 1. The model

For a given peripheral IRQ on Cortex-M0, the elapsed cycles from the
hardware event to the first executed instruction of the C handler is

```
L_total =  L_arch         CM0 NVIC: register stacking + vector fetch (15-16 cyc, fixed)
        + L_trampoline    BL soft-vector lookup (~7 cyc in this project; 0 in a bare-metal app)
        + L_pipeline      cycles to retire the instruction that was in flight (0..3 typ)
        + L_preempt       sum of execution time of any same/higher-priority ISRs queued ahead
        + L_block         time the CPU spent with PRIMASK=1 (or in NMI-only state) before this IRQ
```

`L_arch` is set by the silicon, `L_trampoline` by the bootloader, `L_pipeline`
by whatever was running. **`L_preempt` and `L_block` are entirely controlled
by user code**, and they're the only knobs that can move worst case by 100s
or 1000s of cycles. The whole point of this lab is to *visualize them*.

### Why CM0 specifically

- **No DWT cycle counter** → we use SysTick's CVR as the cycle clock.
- **No nested preemption between equal priorities** → measurement is clean
  if SysTick is the highest priority. We set `NVIC_SetPriority(SysTick_IRQn, 0)`.
- **No VTOR** → vector table is fixed at 0x00000000 (aliased to BL flash).
  Every IRQ on this board passes through a soft-vector trampoline. That
  trampoline is the `L_trampoline` term above; we measure including it
  because that's what your real handler actually pays.

## 2. The measurement

SysTick is configured for `RVR = 23999` (1 ms period). When CVR reaches 0 it
auto-reloads to 23999 and pends the IRQ. The handler's **first** statement is

```c
uint32_t cvr = SysTick->VAL;
uint32_t lat = 23999 - cvr;     /* cycles since the underflow event */
```

`lat` is the full `L_total`. The lower bound corresponds to firing during an
unmasked, cheap instruction (or WFI); the upper bound is `L_block` + the rest.

[lat_meas.c:18-37](IRQ/lat_meas.c#L18-L37) is the inner loop — keep it that
short or you'll add to the measurement itself.

## 3. The stressors

Five selectable workloads in [IRQ/lat_stress.c](IRQ/lat_stress.c) change
what the CPU is doing when SysTick fires:

| # | Mode      | Mechanism | What it teaches |
|---|-----------|-----------|-----------------|
| 0 | NONE      | scheduler falls into `__WFI` | wakeup-from-sleep latency (best case) |
| 1 | BUSY      | tight ALU loop, no PRIMASK changes | normal-running-code latency |
| 2 | CRIT_64   | `__disable_irq()` for ~64 cyc, loop | bimodal: small block, frequent firing |
| 3 | CRIT_256  | …256 cyc | wider tail |
| 4 | CRIT_1024 | …1024 cyc | worst-case `L_block` clearly visible |

The 1024-cycle case is the **schedulability bombshell** for a FOC current
loop running at 10 kHz on a 24 MHz core: 1024 cyc = 42 µs, which is already
40 % of the 100 µs PWM period. Any library function or driver routine that
takes a 1 ms global lock would single-handedly kill your control loop.

## 4. The expected numbers

Order-of-magnitude predictions (your measurements will confirm or refute):

| Mode      | min (cyc) | max (cyc) | shape |
|-----------|-----------|-----------|-------|
| NONE      | ~22       | ~30       | narrow peak — WFI wakeup is fast & deterministic |
| BUSY      | ~25       | ~35       | narrow peak shifted right (mid-instruction retire cost) |
| CRIT_64   | ~25       | ~95       | bimodal: bin 24-31 + bin 88-95 |
| CRIT_256  | ~25       | ~290      | bimodal with a long shelf to ~288 |
| CRIT_1024 | ~25       | ~1060     | most samples are outliers (`>=128`) |

If your numbers differ wildly, the candidates are: (a) different optimization
level changed how many cycles are in the BL trampoline, (b) flash wait states
got enabled at boot (24 MHz should be 0 WS — verify), (c) someone added an
unintended `__disable_irq` somewhere in the BSP.

## 5. FOC mapping (how to use the numbers)

Suppose you're building a 10 kHz FOC current loop:

```
PWM_PERIOD      = 24000 / 10 = 2400 cyc
ADC_DEADLINE    = 2400 cyc - whatever your handler itself takes
HEADROOM        = ADC_DEADLINE - worst-case L_total
```

With `worst-case L_total ≈ 30 cyc` (best case), you can afford to spend
**~2370 cyc** inside the handler — generous.

With `worst-case L_total ≈ 1060 cyc` (any 1024-cycle critical section in your
firmware), you have only **~1340 cyc** in the handler. A naive `printf`
already blows that.

**Rules that follow:**

1. **Never take a critical section longer than 1/3 of your tightest IRQ
   deadline.** This lab is the proof. Run mode 4, look at max, multiply.
2. **Move long-running work to a lower-priority task.** The IRQ should
   only sample, scale, and post; control law math can run from main if it
   fits, or from a `coop_task_t` if you want fixed-cadence semantics.
3. **For the few critical sections you can't avoid** (writing a shared
   16-bit field on a part with no atomic 32-bit single-cycle store), keep
   them under 16 cycles. Then they vanish into `L_pipeline`.
4. **The BL soft-vector trampoline costs you ~7 cyc forever.** If your
   FOC loop is so tight it can't pay 7 cycles, link the FOC firmware as
   a bare-metal image at 0x01000000 and drop the bootloader for that
   product variant.

## 6. UART shell

115200 8N1. Single-byte commands (no enter required):

| Cmd | Action |
|---|---|
| `0`..`4` | switch stressor |
| `s` | snapshot stats (min/max/avg/count/last) |
| `h` | render 16-bin histogram (8-cycle bins, 0..127) + outlier count |
| `r` | reset stats |
| `i` | show active stressor |
| `?` | help |

Typical session:

```
0         ← idle/WFI
r         ← reset
        … wait ~5 s …
s         ← see numbers for WFI baseline
h         ← see the narrow peak
4         ← switch to CRIT_1024
r         ← reset
        … wait ~5 s …
h         ← see the broad shelf reaching ~1060 cyc, all in the outlier bin
```

## 7. Build

Same recipe as `APP_LOG/`:

1. Copy `APP_COOP/TX32F01_COOP.uvprojx` → `APP_IRQ/TX32F01_IRQ.uvprojx`.
2. Replace source groups with: `COOP/coop_sched.c`,
   `IRQ/{lat_meas,lat_stress,lat_shell}.c`, `USER/main.c`.
3. Linker → Scatter file: `..\TX32F01_IRQ.sct`.
4. Include paths: `..\COOP`, `..\IRQ`, `..\..\APP_PATCH`,
   `..\..\Device\TX32F01\Include`, `..\..\Device\CMSIS\KEIL_CORE`.
5. Add `..\..\APP_PATCH\app_softvec.c`.
6. `fromelf --bin --output ./Objects/firmware.bin ./Objects/TX32F01.axf`.
7. OTA via `ymodem_send.ps1`.

## 8. Known limitations / future work

- **The trampoline is in the measurement.** This is realistic for *this BL*
  but not for a different bootloader. To isolate `L_arch + L_pipeline`,
  build as a bare-metal image and re-run.
- **Only SysTick is measured.** Real peripherals (UART RX, SPI, ADC) may
  have their own internal-event-to-NVIC-pending delay (typ. 1-2 cyc).
  Measure those separately by hooking your real peripheral's handler the
  same way (read SYST_CVR first thing).
- **Stress modes are coarse.** For a paper-quality study, sweep PRIMASK
  hold lengths in 16-cycle increments and look at where the max latency
  starts to track the hold length 1:1.
- **No flash-WS / no DMA contention scenarios.** TX32F01 lacks DMA and
  runs 0 WS at 24 MHz, so neither is interesting. Worth re-running this
  lab on any part where either is present.
