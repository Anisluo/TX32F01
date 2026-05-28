# APP_FOC/MATH — Q15 fixed-point math for TX32F01

Drop-in fixed-point math primitives for the FOC inner loop. Cortex-M0 has no
FPU and no hardware divider, so everything here is integer + LUT.

## Files

| File | What it contains |
|---|---|
| `q15_math.h` | Public API: `q15_t`, `q15_mul`, `q15_sin_cos`, `q15_sqrt_u32`, `q15_atan2` |
| `q15_math.c` | 65-entry quarter-wave sine LUT (130 B), 16-entry CORDIC table (32 B) |

ROM cost: ~600 B code + ~180 B tables. SRAM cost: 0.

## Number formats

- `q15_t` — signed Q1.15 in an `int16_t`. `+1.0 ≈ 32767`.
- angle — `uint16_t` per full electrical revolution. Wraps modularly so
  `angle += step` is the only thing you ever need to do.

## Integrating into APP_FOC (Keil)

1. `Project → Add Group` → `MATH`.
2. Add `q15_math.c` to the group.
3. `Options for Target → C/C++ → Include Paths`: append `..\MATH`.
4. Include `q15_math.h` from any `foc_*.c` file that needs it.

## Validation plan

Before trusting this in a control loop:

1. Single-step compare against `float` sin/cos at 256 angles, log max error.
2. With the motor off, drive `q15_sin_cos` from a Timer ISR at 20 kHz and
   verify the output via DAC or PWM filter — should look clean on a scope.
3. Measure `q15_sin_cos` cycle count via SysTick CVR before/after a call.
   Budget: < 100 cycles. If it's higher, check that Keil is using `-O2`
   and inlining the `static __inline` helpers.

## Known limitations

- `q15_mul` does not saturate. Use `q15_mul_sat` when operands can produce
  `|result| > 1.0` (e.g. controller outputs before clipping).
- `q15_atan2` accuracy degrades when `|x|` or `|y|` exceeds ~2^29 because
  the CORDIC shifts saturate. Pre-scale large inputs.
- The sine LUT linear interp has worst-case error ~1.5 LSB near zero
  crossings. For high-accuracy applications, switch to a 257-entry table.
