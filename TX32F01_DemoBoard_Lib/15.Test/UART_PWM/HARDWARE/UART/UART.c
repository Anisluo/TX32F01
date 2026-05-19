#include "TX32F01_periph.h"
#include "UART.h"
#include "systick.h"
#include "scpi_core.h"

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif


static char s_line_buf[UART_RX_LINE_MAX];
static volatile uint16_t s_line_len=0;


void	UART_Send(u16 Data)
{
    UART_ClearFlag(UART_TCIF);//清除接收标志位
    UART_SendData(Data);//发送数据
    while (UART_GetFlagStatus(UART_TCIF)==0);//等待发送完成
}

#ifdef USE_IAR
PUTCHAR_PROTOTYPE
{
    // TDR和发送移位寄存器都空发送。
    UART_Send(ch);
    return ch;
}
#else

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB
#pragma import(__use_no_semihosting)
//标准库需要的支持函数
struct __FILE
{
    int handle;

};

FILE __stdout;
//定义_sys_exit()以避免使用半主机模式
void _sys_exit(int x)
{
    x = x;
}

//重定义fputc函数
int fputc(int ch, FILE *f)
{
    // TDR和发送移位寄存器都空发送。
    UART_Send(ch);
    return ch;
}

#endif

void UART_NVIC_Init(u32 bps)
{
    UART_InitTypeDef UART_InitStructure;

    SCU_Unlock();//解锁SCU，可对其余SCU寄存器操作。
    SCU_PeriphClockCmd(Periph_GPIO3,ENABLE);//打开GPIO3外设时钟
    SCU_PeriphClockCmd(Periph_UART,ENABLE);//打开UART外设时钟
    SCU_Lock();//上锁SCU，不可对其余SCU寄存器操作。

    // GPIO初始化
    GPIO_Init(GPIO3,PIN06,GPIO_MODE_AF);//设置为复用模式
    GPIO_Init(GPIO3,PIN07,GPIO_MODE_AF);//设置为复用模式
    GPIO_PinRemapConfig(GPIO3,PIN07,GPIO_AF_UART_TX);//复用为TX
    GPIO_PinRemapConfig(GPIO3,PIN06,GPIO_AF_UART_RX);//复用为RX

    UART_DeInit();//UART寄存器恢复默认值

    // UART初始化
    UART_InitStructure.UART_BaudRate = bps;//串口波特率
    UART_InitStructure.UART_WordLength = UART_8DATABIT;//8位数据格式
    UART_InitStructure.UART_StopBits = UART_1STOPBIT;//一个停止位
    UART_InitStructure.UART_Parity = UART_Pority_None;//无奇偶校验位
    UART_InitStructure.UART_Mode = UART_Mode_Rx | UART_Mode_Tx;//收发模式
    UART_Init(&UART_InitStructure);//初始化串口

    UART_ITConfig(UART_RDNEIE,ENABLE);//使能接收中断
    NVIC_EnableIRQ(UART_IRQn);//打开UART中断
    NVIC_SetPriority(UART_IRQn, 0x0);//中断优先级设置

    UART_Cmd(ENABLE);//使能串口
}

//void UART_Handler(void)
//{
//    u8 Receive;
//	
//    if(UART_GetFlagStatus(UART_RDNEIF))//判断接收数据寄存器不为空
//    {
//        Receive=UART_ReceiveData();//接收数据
//        UART_ClearFlag(UART_RDNEIF);//清除接收标志位
//        printf("接收到的数据：0x%x\r\n",Receive);
//    }
//}

//uart串口修改
void UART_Handler(void)
{
    u8 ch;
	
    if(UART_GetFlagStatus(UART_RDNEIF))//判断接收数据寄存器不为空
    {
        ch=UART_ReceiveData();//接收数据
        UART_ClearFlag(UART_RDNEIF);//清除接收标志位
			
				//统一换行处理/r,/n,/r/n
			if(ch=='\r' || ch=='\n'){
				if(s_line_len>0){
					s_line_buf[s_line_len]='\0';
					SCPI_ParseLine(s_line_buf);//交给协议层处理
					s_line_len=0;
				}
				
			}else{
				if(s_line_len<UART_RX_LINE_MAX-1){
					s_line_buf[s_line_len++]=ch;
				}else{
					s_line_len=0;
				}
			}
    }
}




