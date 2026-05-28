/*
 * log_shell.h — UART command shell for inspecting / managing the log.
 *
 * Commands (single byte, no echo, no line buffering):
 *     ?    — print help
 *     s    — print stats (jedec, sectors, head, tail, next seq)
 *     r    — dump all records (oldest → newest), one per line
 *     c    — clear log
 *     t    — inject a test record (incrementing payload)
 *
 * Dump is spread across ticks (one record per call) so the COOP scheduler
 * isn't starved.
 */
#ifndef _LOG_SHELL_H
#define _LOG_SHELL_H

void log_shell_init(void);
void log_shell_tick(void);

#endif
