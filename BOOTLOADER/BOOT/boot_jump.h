#ifndef _BOOT_JUMP_H
#define _BOOT_JUMP_H

#include "boot_layout.h"

/* SRAM 头部的软向量表，BL 和 APP 共用同一地址 */
extern volatile isr_t * const g_soft_vec;   /* 指向 SOFT_VECTOR_BASE */

BOOL bl_app_meta_valid(void);
void bl_jump_to_app(void);    /* 不会返回 */

#endif
