#ifndef __LED_H
#define __LED_H


#define LED(x)         (x==0)?(GPIO_ResetBits(GPIO0,PIN03)):(GPIO_SetBits(GPIO0,PIN03))
#define LED_TOGGLE()    GPIO_Toggle(GPIO0,PIN03)

void LED_Init(void);


#endif
