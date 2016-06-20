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
#include "mem.h"
#include "GPIO.h"

#include "user_interface.h"
#include "smartconfig.h"
#include "espconn.h"

#include "html_file.h"
#include "SHT20.h"
#include "i2c_master.h"
#include "lewei50.h"
#include "TCPclient.h"
#include "UDPsend.h"
#include "user_main.h"
#include "Mysntp.h"

#include "hw_timer.c"


typedef struct{
	uint32 init_flg;
	uint16 cnt;
	uint16 wifi_config;
	Mix32 data[INITFLAG_MEM_NUM];//[0]=temprature  [1]=humidity;
}INITFLAG_MEM;

INITFLAG_MEM init_data ;


struct espconn esp_conn;
LOCAL esp_tcp esptcp;
LOCAL os_timer_t check_ip_timer;



typedef enum {
	work_state_noip           = 0,	//did not got ip
	work_state_set_ser,				//set server
	work_state_got_set,				//got set data
	work_state_conn_ap,				//connected to another AP
	work_state_conn_ser,			//connected to server
	work_state_send_ok				//send OK
} _work_state;
_work_state work_state;

uint32 interval_timer_cnt;
uint32 content_length;
uint32 ssid_length;
char apssid[10][32];
char myssid[32];
char mypsw[64];
char mymacaddr[]="A1:B2:C3:D4:E5:F6";
char conn_ssid[32];
char conn_psw[64];
char ssidloop=0;
char ssid_send[1024];//the format to send include SSIDs

char Encode_tab[8]="69523164";


char config_state=0;
uint8 HoldFlag=0;//由上位机串口发出=1 不休眠 等待      ；=0 正常工作循环
uint8 HoldSoft;//上位机主动停止模块标志  =1 不休眠 等待      ；=0 正常工作循环
uint8 SNTPtimeout=0;
uint16 FirstStart=0;//初次上电标志
uint16 ConfigServer=0;//建立配置服务器标志
uint32 WitchSec=0;//当前使用的是哪个参数区域

int32	RTimeSec=-1;//需要读取的timesec
int32	RDataSec=-1;//

extern uint8 UDPsend_tab[];
extern uint8 	UDPdata[];
extern int16_t   SHT_humi;
extern int16_t   SHT_temp;


#define SERVER_LOCAL_PORT   80//1112

os_timer_t rtc_test_t;
os_timer_t read_timer;//用于串口读取数据延时以及回调执行
os_timer_t test_timer;
os_timer_t search_timer;//search ap timeout timer
os_timer_t led_flash_timer;
os_timer_t run_timer;

extern os_timer_t watch_timer;
extern struct espconn user_tcp_conn;

void ICACHE_FLASH_ATTR 			esp_VA_load_param			(void);
void ICACHE_FLASH_ATTR			Init_eeprom					(uint32 sec);//初始化配置参数区域
void ICACHE_FLASH_ATTR 			esp_VA_save_param			(uint32 sec);//保存参数到指定区域
void 							test_timer_cb				();
void 							read_timer_cb				();
void 							search_timer_cb				();
void 							led_run_timer_cb			();
void 							led_flash_timer_cb			();

LOCAL void ICACHE_FLASH_ATTR 	tcp_server_sent_cb			(void *arg);
LOCAL void ICACHE_FLASH_ATTR 	tcp_server_recv_cb			(void *arg, char *pusrdata, unsigned short length);
LOCAL void ICACHE_FLASH_ATTR 	tcp_server_discon_cb		(void *arg);
LOCAL void ICACHE_FLASH_ATTR 	tcp_server_recon_cb			(void *arg, sint8 err);
LOCAL void 						tcp_server_mutli_sent		(void);
LOCAL void ICACHE_FLASH_ATTR 	tcp_server_listen			(void *arg);
void ICACHE_FLASH_ATTR 			user_tcpserver_init			(uint32 port);
void ICACHE_FLASH_ATTR 			user_esp_platform_check_ip	(void);
void ICACHE_FLASH_ATTR 			user_set_station_config		(void);
void ICACHE_FLASH_ATTR 			scan_done					(void *arg, STATUS status);
void ICACHE_FLASH_ATTR 			user_scan					(void);
void ICACHE_FLASH_ATTR 			smartconfig_done			(sc_status status, void *pdata);
void ICACHE_FLASH_ATTR 			user_set_softap_config		(void);

void ICACHE_FLASH_ATTR 			key_init					(void);
void ICACHE_FLASH_ATTR 			pin_init					(uint32 gpio_name,uint8 gpio_id,uint8 gpio_func);

void ICACHE_FLASH_ATTR			record_sensor				(void);
void 							Read_data					(uint32 readsec);
void ICACHE_FLASH_ATTR			RTC_memory_read				(void);
void 							user_rf_pre_init			(void);
void 							user_init					(void);


/*************************************/
//20160107新增硬件定时器部分
/***************************************/
#define REG_READ(_r) (*(volatile uint32 *)(_r))
#define WDEV_NOW() REG_READ(0x3ff20c00)
uint32 tick_now2 = 0;

void hw_test_timer_cb(void)
{
	static uint16 j = 0;
	j++;
	if( (WDEV_NOW() - tick_now2) >= 25000000 )
	{
		static u32 idx = 1;
		tick_now2 = WDEV_NOW();
		os_printf("b%u:%d\n",idx++,j);
		j = 0;

		if((HoldFlag==0)&&(HoldSoft==0))//hold to recevie command from PC
		{
			espconn_disconnect(&user_tcp_conn);
			espconn_delete(&user_tcp_conn);
			gpio_output_set( 0, 0, 0, BIT5); os_printf("hw4t\r\n");
			os_printf("in hw_timer sleep:%d S. \r\n",interval_timer_cnt/1000000);
			system_deep_sleep(interval_timer_cnt); os_printf("hw5t\r\n");
		}
	}
}




//void ICACHE_FLASH_ATTR user_esp_platform_check_ip(void);
//void ICACHE_FLASH_ATTR user_tcpserver_init(uint32 port);


#define esp_VA_user_param_addr 0x3c
#define esp_VA_user_param_addr2 0x3d
#define esp_VA_user_param_addr3 0x3e

struct VA_saved_param	VA_param;
struct VA_saved_param	TS_param;

void ICACHE_FLASH_ATTR
esp_VA_load_param(void)
{
	char ssid_temp[32];
	uint16 i=2;
	while(i--)
	{
		//先读取第三区 判断先使用哪个区域的数据
		spi_flash_read(esp_VA_user_param_addr3 * SPI_FLASH_SEC_SIZE,(uint32 *)&WitchSec,sizeof( WitchSec ));
		if(WitchSec==1)
		{
			spi_flash_read(esp_VA_user_param_addr * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param ));
			if(VA_param.param_OK==1)//若此组数据是好的
			{
				WitchSec=1;i=0;
			}
		}
		else if(WitchSec==2)
		{
			spi_flash_read(esp_VA_user_param_addr2 * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param ));
			if(VA_param.param_OK==1)//若此组数据是好的
			{
				WitchSec=2;i=0;
			}
		}
		else
		{
			spi_flash_read(esp_VA_user_param_addr * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param ));
			if(VA_param.param_OK==1)//若此组数据是好的
			{
				WitchSec=1;
				spi_flash_erase_sector(esp_VA_user_param_addr3);
				spi_flash_write(esp_VA_user_param_addr3 * SPI_FLASH_SEC_SIZE,(uint32 *)&WitchSec,sizeof( WitchSec ));
			}
			else
			{
				spi_flash_read(esp_VA_user_param_addr2 * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param ));
				if(VA_param.param_OK==1)//若此组数据是好的
				{
					WitchSec=2;
					spi_flash_erase_sector(esp_VA_user_param_addr3);
					spi_flash_write(esp_VA_user_param_addr3 * SPI_FLASH_SEC_SIZE,(uint32 *)&WitchSec,sizeof( WitchSec ));
				}
				else
				{
					Init_eeprom(3);
					WitchSec=1;
					spi_flash_erase_sector(esp_VA_user_param_addr3);
					spi_flash_write(esp_VA_user_param_addr3 * SPI_FLASH_SEC_SIZE,(uint32 *)&WitchSec,sizeof( WitchSec ));
				}
			}
		}
	}
	os_printf("WitchSec:%d\r\n",WitchSec);//当前区域



	//2015-12-04 新增 将八位SN分别加上对应字符，防止太容易被检索到
	for(i=2;i<10;i++)
	{
		VA_param.param_SN[i]-=Encode_tab[i-2];
	}

	os_printf("VA_param.init_flag:%x \r\n",VA_param.init_flag);
	os_printf("VA_param.param_interval:%x \r\n",VA_param.param_interval);
	os_printf("VA_param.param_ap_ssid:%s \r\n",VA_param.param_ap_ssid);
	os_printf("VA_param.param_ap_psw:%s \r\n",VA_param.param_ap_psw);
	os_printf("VA_param.param_my_ssid:%s \r\n",VA_param.param_my_ssid);
	os_printf("VA_param.param_my_psw:%s \r\n",VA_param.param_my_psw);VA_param.param_NC=0;
	os_printf("VA_param.param_SN:%s \r\n",VA_param.param_SN);
	os_printf("VA_param.param_SaveSection:%d \r\n",VA_param.param_SaveSection);
	os_printf("VA_param.param_ModelFunc:%d \r\n",VA_param.param_ModelFunc);
	os_printf("VA_param.param_TCal:%d \r\n",VA_param.param_TCal);
	os_printf("VA_param.param_HCal:%d \r\n",VA_param.param_HCal);
	os_printf("VA_param.param_test:%d \r\n",VA_param.param_test);
	os_printf("VA_param.param_userkey:%s \r\n",VA_param.param_userkey);
	os_printf("VA_param.param_OK:%d \r\n",VA_param.param_OK);


	switch(VA_param.param_interval){
	case 0x31: 	interval_timer_cnt=(1*60-6)*1000000;break;//1min
	case 0x32: 	interval_timer_cnt=(5*60-6)*1000000;break;//5min
	case 0x33: 	interval_timer_cnt=(10*60-6)*1000000;break;//10min
	case 0x34: 	interval_timer_cnt=(30*60-6)*1000000;break;//30min
	case 0x35: 	interval_timer_cnt=3594000000UL;break;//(60*60-6)*100000;break;//60min  over flow
	default:	interval_timer_cnt=(5*60-6)*1000000;break;
	}
	os_printf("get interval_timer_cnt:%d!!! \r\n",interval_timer_cnt);
}

void ICACHE_FLASH_ATTR
Init_eeprom(uint32 sec)
{
	char ssid_temp[32];
	wifi_get_macaddr(STATION_IF,mymacaddr);
	//	define the myssid by last three mac.
	os_sprintf(ssid_temp,"V&A_%02x%02x%02x wifi_HT",mymacaddr[3], mymacaddr[4], mymacaddr[5]);

	spi_flash_erase_sector(esp_VA_user_param_addr);
	VA_param.init_flag=INIT_MAGIC;
	VA_param.param_interval=0x32;//default 5min
	strcpy(VA_param.param_ap_ssid,"RD");
	strcpy(VA_param.param_ap_psw,"322325961");
	strcpy(VA_param.param_my_ssid,ssid_temp);
	strcpy(VA_param.param_my_psw,"69523223");
	strcpy(VA_param.param_SN,"00B5A3B051");//格式00xxxxxxxx		程序测试样机  OFFICE4

	VA_param.param_NC=0;
	VA_param.param_SaveSection=0;//按照缓存大小为一个计数
	VA_param.param_ModelFunc=3;//
	VA_param.param_TCal = 0;
	VA_param.param_HCal = 0;
	strcpy(VA_param.param_userkey,"cb4e016ea14e40c4a41e9a10f46f0436");
	VA_param.param_OK=1;

	esp_VA_save_param(sec);
}

void ICACHE_FLASH_ATTR
esp_VA_save_param(uint32 sec)
{
	char i=0;
	//2015-12-04 新增 将八位SN分别加上对应字符，防止太容易被检索到
	//strcpy(VA_param.param_SN,"005FD41B94");
	for(i=2;i<10;i++)
	{
		VA_param.param_SN[i]+=Encode_tab[i-2];
	}
	/*if(sec==1)
	{
		spi_flash_erase_sector(esp_VA_user_param_addr);
		spi_flash_write(esp_VA_user_param_addr * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param));
	}*/
	/*if(sec==2)
	{
		spi_flash_erase_sector(esp_VA_user_param_addr2);
		spi_flash_write(esp_VA_user_param_addr2 * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param));
	}*/
	if((sec==1)||(sec==3)||(sec==2))//将保存的数据都写入两个区域
	{
		spi_flash_erase_sector(esp_VA_user_param_addr);
		spi_flash_write(esp_VA_user_param_addr * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param));
		spi_flash_erase_sector(esp_VA_user_param_addr2);
		spi_flash_write(esp_VA_user_param_addr2 * SPI_FLASH_SEC_SIZE,(uint32 *)&VA_param,sizeof( VA_param));
	}

	//2015-12-04 新增 将八位SN分别加上对应字符，防止太容易被检索到
	for(i=2;i<10;i++)
	{
		VA_param.param_SN[i]-=Encode_tab[i-2];
	}
}

void test_timer_cb(void)//now test for sntp get realtime
{
	 //os_printf("\r\nready to sleep.\r\n");
	 //system_deep_sleep_set_option(2);//Don't make RF calibration after the waking-up from deep-sleep with a lower power consumption.
	 //system_deep_sleep(3100000);//uint32 time_in_us   sleep  3S
	//uint16 gpio_status;
	//gpio_status = GPIO_REG_READ(GPIO_IN_ADDRESS);
	//os_printf("gpio_status:%04x! \r\n",gpio_status);

	uint32 current_stamp;
	current_stamp = sntp_get_current_timestamp();
	SNTPtimeout++;
	if(SNTPtimeout>20)//cannot get the real time,
	{
		SNTPtimeout=0;
		os_timer_disarm(&test_timer);
		HoldFlag=0;
	}
	if(current_stamp>0)//等待获取到时间了再打印时间信息
	{
		os_printf("sntp: %d, %s \n",current_stamp, sntp_get_real_time(current_stamp));

		SaveTime(current_stamp,VA_param.param_SaveSection);

		os_timer_disarm(&test_timer);
		HoldFlag=0;
	}

	//os_printf("sntp: %d, %s \n",current_stamp, sntp_get_real_time(current_stamp));

}

//int32	RTimeSec=-1;//需要读取的timesec
//int32	RDataSec=-1;//
void read_timer_cb(void)
{

	os_timer_disarm(&read_timer);
	if(RTimeSec>=0)
	{
		user_time_t my_time;
		my_time=ReadTime(RTimeSec);
		if(my_time.Savesec<=VA_param.param_SaveSection)
		{
			my_time.Timenc[0]=0;//防止字符串调用溢出。
			os_printf("SECTIME:%s\r\n",my_time.Time24.TimeTab);
		}
		RTimeSec=-1;
	}
	else if(RDataSec>=0)
	{
		Read_data(RDataSec);
		RDataSec=-1;
	}
}


/******************************************************************************
 * FunctionName : search_timer_cb
 * Description  : timer search_timer callback.if search ap timeout at the first start;
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void search_timer_cb()
{
	os_timer_disarm(&search_timer);
	system_restart();

	//qq user_set_softap_config();//set 8266 ssid psw
	//qq user_tcpserver_init(SERVER_LOCAL_PORT);

	//qq os_timer_arm(&led_flash_timer,1000,1);

	//qq os_printf("work_state:%d !!! \r\n",work_state);
}


void run_timer_cb()
{

	if(work_state==work_state_send_ok)
	{

		//os_sprintf(UDPsend_tab,"SHT_temp:%d\r\nSHT_humi:%d\r\n\0",SHT_temp,SHT_humi);
		//user_udp_send("working!\r\n");
		//os_timer_disarm(&run_timer);
		//system_restart();
		//system_deep_sleep(interval_timer_cnt);
		//system_deep_sleep(7000000);//5S for test
		//os_sprintf(UDPdata,"{\"device\":\"TH_meter\",\r\n \"SN\":%s,\r\n \"data\":\r\n {\"temprature\":%d,\r\n  \"humidity\":%d\r\n }\r\n}\r\n",VA_param.param_SN,SHT_temp,SHT_humi);
		//user_udp_send(UDPdata);

		//以下为20151214 新增上位机hold功能后的此部分程序
		os_timer_disarm(&run_timer);
		if((HoldFlag==0)&&(HoldSoft==0))//hold to recevie command from PC
		{
			espconn_disconnect(&user_tcp_conn);
			espconn_delete(&user_tcp_conn);
			gpio_output_set( 0, 0, 0, BIT5); os_printf("4t\r\n");
			os_printf("in run_timer sleep:%d S. \r\n",interval_timer_cnt/1000000);
			system_deep_sleep(interval_timer_cnt);
			//system_deep_sleep(7000000);//7S for test
		}
		else
		{
			os_timer_arm(&run_timer,500,1);
		}
	}
}

void led_flash_timer_cb()
{
	static uint8 cnt10;

	cnt10=cnt10^1;

	if(cnt10==0)
	{
		gpio_output_set( 0, BIT5, BIT5, 0);//GPIO5 --0
	}
	else
	{
		gpio_output_set( BIT5, 0, BIT5, 0);//GPIO5 --1
	}
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
   // os_printf("tcp sent cb \r\n");
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
	char *pBuf,*pBuf_ssid,*pBuf_psw,*pBuf_myssid,*pBuf_mypsw,*pBuf_mysn,*pStr,i=0,templen,temp_setssid[100],temp_setpsw[100],temp_setsn[100];


	char *value_temp[65];
	char GB2312_test[100];
	char *testbuf;

	struct station_config stationConf;

	os_printf("tcp rece cb: \r\n",pusrdata);
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
	   os_printf("true tcp recv : %s \r\n", pusrdata);
	   os_printf("tcp send: %s \r\n", SendStr);
   }
   else if(pBuf=strstr(pusrdata,"box="))
   {
	   os_printf("%s\r\n",pBuf);
	   while ( ( pStr = strtok ( pBuf, "=" ) ) != NULL )
	  		{
	  			value_temp[ i ++ ] = pStr;
	  			pBuf = NULL;
	  			if(i>5)break;
	  		}
	   pBuf_ssid=value_temp[1];
	   while((*pBuf_ssid)!='&'){pBuf_ssid++;}
	   *(pBuf_ssid)=0;
	   os_strcpy(conn_ssid , value_temp[1]);
	  //qq os_strcpy(conn_ssid , apssid[ssidloop-(*(value_temp[1])-0x30)]);

	  pBuf_psw=value_temp[2];
	       while((*(pBuf_psw)!='-')&&(*(pBuf_psw)!='&')){pBuf_psw++;}
	       if((*(pBuf_psw)=='-'))//PSW-myssid-mypsw-SN   //(00xxxxxxxx)
	       {
	    	   os_printf("1 - \r\n");
			   //pBuf_psw--;
			   strcpy(temp_setssid,pBuf_psw+1);
			   pBuf_myssid=temp_setssid;
	    	   while((*(pBuf_myssid)!='-')&&(*(pBuf_myssid)!='&')){pBuf_myssid++;}
	    	   if((*(pBuf_myssid)=='-'))
	    	   {
	    		   os_printf("2 - \r\n");
	    		   //pBuf_myssid--;
	    		   strcpy(temp_setpsw,pBuf_myssid+1);
	    		   pBuf_mypsw=temp_setpsw;
	    		   while((*(pBuf_mypsw)!='-')&&(*(pBuf_mypsw)!='&')){pBuf_mypsw++;}
	    		   if((*(pBuf_mypsw)=='-'))
	    		   {
	    			   os_printf("3 - \r\n");
	    			   strcpy(temp_setsn,pBuf_mypsw+1);
	    			   pBuf_mysn=temp_setsn;
	    			   while((*(pBuf_mysn)!='-')&&(*(pBuf_mysn)!='&')){pBuf_mysn++;}
	    			   *(pBuf_mysn) =0;
	    			   os_strcpy(VA_param.param_SN,temp_setsn);
	    			   os_printf("get sn:%s \r\n",VA_param.param_SN);
	    		   }
	    		   *(pBuf_mypsw) =0;
	    		   os_strcpy(VA_param.param_my_psw,temp_setpsw);
	    		   os_printf("get psw:%s \r\n",VA_param.param_my_psw);
	    	   }
	    	   *(pBuf_myssid)=0;

	    	   os_strcpy(VA_param.param_my_ssid,temp_setssid);
	       }

	       *(pBuf_psw)=0;//set the end \0
	       templen=strlen(value_temp[2]);
	       os_strcpy(conn_psw,value_temp[2]);//get password

	       VA_param.param_interval= *(value_temp[3]);//interval
	       os_printf("interval=%d \r\n",VA_param.param_interval);
	       switch(VA_param.param_interval){
	       	case 0x31: 	interval_timer_cnt=(1*60-6)*1000000;break;//1min
	       	case 0x32: 	interval_timer_cnt=(5*60-6)*1000000;break;//5min
	       	case 0x33: 	interval_timer_cnt=(10*60-6)*1000000;break;//10min
	       	case 0x34: 	interval_timer_cnt=(30*60-6)*1000000;break;//30min
	       	case 0x35: 	interval_timer_cnt=(60*60-6)*100000;break;//60min  over flow
	       	default:	interval_timer_cnt=(5*60-6)*1000000;break;
	       	}

	       strcpy(VA_param.param_ap_ssid,conn_ssid);
	       strcpy(VA_param.param_ap_psw,conn_psw);

	       esp_VA_save_param(WitchSec);

	       stationConf.bssid_set = 0;
	       os_memcpy(&stationConf.ssid, conn_ssid, 64);
	       os_memcpy(&stationConf.password, conn_psw, 64);
	       os_printf("disconnect?: %d! \r\n",wifi_station_disconnect());

	       os_printf("Try to connect to %s! \r\n",stationConf.ssid);
	       os_printf("psw %s! \r\n",stationConf.password);
	       work_state=work_state_got_set;

	       wifi_station_set_config(&stationConf);//config the new connect
	       wifi_station_connect();//connect to new AP
	       os_timer_disarm(&check_ip_timer);
	       //在配置模式下收到配置数据后再启动连接查询定时器中断
	       os_timer_setfn(&check_ip_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
	       os_timer_arm(&check_ip_timer, 400, 0);
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
    if(ret==0)
    {
    	work_state=work_state_set_ser;
    	os_printf("espconn_accept [%d] !!! \r\n", ret);
    }
    else
    {
    	os_printf("espconn_accept [%d] !!! \r\n", ret);
    }
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
    os_timer_disarm(&check_ip_timer);

   //get ip info of ESP8266 station
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {

    	os_printf("got ip !!! \r\n");
    	config_state=0;

    	HoldFlag=0;//使能到点休眠

    	os_timer_disarm(&led_flash_timer);//停止定时器
    	gpio_output_set( 0,BIT5, BIT5, 0);//GPIO5 --0 熄灭led
    	wifi_station_set_auto_connect(true);//使能自动重连，在上电自动尝试连接不成功会禁止 在这里打开。

    	os_timer_disarm(&watch_timer);
    	os_timer_arm(&watch_timer,15000,1);//获取到IP后就开启限时重启定时器

    	hw_timer_init(NMI_SOURCE,1);
    	hw_timer_set_func(hw_test_timer_cb);
    	hw_timer_arm(5000000);

    	userUDPinit();//初始化udp
    	os_sprintf(UDPdata,"{\"device\":\"TH_meter\",\r\n \"SN\":%s,\r\n \"data\":\r\n {\"temprature\":%d,\r\n  \"humidity\":%d\r\n }\r\n}\r\n",VA_param.param_SN,SHT_temp,SHT_humi);
    	user_udp_send(UDPdata);//send local data

    	if(init_data.cnt==1)//新的记录段开始 获取时间
    	{
    		GetTime();
    		os_timer_arm(&test_timer,500,1);
    	}


    	if(VA_param.param_ModelFunc==3)
    	{
    		espconn_delete(&esp_conn);
			if ( work_state==work_state_noip ) {
				os_timer_disarm(&search_timer);
				os_printf("disarm search_timer !!! \r\n");
				work_state=work_state_conn_ap;
				wifi_set_opmode(STATION_MODE);
				user_tcp_connect();
				//user_tcpserver_init(SERVER_LOCAL_PORT);

			} else if (work_state==work_state_got_set) {
				work_state=work_state_conn_ap;
				esp_VA_save_param(WitchSec);//save the connected ap
				os_printf("manual set connect !!! \r\n");
				os_printf("AP saved!!! \r\n");
				wifi_set_opmode(STATION_MODE);
				user_tcp_connect();
			}else if(work_state==work_state_set_ser){
				os_printf("the saved ap connect timeout ,the server is set but still get ip auto !!! \r\n");
				work_state=work_state_conn_ap;
				wifi_set_opmode(STATION_MODE);
				user_tcp_connect();
			}
    	}
    	else
    	{
    		os_timer_arm(&run_timer,500,1);//wait for udp send and sleep in the timer cb
    	}


    } else {

        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {

         os_printf("connect fail !!! \r\n");

        } else {

           //re-arm timer to check ip
            os_timer_setfn(&check_ip_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&check_ip_timer, 400, 0);
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
   //char ssid[32] = "VA_office";
   //char password[64] = "69523223";//"32232596";
   struct station_config stationConf;

   //need not mac address
   stationConf.bssid_set = 0;

   //Set ap settingsl
   os_memcpy(&stationConf.ssid, VA_param.param_ap_ssid, 64);
   os_memcpy(&stationConf.password, VA_param.param_ap_psw, 64);
   wifi_station_set_config(&stationConf);

   //wifi_station_connect();
   //os_printf("Try to connect to RD! \r\n");
   //set a timer to check whether got ip from router succeed or not.
   os_timer_disarm(&check_ip_timer);
   os_timer_setfn(&check_ip_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
   os_timer_arm(&check_ip_timer, 400, 0);

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
  char *pBuf,mactemp[20],i;

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

  pBuf=strstr(before_ssid_html,"0123456");//replace the default SN 0123456789->00xxxxxxxx
  for(i=0;i<10;i++)
  {
	*(pBuf+i)=VA_param.param_SN[i];
  }

  wifi_get_macaddr(STATION_IF,mymacaddr);
  //os_printf("mymacaddr:%s !!!\r\n",mymacaddr);

  //os_printf("abcdef_%02x%02x%02x%02x%02x%02x\r\n", mymacaddr[0],mymacaddr[1],\
	//	  	  	  	  	  mymacaddr[2],mymacaddr[3], mymacaddr[4], mymacaddr[5]);

  pBuf=strstr(before_ssid_html,"A1:B2");

  os_sprintf(mactemp, "%02x:%02x:%02x:%02x:%02x:%02x", mymacaddr[0],mymacaddr[1],\
		  mymacaddr[2],mymacaddr[3], mymacaddr[4], mymacaddr[5]);
  for(i=0;i<17;i++)
  {
	  *(pBuf+i)=mactemp[i];

  }
//if EXT RST
  if(config_state==1)//if config key is pressed
  {
	  os_timer_arm(&led_flash_timer,1000,1);
	  os_printf("config key is pressed,prepare for manual set!!!\r\n");
	  //qq user_set_softap_config();//set 8266 ssid psw
	  //qq wifi_station_set_auto_connect(false);//20151224 上电不自动连接路由
	  //qq user_set_softap_config();//set 8266 ssid psw
	 user_tcpserver_init(SERVER_LOCAL_PORT);
		 os_printf("tcpserver! \r\n");
	 os_timer_arm(&led_flash_timer,1000,1);
		 os_printf("flash led! \r\n");

  }
  else if(config_state==0)
  {
	  os_timer_disarm(&led_flash_timer);//停止定时器
	  gpio_output_set( BIT5, 0, BIT5, 0);//GPIO5 --1 熄灭led
	  user_set_station_config();//set the target AP ssid psw
	  if(FirstStart==1)//复位启动
	  {
		  wifi_station_connect();
	  }
  }



  //qq <option value="First">First</option>
  ssid_send[0]='\0';
  ssid_NO=0;
  while(ssidloop-ssid_NO>0)
  {
	ssid_NO++;
	//qq if(ssid_NO>9){ssid_length++;}
	//qq os_sprintf ( cStr, "<option value=\"%d\" class=\"column1\">%s</option>\r\n",ssid_NO,apssid[ssidloop-ssid_NO]  );
	os_sprintf ( cStr, "<option value=\"%s\">%s</option>\r\n",apssid[ssidloop-ssid_NO],apssid[ssidloop-ssid_NO]);

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
	record_sensor();

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

/******************************************************************************
 * FunctionName : user_set_softap_config
 * Description  : set SSID and password of ESP8266 softAP
 * Parameters   : none
 * Returns      : none
 * attention	:20151228 注意在udp发送中设置了通过station广播UDP包   而AP的DHCP功能也是使用UDP实现 这里需要将广播重新设置到softAP.
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_set_softap_config(void)
{
   struct softap_config config;
   struct ip_info ipinfo;

   wifi_set_opmode(STATIONAP_MODE);
   wifi_softap_get_config(&config); // Get config first.
   wifi_softap_dhcps_stop();

   ipinfo.ip.addr = ipaddr_addr("192.168.4.1");
   ipinfo.gw.addr = ipaddr_addr("192.168.4.1");
   ipinfo.netmask.addr = ipaddr_addr("255.255.255.0");

   os_memset(config.ssid, 0, 32);
   os_memset(config.password, 0, 64);
   os_memcpy(config.ssid,VA_param.param_my_ssid, 64);
   os_memcpy(config.password, VA_param.param_my_psw, 64);

   config.authmode = 3;
   config.ssid_hidden=0;
   config.channel=8;
   config.ssid_len = 0;// or its actual length
   config.max_connection = 4; // how many stations can connect to ESP8266 softAP at most.
   config.beacon_interval= 100;

   wifi_softap_set_config(&config);// Set ESP8266 softap config .
   wifi_set_phy_mode(PHY_MODE_11G);
   //qq wifi_set_broadcast_if(SOFTAP_IF);STATIONAP_MODE
   wifi_set_broadcast_if(STATIONAP_MODE);

   wifi_set_ip_info(SOFTAP_IF, &ipinfo);

   wifi_softap_dhcps_start();


}

/******************************************************************************
 * FunctionName : key_init
 * Description  : init keys
 * Parameters   : key_param *keys - keys parameter, which inited by key_init_single
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
pin_init(uint32 gpio_name,uint8 gpio_id,uint8 gpio_func)
{
       	PIN_FUNC_SELECT(gpio_name,gpio_func);
        PIN_PULLUP_EN(gpio_name);
        gpio_output_set(0, 0, 0, GPIO_ID_PIN(gpio_id));

        gpio_register_set(GPIO_PIN_ADDR(gpio_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                          | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                          | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));

        //clear interrupt status
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpio_id));
}

void ICACHE_FLASH_ATTR
key_init(void)
{
    pin_init(PERIPHS_IO_MUX_GPIO4_U,4,0);
    pin_init(PERIPHS_IO_MUX_GPIO5_U,5,0);
}


/******************************************************************************
 * FunctionName : record_sensor
 * Description  : record the sensor data
 * use			: init_data rtc memory R/W  flash R/W
 * Parameters   : none
 * Returns      : none
 *
 *
*******************************************************************************/
void ICACHE_FLASH_ATTR
record_sensor(void)
{
	uint32 	PageSection=0;//当前写入页面的哪一段
	uint32 	PageTemp[1024]={0};
	typedef Mix32 SECData[INITFLAG_MEM_NUM]; //
	SECData Readtemp[SECTION_NUM_PerPAGE];
	uint16 	ThisPage=0;
	uint16  i,j,k;
//#define SENSOR_DATA_STARTADDR		0x100   //256*4K=1M
//#define SENSOR_DATA_ENDADDR		0x2ff   //0x300*1000=300000=3M
	if(SHT20_read())//to void record_sensor();
	{
		system_rtc_mem_read( INITFLAG_MEM_ADDR, &init_data, sizeof(INITFLAG_MEM) );
		os_printf("\r\ncnt:%d\r\n",init_data.cnt);
		init_data.data[init_data.cnt].shortB[0]=SHT_temp;
		os_printf("init_data.data:%d\r\n",init_data.data[init_data.cnt].shortB[0]);
		init_data.data[init_data.cnt].shortB[1]=SHT_humi;
		os_printf("init_data.data:%d\r\n",init_data.data[init_data.cnt].shortB[1]);

		if(init_data.cnt<(INITFLAG_MEM_NUM-1))
		{
			init_data.cnt++;
		}
		else//save to flash
		{
			init_data.cnt=0;
			//SECTION_NUM_PerPAGE  每一页的section数目
			ThisPage = SENSOR_DATA_STARTADDR + (VA_param.param_SaveSection / SECTION_NUM_PerPAGE);
			os_printf("ThisPage %d\r\n",ThisPage);

			PageSection = VA_param.param_SaveSection % SECTION_NUM_PerPAGE;
			os_printf("PageSection %d\r\n",PageSection);

			//read out
			//os_printf("PageTemp length %d\r\n",sizeof( PageTemp ));
			spi_flash_read(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&Readtemp,sizeof( PageTemp ));

			memcpy(Readtemp[PageSection],init_data.data,sizeof(init_data.data));//

			spi_flash_erase_sector(ThisPage);
			spi_flash_write(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&Readtemp,sizeof( Readtemp));

			os_printf("VA_param.param_SaveSection %d\r\n",VA_param.param_SaveSection);

			/*os_delay_us(30000);
			for(i=0;i<PageSection+1;i++)
			{
				for(j=0;j<INITFLAG_MEM_NUM;j++)
				{
					for(k=0;k<2;k++)
					{
						//os_printf("%d ",Readtemp[PageSection][j].shortB[k]);
						os_printf("%d",Readtemp[i][j].shortB[k]);
					}
					os_printf("-");
				}
			}*/
			/*for(j=0;j<INITFLAG_MEM_NUM;j++)//output one section
			{
				for(k=0;k<2;k++)
				{
					os_printf("%d ",Readtemp[PageSection][j].shortB[k]);
				}
				os_printf("-");
			}*/
			VA_param.param_SaveSection++;
			if(VA_param.param_SaveSection>MAX_SECTION){VA_param.param_SaveSection=0;}
			esp_VA_save_param(WitchSec);//save the sec point
		}
		system_rtc_mem_write(INITFLAG_MEM_ADDR, &init_data, sizeof(INITFLAG_MEM));
	}

}
void Read_data(uint32 readsec)
{
	uint32 	PageSection=0;//当前写入页面的哪一段
	uint32 	PageTemp[1024]={0};
	typedef Mix32 SECData[INITFLAG_MEM_NUM]; //
	SECData Readtemp[SECTION_NUM_PerPAGE];
	uint16 	ThisPage=0;
	uint16  i,j,k;

	ThisPage = SENSOR_DATA_STARTADDR + (readsec / SECTION_NUM_PerPAGE);os_printf("page%d\r\n",ThisPage);
	PageSection = readsec % SECTION_NUM_PerPAGE;os_printf("pagesec%d\r\nSECDATA:",PageSection);
	spi_flash_read(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&Readtemp,sizeof( PageTemp ));
	for(j=0;j<INITFLAG_MEM_NUM;j++)
	{
		os_printf("%d",Readtemp[PageSection][j].shortB[0]);os_printf("-");
		os_printf("%d\r\n",Readtemp[PageSection][j].shortB[1]);
	}
}


void ICACHE_FLASH_ATTR
RTC_memory_read()
{
	char ssid_temp[32];

	 system_rtc_mem_read( INITFLAG_MEM_ADDR, &init_data, sizeof(INITFLAG_MEM) );
	 if(init_data.init_flg!=NOT_POWER_ON)//不等于 ，表示上电启动
	 {
		os_printf("power on! \r\n");
		init_data.init_flg=NOT_POWER_ON;
		init_data.cnt=0;
		system_rtc_mem_write(INITFLAG_MEM_ADDR, &init_data, sizeof(INITFLAG_MEM));
		os_printf("powered,set the (NOT_POWER_ON)! \r\n");

	 }
	 else//ext RST    reset the SSID AP
	 {
		 config_state=1;//reconfig flag
		 wifi_get_macaddr(STATION_IF,mymacaddr);
		 os_sprintf(ssid_temp,"V&A_%02x%02x%02x wifi_HT",mymacaddr[3], mymacaddr[4], mymacaddr[5]);

		strcpy(VA_param.param_ap_ssid,"RD");
		strcpy(VA_param.param_ap_psw,"322325961");
		strcpy(VA_param.param_my_ssid,ssid_temp);
		strcpy(VA_param.param_my_psw,"69523223");
		 esp_VA_save_param(WitchSec);

		 os_printf("Ext Rst! \r\n");
		 os_printf("RTC_memory INIT,config_state=%d \r\n",config_state);
		 os_printf("NOT_POWER_ON=%x \r\n",NOT_POWER_ON);
		 //以下将配置服务器的建立移到这里
		 os_timer_disarm(&search_timer);
		 wifi_station_set_auto_connect(false);//20151224 上电不自动连接路由

		 user_set_softap_config();//set 8266 ssid psw

		 HoldFlag=1;//设置模式不然模块休眠
	 }
}

void user_rf_pre_init(void)
{
	//system_phy_set_rfoption();

}


/*********************************************/
//2015-12-04 新增 将八位SN分别加上对应字符，防止太容易被检索到
//2015-12-22 新增存储每段记录数据时间戳，没次开始新的记录段则通过SNTP服务器获取当前时间并保存在独立的存储位置。
//在开始新的记录前由上位机向模块发送第一次记录的时间，之后 每次cnt计数为0时 即sec为最新值时 在获取到ip的位置使能SNTP获取当前时间并在定时器回调函数中保存时间。
//数据存储每sec 32*4byte  2M 	(200h-2ffh)(4Kpage)
// 时间存每sec 32byte	  512K	(300h-380h)(4Kpage)
//watch_timer 用于固定时间强制将模块进入睡眠间隔  防止某时间不成功卡住不运行
//search_timer 用于在上电时给一段时间尝试连接之前保存的AP 时间到连不上就复位保存的AP,并建立TCPserver  等待用户配置
//上电 ，外部复位，软件复位 都会触发读取RTC memory 内容，先判断是不是上电启动，或者其他复位-》则建立softAP并在扫描周围AP结束后将建立TCPserver 等待用户连接配置。
//睡眠醒来会直接进行重连，重连最多持续到watch时间到来。
//
/********************************************/

void user_init(void)
{
	uint16 gpio_status;


	uart_init(115200, 115200);
	uart_rx_intr_enable(UART0);

    os_printf("SDK version:%s\n", system_get_sdk_version());
    system_deep_sleep_set_option(2);//唤醒不做RF校准
    wifi_station_set_auto_connect(true);//20151224 上电自动连接路由

    //硬件定时器 用于当机复位
    //hw_timer_init(NMI_SOURCE,1);
    //hw_timer_set_func(hw_test_timer_cb);
    //hw_timer_arm(5000000);

    work_state=work_state_noip;

/*****************************/
//test
/*    os_printf("-------------------------\r\n");
    void system_print_meminfo ();
    os_printf("-------------------------\r\n");*/
    os_printf("%d\r\n",system_get_free_heap_size());
    os_printf("-------------------------\r\n");
/**********************/


    esp_VA_load_param();//加载参数（未做保护，待加入）

    wifi_set_opmode(STATION_MODE);
	//qq wifi_set_opmode(STATIONAP_MODE);

	key_init();

	i2c_master_gpio_init();

	//user_set_softap_config();//set 8266 ssid psw

	os_timer_disarm(&search_timer);
	os_timer_setfn(&search_timer,search_timer_cb,NULL);
	os_timer_arm(&search_timer,20000,1);//设置一个较大的定时值(9000)可使在自动连接成功前不显示出AP(web服务器不建立)

	os_timer_disarm(&watch_timer);//限制每次发送的时间不超过20s
	os_timer_setfn(&watch_timer,watch_timer_cb,NULL);
	os_timer_arm(&watch_timer,25000,1);//15s 相当于watchdog//在判定为初次上电时将其禁止！
	//os_timer_disarm(&watch_timer);

			/*enum rst_reason {
			REASON_DEFAULT_RST		= 0,
				REASON_WDT_RST			= 1,
				REASON_EXCEPTION_RST	= 2,
				REASON_SOFT_WDT_RST   	= 3,
				REASON_SOFT_RESTART 	= 4,
				REASON_DEEP_SLEEP_AWAKE	= 5,
				REASON_EXT_SYS_RST      = 6
			};*/
			//判断是上电还是外部复位
            struct rst_info* user_rst_info;
            user_rst_info=system_get_rst_info();
            os_printf("restart reason:%d \r\n",user_rst_info->reason);
            switch(user_rst_info->reason)
			{
			case 0:	break;
			case 1:	break;
			case 2:
				FirstStart=1;//should scan APs  for the power on and EXT rst,
				os_timer_disarm(&watch_timer);
				RTC_memory_read();
				break;//external restart for config
			case 3:	break;
			case 4:	//注意因为超时未连接触发的系统重启，会跳至此。同样再加入超时判断。
					FirstStart=1;//should scan APs  for the power on and EXT rst,
					os_timer_disarm(&watch_timer);//
					RTC_memory_read();
					break;//external restart for config
				break;

			case 5:	//每次睡眠醒来20S定时复位，和连接限时公用一个定时器 防止由于路由器重连不上时死机   一直显示beacon timeout  而不继续下一步 卡住。
				os_timer_disarm(&search_timer);
					break;
			case 6:
				FirstStart=1;//should scan APs  for the power on and EXT rst,
				os_timer_disarm(&watch_timer);//
				RTC_memory_read();
						break;//external restart for config
			default:break;

			}

	/*user_time_t my_time;//test to read the time.
	my_time=ReadTime(VA_param.param_SaveSection-1);
	if(my_time.Savesec<=VA_param.param_SaveSection)
	{
		os_printf("3t\r\n");my_time.Timenc[0]=0;//防止字符串调用溢出。
		os_printf("4t\r\n");os_printf("TIME:%s\r\n",my_time.Time24.TimeTab);
	}*/


	os_timer_disarm(&read_timer);
	os_timer_setfn(&read_timer,read_timer_cb,NULL);
	//os_timer_arm(&read_timer,20,1);

	os_timer_disarm(&test_timer);
	os_timer_setfn(&test_timer,test_timer_cb,NULL);
	//os_timer_arm(&test_timer,500,1);

    os_timer_disarm(&run_timer);//led flash timer
    os_timer_setfn(&run_timer,run_timer_cb,NULL);
    //os_timer_arm(&run_timer,100,1);

    os_timer_disarm(&led_flash_timer);//led flash timer
    os_timer_setfn(&led_flash_timer,led_flash_timer_cb,NULL);
    //os_timer_arm(&led_flash_timer,1000,1);





    if(FirstStart==1)//复位启动
    {// wifi scan has to after system init done.
    	system_init_done_cb(user_scan); //record_sensor();
    }
    else//睡眠启动
    {
    	 if(config_state==1)//if config key is pressed
    	  {
    		 os_timer_arm(&led_flash_timer,1000,1);
    		 os_printf("manual config!\r\n");
    		 wifi_station_set_auto_connect(false);//20151224 上电不自动连接路由
				 os_printf("dis_auto connect! \r\n");
			 //qq user_set_softap_config();//set 8266 ssid psw
				 os_printf("softap! \r\n");
			 user_tcpserver_init(SERVER_LOCAL_PORT);
				 os_printf("tcpserver! \r\n");
			 os_timer_arm(&led_flash_timer,1000,1);
				 os_printf("flash led! \r\n");
    	  }
    	  else if(config_state==0)
    	  {
    		  os_timer_disarm(&led_flash_timer);//停止定时器
    		  gpio_output_set(  0,BIT5, BIT5, 0);//GPIO5 --0 熄灭led
    		  user_set_station_config();//set the target AP ssid psw
    	  }
    	 system_init_done_cb(record_sensor);  //record_sensor();
    }




    //smartconfig_start(smartconfig_done);

}





