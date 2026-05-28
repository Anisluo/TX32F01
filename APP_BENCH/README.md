# APP_BENCH — TX32F01 CPU benchmarking

Two benchmarks in one project:

1. **Built-in `bench_core`** — runs immediately, no external dependencies.
   Reports cycles-per-iteration for q15 math, memcpy, bubble sort, CRC32,
   and a synthetic FOC inner loop. Use this to size your ADC ISR and to
   detect compiler/optimization regressions.

2. **EEMBC Coremark** — optional. Provides a single comparable score
   against other vendors' MCUs. Requires you to drop the Coremark
   sources into `APP_BENCH/COREMARK/` (license: read EEMBC's before
   redistributing).

## Files

```
APP_BENCH/
├── BENCH/
│   ├── bench_core.h      # API
│   └── bench_core.c      # built-in suite
├── PORT/
│   ├── core_portme.h     # Coremark port header (TX32F01)
│   └── core_portme.c     # SysTick-based monotonic timer + tiny printf
├── USER/
│   └── main.c            # entry: runs bench_core, then Coremark if enabled
├── TX32F01_BENCH.sct     # linker script (mirrors APP_FOC layout)
└── README.md
```

## Quick start (built-in suite only)

1. **Create Keil project** `TX32F01_BENCH.uvprojx` based on any existing APP
   project (copy APP_FOC's settings, then strip FOC sources).
2. **Sources to add**:
   - `APP_BENCH/USER/main.c`
   - `APP_BENCH/BENCH/bench_core.c`
   - `APP_FOC/MATH/q15_math.c`              ← bench depends on q15_math
   - `APP_PATCH/app_softvec.c`              ← BL compatibility glue
   - `Device/TX32F01/Source/ARM/startup_TX32F01.s`
   - HAL sources for SCU + UART + (nothing else)
3. **Scatter**: `APP_BENCH/TX32F01_BENCH.sct`
4. **Include paths**: `APP_BENCH/BENCH`, `APP_FOC/MATH`, `APP_PATCH`,
   plus the standard HAL include paths.
5. **Flash via YMODEM** (BL handles the OTA).
6. **Connect at 115200, 8N1**. After "[BENCH] APP_BENCH up" you should
   see a table of cycles/iter for each test.

## Adding Coremark

1. Clone `https://github.com/eembc/coremark`. Read `LICENSE.md`.
2. Copy these files into `APP_BENCH/COREMARK/`:
   - `core_main.c` (rename `main` → `coremark_main`; APP startup already
     defines a `main`)
   - `core_list_join.c`, `core_matrix.c`, `core_state.c`, `core_util.c`
   - `coremark.h`
3. In Keil: add the COREMARK group with the files above. Also add
   `APP_BENCH/PORT/core_portme.c`.
4. C/C++ → Define: add `COREMARK_ENABLED=1`. Optionally tune
   `ITERATIONS=400` (default).
5. Build. The map file should show free space if Coremark fits in 22 KB.
   If not, drop ITERATIONS or move `TOTAL_DATA_SIZE` to 1500.
6. Run. After the built-in bench you'll see Coremark's banner, then
   results in this form:

```
CoreMark 1.0 : <score> / armcc -O2 / STATIC
```

Compare against other Cortex-M0 results at https://www.eembc.org/coremark/.

## Expected ballpark numbers (24 MHz Cortex-M0, -O2)

These are *guideposts*, not promises — measure on your silicon:

| Test            | Expected cyc/iter |
|-----------------|------------------:|
| `q15_mul`       | 3 – 5             |
| `q15_sin_cos`   | 60 – 90           |
| `q15_sqrt_u32`  | 120 – 180         |
| `q15_atan2`     | 200 – 320         |
| `memcpy_256B`   | 400 – 550         |
| `bubble_sort_64`| 2200 – 2800       |
| `crc32_1KB`     | 280 – 350 μs      |
| `foc_inner`     | 250 – 400         |
| Coremark        | 30 – 40           |

If your `foc_inner` is < 2400 cyc/iter, the FOC ADC ISR fits inside a
100 μs PWM period with margin.

## Why a custom bench in addition to Coremark

Coremark gives you *one* number. It says nothing about whether your
particular hot path (the FOC ISR) will fit. `bench_core` is a per-op
microscope; Coremark is a thermometer. You want both.
