
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


#define IIC_Add 0xB8    //������ַ

/*****************Function Declaration*************************/
/*----define to easier to use-----*/
        #define uchar unsigned char         
        #define uint  unsigned int
        #define ulong unsigned long
				#define u8 		unsigned char
				#define u16 	unsigned short int
        #define u32 	unsigned  int
				 

/*================================================================
��  Name  ��void Delay(uint t)
��Function��delay
��  Notes ��
�� Author ��dingshidong
��  Data  ��2012.08.07 
================================================================*/
//void Delay(uint t);

/*================================================================
��  Name  ��void I2CDelay (uchar t)
��Function��ģ��IIC�õĶ���ʱ
��  Notes ��
�� Author ��dingshidong
��  Data  ��2012.08.07 
================================================================*/
void I2CDelay1(u16 t);

/*================================================================
��  Name  ��void I2CInit(void)
��Function��I2C��ʼ��������״̬
��  Notes ��
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
void I2CInit1(void);

/*================================================================
��  Name  ��void I2CStart(void)
��Function��I2C��ʼ�ź�
��  Notes ��SCL��SDAͬΪ�ߣ�SDA����ɵ�֮��SCL����ɵ�
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
void I2CStart1(void);

/*================================================================
���� �ơ�void I2CStop(void)
���� �ܡ�I2Cֹͣ�ź�
���� ע��SCL��SDAͬΪ�ͣ�SCL����ɸ�֮��SDA����ɸ�
���� �ߡ�dingshidong
��ʱ �䡿2012��8��7��
================================================================*/
void I2CStop1(void);

/*================================================================
��  Name  ��uchar I2C_Write_Byte(uchar WRByte)
��Function��I2Cдһ���ֽ����ݣ�����ACK����NACK
��  Notes ���Ӹߵ��ͣ����η���
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
uchar I2C_Write_Byte1(uchar Write_Byte);

/*================================================================
��  Name  ��uchar I2C_Read_Byte(uchar AckValue)
��Function��I2C��һ���ֽ����ݣ���ڲ������ڿ���Ӧ��״̬��ACK����NACK
��  Notes ���Ӹߵ��ͣ����ν���
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
uchar I2C_Read_Byte1(uchar AckValue);


/*================================================================
��  Name  ��void SoftReset(void)
��Function��SHT20�����λ���������е���
��  Notes ���Ӹߵ��ͣ����ν���
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
void SoftReset1(void);

/*================================================================
��  Name  ��void SET_Resolution(void)
��Function��д�Ĵ��������÷ֱ���
��  Notes ��
�� Author ��dingshidong
��  Data  ��2012.08.07
================================================================*/
void SET_Resolution1(void);

/*================================================================
��  Name  ��float ReadSht20(char whatdo)
��Function����ȡ��������
��  Notes ��
�� Author ��dingshidong
��  Data  ��2012.08.07
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

