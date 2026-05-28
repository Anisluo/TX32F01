#ifndef APP_FOC_FOC_SHELL_H
#define APP_FOC_FOC_SHELL_H

#include <stdint.h>

void foc_shell_init(void);
void foc_shell_tick(void);      /* call from main loop ~100 Hz */

#endif
