//#include "TX32F01_periph.h"
#include "pwm.h"

//sysclk=24M
/******************************************************
输入参数：clk：PWM输出频率
				 duty：占空比，范围0~100，对应0%~100%
输出参数：0：PASS
				 1:占空比超出范围				 
******************************************************/

//20,21,22,23 24端口 4通道可通过CH1控制
//02,03
//总共可以凑出来7个端口
typedef struct {
    GPIO_Type 		*port;
    uint32_t      pin;
		BOOL				  enable;
} PwmPin;


static PwmPin currentPin={GPIO0,PIN02};//默认


static  PwmPin currentMap[] = {
    {GPIO0, PIN02,TRUE}, 	// ch=0
    {GPIO0, PIN03,FALSE}, // ch=1
    {GPIO2, PIN00,FALSE}, // ch=2
    {GPIO2, PIN01,FALSE}, // ch=3
    {GPIO2, PIN02,FALSE}, // ch=4  
    {GPIO2, PIN03,FALSE}, // ch=5
    {GPIO2, PIN04,FALSE}, // ch=6
};


static const PwmPin s_map[] = {
    {GPIO0, PIN02,TRUE}, // ch=0
    {GPIO0, PIN03,FALSE}, // ch=1
    {GPIO2, PIN00,FALSE}, // ch=2
    {GPIO2, PIN01,FALSE}, // ch=3
    {GPIO2, PIN02,FALSE}, // ch=4  
    {GPIO2, PIN03,FALSE}, // ch=5
    {GPIO2, PIN04,FALSE}, // ch=6
};




u8 TIM0_PWM_Init(u32 clk,u8 duty)
{
		int i;
    if(duty>100)
    return 1;

    TIM_InitTypeDef TIM_InitStructure;
    u32 SYSCLK=GetSystemClock();//得到当前系统时钟
    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_TIM0,ENABLE);//打开TIM0外设时钟
    SCU_PeriphClockCmd(Periph_GPIO0,ENABLE);//打开GPIO00外设时钟
    SCU_PeriphClockCmd(Periph_GPIO2,ENABLE);//打开GPIO2外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

		//关闭无关通道
		for(i=0;i<7;i++){
			if(!currentMap[i].enable){
			currentMap[i].port->MDR&=~((0x3)<<(currentMap[i].pin*2));//清除当前寄存器状态
			currentMap[i].port->AFR&=~((0xf)<<(currentMap[i].pin*4));
			GPIO_Init(currentMap[i].port,currentMap[i].pin,GPIO_MODE_OUTPUT_PP);//配置成模拟输入高阻态
			GPIO_ResetBits(currentMap[i].port,currentMap[i].pin);
			}
		}

		
    GPIO_Init(GPIO0,PIN02,GPIO_MODE_AF);//
    GPIO_PinRemapConfig(GPIO0,PIN02,GPIO_AF_T0CH);//OCx
    //GPIO_PinRemapConfig(GPIO0,PIN03,GPIO_AF_T0CHN);//0CxN

    TIM_DeInit(TIM0);//TIM0寄存器恢复默认值

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CompareOut;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 3-1;//TIM时钟为系统时钟3分频
    TIM_InitStructure.TIM_Period = SYSCLK/(2+1)/clk-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM
    TIM_SetCompare(TIM0,(TIM0->ARR+1)*(duty)/100-1);//设置比较值，即调节占空比，按照高电平占比调节
    TIM_SetOCx_Polarity(TIM0,TIM_OCx_High);//设置OCx初始电平高
    TIM_OCxOutEnable(TIM0,ENABLE);//OCx输出使能
    TIM_Cmd(TIM0,ENABLE);//计数器使能

    return 0;
}

/******************************************************
输入参数：duty：占空比，范围0~100，对应0%~100%
输出参数：无
******************************************************/
u8  TIM0_PWM_Set_Duty(u8 duty)
{
    if(duty>100)
    return FALSE;

    u16 CCRVAL=(TIM0->ARR+1)*(duty)/100-1;//高电平占用的时间

    TIM_SetCompare(TIM0,CCRVAL);//设置比较输出模式比较值
		
    return TRUE;
}


void PWM_SetDuty(uint8_t duty)
{
		TIM0_PWM_Set_Duty(duty);
    //s_duty = duty;
    // TODO: CCR = duty% * ARR
}





void  PWM_Set_Ch_Duty(uint8_t ch,uint8_t duty){

		TIM_InitTypeDef TIM_InitStructure;
		u32 SYSCLK=GetSystemClock();//得到当前系统时钟
		TIM_OCxOutEnable(TIM0,DISABLE);//先关闭输出

		//关闭当前通道
		
		currentPin.port->MDR&=~((0x3)<<(currentPin.pin*2));//清除当前寄存器状态
		currentPin.port->AFR&=~((0xf)<<(currentPin.pin*4));
		GPIO_Init(currentPin.port,currentPin.pin,GPIO_MODE_OUTPUT_PP);//配置成模拟输入高阻态
		GPIO_ResetBits(currentPin.port,currentPin.pin);
	
		currentPin.port=s_map[ch].port;
		currentPin.pin=s_map[ch].pin;
		
	
		GPIO_Init(s_map[ch].port,s_map[ch].pin,GPIO_MODE_AF);
		GPIO_PinRemapConfig(s_map[ch].port,s_map[ch].pin,GPIO_AF_T0CH);//OCx

			
    TIM_DeInit(TIM0);//TIM0寄存器恢复默认值
		u32 clk=1000;

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CompareOut;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 3-1;//TIM时钟为系统时钟3分频
    TIM_InitStructure.TIM_Period = SYSCLK/(2+1)/clk-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM
		if (duty == 0) {
        // 真·0%：常低
        TIM_SetOCx_Polarity(TIM0, TIM_OCx_Low);
        TIM0->CCR = 0;                 // 允许 0
    } else{
		  TIM_SetCompare(TIM0,(TIM0->ARR+1)*(duty)/100-1);//设置比较值，即调节占空比，按照高电平占比调节
			TIM_SetOCx_Polarity(TIM0,TIM_OCx_High);//设置OCx初始电平高
		}
    TIM_OCxOutEnable(TIM0,ENABLE);//OCx输出使能
    TIM_Cmd(TIM0,ENABLE);//计数器使能
}


void  PWM_Set_Ch_Duty_cnt(uint8_t chs,uint8_t duty){

		int i;
		TIM_InitTypeDef TIM_InitStructure;
		u32 SYSCLK=GetSystemClock();//得到当前系统时钟
		TIM_OCxOutEnable(TIM0,DISABLE);//先关闭输出

		//关闭当前通道
		for(i=0;i<7;i++){
			if(currentMap[i].enable){
			currentMap[i].port->MDR&=~((0x3)<<(currentMap[i].pin*2));//清除当前寄存器状态
			currentMap[i].port->AFR&=~((0xf)<<(currentMap[i].pin*4));
			GPIO_Init(currentMap[i].port,currentMap[i].pin,GPIO_MODE_OUTPUT_PP);//配置成模拟输入高阻态
			GPIO_ResetBits(currentMap[i].port,currentMap[i].pin);
			}
		}
		
		//复位所有通道
		for(i=0;i<7;i++){
			currentMap[i].enable=FALSE;//复位所有通道
		}
		//更新通道列表
		for(i=0;i<7;i++){
			currentMap[i].enable=chs&(1<<i);//更新通道列表
		}
		
		for(i=0;i<7;i++){
			if(currentMap[i].enable){
				GPIO_Init(currentMap[i].port,s_map[i].pin,GPIO_MODE_AF);
				GPIO_PinRemapConfig(currentMap[i].port,currentMap[i].pin,GPIO_AF_T0CH);//OCx
			}
		}
		
			
    TIM_DeInit(TIM0);//TIM0寄存器恢复默认值
		u32 clk=1000;

    //TIM0初始化
    TIM_InitStructure.TIM_Mode = TIM_Mode_CompareOut;//比较输出模式（PWM输出）
    TIM_InitStructure.TIM_Prescaler = 3-1;//TIM时钟为系统时钟3分频
    TIM_InitStructure.TIM_Period = SYSCLK/(2+1)/clk-1;//设置预分配系数，即PWM输出频率
    TIM_Init(TIM0,&TIM_InitStructure);//初始化TIM
		if (duty == 0) {
        // 真·0%：常低
        TIM_SetOCx_Polarity(TIM0, TIM_OCx_Low);
        TIM0->CCR = 0;                 // 允许 0
    } else{
		  TIM_SetCompare(TIM0,(TIM0->ARR+1)*(duty)/100-1);//设置比较值，即调节占空比，按照高电平占比调节
			TIM_SetOCx_Polarity(TIM0,TIM_OCx_High);//设置OCx初始电平高
		}
    TIM_OCxOutEnable(TIM0,ENABLE);//OCx输出使能
    TIM_Cmd(TIM0,ENABLE);//计数器使能
	
	
}








uint8_t PWM_GetDuty(void)
{
		uint32_t arr=TIM0->ARR;
	  uint32_t ccr=TIM0->CCR;
		if(arr==0) return 0;
	  uint8_t duty=(ccr+1)*100/(arr+1);
    return duty;
}

void PWM_SetFreq(uint32_t freq)
{
		 uint32_t SYSCLK = GetSystemClock();
    uint32_t prescaler = TIM0->DIV + 1;
    uint32_t new_arr = (SYSCLK / (prescaler * freq)) - 1;

    TIM_Cmd(TIM0, DISABLE);   // 暂停计数器
    TIM0->ARR = new_arr;      // 更新周期寄存器
    //TIM0->EGR = 1;            // 触发更新事件，让ARR生效
    TIM_Cmd(TIM0, ENABLE);    // 重新启用
 //   s_freq = hz;
    // TODO: PSC/ARR 计算并写寄存器
}

uint32_t PWM_GetFreq(void)
{
		uint32_t s_freq=0;
    return s_freq;
}








