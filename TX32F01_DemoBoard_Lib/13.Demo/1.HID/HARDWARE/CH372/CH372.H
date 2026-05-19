#ifndef ch372_h_
#define ch372_h_

#include<stdint.h>
#include<stdbool.h>
/*******
Data:D7~D0  P17 P16 P15 P14 P23 P22 P21 P20 
********/
#define CH375_INT(x)     (x==0)?(GPIO_ResetBits(GPIO2,PIN05)):(GPIO_SetBits(GPIO2,PIN05))  //CH375ÖÐ¶ÏÐÅºÅÊä³ö¶Ë£¬µÍµçÆ½Êä³ö
#define CH375_CS(x)     (x==0)?(GPIO_ResetBits(GPIO2,PIN04)):(GPIO_SetBits(GPIO2,PIN04))  //CH375Æ¬Ñ¡¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
#define CH375_RD(x)     (x==0)?(GPIO_ResetBits(GPIO3,PIN01)):(GPIO_SetBits(GPIO3,PIN01))  //CH375¶Á¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
#define CH375_WR(x)     (x==0)?(GPIO_ResetBits(GPIO3,PIN00)):(GPIO_SetBits(GPIO3,PIN00))  //CH375Ð´¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
#define CH375_A0(x)     (x==0)?(GPIO_ResetBits(GPIO3,PIN03)):(GPIO_SetBits(GPIO3,PIN03))  //CH375ÃüÁî/Êý¾ÝÑ¡Ôñ¿ØÖÆ¶Ë£¬µÍµçÆ½Ê±Ð´Êý¾Ý£¬¸ßµçÆ½Ê±Ð´ÃüÁî

#define DATAPORTH GPIO1
#define DATAPORTL GPIO2

//sbit CH375_INT=P3^2;  //CH375ÖÐ¶ÏÐÅºÅÊä³ö¶Ë£¬µÍµçÆ½Êä³ö
//sbit CH375_CS=P3^3;   //CH375Æ¬Ñ¡¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
//sbit CH375_RD=P3^4;   //CH375¶Á¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
//sbit CH375_WR=P3^5;   //CH375Ð´¿ØÖÆ¶Ë£¬µÍµçÆ½ÓÐÐ§£¬Ð¾Æ¬ÄÚÖÃÉÏÀ­µç×è
//sbit CH375_A0=P3^7;   //CH375ÃüÁî/Êý¾ÝÑ¡Ôñ¿ØÖÆ¶Ë£¬µÍµçÆ½Ê±Ð´Êý¾Ý£¬¸ßµçÆ½Ê±Ð´ÃüÁî


void delay2us(void);
void delay50ms(void);
void CH375_GPIOInit(void);
void CH375_WRCMD(unsigned char cmd);
void CH375_WRDAT(unsigned char dat);
unsigned char CH375_REDAT(void);
void CH375_REDATGPIOON(void);
void CH375_REDATGPIOOFF(void);


/******************************************************************/
//¶ÁÈ¡ÖÐ¶Ï×´Ì¬
#define CH375_INT_READ() (GPIO_ReadInputDataBit(GPIO2, PIN05))

//Êý¾Ý×ÜÏßÐ´Èë£¨8bitÊý¾Ý²ð·ÖÎª¸ßµÍ4Î»£)
#define DATA_WRITE(data)  do { \
    DATAPORTH->ODR = (DATAPORTH->ODR & 0x0F) | ((data) & 0xF0); \
    DATAPORTL->ODR = (DATAPORTL->ODR & 0xF0) | ((data) & 0x0F); \
} while(0)

// Êý¾Ý×ÜÏß¶ÁÈ¡£¨ºÏ²¢H_4 L_4)
#define DATA_READ()     ((DATAPORTH->IDR & 0xF0) | (DATAPORTL->IDR & 0x0F))


// delay
#define DELAY_SETUP()   delay2us()
#define DELAY_HOLD()    delay2us()
#define DELAY_ACCESS()  do { delay2us(); delay2us(); delay2us(); } while(0)

/******************************************************/



static void DataBus_SetOutput(void);
static void DataBus_SetInput(void);
static void DataBus_SetIdle(void);
void CH375_WRITE_CMD(uint8_t cmd);
void CH375_WRITE_DATA(uint8_t cmd);
uint8_t CH375_READ_DATA(void);
void CH375_WRITE_DATA_BULK(const uint8_t *buffer, uint8_t length);

/**
 * ÅúÁ¿¶ÁÈ¡Êý¾Ý
 * @param buffer: Êý¾Ý»º³åÇø
 * @param length: Òª¶ÁÈ¡µÄ³¤¶È
 */
void CH375_READ_DATA_BULK(uint8_t *buffer, uint8_t length);

#endif



