# APP_LOG — data-acquisition firmware on external SPI NOR

A tiny ring-log file system for the TX32F01 (Cortex-M0, 32 KB Flash, 4 KB SRAM)
that records sensor records to an external W25Q-family SPI NOR. Runs behind the
bootloader at `0x01002000`, OTA-upgradable via YMODEM.

## What's in the box

| File | Purpose | LoC (≈) |
|---|---|---|
| [LOG/spi_nor.c](LOG/spi_nor.c) | W25Q16 driver — sync read, async erase/program | 100 |
| [LOG/log_bd.c](LOG/log_bd.c)   | Block-device wrapper (sector/page addressing) | 60 |
| [LOG/log_ring.c](LOG/log_ring.c) | Append-only ring log, boot recovery, retention | 200 |
| [LOG/log_shell.c](LOG/log_shell.c) | UART command shell (`?` `s` `r` `c` `t`) | 130 |
| [USER/main.c](USER/main.c)     | COOP scheduler wiring (5 tasks) | 130 |
| [TX32F01_LOG.sct](TX32F01_LOG.sct) | Scatter: same as APP_COOP | 13 |

## Resource budget

| | Flash | SRAM |
|---|---|---|
| COOP scheduler | 2.0 KB | 0.6 KB |
| SPI NOR + block dev | 1.2 KB | 0 |
| Ring log + 256B page buf | 1.5 KB | 0.3 KB |
| UART shell | 0.5 KB | 0.1 KB |
| **Total** | **~5.2 KB / 22 KB** | **~1.0 KB / 4 KB** |

## On-flash format

Each record occupies exactly one 256-byte NOR page (one record per page):

```
offset  size  field
  0      4   magic     'LOGR' (0x4C4F4752 LE)
  4      4   seq       monotonic across the chip's lifetime
  8      4   ts_ms     coop_now_ms() at write time
 12      1   len       payload bytes, 0..240
 13      1   type      user-defined log class
 14      2   crc16     CCITT poly 0x1021, init 0xFFFF, over header[0..14)+payload[0..len)
 16    ≤240   payload
```

Sectors (4 KB = 16 pages = 16 records) used as a ring. When head fills, it
rolls onto the pre-erased next sector; tail advances on overflow.

## Capacity (W25Q16, 2 MB, 512 sectors)

- 16 records × 512 sectors = **8192 records** capacity.
- At 1 sample/sec → **~2.3 hours** retention before oldest overwritten.
- At 1 sample/min → **~5.7 days** retention.

## Boot recovery

Scans the first page of every sector for `magic == LOGR` (~25 ms at 3 MHz SCK
on a 2 MB chip). Picks the sector containing the highest sequence number, walks
its pages forward to find the last valid record → that's the write head.
Tail = sector with lowest sequence number. Survives unclean shutdown.

## UART shell

115200 8N1, single-character commands (no enter required):

| Cmd | Action |
|---|---|
| `?` | help |
| `s` | print JEDEC + sector count + head/tail/seq |
| `r` | dump all records (oldest → newest, paced one per tick) |
| `c` | clear log via chip-erase (~25-100 s) |
| `t` | inject a test record |

Any key during a dump aborts it.

## Build

1. In Keil, copy `APP_COOP/TX32F01_COOP.uvprojx` → `APP_LOG/TX32F01_LOG.uvprojx`.
2. Replace source groups:
   - `COOP/` → `APP_LOG/COOP/coop_sched.c`
   - `LOG/`  → `APP_LOG/LOG/{spi_nor,log_bd,log_ring,log_shell}.c`
   - `USER/` → `APP_LOG/USER/main.c`
3. Linker → Scatter file → `..\TX32F01_LOG.sct`.
4. C/C++ include paths:
   - `..\COOP`
   - `..\LOG`
   - `..\..\APP_PATCH`
   - `..\..\Device\TX32F01\Include`
   - `..\..\Device\CMSIS\KEIL_CORE`
5. Add `..\..\APP_PATCH\app_softvec.c` to the project (for `app_request_bootloader_update`).
6. Build, `fromelf --bin --output ./Objects/firmware.bin ./Objects/TX32F01.axf`.
7. OTA: `.\ymodem_send.ps1 -ComPort COMx -BinPath .\Objects\firmware.bin`.

## Wiring

| Signal | Pin |
|---|---|
| SPI CS | GPIO2.PIN04 (manual GPIO) |
| SPI CLK | GPIO2.PIN05 |
| SPI MOSI | GPIO3.PIN00 |
| SPI MISO | GPIO3.PIN01 |
| UART TX | GPIO3.PIN07 |
| UART RX | GPIO3.PIN06 |
| LED | GPIO0.PIN03 |

Identical to the vendor SPI flash demo — no board re-wiring needed.

## Known limitations (v1)

- Single-threaded (cooperative). `append()` and iteration share a 256 B page
  buffer; do not interleave them.
- SPI is polled (~300 KB/s @ 24 MHz SYSCLK / 8). Move to hardware DMA for
  sustained > ~5 KHz logging.
- `c` (chip erase) blocks until done; no progress indication. For a
  non-blocking variant, switch to per-sector erase looping in `log_ring_tick`.
- No timestamp wall-clock — `ts_ms` is uptime since boot. Persist a UTC base
  to your record payload if you need calendar time.
