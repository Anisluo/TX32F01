#include "foc_shell.h"
#include "foc_loop.h"
#include "foc_pwm.h"
#include "foc_adc.h"
#include "foc_hall.h"
#include "fault_dump.h"
#include "TX32F01_periph.h"

/* Tiny non-blocking UART shell.  No malloc, no scanf.  Commands:
 *
 *   idle             — go to safe state
 *   align            — apply Id at θ=0 for FOC_ALIGN_MS
 *   vf <Hz>          — open-loop V/F at <Hz> electrical
 *   foc <rpm>        — closed-loop sensored FOC
 *   stop             — force PWM safe (BDTR)
 *   tel              — toggle 100 ms telemetry print
 *   ?                — help
 */

static void putc_(char c) {
    UART_ClearFlag(UART_TCIF);
    UART_SendData((uint16_t)c);
    while (UART_GetFlagStatus(UART_TCIF) == 0) { }
}
static void puts_(const char *s) { while (*s) putc_(*s++); }
static void put_i(int32_t v) {
    if (v < 0) { putc_('-'); v = -v; }
    char b[11]; int n = 0;
    if (v == 0) { putc_('0'); return; }
    while (v && n < 11) { b[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) putc_((unsigned char)b[n]);
}
static void put_hex4(uint16_t v) {
    static const char *H = "0123456789ABCDEF";
    putc_(H[(v>>12)&0xF]); putc_(H[(v>>8)&0xF]);
    putc_(H[(v>>4)&0xF]);  putc_(H[v&0xF]);
}

#define BUF_N 32
static char     s_buf[BUF_N];
static uint8_t  s_buf_len;
static uint8_t  s_telemetry_on = 1;
static uint16_t s_tel_ms;

static int strn_eq(const char *a, const char *b, uint16_t n)
{
    for (uint16_t i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

static uint32_t parse_u32(const char *s)
{
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (uint32_t)(*s - '0'); ++s; }
    return v;
}

static void print_help(void)
{
    puts_("\r\n  idle | align | vf <Hz> | foc <rpm> | stop | tel"
          "\r\n  fault           — show boot/fault counters"
          "\r\n  crash [0|1|2]   — trigger HardFault (0=badPC, 1=NULL, 2=badLDR)"
          "\r\n  ?               — this help\r\n> ");
}

/* Local putc adapter for fault_dump_drain_to_uart. */
static void shell_putc_for_dump(char c) { putc_(c); }

static void exec_line(void)
{
    s_buf[s_buf_len] = '\0';
    if (s_buf_len == 0) { puts_("> "); return; }

    if (strn_eq(s_buf, "idle", 4))         foc_loop_set_mode(FOC_MODE_IDLE);
    else if (strn_eq(s_buf, "align", 5))   foc_loop_set_mode(FOC_MODE_ALIGN);
    else if (strn_eq(s_buf, "vf ", 3)) {
        foc_loop_set_vf((uint16_t)parse_u32(s_buf + 3));
        foc_loop_set_mode(FOC_MODE_VF_OPEN);
    }
    else if (strn_eq(s_buf, "foc ", 4)) {
        foc_loop_set_speed_ref((int16_t)parse_u32(s_buf + 4));
        foc_loop_set_mode(FOC_MODE_FOC_CLOSED);
    }
    else if (strn_eq(s_buf, "stop", 4))    foc_pwm_force_safe();
    else if (strn_eq(s_buf, "tel", 3))     s_telemetry_on ^= 1;
    else if (strn_eq(s_buf, "fault", 5)) {
        puts_("\r\nboot_count="); put_i((int32_t)fault_dump_boot_count());
        puts_(" fault_count=");   put_i((int32_t)fault_dump_fault_count());
        if (fault_dump_get()) {
            puts_("\r\npending record:\r\n");
            fault_dump_drain_to_uart(shell_putc_for_dump);
        } else {
            puts_("\r\nno pending record.");
        }
    }
    else if (strn_eq(s_buf, "crash", 5)) {
        /* Safety: gate the motor before we deliberately fault, so the
         * power stage is in BDTR-safe state when the CPU reboots. */
        foc_pwm_force_safe();
        uint32_t k = (s_buf_len > 6) ? parse_u32(s_buf + 6) : 0U;
        puts_("\r\ncrashing... ");
        fault_dump_trigger_test((fault_test_kind_t)(k <= 2U ? k : 0U));
        /* unreachable */
    }
    else                                    print_help();

    puts_("\r\n> ");
}

void foc_shell_init(void)
{
    s_buf_len = 0;
    puts_("\r\n[FOC] ready.  type ? for help.\r\n> ");
}

void foc_shell_tick(void)
{
    /* Pull characters out of UART without blocking. */
    if (UART_GetFlagStatus(UART_RDNEIF)) {
        char c = (char)UART_ReceiveData();
        if (c == '\r' || c == '\n') {
            putc_('\r'); putc_('\n');
            exec_line();
            s_buf_len = 0;
        } else if ((c == 8 || c == 127) && s_buf_len > 0) {
            s_buf_len--;
            puts_("\b \b");
        } else if (s_buf_len < BUF_N - 1) {
            s_buf[s_buf_len++] = c;
            putc_(c);
        }
    }

    /* 100 ms telemetry */
    if (++s_tel_ms >= 100 && s_telemetry_on) {
        s_tel_ms = 0;
        puts_("\r[T] m=");  put_i((int32_t)g_foc.mode);
        puts_(" id=");      put_i(g_foc.id_meas);
        puts_(" iq=");      put_i(g_foc.iq_meas);
        puts_(" vd=");      put_i(g_foc.vd_q15);
        puts_(" vq=");      put_i(g_foc.vq_q15);
        puts_(" vb=");      put_i(g_foc.vbus_raw);
        puts_(" h=");       put_i(g_foc.hall_code);
        puts_(" th=0x");    put_hex4(g_foc.angle);
        puts_(" cyc=");     put_i(g_foc.max_loop_cyc);
        puts_("        \r\n> ");
        g_foc.max_loop_cyc = 0;
    }
}
