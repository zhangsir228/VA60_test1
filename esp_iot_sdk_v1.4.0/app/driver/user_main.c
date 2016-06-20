/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "uart.h"

#include "user_interface.h"
#include "smartconfig.h"
#include "espconn.h"

#include "html_file.h"
#include "SHT20.h"
#include "i2c_master.h"



LOCAL struct espconn esp_conn;
LOCAL esp_tcp esptcp;
LOCAL os_timer_t test_timer;
uint32 content_length;
uint32 ssid_length;
char apssid[10][32];
char myssid[32];
char mypsw[64];
char mymacaddr[6];
char conn_ssid[32];
char conn_psw[64];
char ssidloop=0;
char ssid_send[1024];//the format to send include SSIDs

extern int16_t   SHT_humi;
extern int16_t   SHT_temp;

#define SERVER_LOCAL_PORT   80//1112

os_timer_t rtc_test_t;
os_timer_t period_1s;
void ICACHE_FLASH_ATTR user_esp_platform_check_ip(void);

void test_timer_cb()
{
	 os_printf("\r\nready to sleep.\r\n");

	 system_deep_sleep_set_option(2);//Don't make RF calibration after the waking-up from deep-sleep with a lower power consumption.

	 system_deep_sleep(3100000);//uint32 time_in_us   sleep  3S

}

void period_5s_timer_cb()
{
	if(SHT20_read())
	{
		//os_printf("SHT20_read succeed!!! \r\n");
		os_printf("SHT_temp:%d!!! \r\n",SHT_temp);
		os_printf("SHT_humi:%d!!! \r\n",SHT_humi);
	}
	 //os_printf("\r\nI'm running\r\n");

	 /*static uint8 cnt10;

	 	cnt10=cnt10^1;

	 	if(cnt10==0)
	 	{
	 		gpio_output_set( 0, BIT2, BIT2, 0);//GPIO2 --0
	 	}
	 	else
	 	{
	 		gpio_output_set( BIT2, 0, BIT2, 0);//GPIO2 --0
	 	}*/
}


/******************************************************************************
 * FunctionName : tcp_server_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_sent_cb(void *arg)
{
   //data sent successfully
    //os_printf("tcp sent cb \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
	char cStr[200];
	char SendStr[4096];
	char *pBuf,*pStr,i=0,templen;
	char *value_temp[65];
	struct station_config stationConf;
/*********************************************/
	content_length=os_strlen(before_ssid_html)+os_strlen(before_end_html)+ssid_length;
	os_sprintf(cStr,"%d",content_length);
	    res200_html[79]=*cStr;
	    res200_html[80]=*(cStr+1);
	    res200_html[81]=*(cStr+2);
	    res200_html[82]=*(cStr+3);//将总共需要发送的字符数写入

    //received some data from tcp connection
   struct espconn *pespconn = arg;
   //os_strlen
   if(os_strstr(pusrdata,"GET"))
   {
	   os_strcpy(SendStr,res200_html);
	   os_strcat(SendStr,before_ssid_html);
	   os_strcat(SendStr,ssid_send);
	   os_strcat(SendStr,before_end_html);
	   espconn_sent(pespconn, SendStr, os_strlen(SendStr));
	   //espconn_sent(pespconn, res200_html, os_strlen(res200_html));
	   //espconn_sent(pespconn, before_ssid_html, os_strlen(before_ssid_html));
	   //espconn_sent(pespconn, before_end_html, os_strlen(before_end_html));
	   os_printf("true tcp recv : %s \r\n", pusrdata);
   }
   else if(pBuf=strstr(pusrdata,"ssid="))
   {
	   os_printf("%s\r\n",pBuf);
	   while ( ( pStr = strtok ( pBuf, "=" ) ) != NULL )
	  		{
	  			value_temp[ i ++ ] = pStr;
	  			pBuf = NULL;
	        if(i>5)break;
	  		}
	  os_strcpy(conn_ssid , apssid[ssidloop-(*(value_temp[1])-0x30)]);
	   	   pBuf=value_temp[2];
	       while(*(pBuf++)!='&'){;}
	       *(--pBuf)=0;
	       templen=strlen(value_temp[2]);
	       os_strcpy(conn_psw,value_temp[2]);


	       stationConf.bssid_set = 0;
	       //stationConf.ssid=conn_ssid;
	       //stationConf.password=conn_psw;
	       os_memcpy(&stationConf.ssid, conn_ssid, 32);
	       os_memcpy(&stationConf.password, conn_psw, 64);
	       os_printf("disconnect?: %d! \r\n",wifi_station_disconnect());

	       os_printf("Try to connect to %s! \r\n",stationConf.ssid);
	       os_printf("psw %s! \r\n",stationConf.password);

	       wifi_station_set_config(&stationConf);//config the new connect
	       wifi_station_connect();//connect to new AP
	       os_timer_disarm(&test_timer);
	       os_timer_setfn(&test_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
	       os_timer_arm(&test_timer, 100, 0);
   }
   else
   {
	   os_printf("false tcp recv : %s \r\n", pusrdata);
   }
   os_printf("%s\r\n", arg);
   //espconn_sent(pespconn, pusrdata, length);
}

/******************************************************************************
 * FunctionName : tcp_server_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_discon_cb(void *arg)
{
   //tcp disconnect successfully
    os_printf("tcp disconnect succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recon_cb(void *arg, sint8 err)
{
   //error occured , tcp connection broke.

    os_printf("reconnect callback, error code %d !!! \r\n",err);
}

LOCAL void tcp_server_mutli_sent(void)
{
   struct espconn *pesp_conn = &esp_conn;

   remot_info *premot = NULL;
   uint8 count = 0;
   sint8 value = ESPCONN_OK;
   if (espconn_get_connection_info(pesp_conn,&premot,0) == ESPCONN_OK){
      char *pbuf = "tcp_server_mutli_sent\n";
      for (count = 0; count < pesp_conn->link_cnt; count ++){
         pesp_conn->proto.tcp->remote_port = premot[count].remote_port;
         pesp_conn->proto.tcp->remote_ip[0] = premot->remote_ip[0];
         pesp_conn->proto.tcp->remote_ip[1] = premot->remote_ip[1];
         pesp_conn->proto.tcp->remote_ip[2] = premot->remote_ip[2];
         pesp_conn->proto.tcp->remote_ip[3] = premot->remote_ip[3];
      //   os_memcpy(pesp_conn->proto.tcp->remote_ip, premot->remote_ip);
         //espconn_sent(pesp_conn, pbuf, os_strlen(pbuf));
      }
   }
}

/******************************************************************************
 * FunctionName : tcp_server_listen
 * Description  : TCP server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_listen(void *arg)
{
    struct espconn *pesp_conn = arg;
    os_printf("tcp_server_listen !!! \r\n");

    espconn_regist_recvcb(pesp_conn, tcp_server_recv_cb);
    espconn_regist_reconcb(pesp_conn, tcp_server_recon_cb);
    espconn_regist_disconcb(pesp_conn, tcp_server_discon_cb);

    espconn_regist_sentcb(pesp_conn, tcp_server_sent_cb);
   tcp_server_mutli_sent();
}

/******************************************************************************
 * FunctionName : user_tcpserver_init
 * Description  : parameter initialize as a TCP server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_tcpserver_init(uint32 port)
{
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&esp_conn, tcp_server_listen);

    sint8 ret = espconn_accept(&esp_conn);
    espconn_regist_time(&esp_conn,120,0);//timeout interval.

    os_printf("espconn_accept [%d] !!! \r\n", ret);

}


/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_check_ip(void)
{
    struct ip_info ipconfig;

   //disarm timer first
    os_timer_disarm(&test_timer);

   //get ip info of ESP8266 station
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {

      os_printf("got ip !!! \r\n");
      espconn_delete(&esp_conn);
      user_tcpserver_init(SERVER_LOCAL_PORT);

		 	 os_timer_disarm(&period_1s);//led flash timer   1Hz
		 	 os_timer_setfn(&period_1s,period_5s_timer_cb,NULL);
		 	 os_timer_arm(&period_1s,5000,1);
    } else {
    		os_timer_disarm(&period_1s);//disable
        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {

         os_printf("connect fail !!! \r\n");

        } else {

           //re-arm timer to check ip
            os_timer_setfn(&test_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&test_timer, 400, 0);
            os_printf("connecting:%d \r\n",wifi_station_get_connect_status());
        }
    }
}

/******************************************************************************
 * FunctionName : user_set_station_config
 * Description  : set the router info which ESP8266 station will connect to
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_set_station_config(void)
{
   // Wifi configuration
   char ssid[32] = "VA_office";
   char password[64] = "69523223";//"32232596";
   struct station_config stationConf;

   //need not mac address
   stationConf.bssid_set = 0;

   //Set ap settings
   os_memcpy(&stationConf.ssid, ssid, 32);
   os_memcpy(&stationConf.password, password, 64);
   wifi_station_set_config(&stationConf);
   //os_printf("Try to connect to RD! \r\n");
   //set a timer to check whether got ip from router succeed or not.
   os_timer_disarm(&test_timer);
   os_timer_setfn(&test_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
   os_timer_arm(&test_timer, 400, 0);

}

//these below  copy from forum
/******************************************************************************
 * FunctionName : scan_done
 * Description  : scan done callback
 * Parameters   :  arg: contain the aps information;
                          status: scan over status
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
scan_done(void *arg, STATUS status)
{
  uint8 ssid[33];
  char temp[128];
  char str_len=0;
  char ssid_NO=0;
  char cStr[256];

  if (status == OK)
  {
    struct bss_info *bss_link = (struct bss_info *)arg;
    bss_link = bss_link->next.stqe_next;//ignore the first one , it's invalid.

    	ssidloop=0;

    while (bss_link != NULL)
    {
      os_memset(ssid, 0, 33);
      if (os_strlen(bss_link->ssid) <= 32)
      {
        os_memcpy(ssid, bss_link->ssid, os_strlen(bss_link->ssid));
      }
      else
      {
        os_memcpy(ssid, bss_link->ssid, 32);
      }
      os_memcpy( apssid[ssidloop++],ssid,os_strlen(ssid));
      //myssid[ssidloop++]=ssid;

      os_printf("(%d,\"%s\",%d,\""MACSTR"\",%d)\r\n",
                 bss_link->authmode, ssid, bss_link->rssi,
                 MAC2STR(bss_link->bssid),bss_link->channel);
      bss_link = bss_link->next.stqe_next;
    }
  }
  else
  {
     os_printf("scan fail !!!\r\n");
  }
  wifi_get_macaddr(SOFTAP_IF,mymacaddr);
  ssid_send[0]='\0';
  ssid_NO=0;
  while(ssidloop-ssid_NO>0)
  {
	ssid_NO++;
	if(ssid_NO>9){ssid_length++;}
	os_sprintf ( cStr, "<option value=\"%d\" class=\"column1\">%s</option>\r\n",ssid_NO,apssid[ssidloop-ssid_NO]  );
	//ssidloop--;
	os_printf("%s\r\n",cStr);
	ssid_length+=os_strlen(cStr);
	os_strcat(ssid_send,cStr);
	//ssid_send
  }

}

/******************************************************************************
 * FunctionName : user_scan
 * Description  : wifi scan, only can be called after system init done.
 * Parameters   :  none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_scan(void)
{
   if(wifi_get_opmode() == SOFTAP_MODE)
   {
     os_printf("ap mode can't scan !!!\r\n");
     return;
   }
   wifi_station_scan(NULL,scan_done);

}



void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
    switch(status) {
        case SC_STATUS_WAIT:
            os_printf("SC_STATUS_WAIT\n");
            break;
        case SC_STATUS_FIND_CHANNEL:
            os_printf("SC_STATUS_FIND_CHANNEL\n");
            break;
        case SC_STATUS_GETTING_SSID_PSWD:
            os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
			sc_type *type = pdata;
            if (*type == SC_TYPE_ESPTOUCH) {
                os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
            } else {
                os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
            }
            break;
        case SC_STATUS_LINK:
            os_printf("SC_STATUS_LINK\n");
            struct station_config *sta_conf = pdata;

	        wifi_station_set_config(sta_conf);
	        wifi_station_disconnect();
	        wifi_station_connect();
            break;
        case SC_STATUS_LINK_OVER:
            os_printf("SC_STATUS_LINK_OVER\n");
            if (pdata != NULL) {
                uint8 phone_ip[4] = {0};

                os_memcpy(phone_ip, (uint8*)pdata, 4);
                os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);

                os_timer_disarm(&rtc_test_t);
				os_timer_setfn(&rtc_test_t,test_timer_cb,NULL);
				os_timer_arm(&rtc_test_t,5000,1);
				os_printf("\r\nEnable timer!\r\n");
            }
            smartconfig_stop();
            //enable sleep timer


            break;
    }

}

void user_rf_pre_init(void)
{
}

void user_init(void)
{
	uart_init(115200, 115200);
    os_printf("SDK version:%s\n", system_get_sdk_version());

    i2c_master_gpio_init();
    SET_Resolution();


    //config the GPIO2 as output --1
    	/*PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U,0);//config the pin2 for LED
        PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);
        gpio_output_set( BIT2, 0, BIT2, 0);
*/
        //sleep timer
   /* os_timer_disarm(&rtc_test_t);
	os_timer_setfn(&rtc_test_t,test_timer_cb,NULL);
	os_timer_arm(&rtc_test_t,10000,1);//ms
	os_printf("\r\nEnable timer!\r\n");*/

	//running timer
    	/*os_timer_disarm(&period_1s);//led flash timer   1Hz
		os_timer_setfn(&period_1s,period_5s_timer_cb,NULL);
		os_timer_arm(&period_1s,5000,1);*/


    //wifi_set_opmode(STATION_MODE);
    wifi_set_opmode(STATIONAP_MODE);
    // wifi scan has to after system init done.


      user_set_station_config();
     system_init_done_cb(user_scan);

    //smartconfig_start(smartconfig_done);
    //sleep_type
}





