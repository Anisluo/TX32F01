/**
 * CH375 USB UPS电源设备实现 - 简化版
 * 专注于USB枚举过程
 */

#include "CH372.h"
#include "CH375INC.h"
#include "TX32F01_periph.h"
#include "exti.h"
#include "usb.h"
#include "UART.h"

#define MAX_PACKEG_SIZE 0x08

/***********************************************************************************
****配置和调试宏
************************************************************************************/
#define LOBYTE(x)   ((u8)((x) & 0xFF))
#define HIBYTE(x)   ((u8)(((x) >> 8) & 0xFF))

#define USB_DEBUG_ENABLED 0
#if USB_DEBUG_ENABLED
#define USB_DBG(...)   printf(__VA_ARGS__)
#else
#define USB_DBG(...)   do{}while(0)
#endif

/***********************************************************************************
****HID报告描述符 - UPS电源设备
************************************************************************************/
static const unsigned char Hid_des[] = {
    0x05, 0x84,         // 使用页面 (电源设备)
    0x09, 0x04,         // 使用 (UPS)
    0xA1, 0x01,         // 集合 (应用程序)
    
    // 电源摘要集合
    0x09, 0x24,         // 使用 (电源摘要)
    0xA1, 0x02,         // 集合 (物理)
    
    // 报告ID 1 - UPS状态特征报告
    0x85, 0x01,         // 报告ID (1)
    
    // 电源摘要ID
    0x09, 0x25,         // 使用 (电源摘要ID)
    0x15, 0x00,         // 逻辑最小值 (0)
    0x25, 0xFF,         // 逻辑最大值 (255)
    0x75, 0x08,         // 报告大小 (8)
    0x95, 0x01,         // 报告计数 (1)
    0xB1, 0x03,         // 特征 (常量, 变量, 绝对)
    
    // 电池存在状态
    0x05, 0x85,         // 使用页面 (电池系统)
    0x09, 0xD1,         // 使用 (电池存在)
    0x15, 0x00,         // 逻辑最小值 (0)
    0x25, 0x01,         // 逻辑最大值 (1)
    0x75, 0x01,         // 报告大小 (1)
    0x95, 0x01,         // 报告计数 (1)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    // 交流电存在状态
    0x09, 0xD0,         // 使用 (交流电存在)
    0x75, 0x01,         // 报告大小 (1)
    0x95, 0x01,         // 报告计数 (1)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    // 充电状态
    0x09, 0x44,         // 使用 (充电)
    0x75, 0x01,         // 报告大小 (1)
    0x95, 0x01,         // 报告计数 (1)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    // 放电状态
    0x09, 0x45,         // 使用 (放电)
    0x75, 0x01,         // 报告大小 (1)
    0x95, 0x01,         // 报告计数 (1)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    // 填充 (4位)
    0x75, 0x01,         // 报告大小 (1)
    0x95, 0x04,         // 报告计数 (4)
    0xB1, 0x01,         // 特征 (常量)
    
    // 电池百分比 (0-100%)
    0x09, 0x66,         // 使用 (剩余容量)
    0x15, 0x00,         // 逻辑最小值 (0)
    0x25, 0x64,         // 逻辑最大值 (100)
    0x75, 0x08,         // 报告大小 (8)
    0x95, 0x01,         // 报告计数 (1)
    0x65, 0x00,         // 单位 (无)
    0x55, 0x00,         // 单位指数 (0)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    // 电压 (以0.1V为单位)
    0x05, 0x84,         // 使用页面 (电源设备)
    0x09, 0x30,         // 使用 (电压)
    0x15, 0x00,         // 逻辑最小值 (0)
    0x26, 0xFF, 0x00,   // 逻辑最大值 (255)
    0x75, 0x08,         // 报告大小 (8)
    0x95, 0x01,         // 报告计数 (1)
    0x65, 0x00,         // 单位 (无)
    0x55, 0x00,         // 单位指数 (0)
    0xB1, 0x02,         // 特征 (数据, 变量, 绝对)
    
    0xC0,               // 结束集合 (物理)
    0xC0                // 结束集合 (应用程序)
};

#define HID_REPORT_DESC_LEN sizeof(Hid_des)

/***********************************************************************************
****全局变量
************************************************************************************/
unsigned char mVarSetupRequest;
unsigned char mVarSetupLength;
unsigned char *VarSetupDescr;
unsigned char VarUsbAddress;
unsigned char CH375CONFLAG;
unsigned char enumeration_step;
unsigned char UsbDeviceState;
unsigned char AddressSetPending;

/* USB设备状态 */
#define USB_STATE_DETACHED      0
#define USB_STATE_DEFAULT       3
#define USB_STATE_ADDRESS       4
#define USB_STATE_CONFIGURED    5

/* 请求包结构 */
typedef union _REQUEST_PACK {
    unsigned char buffer[8];
    struct {
        unsigned char bmRequestType;
        unsigned char bRequest;
        unsigned int  wValue;
        unsigned int  wIndex;
        unsigned int  wLength;
    } r;
} mREQUEST_PACKET;

static mREQUEST_PACKET request;

/***********************************************************************************
****USB描述符
************************************************************************************/
static const unsigned char DevDes[] = {
    0x12, 0x01,          // bLength, bDescriptorType (设备)
    0x10, 0x01,          // bcdUSB 2.00
    0x00, 0x00, 0x00,    // bDeviceClass/SubClass/Protocol (按接口)
    MAX_PACKEG_SIZE,                // bMaxPacketSize0 = 8
    0x34, 0x12,          // idVendor (0x1234 - 通用测试VID)
    0x01, 0x56,          // idProduct (0x5601 - 通用测试PID)
    0x00, 0x01,          // bcdDevice 1.00
    0x01, 0x02, 0x03,    // iManufacturer, iProduct, iSerial
    0x01                 // bNumConfigurations
};

//static  unsigned char ConDes[] = {
//    /* 配置描述符 */
//    0x09, 0x02,          // bLength=9, bDescriptorType=2
//    0x1B, 0x00,          // wTotalLength = 27 (9+9+9)
//    0x00,                // bNumInterfaces = 1
//    0x01,                // bConfigurationValue = 1
//    0x00,                // iConfiguration = 0
//    0x80,                // bmAttributes = 0x80
//    0x32,                // bMaxPower = 100mA
//};
//    
//    /* 接口描述符 */
//    0x09, 0x04,          // bLength, bDescriptorType
//    0x00, 0x00,          // bInterfaceNumber, bAlternateSetting
//    0x00,                // bNumEndpoints = 0
//    0x03, 0x00, 0x00,    // bInterfaceClass=HID, SubClass=0, Protocol=0
//    0x00,                // iInterface
//    
//    /* HID描述符 */
//    0x09, 0x21,          // bLength, bDescriptorType=HID
//    0x11, 0x01,          // bcdHID 1.11
//    0x00,                // bCountryCode
//    0x01,                // bNumDescriptors
//    0x22,                // bDescriptorType = 报告
//    LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN),
//};

//static const unsigned char ConDes[] = {
//    /* 配置描述符 */
//    0x09, 0x02,          // bLength=9, bDescriptorType=2
//    0x1B, 0x00,          // wTotalLength = 27 (9+9+9)
//    0x01,                // bNumInterfaces = 1
//    0x01,                // bConfigurationValue = 1
//    0x00,                // iConfiguration = 0
//    0x80,                // bmAttributes = 0x80
//    0x32,                // bMaxPower = 100mA
//    
//    /* 接口描述符 */
//    0x09, 0x04,          // bLength, bDescriptorType
//    0x00, 0x00,          // bInterfaceNumber, bAlternateSetting
//    0x00,                // bNumEndpoints = 0
//    0x03, 0x00, 0x00,    // bInterfaceClass=HID, SubClass=0, Protocol=0
//    0x00,                // iInterface
//    
//    /* HID描述符 (属于上面的接口) */
//    0x09, 0x21,          // bLength, bDescriptorType=HID
//    0x11, 0x01,          // bcdHID 1.11
//    0x00,                // bCountryCode
//    0x01,                // bNumDescriptors
//    0x22,                // bDescriptorType = 报告
//    LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN),
//};

static const unsigned char ConDes[] = {
    /* 配置描述符 */
    0x09, 0x02,          // bLength=9, bDescriptorType=2
    0x22, 0x00,          // wTotalLength = 34
    0x01,                // bNumInterfaces = 1
    0x01,                // bConfigurationValue = 1
    0x00,                // iConfiguration = 0
    0x80,                // bmAttributes = 0x80 (总线供电)
    0x32,                // bMaxPower = 100mA

    /* 接口描述符 */
    0x09, 0x04,          // bLength, bDescriptorType
    0x00, 0x00,          // bInterfaceNumber, bAlternateSetting
    0x01,                // bNumEndpoints = 1
    0x03, 0x00, 0x00,    // bInterfaceClass=HID, Su bClass=0, Protocol=0
    0x00,                // iInterface

    /* HID描述符 */
    0x09, 0x21,          // bLength, bDescriptorType=HID
    0x11, 0x01,          // bcdHID 1.11
    0x00,                // bCountryCode
    0x01,                // bNumDescriptors
    0x22,                // bDescriptorType = 报告
    LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN),

    /* 端点描述符 */
    0x07, 0x05,          // bLength, bDescriptorType
    0x81,                // bEndpointAddress (EP1 IN)
    0x03,                // bmAttributes (中断)
    0x08, 0x00,          // wMaxPacketSize = 8
    0x20                 // bInterval = 32ms
};

//static  unsigned char ConDes[] = {
//    /* 配置描述符 */
//    0x09, 0x02,          // bLength, bDescriptorType
//    0x22, 0x00,          // wTotalLength = 34
//    0x01,                // bNumInterfaces
//    0x01,                // bConfigurationValue
//    0x00,                // iConfiguration
//    0x80,                // bmAttributes (总线供电)
//    0x32,                // bMaxPower = 100mA

//    /* 接口描述符 */
//    0x09, 0x04,          // bLength, bDescriptorType
//    0x00, 0x00,          // bInterfaceNumber, bAlternateSetting
//    0x01,                // bNumEndpoints = 1 (EP1 IN)
//    0x03, 0x01, 0x00,    // bInterfaceClass=HID, SubClass=0, Protocol=0
//    0x00,                // iInterface

//    /* HID描述符 */
//    0x09, 0x21,          // bLength, bDescriptorType=HID
//    0x10, 0x01,          // bcdHID 1.11
//    0x21,                // bCountryCode
//    0x01,                // bNumDescriptors
//    0x22,                // bDescriptorType = 报告
//    LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN), // wDescriptorLength

//    /* 端点描述符 */
//    0x07, 0x05,          // bLength, bDescriptorType
//    0x81,                // bEndpointAddress (EP1 IN)
//    0x03,                // bmAttributes (中断)
//    0x08, 0x00,          // wMaxPacketSize = 8
//    0xFF                 // bInterval = 32ms
//};

/* 完整 43B 配置 */
//static const unsigned char ConDes[43] = {
//    /* Config (9) */
//    0x09, 0x02, 0x2B, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
//    /* Interface 0: HID (9) */
//    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00,
//    /* HID (9) */
//    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN),
//    /* EP1 IN (7) */
//    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x20,
//    /* Interface 1: Vendor 占位, 0 端点 (9) */
//    0x09, 0x04, 0x01, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00
//};



/* 字符串描述符 */
static const unsigned char LangDes[]   = {0x04, 0x03, 0x09, 0x04}; // 英语 (美国)
// iManufacturer
static const unsigned char Chang[] = {
    0x0E, 0x03,   // 长度=14字节, 类型=String Descriptor
    'A',0, 'C',0, 'M',0, 'E',0, ' ',0, 'I',0
};

// iProduct
static const unsigned char SerDes[] = {
    0x10, 0x03,   // 长度=16字节
    'U',0, 'P',0, 'S',0, ' ',0, 'D',0
};

// iSerialNumber
static const unsigned char SerialDes[] = {
    0x18, 0x03,   // 长度=24字节
    'U',0, 'P',0, 'S',0, '2',0, '0',0, '2',0, '5',0, '0',0, '8',0, '0',0, '1',0
};

/* HID描述符 (9字节) */
static const unsigned char HidDesc[] = {
    0x09, 0x21,          // bLength, bDescriptorType(HID)
    0x11, 0x01,          // bcdHID 1.11
    0x00,                // bCountryCode
    0x01,                // bNumDescriptors
    0x22,                // bDescriptorType = 报告
    LOBYTE(HID_REPORT_DESC_LEN), HIBYTE(HID_REPORT_DESC_LEN), // wDescriptorLength
};

/***********************************************************************************
****辅助函数
************************************************************************************/
static inline void USB_Ep0_Stall(void)
{
    USB_DBG("*** EP0停滞 - 不支持的请求 ***\r\n");
    CH375_WRITE_CMD(CMD_UNLOCK_USB);
}

static void mCh375Ep0Up(void)
{
    unsigned char i, len;
	
	
		unsigned char length;
		unsigned char status;
	

//		if(mVarSetupLength==9){
//		
//		
//		
//			CH375_WRITE_CMD(CMD_WR_USB_DATA3);
//      CH375_WRITE_DATA(8);
//			
//			for (i = 0; i < 8; i++) {
//            CH375_WRITE_DATA(request.buffer[i]);
//						//USB_DBG("0x%02X\t",request.buffer[i]);
//      }
// 
//			
//				CH375_WRITE_CMD(CMD_UNLOCK_USB);
//			
//				USB_DBG("9bytes 发送验证，观察效果\r\n");
//			return;

//		}
	
	

    if (mVarSetupLength > 0) {
        len = (mVarSetupLength >= MAX_PACKEG_SIZE) ? MAX_PACKEG_SIZE : mVarSetupLength;
        mVarSetupLength -= len;
			

				//CH375_WRITE_CMD(CMD_UNLOCK_USB);//解锁
        CH375_WRITE_CMD(CMD_WR_USB_DATA3);
        CH375_WRITE_DATA(len);
			  USB_DBG("EP0 上传字节数=%d，剩余字节数=%d\r\n", len,mVarSetupLength);
					
        for (i = 0; i < len; i++) {
            CH375_WRITE_DATA(request.buffer[i]);
						//USB_DBG("0x%02X\t",request.buffer[i]);
        }
				
        if (mVarSetupLength == 0) {
            USB_DBG("*** 数据传输完成 ***\r\n");
            mVarSetupRequest = 0;
        }
//						CH375_WRITE_CMD(CMD_SET_ENDP3);
//		CH375_WRITE_DATA(0xC0);
		//CH375_WRITE_CMD(CMD_UNLOCK_USB);
				
		
    } else {
        USB_DBG("EP0状态阶段\r\n");
//        CH375_WRITE_CMD(CMD_WR_USB_DATA3);
//        CH375_WRITE_DATA(0); // ZLP
//        mVarSetupRequest = 0;
    }
		
		
		
		//CH375_READ_DATA();
		    // 在发送数据后，强制检查CH375状态
    if (mVarSetupLength > 0) {
        USB_DBG("等待下次EP0 IN\r\n");
    }
				
}

static void mCh375DesUp(void)
{
    unsigned char k, copy_len = (mVarSetupLength > 8) ? 8 : mVarSetupLength;
    
    for (k = 0; k < copy_len; k++) {
        request.buffer[k] = *VarSetupDescr;
        VarSetupDescr++;
    }
    
    for (; k < 8; k++) {
        request.buffer[k] = 0;
    }
}

static void Handle_Cmd_EP0_OUT(void){

		unsigned char len,i;
		USB_DBG("(EP0 OUT)\r\n");
//		CH375_WRITE_CMD(CMD_RD_USB_DATA);
//		len=CH375_READ_DATA();
//	
//		for (i = 0; i < len; i++) {
//				request.buffer[i] = CH375_READ_DATA();
//			USB_DBG("*** 0x%02X ", request.buffer[i]);
//		}
//		USB_DBG("\r\n ");
		
		
					// USB_INT_EP0_OUT —— 状态阶段 OUT ZLP
			CH375_WRITE_CMD(CMD_RD_USB_DATA0);           // 读=释放
			uint8_t l = CH375_READ_DATA(); while(l--) (void)CH375_READ_DATA();

			// 摆好姿态等下一笔 SETUP（SETUP 走 OUT）
//			CH375_WRITE_CMD(CMD_SET_ENDP3); CH375_WRITE_DATA(0x0E); // IN=NAK
//			CH375_WRITE_CMD(CMD_SET_ENDP2); CH375_WRITE_DATA(0x00); // OUT=ACK ★必须ACK
			CH375_WRITE_CMD(CMD_UNLOCK_USB);
		
		
   // CH375_WRITE_CMD(CMD_UNLOCK_USB);
}




/***********************************************************************************
****SETUP命令处理 - 核心枚举逻辑
************************************************************************************/
static void Handle_Cmd_Setup(void)
{
    unsigned char i, length;

    CH375_WRITE_CMD(CMD_RD_USB_DATA0);
    length = CH375_READ_DATA();
		for (i = 0; i < length; i++) {
				request.buffer[i] = CH375_READ_DATA();
			USB_DBG("*** 0x%02X ", request.buffer[i]);
		}
		USB_DBG("\r\n ");
		

    
    if (length != 8) {
        USB_DBG("错误: SETUP包长度不正确: %d\r\n", length);
        CH375_WRITE_CMD(CMD_UNLOCK_USB);
        return;
    }




    mVarSetupLength = request.buffer[6] | (request.buffer[7] << 8);
    enumeration_step++;
    USB_DBG("*** 枚举步骤 %d ***\r\n", enumeration_step);

    const u8 recipient = (request.r.bmRequestType & 0x1F);
    const u8 type      = (request.r.bmRequestType & 0x60);
		USB_DBG("*** 类型标识符0x%02X ***\r\n", request.r.bmRequestType);
    if (type == 0x20) {
        /* HID类请求 */
        USB_DBG("*** HID类请求: 0x%02X ***\r\n", request.r.bRequest);
        
        switch (request.r.bRequest) {
            case 0x01: // GET_REPORT
                USB_DBG("*** GET_REPORT请求 ***\r\n");
                u8 rpt_type = (request.r.wValue >> 8) & 0xFF;
                u8 rpt_id = request.r.wValue & 0xFF;
                
                if (rpt_type == 3 && rpt_id == 1) { // 特征报告ID 1
                    // 返回UPS状态数据
                    request.buffer[0] = 0x01;  // 报告ID
                    request.buffer[1] = 0x01;  // 电源摘要ID
                    request.buffer[2] = 0x03;  // 状态标志(电池+交流电存在)
                    request.buffer[3] = 85;    // 电池百分比
                    request.buffer[4] = 120;   // 电压(12.0V)
                    request.buffer[5] = 0;     // 电流
                    request.buffer[6] = 25;    // 温度
                    request.buffer[7] = 0;     // 保留
                    
                    if (mVarSetupLength > 8) mVarSetupLength = 8;
                    mVarSetupRequest = 0x01;
                } else {
                    USB_Ep0_Stall();
                    return;
                }
                break;

            case 0x03: // GET_IDLE
                request.buffer[0] = 0x00;
                mVarSetupLength = 1;
                mVarSetupRequest = 0x03;
                break;

            case 0x0A: // SET_IDLE
                mVarSetupLength = 0;
                mVarSetupRequest = 0x0A;
                break;

            default:
                USB_Ep0_Stall();
                return;
        }
    }
    else if (type == 0x00) {
        /* 标准请求 */
        mVarSetupRequest = request.r.bRequest;
        
        switch (request.r.bRequest) {
            case DEF_USB_GET_DESCR: {
                const u8 desc_type = request.buffer[3];
                const u8 desc_idx  = request.buffer[2];
                
                USB_DBG("获取描述符，类型: %d, 索引: %d\r\n", desc_type, desc_idx);

                switch (desc_type) {
                    case 1: // 设备描述符
                        USB_DBG("*** 请求设备描述符 ***\r\n");
                        VarSetupDescr = (unsigned char*)DevDes;
                        if (mVarSetupLength > sizeof(DevDes))
                            mVarSetupLength = sizeof(DevDes);
												USB_DBG("*** 请求数据长=%d ***\r\n",mVarSetupLength);
												
												
												
                        break;

                    case 2: // 配置描述符
                        USB_DBG("*** 请求配置描述符 ***\r\n");
												USB_DBG("*** PC请求数据长=%d ***\r\n",mVarSetupLength);
                        VarSetupDescr = (unsigned char*)ConDes;
												//mVarSetupLength=9;
                        if (mVarSetupLength > sizeof(ConDes))
                            mVarSetupLength = sizeof(ConDes);
												
												
												USB_DBG("*** 实际请求数据长=%d ***\r\n",mVarSetupLength);
                        break;

                    case 3: // 字符串描述符
                        
                        if (desc_idx == 0) {
                            VarSetupDescr = (unsigned char*)LangDes;
                            if (mVarSetupLength > sizeof(LangDes))
                                mVarSetupLength = sizeof(LangDes);
														
														
														
														
														USB_DBG("请求语言描述符\r\n");
                        } else if (desc_idx == 1) {
                            VarSetupDescr = (unsigned char*)Chang;
                            if (mVarSetupLength > sizeof(Chang))
                                mVarSetupLength = sizeof(Chang);
														USB_DBG("请求厂商字符串\r\n");
                        } else if (desc_idx == 2) {
                            VarSetupDescr = (unsigned char*)SerDes;
                            if (mVarSetupLength > sizeof(SerDes))
                                mVarSetupLength = sizeof(SerDes);
														USB_DBG("请求产品字符串\r\n");
                        } else if (desc_idx == 3) {
                            VarSetupDescr = (unsigned char*)SerialDes;
                            if (mVarSetupLength > sizeof(SerialDes))
                                mVarSetupLength = sizeof(SerialDes);
														USB_DBG("请求产品序列号\r\n");
                        } else {
                            USB_Ep0_Stall();
													USB_DBG("无关字符串描述符\r\n");
                            return;
                        }
                        break;

                    case 0x21: // HID描述符
                        USB_DBG("*** 请求HID描述符 ***\r\n");
                        VarSetupDescr = (unsigned char*)HidDesc;
                        if (mVarSetupLength > sizeof(HidDesc))
                            mVarSetupLength = sizeof(HidDesc);
                        break;

                    case 0x22: // HID报告描述符
                        USB_DBG("*** 请求HID报告描述符 ***\r\n");
                        USB_DBG("*** 这是UPS枚举的关键步骤! ***\r\n");
                        VarSetupDescr = (unsigned char*)Hid_des;
                        if (mVarSetupLength > HID_REPORT_DESC_LEN)
                            mVarSetupLength = HID_REPORT_DESC_LEN;
                        break;

                    default:
                        USB_Ep0_Stall();
                        return;
                }

                if (mVarSetupLength > 0 && VarSetupDescr != NULL) {
                    mCh375DesUp();
                }
								mCh375Ep0Up();
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
                break;
            }

            case DEF_USB_SET_ADDRESS:
                VarUsbAddress = request.buffer[2];
                AddressSetPending = 1;
                USB_DBG("*** 设置地址: %d ***\r\n", VarUsbAddress);
                mVarSetupLength = 0;
								CH375_WRITE_CMD(CMD_WR_USB_DATA3);
								CH375_WRITE_DATA(0);
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
						
						
                break;

            case DEF_USB_SET_CONFIG:
                CH375CONFLAG = request.buffer[2];
                if (CH375CONFLAG > 0) {
                    UsbDeviceState = USB_STATE_CONFIGURED;
                    USB_DBG("*** 设置配置: %d ***\r\n", CH375CONFLAG);
                    USB_DBG("*** UPS设备枚举完成! ***\r\n");
                } else {
                    UsbDeviceState = USB_STATE_ADDRESS;
                }
                mVarSetupLength = 0;
                break;

            case DEF_USB_GET_STATUS:
								 USB_DBG("*** GET_STATUS ***\r\n");
                if (recipient == 0x00) { // 设备
                    request.buffer[0] = 0x00; // 总线供电
                    request.buffer[1] = 0x00;
                    mVarSetupLength = 2;
                } else if (recipient == 0x01) { // 接口
                    request.buffer[0] = 0x00;
                    request.buffer[1] = 0x00;
                    mVarSetupLength = 2;
                } else if (recipient == 0x02) { // 端点
                    request.buffer[0] = 0x00; // 未停滞
                    request.buffer[1] = 0x00;
                    mVarSetupLength = 2;
                } else {
                    USB_Ep0_Stall();
                    return;
                }
                break;

            default:
								USB_DBG("*** STALL ***\r\n");
                USB_Ep0_Stall();
                return;
        }
    }
    else {
        USB_Ep0_Stall();
        return;
    }

    // 发送响应
    //mCh375Ep0Up();
    USB_DBG("SETUP TOKEN处理完成\r\n");
}

/***********************************************************************************
****USB中断处理
************************************************************************************/
void EXTI5_Handler(void)
{
    unsigned char temp;
		unsigned char len,i;

    EXTI_ITF_CLEAR(EXTI_LINE_5);

    CH375_WRITE_CMD(CMD_GET_STATUS);
    temp = CH375_READ_DATA();

		USB_DBG("\r\n\r\n");
    USB_DBG("USB进入中断状态: 0x%02X\r\n", temp);

    switch (temp) {
        case USB_INT_EP0_SETUP:
            USB_DBG("(SETUP令牌)\r\n");
            Handle_Cmd_Setup();
            break;

        case USB_INT_EP0_IN:
            USB_DBG("(EP0 IN)\r\n");
//						CH375_WRITE_CMD(CMD_UNLOCK_USB);
            // 继续描述符传输
            if (mVarSetupRequest == DEF_USB_GET_DESCR && mVarSetupLength > 0 ) {
                if (VarSetupDescr != NULL) {
                    mCh375DesUp();
                }
                mCh375Ep0Up();
                
								//USB_DBG("EP0 IN 传输数据\r\n");
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
                break;
            }else{
							USB_DBG("进入EP0 ZLP阶段");
							//分析ep0_in状态
//								CH375_WRITE_CMD(CMD_RD_USB_DATA);
//								len=CH375_READ_DATA();
//								USB_DBG("*** 长度为：%d\r\n",len);
//								for (i = 0; i < len; i++) {
//										request.buffer[i] = CH375_READ_DATA();
//									USB_DBG("*** 0x%02X ", request.buffer[i]);
//								}
//								USB_DBG("\r\n ");
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
						}

            // 延迟地址设置
            if (AddressSetPending) {
                USB_DBG("*** 执行延迟地址设置: %d ***\r\n", VarUsbAddress);
                CH375_WRITE_CMD(CMD_SET_USB_ADDR);
                CH375_WRITE_DATA(VarUsbAddress);
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
                UsbDeviceState = USB_STATE_ADDRESS;
                AddressSetPending = 0;
            } else {
                mVarSetupRequest = 0;
                mVarSetupLength = 0;
                VarSetupDescr = NULL;
								CH375_WRITE_CMD(CMD_UNLOCK_USB);
							
//							   USB_DBG("EP0状态阶段\r\n");
//								CH375_WRITE_CMD(CMD_WR_USB_DATA3);
//								CH375_WRITE_DATA(0); // ZLP
//								mVarSetupRequest = 0;
            }

            break;

        case USB_INT_EP0_OUT:
						Handle_Cmd_EP0_OUT();
            break;

        case USB_INT_CONNECT:
            USB_DBG("(USB设备已连接)\r\n");
            CH375_WRITE_CMD(CMD_UNLOCK_USB);
            break;

        case USB_INT_DISCONNECT:
            USB_DBG("(USB设备已断开)\r\n");
            UsbDeviceState = USB_STATE_DETACHED;
            CH375_WRITE_CMD(CMD_UNLOCK_USB);
            break;

        default:
            if ((temp & 0x0F) == 0x0B || (temp & 0x0F) == 0x07) {
                USB_DBG("(USB总线复位)\r\n");
                // 重置所有状态
                CH375CONFLAG = 0;
                mVarSetupLength = 0;
                mVarSetupRequest = 0;
                VarUsbAddress = 0;
                AddressSetPending = 0;
                VarSetupDescr = NULL;
                UsbDeviceState = USB_STATE_DEFAULT;
                enumeration_step = 0;
            }
            CH375_WRITE_CMD(CMD_UNLOCK_USB);
						//USB_DBG("len=%d",outlen);
            //CH375_READ_DATA();
							
            break;
    }
}

/***********************************************************************************
****CH375初始化
************************************************************************************/
void CH375_Init(void)
{
    u8 temp;
    u16 timeout = 0;

    printf("CH375 UPS设备初始化开始...\r\n");

    // 初始化状态变量
    UsbDeviceState = USB_STATE_DETACHED;
    AddressSetPending = 0;
    CH375CONFLAG = 0;
    mVarSetupLength = 0;
    mVarSetupRequest = 0;
    VarUsbAddress = 0;
    enumeration_step = 0;

    // 验证描述符
    printf("HID报告描述符长度: %d字节\r\n", HID_REPORT_DESC_LEN);
    printf("配置描述符总长度: %d字节\r\n", sizeof(ConDes));

    // 硬件复位
    CH375_WRITE_CMD(CMD_RESET_ALL);
    delay50ms();

    // 设置USB设备模式
    printf("配置USB设备模式...\r\n");
    CH375_WRITE_CMD(CMD_SET_USB_MODE);
    CH375_WRITE_DATA(1);  // 设备模式
    delay2us();
		
		 //CH375_WRITE_CMD(CH375_MAX_DATA_LEN);
		

    // 等待初始化完成
    while (timeout < 1000) {
        temp = CH375_READ_DATA();
        if (temp == CMD_RET_SUCCESS) {
            printf("CH375初始化成功\r\n");
            break;
        }
        delay2us();
        timeout++;
        if (timeout >= 1000) {
            printf("CH375初始化超时\r\n");
            return;
        }
    }

    // 设置端点
    CH375_WRITE_CMD(CMD_SET_ENDP2);
    CH375_WRITE_DATA(0x00);  
    delay2us();
    
    CH375_WRITE_CMD(CMD_SET_ENDP3);
    CH375_WRITE_DATA(0x00);  
    delay2us();

    // 配置中断
    printf("配置中断...\r\n");
    EXTI_GPIO_Config(EXTI_GPIO2, EXTI_LINE_5);
    EXTI_FALEDGE_TRIG_ENABLE(EXTI_LINE_5);
    EXTI_IT_ENABLE(EXTI_LINE_5);
    EXTI_ITF_CLEAR(EXTI_LINE_5);

    NVIC_EnableIRQ(EXTI5_IRQn);
    NVIC_SetPriority(EXTI5_IRQn, 0x0);

    printf("CH375 UPS设备初始化完成\r\n");
    printf("设备状态: %d (等待USB连接)\r\n", UsbDeviceState);
}

/***********************************************************************************
****状态查询函数
************************************************************************************/
const char* UPS_GetDeviceStateString(void)
{
    switch (UsbDeviceState) {
        case USB_STATE_DETACHED:    return "未连接";
        case USB_STATE_DEFAULT:     return "默认状态";
        case USB_STATE_ADDRESS:     return "已分配地址";
        case USB_STATE_CONFIGURED:  return "已配置";
        default:                    return "未知";
    }
}

void UPS_PrintStatus(void)
{
    printf("=== UPS设备状态 ===\r\n");
    printf("USB状态: %s (%d)\r\n", UPS_GetDeviceStateString(), UsbDeviceState);
    printf("枚举步骤: %d\r\n", enumeration_step);
    printf("设备地址: %d\r\n", VarUsbAddress);
    printf("配置标志: %d\r\n", CH375CONFLAG);
    printf("==================\r\n");
}

u8 UPS_IsReady(void) 
{ 
    return (UsbDeviceState == USB_STATE_CONFIGURED); 
}