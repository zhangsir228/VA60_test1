/*
 * Mysntp.c
 *
 *  Created on: 2015年12月21日
 *      Author: Administrator
 */
#include "stdlib.h"
#include "Mysntp.h"

extern struct VA_saved_param	VA_param;
extern uint8 HoldFlag;//由上位机串口发出=1 不休眠 等待      ；=0 正常工作循环
extern uint32 interval_timer_cnt;

//Date: Mon, 14 Dec 2015 07:03:39 GMT
//SNTP TIME(24byte):Tue Dec 22 11:34:05 2015
uint8  	WeekTab[7][4]={
		{"MON"},{"TUE"},{"WED"},{"THU"},{"FRI"},{"SAT"},{"SUN"},
};
uint8 	MonthTab[12][4]={
		{"Jan"},{"Feb"},{"Mar"},{"Apr"},{"May"},{"Jun"},
		{"Jul"},{"Aug"},{"Sep"},{"Oct"},{"Nov"},{"Dec"}
};

user_time_t user_time;
extern void *memcpy(void *, const void *, size_t);
//2015-12-22 新增存储每段记录数据时间戳，没次开始新的记录段则通过SNTP服务器获取当前时间并保存在独立的存储位置。

void ICACHE_FLASH_ATTR
SaveTime(uint32 current_stamp,uint32 savesec)
{
	uint16 	ThisPage=0;
	uint32 	PageSection=0;
	user_time_t temp_time[NUM_PERPAGE];
	char *Buf;

	ThisPage = TIME_STARTADDR + (savesec / NUM_PERPAGE);
	os_printf("time save Page %d\r\n",ThisPage);

	PageSection = savesec % NUM_PERPAGE;
	spi_flash_read(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&temp_time,sizeof( temp_time ));

	Buf=(char *)sntp_get_real_time(current_stamp);
	memcpy(temp_time[PageSection].Time24.TimeTab,Buf,sizeof(temp_time[PageSection].Time24.TimeTab));
	temp_time[PageSection].Savesec=savesec;//保存sec
	temp_time[PageSection].Timenc[0]=0;

	spi_flash_erase_sector(ThisPage);
	spi_flash_write(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&temp_time,sizeof( temp_time));

	os_printf("Save time:%s for sec:%d\r\n",temp_time[PageSection].Time24.TimeTab,temp_time[PageSection].Savesec);

	//free(temp_time);

}

user_time_t ReadTime(uint16 savesec )
{
	uint16 	ThisPage=0;
	uint16 	PageSection=0;
	user_time_t temp_time[NUM_PERPAGE];

	ThisPage = TIME_STARTADDR + (savesec / NUM_PERPAGE);
	os_printf("time read Page %d\r\n",ThisPage);

	PageSection = savesec % NUM_PERPAGE;os_printf("sec %d\r\n",PageSection);
	spi_flash_read(ThisPage * SPI_FLASH_SEC_SIZE,(uint32 *)&temp_time,sizeof( temp_time ));
	return temp_time[PageSection];
}

void GetTime(void)
{
	ip_addr_t *addr = (ip_addr_t *)os_zalloc(sizeof(ip_addr_t));
	//sntp_setservername(0, "us.pool.ntp.org"); // set server 0 by domain name
	//sntp_setservername(1, "ntp.sjtu.edu.cn"); // set server 1 by domain name
	sntp_setservername(0, "time.windows.com"); // set server 0 by domain name
	sntp_setservername(1, "time.nist.gov"); // set server 1 by domain name
	//ipaddr_aton("210.72.145.44", addr);
	ipaddr_aton("131.107.13.100", addr);

	sntp_setserver(2, addr); // set server 2 by IP address
	sntp_init();
	os_free(addr);
	HoldFlag=1;
	//uint32 current_stamp;
	//current_stamp = sntp_get_current_timestamp();
	//os_printf("sntp: %d, %s \n",current_stamp, sntp_get_real_time(current_stamp));
}



