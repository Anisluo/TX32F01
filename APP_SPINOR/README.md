# APP_SPINOR — TX32F01 as a W25Qxx-compatible SPI NOR Flash

A standalone firmware that turns this $0.30 Cortex-M0 into a 24 KB
SPI NOR Flash device. A host SPI master (PC + USB-SPI dongle, another
MCU, an FPGA) sees it as a generic W25Q-class chip:

- Standard JEDEC ID query (`0x9F`) returns `0xEF 0x40 0x0F`
- Standard READ (`0x03`) / FAST_READ (`0x0B`) at any 24-bit address
- Standard PAGE_PROGRAM (`0x02`) for 1–256 byte writes
- 4 KB SECTOR_ERASE (`0x20`), 32 KB / 64 KB block erase, full CHIP_ERASE
- Write-enable / disable (`0x06` / `0x04`), status register (`0x05`)
- 1 ms tick + UART diagnostic stream so a human can watch traffic

No bootloader. The APP owns the reset vector at `0x01000000`.

## Why this matters

Every demo MCU project uses the chip as the *master* talking to a NOR
Flash slave. Doing it the other way around — emulating the *slave* —
forces you to internalise the exact protocol of every JEDEC SPI NOR
device on the market. The same code structure becomes a reusable
toolkit:

- replace a discontinued SPI NOR with this MCU (drop-in compatible)
- add encryption, audit logs, ACLs, hardware-bound device identity
- make a "smart" storage chip that runs a state machine on each access
- prototype a host driver without burning real chips during dev

## Pin map

```
                          TX32F01
                       +-----------+
       host_CS    ---->| GPIO2.03  |   SPI_CS    (HW NSS + EXTI3 frame-end)
       host_CLK   ---->| GPIO3.03  |   SPI_CLK
       host_MOSI  ---->| GPIO3.04  |   SPI_MOSI
       host_MISO  <----| GPIO3.05  |   SPI_MISO
       GND        ====|  GND       |
                       |           |
       UART_TX    <----| GPIO3.07  |   diagnostic out @ 115200 8N1
       UART_RX    ---->| GPIO3.06  |   (unused but available)
                       +-----------+

       SWDIO  /  SWCLK   on GPIO0.PIN00 / PIN01  (debug, untouched)
```

## File layout

```
APP_SPINOR/
├── USER/main.c               entry + init + 1 s stats heartbeat
├── SPINOR/
│   ├── spinor_protocol.h     command opcodes, status bits, JEDEC ID, sizing
│   ├── spinor_emu.h/.c       protocol state machine, byte/frame callbacks
│   ├── spinor_storage.h/.c   read/program/erase backing into internal Flash
├── BSP/
│   ├── bsp_spi_slave.h/.c    SPI peripheral + CS EXTI wiring
│   ├── bsp_uart.h/.c         115200 8N1 diagnostic UART
├── TX32F01_SPINOR.sct        scatter: 6 KB code @ 0x01000000, 24 KB storage @ 0x01001800
├── TX32F01_SPINOR.uvprojx    Keil project
└── README.md
```

## Memory layout

```
internal Flash (32 KB):
  0x01000000 .. 0x010017FF   firmware code           (6 KB)
  0x01001800 .. 0x010077FF   host-visible NOR        (24 KB, 48 internal sectors)
  0x01007800 .. 0x01007FFF   reserved (config / future use)

internal SRAM (4 KB):
  0x20000000 .. 0x20000FFF   normal RW/ZI + stack
                              (no soft-vector reservation — standalone, no BL)
```

## How the protocol layer works

The SPI ISR processes the host's byte stream as it arrives and the
EXTI ISR on CS rising-edge ends each frame. Slow operations (program,
erase) are queued from the ISRs and executed in the main loop, while
the SPI side keeps responding to status reads (so the host's polling
loop on BUSY works correctly).

```
ISR path                                          main loop
========                                          =========
SPI byte
  |
  v
+----- snf_emu_byte(rx) ----+
|  STATE_IDLE -> opcode in  |
|  decode + dispatch        |
|  STATE_ADDR{1,2,3}        |
|  STATE_DUMMY              |
|  STATE_READ_DATA          |
|  STATE_PROGRAM_DATA       |
|  STATE_DONE               |
+-----+---------------------+
      |  return next-byte response
      v
   SPI TX FIFO  -> host MISO

CS rising edge
  |
  v
+----- snf_emu_frame_end() -+
|  if program: latch +      |
|    SR1.BUSY=1, pending=1  |
|  reset state              |
+---------------------------+
                                                   snf_emu_tick():
                                                     if pgm_pending:
                                                        Flash program
                                                        clear BUSY+WEL
                                                     if erase_pending:
                                                        Flash erase 4K / chip
                                                        clear BUSY+WEL
```

The hot path is short:

- byte arrives, ISR consumes it, calls `snf_emu_byte()`, gets a return byte
- return byte goes into TX FIFO for the *next* clocked transfer
- this works because the host's response byte for clock N is actually
  shifted in during clock N — we have a one-byte window to load it
  after seeing rx[N-1]. The SPI hardware's 3-deep FIFO further
  cushions ISR jitter.

## Implementation choices, and why

| choice | rationale |
|---|---|
| SPI mode 0 (CPOL=0, CPHA=0) | most common for SPI NOR; mode 3 is symmetrical and easy to switch to later |
| HW NSS + EXTI3 on CS pin | the SPI peripheral handles per-byte slave-select; EXTI gives us frame-end |
| ISR consumes RX in a tight loop | the 3-deep FIFO + lowest divider means we can target up to ~1 MHz SPI clock without losing bytes |
| Page-program staging buffer (256 B in SRAM) | latch host bytes in ISR, write Flash in main loop; lets status polling keep working during the slow write |
| Erase deferred to main loop | a single internal sector erase is ~5 ms; we have 8 per emulated sector so the whole thing is ~40 ms — host polls BUSY during that |
| WEL auto-clears on prog/erase complete | matches real SPI NOR semantics |
| Reads beyond 24 KB return 0xFF | matches real "erased" NOR, won't fool a host that probes capacity |
| No bootloader | makes the APP standalone — flash and go, no chained boot |

## Building

1. Open `APP_SPINOR/TX32F01_SPINOR.uvprojx` in Keil MDK.
2. Verify Options → Debug → Settings → Flash Download:
   - Erase: **Erase Sectors** (NOT full chip)
   - Programming Algorithm: start `0x01000000`, size `0x1800` (code only)
   - Reset and Run: checked
3. Project → Rebuild All → Flash → Download.
4. Open a serial console at **115200 8N1**, reset the board.

You should see:

```
============================================
  TX32F01 SPI NOR Flash Emulator
  Emulating W25Q-class device, 24 KB backing
  JEDEC ID: 0xEF 0x40 0x0F
  Pins: CS=GPIO2.03 CLK=GPIO3.03 MO=GPIO3.04 MI=GPIO3.05
============================================
[SPINOR] ready. waiting for host SPI activity...
[SPINOR] up=1000ms  rx=0 cmds=0 prog=0 eras=0 sr1=0x00
[SPINOR] up=2000ms  rx=0 cmds=0 prog=0 eras=0 sr1=0x00
```

Stats stay at zero until a host clocks anything in.

## Host-side test plan

### Minimal: detect the chip

Wire to any USB-to-SPI adapter (Bus Pirate, FTDI MPSSE, Aardvark, etc.).
On a Linux PC with `flashrom`:

```sh
flashrom -p ft2232_spi:type=2232H,port=A,divisor=64 --probe
```

Expected: flashrom reports a Winbond device with capacity `2^15 = 32 KB`.
(Our backing is 24 KB; the upper 8 KB reads as `0xFF` which still
passes the chip-detect path of most tools.)

### Full round-trip

```sh
# Read entire emulated chip
flashrom -p ft2232_spi:... -r dump.bin

# Write a 24 KB pattern and read back
dd if=/dev/urandom of=test.bin bs=24K count=1
flashrom -p ft2232_spi:... -w test.bin -N

flashrom -p ft2232_spi:... -r verify.bin
diff -q test.bin verify.bin
```

If `diff` is silent, every layer worked: erase, program, verify-read,
chip-erase between iterations, status polling.

### Watching the UART side

While `flashrom` runs, the emulator UART prints one line per second:

```
[SPINOR] up=12000ms  rx=98304 cmds=384 prog=96 eras=6 sr1=0x00
                       ^      ^       ^       ^      ^
                       |      |       |       |      `--- last status byte handed to host
                       |      |       |       `---------- 6 sector erases issued by host
                       |      |       `------------------ 96 page-program commands
                       |      `-------------------------- 384 distinct SPI frames
                       `--------------------------------- 96 KB of bytes clocked through
```

Numbers should add up: `prog * 256 + sector_overheads ≈ rx`.

## Limitations / known gaps

- **Max host SPI clock ~1 MHz.** Above that, the byte-at-a-time ISR
  falls behind. Fix: switch to DMA-driven SPI with circular RX buffer.
- **Capacity is 24 KB, not a power of 2.** Tools that compute sector
  count from JEDEC capacity byte get 8 sectors and probe `0x6000..0x7FFF`;
  we serve that as `0xFF`. Most tools tolerate it; some flag a warning.
- **No deep power-down power saving.** `0xB9` is acknowledged but the
  MCU just keeps running — adding actual STOP mode here is a follow-up.
- **No write-protect enforcement.** The BP/SEC/TB bits in SR1 are
  preserved across commands but we don't currently check them before
  program/erase. Adding the check is ~10 lines once the protection
  matrix is decided.
- **No SFDP (cmd 0x5A) support.** Modern hosts use SFDP for
  capability discovery instead of (or in addition to) JEDEC ID. Adding
  an SFDP table is a clean follow-up; one ~256-byte table covers it.

## Next steps (good follow-ups for this project)

1. **SFDP support** — add a `0x5A` handler returning a synthesized
   capability table; required by some Linux drivers and dev tools.
2. **Encryption-at-rest** — AES-CTR on program / read, key derived
   from chip die-ID. Host sees plaintext, anyone who desolders the
   MCU and reads its Flash sees ciphertext.
3. **Write counter / audit log** — every program operation records
   `(address, length, monotonic timestamp)` to a hidden region of
   internal Flash. Forensic value for tamper-evident devices.
4. **DMA + higher clock rate** — replace the per-byte ISR with DMA
   circular RX, target 8–12 MHz SPI clock.
5. **Multiple bank emulation** — respond to two CS lines, hand each
   their own backing region. Turns a single MCU into a "dual-chip"
   NOR with one cable.

Each of those is a clean ~300-line addition.
