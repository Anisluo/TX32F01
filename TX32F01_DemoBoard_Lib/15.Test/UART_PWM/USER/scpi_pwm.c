#include "scpi_core.h"
//#include "scpi_pwm.h"
#include "pwm.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

// SOUR:PWM:DUTY [0..100] / SOUR:PWM:DUTY?

static void h_SOUR_PWM_DUTY(const char *param, int query)
{
		int i=0;
    if (query) {
        printf("%d\r\n", PWM_GetDuty());
        return;
    }

		uint8_t chs=utils_parse_int_at(param,0,0);
		
		int v=utils_parse_int_at(param,1,0);//取占空比
    if (v < 0 || v > 100) {
        printf("ERR:Range\r\n");
        return;
    }
		if( chs<0 || chs>255){
			printf("ERROR:ch\r\n");
			return;
		}
		//先关闭当前
		
		PWM_Set_Ch_Duty_cnt(chs,(uint8_t)v);
    //PWM_Set_Ch_Duty((uint8_t)ch,(uint8_t)v);
    printf("ok duty set success:%d\r\n",v);
}

// SOUR:PWM:FREQ <Hz> / SOUR:PWM:FREQ?
static void h_SOUR_PWM_FREQ(const char *param, int query)
{
    if (query) {
        printf("%lu\r\n", (unsigned long)PWM_GetFreq());
        return;
    }
    uint32_t hz = (uint32_t)utils_parse_uint(param, 1000);
    if (hz < 10 || hz > 100000) { // 10 Hz ~ 100 kHz 示例限制
        printf("ERR:Range\r\n");
        return;
    }
    PWM_SetFreq(hz);
    printf("OK\r\n");
}

void SCPI_PWM_Register(void)
{
    SCPI_Register("SOUR:PWM:DUTY", h_SOUR_PWM_DUTY);
    SCPI_Register("SOUR:PWM:FREQ", h_SOUR_PWM_FREQ);
}