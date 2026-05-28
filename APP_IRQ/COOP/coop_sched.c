#include "coop_sched.h"
#include "TX32F01_periph.h"

static coop_task_t *s_tasks;
static uint8_t      s_ntasks;
static volatile uint32_t s_ms;

/* --- CPU load measurement ---
 * 在 SysTick 中每 1 s 计算上一秒里 idle (WFI) 占了多少 ms。
 * 100 - idle_ms 即是负载百分比。统计粒度 1%。
 */
static volatile uint32_t s_idle_ms_acc;          /* 累加这 1 s 内的 idle ms */
static volatile uint8_t  s_last_load_pct;
static volatile uint8_t  s_in_wfi;               /* 由 coop_run 主循环维护 */
static volatile uint16_t s_load_window_ms;

void coop_init(coop_task_t *tasks, uint8_t ntasks)
{
    s_tasks = tasks;
    s_ntasks = ntasks;
    for (uint8_t i = 0; i < ntasks; i++) {
        tasks[i].next_run_ms = 0;
        tasks[i].run_count   = 0;
    }
}

void coop_tick(void)
{
    s_ms++;
    if (s_in_wfi) s_idle_ms_acc++;
    if (++s_load_window_ms >= 1000) {
        s_load_window_ms = 0;
        uint32_t idle = s_idle_ms_acc;
        s_idle_ms_acc = 0;
        if (idle > 1000) idle = 1000;
        s_last_load_pct = (uint8_t)((1000 - idle) / 10);
    }
}

uint32_t coop_now_ms(void)
{
    /* 读 32-bit 字在 CM0 上对齐时是原子的，无需关中断 */
    return s_ms;
}

uint8_t coop_get_cpu_load(void)
{
    return s_last_load_pct;
}

void coop_run(void)
{
    for (;;) {
        uint32_t now = s_ms;
        uint8_t  ran_any = 0;

        for (uint8_t i = 0; i < s_ntasks; i++) {
            /* 用 signed 减法处理 32-bit 回卷（49 天后）。
               (int32_t)(now - next) >= 0 表示该跑了。*/
            if ((int32_t)(now - s_tasks[i].next_run_ms) >= 0) {
                /* 固定节拍：下次时间 = 本次 deadline + period
                   如果系统被长时间打断错过了多个 deadline，
                   这里只补一次，避免"风暴执行"。*/
                s_tasks[i].next_run_ms = now + s_tasks[i].period_ms;
                s_tasks[i].run_count++;
                s_tasks[i].func();
                ran_any = 1;
            }
        }

        if (!ran_any) {
            /* 这一轮没活干，进 WFI 等下次 SysTick。
               不关 IRQ：CM0 的 WFI 会被 pending 中断立即唤醒，
               这样 SysTick ISR 能在 WFI 醒来后立刻执行并看到 s_in_wfi=1。
               极小的 race：中断恰好在 s_in_wfi=1 之前进，则丢一次 tick，可接受。*/
            s_in_wfi = 1;
            __WFI();
            s_in_wfi = 0;
        }
    }
}
