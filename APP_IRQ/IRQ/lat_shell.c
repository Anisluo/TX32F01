#include "lat_shell.h"
#include "lat_meas.h"
#include "lat_stress.h"
#include "TX32F01_periph.h"

static void putc1(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void puts1(const char *s) { while (*s) putc1(*s++); }
static void put_u32(uint32_t v) {
    char b[11]; int n = 0;
    if (!v) { putc1('0'); return; }
    while (v && n < 10) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putc1(b[n]);
}
static int try_getc(uint8_t *out) {
    if (UART_GetFlagStatus(UART_RDNEIF) == 0) return 0;
    *out = (uint8_t)UART_ReceiveData();
    return 1;
}

static void cmd_help(void)
{
    puts1("\r\n=== IRQ latency lab ===\r\n");
    puts1("  0..4  select stressor:\r\n");
    puts1("        0 NONE  (WFI)\r\n");
    puts1("        1 BUSY  (tight spin)\r\n");
    puts1("        2 CRIT_64\r\n");
    puts1("        3 CRIT_256\r\n");
    puts1("        4 CRIT_1024\r\n");
    puts1("  s     stats     h  histogram   r  reset   i  info   ?  help\r\n");
}

static void cmd_info(void)
{
    puts1("[info] stressor=");
    puts1(lat_stress_name(lat_stress_get()));
    puts1("\r\n");
}

static void cmd_stats(void)
{
    lat_stats_t s;
    lat_meas_snapshot(&s);
    if (s.count == 0) { puts1("[stats] (no samples)\r\n"); return; }

    /* avg = sum / count.  64-bit sum / 32-bit count → 32-bit result while count fits. */
    uint32_t avg_cyc = (uint32_t)(s.sum / s.count);

    puts1("[stats] n=");      put_u32(s.count);
    puts1("  min=");          put_u32(s.min);
    puts1("  max=");          put_u32(s.max);
    puts1("  avg=");          put_u32(avg_cyc);
    puts1("  last=");         put_u32(s.last);
    puts1("  out=");          put_u32(s.outliers);
    puts1(" cyc\r\n");
}

/* Print a poor-man's bar chart.  Each '#' = ceil(N/scale).
   Scale chosen so the tallest bin has ~32 chars. */
static void cmd_histogram(void)
{
    lat_stats_t s;
    lat_meas_snapshot(&s);
    if (s.count == 0) { puts1("[hist] (no samples)\r\n"); return; }

    uint32_t peak = 1;
    for (uint32_t i = 0; i < LAT_BUCKETS; i++) if (s.bucket[i] > peak) peak = s.bucket[i];
    if (s.outliers > peak) peak = s.outliers;
    uint32_t scale = (peak + 31U) / 32U;
    if (scale == 0) scale = 1;

    puts1("[hist] (each # = "); put_u32(scale); puts1(" samples)\r\n");
    for (uint32_t i = 0; i < LAT_BUCKETS; i++) {
        uint32_t lo = i * LAT_BUCKET_WIDTH;
        /* line header: " 24- 31 |  ###### 123" */
        putc1(' ');
        if (lo < 100) putc1(' ');
        if (lo < 10)  putc1(' ');
        put_u32(lo);
        putc1('-');
        uint32_t hi = lo + LAT_BUCKET_WIDTH - 1U;
        if (hi < 100) putc1(' ');
        if (hi < 10)  putc1(' ');
        put_u32(hi);
        puts1(" | ");
        uint32_t bars = (s.bucket[i] + scale - 1U) / scale;
        for (uint32_t b = 0; b < bars; b++) putc1('#');
        for (uint32_t b = bars; b < 32U; b++) putc1(' ');
        putc1(' ');
        put_u32(s.bucket[i]);
        puts1("\r\n");
    }
    puts1("   >=128 | ");
    uint32_t obars = (s.outliers + scale - 1U) / scale;
    for (uint32_t b = 0; b < obars; b++) putc1('#');
    for (uint32_t b = obars; b < 32U; b++) putc1(' ');
    putc1(' ');
    put_u32(s.outliers);
    puts1("\r\n");
}

void lat_shell_init(void)
{
    puts1("\r\n[lab] IRQ-latency lab ready — press ? for help\r\n");
}

void lat_shell_tick(void)
{
    uint8_t c;
    if (!try_getc(&c)) return;
    switch (c) {
    case '0': lat_stress_set(STRESS_NONE);      cmd_info(); break;
    case '1': lat_stress_set(STRESS_BUSY);      cmd_info(); break;
    case '2': lat_stress_set(STRESS_CRIT_64);   cmd_info(); break;
    case '3': lat_stress_set(STRESS_CRIT_256);  cmd_info(); break;
    case '4': lat_stress_set(STRESS_CRIT_1024); cmd_info(); break;
    case 's': case 'S': cmd_stats();      break;
    case 'h': case 'H': cmd_histogram();  break;
    case 'r': case 'R': lat_meas_reset(); puts1("[reset]\r\n"); break;
    case 'i': case 'I': cmd_info();       break;
    case '?':           cmd_help();       break;
    case '\r': case '\n': break;
    default: putc1('?'); puts1("\r\n"); break;
    }
}
