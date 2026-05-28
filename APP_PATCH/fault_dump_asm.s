;------------------------------------------------------------------------------
; fault_dump_asm.s — Cortex-M0 HardFault entry trampoline.
;
;   The bootloader's HardFault_Handler reads SRAM[soft_vec + 4] (entry 1 =
;   HARDFAULT) and BX's here. R4-R11 and LR=EXC_RETURN are assumed to be
;   preserved across the BL trampoline (which is a minimal LDR/BX pair).
;
;   This trampoline:
;     1. Spills R4-R11 to the front of the NOINIT region (0x20000F00).
;        (CM0 STMIA only takes low regs, so it's done in two passes.)
;     2. Selects the offending stack pointer from EXC_RETURN bit2.
;     3. Calls fault_dump_c_part(stacked_sp, exc_return).
;
;   The C handler triggers NVIC_SystemReset() and never returns. If it
;   somehow does, we hang so we don't silently fall through.
;------------------------------------------------------------------------------

                AREA    |.text|, CODE, READONLY
                PRESERVE8
                THUMB

                IMPORT  fault_dump_c_part
                EXPORT  fault_dump_hardfault_entry

fault_dump_hardfault_entry  PROC

                ; --- spill R4-R11 to 0x20000F00 (matches offsetof(fault_record_t, r4)==0) ---
                LDR     R0, =0x20000F00
                STMIA   R0!, {R4-R7}            ; r4..r7
                MOV     R4, R8
                MOV     R5, R9
                MOV     R6, R10
                MOV     R7, R11
                STMIA   R0!, {R4-R7}            ; r8..r11

                ; --- pick stacked SP from EXC_RETURN bit 2 (1=PSP, 0=MSP) ---
                MOV     R1, LR                  ; R1 = EXC_RETURN  (preserve before BL clobbers LR)
                MOVS    R0, #4
                TST     R0, R1
                BNE     fd_use_psp
                MRS     R0, MSP
                B       fd_call
fd_use_psp
                MRS     R0, PSP
fd_call
                ; R0 = stacked frame pointer
                ; R1 = EXC_RETURN
                BL      fault_dump_c_part

fd_hang         B       fd_hang
                ALIGN   4                       ; silence A1581W: 4-byte align before END
                ENDP

                END
