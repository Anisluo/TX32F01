#ifndef __EXTI_H
#define __EXTI_H

#define EXTI_GPIO0		0x0
#define EXTI_GPIO1		0x1
#define EXTI_GPIO2		0x2
#define EXTI_GPIO3		0x3

#define EXTI_LINE_0   0x00
#define EXTI_LINE_1   0x01
#define EXTI_LINE_2   0x02
#define EXTI_LINE_3   0x03
#define EXTI_LINE_4   0x04
#define EXTI_LINE_5   0x05
#define EXTI_LINE_6   0x06
#define EXTI_LINE_7   0x07
#define EXTI_LINE_PVD 0x08

#define EXTI_GPIO_Config(GPIOx,line)			 EXTI->CFGR&=~(0x7<<line*3);EXTI->CFGR|=(GPIOx<<line*3)
#define EXTI_RISEDGE_TRIG_ENABLE(line)          (EXTI->RTSR |= 1<<line)
#define EXTI_FALEDGE_TRIG_ENABLE(line)          (EXTI->FTSR |= 1<<line)
#define EXTI_IT_ENABLE(line)                    (EXTI->IMR |= 1<<line)
#define EXTI_IT_DISABLE(line)                   (EXTI->IMR &= ~((uint32_t)1<<line))
#define EXTI_ITF_CLEAR(line)                    (EXTI->PR &= ~((uint32_t)1<<line))

#define KEY_P12   GPIO_ReadInputDataBit(GPIO1,PIN02)

void EXTI_P12_Init(void);


#endif
