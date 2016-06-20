/*
 * user_main.h
 *
 *  Created on: 2015Äê12ÔÂ14ÈÕ
 *      Author: Administrator
 */

#ifndef APP_USER_USER_MAIN_H_
#define APP_USER_USER_MAIN_H_

#define Temperature	0
#define Humidity	1
#define SENSOR_DATA_STARTADDR	0x100   //256*4K=1M
#define SENSOR_DATA_ENDADDR		0x2ff   //0x300*1000=300000=3M]

#define INITFLAG_MEM_NUM 	32 //32section  32per     X=1024/n     32=1024/32
#define INITFLAG_MEM_ADDR 	120
#define NOT_POWER_ON		0x7e7e55aa

#define SECTION_NUM_PerPAGE		(SPI_FLASH_SEC_SIZE/(INITFLAG_MEM_NUM*4))
#define MAX_SECTION				(0x200*SECTION_NUM_PerPAGE)

typedef union{
	int32_t 	LongA;
	int16_t		shortB[2];
	char		charCC[4];
}Mix32;


#endif /* APP_USER_USER_MAIN_H_ */
