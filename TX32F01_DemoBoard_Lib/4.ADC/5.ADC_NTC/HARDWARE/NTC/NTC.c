#include "TX32F01_periph.h"
#include "NTC.h"
#include "ADC.h"
#include "3435tempresist.h"
#include <stdio.h>


//电阻查表转化为温度，范围为-50~150℃
float Compare_tempres(u32 TR)
{
    short cmp_cnt;
    float Percent;
    cmp_cnt =0;
    while (TR<tempresist[cmp_cnt])
    {
        cmp_cnt++;
        if (cmp_cnt>200)
            break;
    }

    Percent = 1.0-(TR-tempresist[cmp_cnt])/((tempresist[cmp_cnt-1]-tempresist[cmp_cnt])*1.0);

    return (cmp_cnt-52-1+Percent);
}

//得到平均值
u16 Get_Average(u16 *Data,u8 Num)
{
    u8 i;
    u32 Sum=0;
    for(i=0; i<Num; i++)
    {
        Sum+=Data[i];
    }
    return (Sum/Num);

}




