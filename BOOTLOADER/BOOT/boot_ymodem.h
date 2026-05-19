#ifndef _BOOT_YMODEM_H
#define _BOOT_YMODEM_H

#include "boot_layout.h"

/* 返回值：>0 = 成功收到的字节数（== app_size）；0 = 失败 */
uint32_t ymodem_recv_to_app(uint32_t *out_size);

#endif
