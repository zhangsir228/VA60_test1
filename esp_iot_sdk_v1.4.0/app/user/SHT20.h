/*
 * SHT20.h
 *
 *  Created on: 2015Äê11ÔÂ3ÈÕ
 *      Author: Administrator
 */

#ifndef __SHT20_H__
#define __SHT20_H__


#define SHT20ADDR 0x80    //SHT20_ADDR
#define ACK	 	0
#define NACK	1

void SET_Resolution(void);
float ReadSHT20(char TMPorRH);
bool SHT20_read(void);

//void




#endif /* APP_INCLUDE_DRIVER_SHT20_H_ */
