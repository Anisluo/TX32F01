#include "log_shell.h"
#include "log_ring.h"
#include "fault_dump.h"
#include "TX32F01_periph.h"

/* TX helpers (mirrors APP_COOP/main.c style). */
static void putc1(char c)
{
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void puts1(const char *s) { while (*s) putc1(*s++); }
static void put_u32(uint32_t v)
{
    char b[11]; int n = 0;
    if (!v) { putc1('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putc1(b[n]);
}
static void put_hex32(uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    putc1('0'); putc1('x');
    for (int i = 7; i >= 0; i--) putc1(H[(v >> (i*4)) & 0xF]);
}

/* Non-blocking RX. Returns 1 + sets *out if a byte is waiting. */
static int try_getc(uint8_t *out)
{
    if (UART_GetFlagStatus(UART_RDNEIF) == 0) return 0;
    *out = (uint8_t)UART_ReceiveData();
    return 1;
}

/* ---------- dump state machine ----------
 * A `r` command can produce thousands of records. We emit one per tick so
 * the cooperative scheduler keeps running.
 */
static enum { SH_IDLE, SH_DUMPING } s_state;
static log_iter_t s_it;
static uint32_t   s_dumped;

static void cmd_help(void)
{
    puts1("commands:\r\n");
    puts1("  ?  help\r\n");
    puts1("  s  stats\r\n");
    puts1("  r  read all records\r\n");
    puts1("  c  clear log\r\n");
    puts1("  t  inject test record\r\n");
    puts1("  f  CRASH (invalid PC) - for fault_dump test\r\n");
    puts1("  F  CRASH (NULL fn ptr) - alternate fault\r\n");
    puts1("  b  print boot/fault counters\r\n");
}

extern uint32_t log_bd_jedec(void);

static void cmd_stats(void)
{
    uint32_t tot, hs, hp, ts, ns;
    log_ring_stats(&tot, &hs, &hp, &ts, &ns);
    puts1("[stats] jedec="); put_hex32(log_bd_jedec());
    puts1(" sectors=");      put_u32(tot);
    puts1(" head=");         put_u32(hs); putc1('.'); put_u32(hp);
    puts1(" tail=");         put_u32(ts);
    puts1(" next_seq=");     put_u32(ns);
    puts1("\r\n");
}

static void cmd_read_begin(void)
{
    log_ring_iter_begin(&s_it);
    s_dumped = 0;
    s_state  = SH_DUMPING;
    puts1("[read] begin\r\n");
}

static void dump_one(void)
{
    static log_record_t rec;
    int got = log_ring_iter_next(&s_it, &rec);
    if (!got) {
        puts1("[read] end (");
        put_u32(s_dumped);
        puts1(" records)\r\n");
        s_state = SH_IDLE;
        return;
    }

    puts1("seq="); put_u32(rec.seq);
    puts1(" ts="); put_u32(rec.ts_ms);
    puts1(" type="); put_u32(rec.type);
    puts1(" len="); put_u32(rec.len);
    puts1(" data=");
    /* print payload as hex (kept short — terminal-friendly) */
    static const char H[] = "0123456789ABCDEF";
    for (uint32_t i = 0; i < rec.len; i++) {
        putc1(H[rec.payload[i] >> 4]);
        putc1(H[rec.payload[i] & 0xF]);
    }
    puts1("\r\n");
    s_dumped++;
}

static void cmd_clear(void)
{
    puts1("[clear] chip erase started (~25-100s on W25Q16, append() blocked meanwhile)\r\n");
    log_ring_clear();
}

static void cmd_test(void)
{
    static uint8_t s_counter;
    uint8_t buf[8];
    for (int i = 0; i < 8; i++) buf[i] = (uint8_t)(s_counter + i);
    s_counter++;
    int rc = log_ring_append(0xAA, buf, 8);
    puts1("[test] rc="); put_u32((uint32_t)(int32_t)rc); puts1("\r\n");
}

void log_shell_init(void)
{
    s_state = SH_IDLE;
    puts1("\r\n[shell] ready — press ? for help\r\n");
}

void log_shell_tick(void)
{
    /* If a dump is in progress, emit one record per tick. New commands are
       still accepted (so the user can interrupt by sending another char,
       though we treat any byte during dump as "abort"). */
    if (s_state == SH_DUMPING) {
        uint8_t c;
        if (try_getc(&c)) {
            puts1("[read] aborted\r\n");
            s_state = SH_IDLE;
            return;
        }
        dump_one();
        return;
    }

    uint8_t c;
    if (!try_getc(&c)) return;

    switch (c) {
    case '?': case 'h': case 'H': cmd_help();       break;
    case 's': case 'S':           cmd_stats();      break;
    case 'r': case 'R':           cmd_read_begin(); break;
    case 'c': case 'C':           cmd_clear();      break;
    case 't': case 'T':           cmd_test();       break;
    case 'f':
        puts1("[fault] forcing HardFault via invalid PC...\r\n");
        fault_dump_trigger_test(FAULT_TEST_INVALID_PC);
        break;
    case 'F':
        puts1("[fault] forcing HardFault via NULL fn ptr...\r\n");
        fault_dump_trigger_test(FAULT_TEST_NULL_FN);
        break;
    case 'b': case 'B':
        puts1("[boot] boot_count=");  put_u32(fault_dump_boot_count());
        puts1(" fault_count=");        put_u32(fault_dump_fault_count());
        puts1("\r\n");
        break;
    case '\r': case '\n':         /* ignore line endings */          break;
    default:                       putc1('?'); putc1('\r'); putc1('\n'); break;
    }
}
