/*
 * lat_shell.h — UART command surface for the IRQ-latency lab.
 *
 * Commands (single byte, no newline):
 *   0..4   select stressor (see lat_stress.h)
 *   s      print stats (min/max/avg/last, count)
 *   h      print histogram (16 × 8-cycle bins, 0..127 + outliers)
 *   r      reset stats
 *   i      info: which stressor is active, RVR
 *   ?      help
 */
#ifndef _LAT_SHELL_H
#define _LAT_SHELL_H

void lat_shell_init(void);
void lat_shell_tick(void);

#endif
