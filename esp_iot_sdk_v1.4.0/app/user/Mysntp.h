/*
 * Mysntp.h
 *
 *  Created on: 2015��12��21��
 *      Author: Administrator
 */

#ifndef APP_USER_MYSNTP_H_
#define APP_USER_MYSNTP_H_

#include "ets_sys.h"
#include "osapi.h"
#include "uart.h"
#include "mem.h"
#include "GPIO.h"

#include "user_interface.h"
#include "smartconfig.h"
#include "espconn.h"
#include "user_main.h"
#include "string.h"

//2015-12-22 �����洢ÿ�μ�¼����ʱ�����û�ο�ʼ�µļ�¼����ͨ��SNTP��������ȡ��ǰʱ�䲢�����ڶ����Ĵ洢λ�á�


#define TIME_STARTADDR		0x300   //256*4K=1M  3M+512K
#define TIME_ENDADDR		0x380   //80H=128   128*4=512K
#define TIME_SAVE_LEN		32		//32byte �洢ʱ���ַ��� 24+6nc+2��¼����
#define NUM_PERPAGE			(SPI_FLASH_SEC_SIZE/TIME_SAVE_LEN)
//SECTION_NUM_PerPAGE
//#define TIME_STRUCT_SIZE	10


//SNTP TIME(24byte):Tue Dec 22 11:34:05 2015
typedef union{
	char TimeTab[24];
	struct{
		char WeekTab[4];//Tue-
		char MonthTab[4];//Dec-
		char DayTab[3];//22-
		char HourTab[3];//11:
		char MinTab[3];//34:
		char SecTab[3];//05-
		char YearTab[4];//2015
	}user_time;
}timeunion_t;

typedef struct{
	timeunion_t Time24;
	char Timenc[6];
	uint16 Savesec;
}user_time_t;




void 		ICACHE_FLASH_ATTR 	SaveTime(uint32 current_stamp,uint32 savesec);
user_time_t 					ReadTime(uint16 savesec );
void 		ICACHE_FLASH_ATTR 	GetTime(void);


#endif /* APP_USER_MYSNTP_H_ */
