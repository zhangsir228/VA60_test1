/*
 * TCPclient.h
 *
 *  Created on: 2015年11月4日
 *      Author: Administrator
 */
#ifndef APP_USER_TCPCLIENT_H_
#define APP_USER_TCPCLIENT_H_

#include "os_type.h"
#include "espconn.h"

#define SENSOR_DATA_NUM 20
#define SENSOR_DATA_MEM_ADDR 120
#define INIT_MAGIC 0x7e7e55aa

#define RTC_CNT_ADDR 120
#define DSLEEP_TIME SENSOR_DEEP_SLEEP_TIME
#define SENSOR_DATA_SET_LEN      50 // max LEN OF STR LIKE :  {"x":1,"y":351}
#define SENSOR_DATA_STREAM "supply-voltage"  //in accord with the data-point name on the server
#define    __SET__DEEP_SLEEP__WAKEUP_NO_RF__    system_deep_sleep_set_option(4)
#define    __SET__DEEP_SLEEP__WAKEUP_NORMAL__	system_deep_sleep_set_option(1)


typedef struct
{
	float 	Vol;
	float	Cur;
	float 	Pow;
	float	Kwh;
	float 	nKwh;
}ATT7053_data_type;


typedef struct{
	uint32 init_flg;
	uint16 cnt;
	uint16 wifi_config;
	uint16 data[SENSOR_DATA_NUM];
}SENSOR_DATA_RTC_MEM;

struct VA_saved_param{

	uint32 	init_flag;
	uint32 	param_interval;
	char 	param_ap_ssid[64];
	char 	param_ap_psw[64];
	char    param_my_ssid[64];//8266 ssid
	char	param_my_psw[64];//8266 psw
	char 	param_SN[10];		//lewei SN
	uint16	param_NC;
	uint32	param_SaveSection;//当前存储的区段 (4K一页 注意每次缓存数以及 多少次缓存翻页)
	uint16	param_ModelFunc;//模块当前的功能 1：get data record 2.get record, udp  3.get record, udp, tcp send
	sint16	param_TCal;
	sint16	param_HCal;
	uint16	param_test;
	char	param_userkey[64];//userkey 33bit
	char    param_OK;//the flag of right data
};

LOCAL void ICACHE_FLASH_ATTR 	data_func() ;

LOCAL void ICACHE_FLASH_ATTR 	tcp_recvTask(os_event_t *events);
void watch_timer_cb(void);
void 	user_tcp_connect(void);
LOCAL void ICACHE_FLASH_ATTR 	user_tcp_connect_cb(void *arg);
LOCAL void ICACHE_FLASH_ATTR 	user_tcp_recon_cb(void *arg, sint8 err);
LOCAL void ICACHE_FLASH_ATTR 	user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length);
LOCAL void ICACHE_FLASH_ATTR	user_tcp_sent_cb(void *arg);
LOCAL void ICACHE_FLASH_ATTR	user_tcp_discon_cb(void *arg);
LOCAL void ICACHE_FLASH_ATTR	user_sent_data(struct espconn *pespconn);

LOCAL void ICACHE_FLASH_ATTR	interval_timer_cb();




#endif /* APP_USER_TCPCLIENT_H_ */
