;//-------- <<< Use Configuration Wizard in Context Menu >>> ------------------
;*/


; <h> Stack Configuration
;   <o> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>

Stack_Size      EQU     0x00000200

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp


; <h> Heap Configuration
;   <o>  Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>

Heap_Size       EQU     0x00000000

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit


                PRESERVE8
                THUMB


; Vector Table Mapped to Address 0 at Reset

                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp              ; Top of Stack
                DCD     Reset_Handler             ; Reset Handler
                DCD     NMI_Handler               ; NMI Handler
                DCD     HardFault_Handler         ; Hard Fault Handler
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     SVC_Handler               ; SVCall Handler
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     PendSV_Handler            ; PendSV Handler
                DCD     SysTick_Handler           ; SysTick Handler

                ; External Interrupts
                DCD     EXTI9_IWDT_Handler   	  ;  0 
                DCD     PVD_Handler           	  ;  1
                DCD     FLASH_Handler         	  ;  2
                DCD     EXTI0_Handler         	  ;  3
                DCD     EXTI1_Handler         	  ;  4
                DCD     EXTI2_Handler         	  ;  5
                DCD     EXTI3_Handler             ;  6
                DCD     EXTI4_Handler         	  ;  7
                DCD     EXTI5_Handler         	  ;	 8
                DCD     EXTI6_Handler         	  ;  9
                DCD     EXTI7_Handler        	  ; 10
                DCD     ADC_Handler               ; 11
                DCD     TIMER0_Handler            ; 12
                DCD     TIMER1_Handler            ; 13
                DCD     TIMER2_Handler            ; 14
                DCD     UART_Handler              ; 15
                DCD     I2C_Handler               ; 16
			    DCD     SPI_Handler               ; 17
__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY


; Reset Handler

;调试reset_handler之后的入口函数
Reset_Handler    PROC
                 EXPORT  Reset_Handler             [WEAK]
     IMPORT  __main
                 LDR     R0, =__main
                 BX      R0
                 ENDP

;Reset_Handler PROC
	;EXPORT Reset_Handler		[WEAK]
	;IMPORT __mymain
	;LDR R0,=__mymain
	;BX R0
	;ENDP



; Dummy Exception Handlers (infinite loops which can be modified)

NMI_Handler     PROC
                EXPORT  NMI_Handler               [WEAK]
                B       .
                ENDP
HardFault_Handler\
                PROC
                EXPORT  HardFault_Handler         [WEAK]
                B       .
                ENDP
SVC_Handler     PROC
                EXPORT  SVC_Handler               [WEAK]
                B       .
                ENDP
PendSV_Handler  PROC
                EXPORT  PendSV_Handler            [WEAK]
                B       .
                ENDP
SysTick_Handler PROC
                EXPORT  SysTick_Handler           [WEAK]
                B       .
                ENDP

Default_Handler PROC

                EXPORT  EXTI9_IWDT_Handler        [WEAK]
                EXPORT  PVD_Handler               [WEAK]
                EXPORT  FLASH_Handler             [WEAK]
                EXPORT  EXTI0_Handler             [WEAK]
                EXPORT  EXTI1_Handler             [WEAK]
                EXPORT  EXTI2_Handler             [WEAK]
                EXPORT  EXTI3_Handler             [WEAK]
                EXPORT  EXTI4_Handler             [WEAK]
                EXPORT  EXTI5_Handler             [WEAK]
                EXPORT  EXTI6_Handler             [WEAK]
                EXPORT  EXTI7_Handler             [WEAK]
                EXPORT  ADC_Handler               [WEAK]
                EXPORT  TIMER0_Handler            [WEAK]
                EXPORT  TIMER1_Handler            [WEAK]
                EXPORT  TIMER2_Handler            [WEAK]
                EXPORT  UART_Handler              [WEAK]
                EXPORT  I2C_Handler               [WEAK]
				EXPORT  SPI_Handler               [WEAK]
                

EXTI9_IWDT_Handler
PVD_Handler
FLASH_Handler
EXTI0_Handler
EXTI1_Handler
EXTI2_Handler
EXTI3_Handler
EXTI4_Handler
EXTI5_Handler
EXTI6_Handler
EXTI7_Handler
ADC_Handler
TIMER0_Handler
TIMER1_Handler
TIMER2_Handler
UART_Handler
I2C_Handler
SPI_Handler
                B       .
                ENDP


                ALIGN


; User Initial Stack & Heap

                IF      :DEF:__MICROLIB

                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                ELSE

                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap
__user_initial_stackheap

                LDR     R0, =  Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, = (Heap_Mem +  Heap_Size)
                LDR     R3, = Stack_Mem
                BX      LR

                ALIGN

                ENDIF


                END
