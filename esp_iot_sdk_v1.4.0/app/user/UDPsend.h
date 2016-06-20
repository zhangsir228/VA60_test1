/*
 * UDPsenddata.h
 *
 *  Created on: 2015Äê11ÔÂ30ÈÕ
 *      Author: lea.z
 */

#ifndef APP_USER_UDPSEND_H_
#define APP_USER_UDPSEND_H_

#include "espconn.h"

#define ABC4  1234


LOCAL void ICACHE_FLASH_ATTR	user_udp_recv_cb			(void *arg, char *pusrdata, unsigned short length);
void ICACHE_FLASH_ATTR user_udp_send(char *senddata);
LOCAL void ICACHE_FLASH_ATTR	user_udp_sent_cb			(void *arg);
void ICACHE_FLASH_ATTR userUDPinit(void);






#endif /*APP_USERUDP_senddata_H_ */


