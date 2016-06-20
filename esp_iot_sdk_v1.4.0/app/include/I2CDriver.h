
/***************************************************************
*   File name    :  SHT20.h  
*   Description  :  SHT20 Humidity and Temperature Sensors 
*     M C U      :  STC89C52RC
*   Compiler     :  Keil uVision V4.00a (C51)
*   Created by   :  dingshidong
*   Copyright    :  Copyright(c) 2012 Wuxi CASILINK Tech.CO.,Ltd   
*   Created data :  2012.08.07  
*   Modified data:        2012.08.10 
*   Vision       :  V1.0
*****************************************************************/

#ifndef __I2CDRIVER_H
#define __I2CDRIVER_H

/* Includes ------------------------------------------------------------------*/
 #include "stm32f0xx.h"


#define IIC_Add 0xB8    //器件地址

/*****************Function Declaration*************************/
/*----define to easier to use-----*/
        #define uchar unsigned char         
        #define uint  unsigned int
        #define ulong unsigned long
				#define u8 		unsigned char
				#define u16 	unsigned short int
        #define u32 	unsigned  int
				 

/*================================================================
【  Name  】void Delay(uint t)
【Function】delay
【  Notes 】
【 Author 】dingshidong
【  Data  】2012.08.07 
================================================================*/
//void Delay(uint t);

/*================================================================
【  Name  】void I2CDelay (uchar t)
【Function】模拟IIC用的短延时
【  Notes 】
【 Author 】dingshidong
【  Data  】2012.08.07 
================================================================*/
void I2CDelay1(u16 t);

/*================================================================
【  Name  】void I2CInit(void)
【Function】I2C初始化，空闲状态
【  Notes 】
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
void I2CInit1(void);

/*================================================================
【  Name  】void I2CStart(void)
【Function】I2C起始信号
【  Notes 】SCL、SDA同为高，SDA跳变成低之后，SCL跳变成低
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
void I2CStart1(void);

/*================================================================
【名 称】void I2CStop(void)
【功 能】I2C停止信号
【备 注】SCL、SDA同为低，SCL跳变成高之后，SDA跳变成高
【作 者】dingshidong
【时 间】2012年8月7日
================================================================*/
void I2CStop1(void);

/*================================================================
【  Name  】uchar I2C_Write_Byte(uchar WRByte)
【Function】I2C写一个字节数据，返回ACK或者NACK
【  Notes 】从高到低，依次发送
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
uchar I2C_Write_Byte1(uchar Write_Byte);

/*================================================================
【  Name  】uchar I2C_Read_Byte(uchar AckValue)
【Function】I2C读一个字节数据，入口参数用于控制应答状态，ACK或者NACK
【  Notes 】从高到低，依次接收
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
uchar I2C_Read_Byte1(uchar AckValue);


/*================================================================
【  Name  】void SoftReset(void)
【Function】SHT20软件复位，主函数中调用
【  Notes 】从高到低，依次接收
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
void SoftReset1(void);

/*================================================================
【  Name  】void SET_Resolution(void)
【Function】写寄存器，设置分辨率
【  Notes 】
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
void SET_Resolution1(void);

/*================================================================
【  Name  】float ReadSht20(char whatdo)
【Function】读取函数函数
【  Notes 】
【 Author 】dingshidong
【  Data  】2012.08.07
================================================================*/
float ReadSht201(char whatdo);




void I2C_delay1(void);
bool I2C_Start1(void);
void I2C_Stop1(void);
void I2C_Ack1(void);
void I2C_NoAck1(void);
bool I2C_WaitAck1(void);
void I2C_SendByte1(u8 SendByte);
u8 I2C_ReceiveByte1(void);
void Systick_Delay_1ms1(uint32_t nCount);
bool I2C_CMD1(u8 *pbuffer);
bool I2C_Read1(u8 *pj);
bool  SH120_Read1(void);
void TempAndRHDisp1(void);

#endif 

