/*
 * TCPclient.c
 *
 *  Created on: 2015年11月4日
 *      Author: Administrator
 */
#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "uart.h"
#include "espconn.h"
#include "upgrade.h"

#include "TCPclient.h"
#include "SHT20.h"
#include "UDPsend.h"

void ICACHE_FLASH_ATTR			record_sensor				(void);

#define NET_DOMAIN "cn.bing.com"
#define pheadbuffer "GET / HTTP/1.1\r\nUser-Agent: curl/7.37.0\r\nHost: %s\r\nAccept: */*\r\n\r\n"
#define OneMin 60000000

#define packet_size   (2 * 1024)




 char    POST_main[]="POST /api/V1/gateway/UpdateSensorsbysn/5FD41B94 HTTP/1.1\r\nHost: ht.api.lewei50.com\r\nConnection:keep-alive\r\nContent-Length:   \r\n\r\n[{\"Name\":\"T1\",\"Value\":\"    \"},{\"Name\":\"H1\",\"Value\":\"    \"}]\r\n";

    //char    POST_tab1[]="POST /api/V1/gateway/UpdateSensorsbysn/CD473DF8 HTTP/1.1\r\n";//7132A730
    //char    POST_tab1[]="POST /api/V1/gateway/UpdateSensorsbysn/7132A730 HTTP/1.1\r\n";//7132A730
    char    POST_tab1[]="POST /api/V1/gateway/UpdateSensorsbysn/";//   7132A730  5FD41B94将设备SN由保存的数据提取 在esp8266_init
    char    POST_tab3[]="         HTTP/1.1\r\nHost: ht.api.lewei50.com\r\n";//the first 8 pos for SN
    char    POST_tab4[]="Connection:keep-alive\r\n";
    char    POST_tab5[]="Content-Length:   \r\n\r\n";
    char    POST_tab6[]="[{\"Name\":\"T1\",\"Value\":\"    \"},";//[{"Name":"temperature","Value":"32"}]
    char    POST_tab7[]="{\"Name\":\"H1\",\"Value\":\"    \"}]\r\n";

extern int16_t   SHT_humi;
extern int16_t   SHT_temp;
extern uint8 	UDPsend_tab[];
extern uint8 	UDPdata[];

extern uint32 interval_timer_cnt;
extern struct VA_saved_param	VA_param;
extern os_timer_t led_flash_timer;
extern os_timer_t run_timer;

typedef enum {
	work_state_noip           = 0,	//did not got ip
	work_state_set_ser,				//set server
	work_state_got_set,				//got set data
	work_state_conn_ap,				//connected to another AP
	work_state_conn_ser,			//connected to server
	work_state_send_ok				//send OK
} _work_state;
extern work_state;



ATT7053_data_type    ATT7053_data;

extern struct espconn esp_conn;
os_timer_t test_timer;
struct espconn user_tcp_conn;
struct _esp_tcp user_tcp;
ip_addr_t tcp_server_ip;

os_timer_t interval_timer;//post interval timer
os_timer_t watch_timer;


#define tcp_recvTaskPrio        1
#define tcp_recvTaskQueueLen    10
os_event_t    tcp_recvTaskQueue[tcp_recvTaskQueueLen];

LOCAL void ICACHE_FLASH_ATTR ///////
tcp_recvTask(os_event_t *events)
{
	espconn_disconnect(&user_tcp_conn);os_printf("dis ttt\r\n");
}





void watch_timer_cb(void)
{
	//qq system_restart();
	//system_deep_sleep(OneMin);
	espconn_disconnect(&user_tcp_conn);
	system_deep_sleep(interval_timer_cnt);//
}

void ICACHE_FLASH_ATTR
user_tcp_connect(void)
{
	// struct espconn *myesp_conn = &user_tcp_conn;
	 espconn_disconnect(&user_tcp_conn);//disconnect first.

	 os_timer_disarm(&watch_timer);//限制每次发送的时间不超过20s
	 os_timer_setfn(&watch_timer,watch_timer_cb,NULL);
	//qq os_timer_arm(&watch_timer,15000,1);//20s 相当于watchdog//限制获取

	 user_tcp_conn.proto.tcp = &user_tcp;
	 user_tcp_conn.type = ESPCONN_TCP;
	 user_tcp_conn.state = ESPCONN_NONE;

	 const char esp_tcp_server_ip[4] = {42,121,254,11}; // remote IP of TCP server
	 //const char esp_tcp_server_ip[4] = {192,168,1,220}; // remote IP of TCP server

	 os_memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_tcp_server_ip, 4);

	 user_tcp_conn.proto.tcp->remote_port = 80;  // remote port

	 user_tcp_conn.proto.tcp->local_port = espconn_port(); //local port of ESP8266

	 espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb); // register connect callback
	 espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb); // register reconnect callback as error handler

	 switch(espconn_connect(&user_tcp_conn)){
	 	 case ESPCONN_OK: 			work_state=work_state_conn_ser;
									os_timer_disarm(&led_flash_timer);//停止定时器
									gpio_output_set( BIT5, 0, BIT5, 0);//GPIO5 --1 熄灭led
	 	 	 	 	 	 	 	 	 break;

	 	 case ESPCONN_MEM:			os_printf("Out of memory error.");
	 	 	 	 	 	 	 	 	 gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	 	 	 	 	 	 	 	 	 //qq system_deep_sleep(interval_timer_cnt);
									 work_state=work_state_conn_ap;
									 os_timer_arm(&run_timer,500,1);
	 	 	 	 	 	 	 	 	 break;//当连接出错时睡眠再重试

	 	 case ESPCONN_TIMEOUT: 		os_printf("Timeout.");os_timer_disarm(&interval_timer);
	 	 	 	 	 	 	 	 	 gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	 	 	 	 	 	 	 	 	 //qq system_deep_sleep(interval_timer_cnt);
									 work_state=work_state_conn_ap;
									 os_timer_arm(&run_timer,500,1);
	 	 	 	 	 	 	 	 	 break;

	 	 case ESPCONN_RTE: 			os_printf("Routing problem.");os_timer_disarm(&interval_timer);
	 	 	 	 	 	 	 	 	 gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	 	 	 	 	 	 	 	 	 //qq system_deep_sleep(interval_timer_cnt);
	 	 	 	 	 	 	 	 	 work_state=work_state_conn_ap;
	 	 	 	 	 	 	 	 	 os_timer_arm(&run_timer,500,1);
	 	 	 	 	 	 	 	 	 break;

	 	 case ESPCONN_INPROGRESS:	os_printf("Operation in progress");os_timer_disarm(&interval_timer);
	 	 	 	 	 	 	 	 	 gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	 	 	 	 	 	 	 	 	 //qq system_deep_sleep(interval_timer_cnt);
									 work_state=work_state_conn_ap;
									 os_timer_arm(&run_timer,500,1);
	 	 	 	 	 	 	 	 	 break;

	 	 default :					 os_printf("unknow reason TCP connect fail!\r\n");
	 	 	 	 	 	 	 	 	 gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	 	 	 	 	 	 	 	 	 //qq system_deep_sleep(interval_timer_cnt);
									 work_state=work_state_conn_ap;
									 os_timer_arm(&run_timer,500,1);
	 	 	 	 	 	 	 	 	 break;
	 		//ESPCONN_RTE
	 }
}

/******************************************************************************
 * FunctionName : user_tcp_connect_cb
 * Description  : A new incoming tcp connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_connect_cb(void *arg)
{
    struct espconn *pespconn = arg;

    os_printf("connect ht.lewei50,com succeed !!! \r\n");

    espconn_regist_recvcb(pespconn, user_tcp_recv_cb);
    espconn_regist_sentcb(pespconn, user_tcp_sent_cb);
    espconn_regist_disconcb(pespconn, user_tcp_discon_cb);

   /*qq if(SHT20_read())//to void record_sensor();
	{
		os_printf("SHT_temp:%d!!! \r\n",SHT_temp);
		os_printf("SHT_humi:%d!!! \r\n",SHT_humi);
	}*///the data collection moved to  init_done _cb

    user_sent_data(pespconn);


    //qqq os_printf("prepare to sleep interval_timer_cnt:%d!!! \r\n",interval_timer_cnt);
    /*os_timer_disarm(&interval_timer);//led flash timer   1Hz
    os_timer_setfn(&interval_timer,interval_timer_cb,NULL);
    os_timer_arm(&interval_timer,15000,1);*/

    //system_deep_sleep(interval_timer_cnt);

}

/******************************************************************************
 * FunctionName : user_tcp_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recon_cb(void *arg, sint8 err)
{
   //error occured , tcp connection broke. user can try to reconnect here.
	//ESCONN_TIMEOUT
    os_printf("reconnect callback, error code %d !!! \r\n",err);
    os_printf("sleep:%d S. \r\n",interval_timer_cnt/1000000); os_printf("1t\r\n");
    gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0

    //qq system_deep_sleep(interval_timer_cnt);
	 work_state=work_state_conn_ap; os_printf("2t\r\n");

	 os_timer_arm(&run_timer,500,1);//使用此定时器用于等待udp发送完成后再进入休眠
}

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
   //received some data from tcp connection
	//复制指定长度的的内容至临时数组
	char temptab[2048];
	os_memcpy(temptab, pusrdata, length);
	*(temptab+length)=0;

	os_printf("tcp recv: %s \r\n", temptab);//接受server 应答数据

    if((os_strstr(temptab,"{\"Successful\":true,\"Message\":\"Successful. \"}"))||(os_strstr(temptab,"limit is 10s")))
    {
    	//qq os_sprintf(UDPdata,"{\"device\":\"TH_meter\",\r\n \"SN\":%s,\r\n \"data\":\r\n {\"temprature\":%d,\r\n  \"humidity\":%d\r\n }\r\n}\r\n",VA_param.param_SN,SHT_temp,SHT_humi);
    	//qq user_udp_send(UDPdata);

    	work_state=work_state_send_ok;
    	//qq system_deep_sleep(interval_timer_cnt);
    	os_timer_arm(&run_timer,500,1);//使用此定时器用于等待udp发送完成后再进入休眠
    }
   /* else if(os_strstr(pusrdata,"limit is 10s"))
    {
    	os_printf("lewei server return a time limit warning!!! \r\n");

    }*/
    else
    {
    	os_printf("lewei server return a error message!!! \r\n");
    	gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0 点亮led

    	//qq user_tcpserver_init(80);
		//qq os_timer_arm(&led_flash_timer,1000,1);
		work_state=work_state_conn_ser;
		os_timer_arm(&run_timer,500,1);
    	//stop here,and the module will restart by the watch timer 20s later.
    }

}
/******************************************************************************
 * FunctionName : user_tcp_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_sent_cb(void *arg)
{
   //data sent successfully
    os_printf("tcp sent succeed !!! \r\n");
}
/******************************************************************************
 * FunctionName : user_tcp_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_discon_cb(void *arg)
{
   //tcp disconnect successfully
    os_printf("tcp disconnect succeed !!! \r\n");
    os_timer_disarm(&run_timer);
    os_timer_arm(&run_timer,500,1);
}
/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_sent_data(struct espconn *pespconn)
{
	sint8 i=0;
	uint16 POST_length,data_length;
	char *pStr;

    //qqq os_printf("ready to post !!! \r\n");
    for(i=0;i<8;i++)//use the last 8 SN
    {
    	POST_tab3[i]=VA_param.param_SN[i+2];//mySN[i];
    }
    POST_length=strlen(POST_tab1)+strlen(POST_tab3)\
                  +strlen(POST_tab4)+strlen(POST_tab5)\
                  +strlen(POST_tab6)+strlen(POST_tab7);
    data_length=strlen(POST_tab6)+strlen(POST_tab7);
      *(POST_tab5+16)=data_length/10+0x30;
      *(POST_tab5+17)=data_length%10+0x30;

    pStr=strstr(POST_tab6,"Value");
    *(pStr+8)=SHT_temp/100+0x30;
    *(pStr+9)=SHT_temp%100/10+0x30;
    *(pStr+10)='.';
    *(pStr+11)=SHT_temp%10+0x30;

    pStr=strstr(POST_tab7,"Value");
    *(pStr+8)=SHT_humi/100+0x30;
    *(pStr+9)=SHT_humi%100/10+0x30;
    *(pStr+10)='.';
    *(pStr+11)=SHT_humi%10+0x30;

    strcpy(POST_main,POST_tab1);
          strcat(POST_main,POST_tab3);
          strcat(POST_main,POST_tab4);
          strcat(POST_main,POST_tab5);
          strcat(POST_main,POST_tab6);
          strcat(POST_main,POST_tab7);

	  //qqq os_printf("%s",POST_main);//上发数据
	  espconn_sent(pespconn, POST_main,POST_length);

    //os_sprintf(pbuf, pheadbuffer, NET_DOMAIN);
    //espconn_sent(pespconn, pbuf, os_strlen(pbuf));
    //os_free(pbuf);

}

LOCAL void ICACHE_FLASH_ATTR
interval_timer_cb()
{
	user_tcp_connect();
}








