# APP_CAN — hand-written CAN 2.0A stack + multi-node time sync

A from-scratch software CAN MAC ("SoftCAN") on the TX32F01 (Cortex-M0
@ 24 MHz, no on-chip CAN), plus a CANopen/PTP-flavoured two-step time
synchronisation protocol on top of it.

## Why bit-bang CAN at all

The TX32F01 has UART, SPI, I²C, ADC, timers — but no CAN. To run a
CAN stack on this MCU we have to implement the entire MAC layer in
software over a single open-drain GPIO. This is not a production
substitute for a real CAN peripheral; it is a complete reference
implementation of the protocol and an honest playground for
multi-node timing experiments.

## What's implemented

- 11-bit standard frames, DLC 0..8, data and RTR
- Bit-stuffing (5+1) over SOF..CRC
- CRC-15 (polynomial 0x4599)
- 1-IRQ-per-bit, sample point at 75 % of bit time
- SOF capture via EXTI falling edge (used as timestamp source for sync)
- Arbitration with TX→RX downgrade on lost recessive bit
- Bit-monitoring (bit error during transmit) outside arbitration
- ACK slot honoured by listeners (drive dominant for second half)
- Stuff / form / CRC / ACK / bit error detection
- TEC / REC counters, error-active / passive / bus-off state
- Frame queue, RX callback, TX callback (used by the sync layer)
- 32-bit µs free-running clock from TIM2 (used by sync layer)

Out of scope:

- 29-bit extended IDs (IDE=1)
- Active error frame *emission* (we detect errors and bump counters
  but do not transmit the 6-bit dominant error flag)
- Automatic bus-off recovery (manual reset)

## Default pin map / wiring

Per node:

| Function | Pin               | Notes                                  |
| -------- | ----------------- | -------------------------------------- |
| CAN_TX   | GPIO1.PIN00       | Open-drain output (drives bus low)     |
| CAN_RX   | GPIO1.PIN01       | Input pull-up; EXTI line 1             |
| UART     | GPIO3.PIN06/PIN07 | Stats output @115200 8N1               |

Bus:

```
    3V3
     │
    4.7 kΩ        (one pull-up anywhere on the bus is enough)
     │
     ●─── node A CAN_TX ─┬── node A CAN_RX
     │                   │
     ●─── node B CAN_TX ─┴── node B CAN_RX
     │
    ...
```

All `CAN_TX` pins are tied together (wire-OR via open-drain). All
`CAN_RX` pins go to the same wire — the simplest layout is to
externally bridge `PIN00↔PIN01` on every node and connect all of
those nodes' joined-pair to one bus wire with one pull-up.

For a single-board sanity test, bridge `PIN00` and `PIN01` on one
board, leave `IS_MASTER = 1`, and the master will receive its own
frames back (self-ACK works because TX and RX share the same sample
window).

## Default timing

- Bitrate: **10 000 bit/s** (`SOFTCAN_BITRATE_DEFAULT`)
- Bit time: 100 µs = 2400 HCLK cycles @ 24 MHz
- Sample point: 75 % (6 of 8 time quanta)
- Per-bit ISR budget: ~2400 cycles; actual ISR work is well under 200

You can push to 20 kbit/s by changing the `bitrate` field in the
config; beyond that the per-bit ISR latency on the M0 starts to bite
into the sample window.

## Time sync protocol

The master broadcasts every `sync_period_ms` (default 100 ms):

1. **SYNC** — ID `0x080`, DLC 1, data[0] = wrap-around counter.
   The master records its local µs at the SOF of this frame.
2. **FUP** (Follow-Up) — ID `0x081`, DLC 8, data[0..3] = master µs
   at the SYNC SOF (little-endian uint32), data[4] = counter, rest
   zero. Sent on the next 1 ms tick after the SYNC TX-complete
   callback.

Each slave:

- On RX of SYNC, latches its local µs at the EXTI SOF edge.
- On RX of matching-counter FUP, applies the pair `(T_local, T_master)`
  to its virtual clock model:

  ```
  virtual(now) = anchor_virtual + (now − anchor_local) · (1 + skew_ppm·1e-6)
  ```

  On each accepted pair, the anchor is re-pinned to `(T_local, T_master)`
  and the skew is updated by a low-pass filter of the inter-event drift:

  ```
  inst_skew_ppm = (ΔT_master − ΔT_local) / ΔT_local · 1e6
  skew_ppm     += α · (inst_skew_ppm − skew_ppm),   α = 1/8
  ```

This is the same two-step time-message structure that PTP / AUTOSAR
Global Time use, and the same offset+skew servo SNTP uses. The split
into SYNC + FUP keeps the master's timestamp value independent of
in-frame transmission latency.

## Expected results

With two boards running this firmware at 24 MHz crystals (typical
TX32F01 spec ±50 ppm), after ~10 sync events the slave should report:

- `off=±10..30 µs` — residual error, dominated by EXTI ISR entry
  asymmetry between master TX SOF capture and slave RX SOF capture
- `skew=±50..150 ppm` — actual crystal mismatch
- `v` (virtual µs) staying within ~30 µs of the master's local µs
  between updates

If you scope the bus you should see SYNC and FUP frames every
100 ms; FUP follows SYNC by 1–2 ms (one main-loop tick).

## How to build

Copy the layout of `APP_IRQ` or `APP_RTOS` to make a new Keil project
`APP_CAN/TX32F01_CAN.uvprojx`:

- Include groups: `USER` (main.c), `CAN` (softcan.c, can_sync.c),
  `HAL_lib` (existing HAL .c files), `Startup`
- Include paths: `./USER`, `./CAN`, `../Device/TX32F01/Include`,
  `../Device/TX32F01/HAL_lib/inc`
- Link base: `0x01002000` (same as APP/APP_IRQ), so it boots from
  the existing bootloader

For a single-board test set `IS_MASTER = 1` in `main.c` and jumper
`PIN00↔PIN01`. For two-board sync, build one image with
`IS_MASTER = 1`, another with `IS_MASTER = 0`, flash and wire as
above. Watch the UART of the slave board.

## Files

- `CAN/softcan.h` — public API, frame type, stats type
- `CAN/softcan.c` — the bit-bang MAC
- `CAN/can_sync.h` — sync protocol API
- `CAN/can_sync.c` — master scheduler + slave clock servo
- `USER/main.c` — wiring, init, periodic UART stats
