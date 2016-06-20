/*
 * UDPsend.c
 *
 *  Created on: 2015Äê11ÔÂ30ÈÕ
 *      Author: Administrator
 */




/******************************************************************************
     * Copyright 2013-2014 Espressif Systems
     *
*******************************************************************************/

#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

#include "espconn.h"
#include "user_json.h"
#include "user_devicefind.h"

#include "UDPsend.h"

uint8 UDPsend_tab[200]={0};

uint8 UDPdata[500]={
		0//"{\"device\":\"TH_meter\",\r\n\"SN\":0012345678,\r\n\"data\":{\"temprature\":12.3,\r\n\"humidity\":45.6\r\n}\r\n}"

};


LOCAL struct espconn user_udp_espconn;

/******************************************************************************
  * FunctionName : user_udp_recv_cb
  * Description  : Processing the received udp packet
  * Parameters   : arg -- Additional argument to pass to the callback function
  *                pusrdata -- The received data (or NULL when the connection has been closed!)
  *                length -- The length of received data
  * Returns      : none
 *******************************************************************************/
 LOCAL void ICACHE_FLASH_ATTR
 user_udp_recv_cb(void *arg, char *pusrdata, unsigned short length)
 {

     //os_printf("recv udp data: %s\n", pusrdata);
	 os_printf("%s\r\n", pusrdata);
	 //uart0_tx_buffer(pusrdata, strlen(pusrdata));

 }

 /******************************************************************************
      * FunctionName : user_udp_send
      * Description  : udp send data
      * Parameters  : none
      * Returns      : none
 *******************************************************************************/
 void ICACHE_FLASH_ATTR
 user_udp_send(char *senddata)
 {
     char DeviceBuffer[40] = {0};
     char hwaddr[6];
     struct ip_info ipconfig;

     const char udp_remote_ip[4] = { 255, 255, 255, 255};
     os_memcpy(user_udp_espconn.proto.udp->remote_ip, udp_remote_ip, 4); // ESP8266 udp remote IP need to be set everytime we call espconn_sent
     user_udp_espconn.proto.udp->remote_port = 1025;  // ESP8266 udp remote port need to be set everytime we call espconn_sent

     //wifi_get_macaddr(STATION_IF, hwaddr);

     // os_printf("%s \r\n",senddata);

     espconn_sent(&user_udp_espconn, senddata, os_strlen(senddata));
      os_printf( "UDP send ok\r\n" );
 }

 /******************************************************************************
      * FunctionName : user_udp_sent_cb
      * Description  : udp sent successfully
      * Parameters  : arg -- Additional argument to pass to the callback function
      * Returns      : none
 *******************************************************************************/
  LOCAL void ICACHE_FLASH_ATTR
  user_udp_sent_cb(void *arg)
  {
      struct espconn *pespconn = arg;

      //test/ os_printf("user_udp_send successfully !!!\n");

  }


void ICACHE_FLASH_ATTR
userUDPinit(void)
  {
	  //set UDP***************************************/
		wifi_set_broadcast_if(STATIONAP_MODE); // send UDP broadcast from both station and soft-AP interface

		user_udp_espconn.type = ESPCONN_UDP;
		user_udp_espconn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
		user_udp_espconn.proto.udp->local_port = 25000;  //espconn_port();25000;// set a available  port

		const char udp_remote_ip[4] = {255, 255, 255, 255};

		os_memcpy(user_udp_espconn.proto.udp->remote_ip, udp_remote_ip, 4); // ESP8266 udp remote IP

		user_udp_espconn.proto.udp->remote_port = 1025;  // ESP8266 udp remote port

		espconn_regist_recvcb(&user_udp_espconn, user_udp_recv_cb); // register a udp packet receiving callback
		espconn_regist_sentcb(&user_udp_espconn, user_udp_sent_cb); // register a udp packet sent callback

		espconn_create(&user_udp_espconn);   // create udp
		user_udp_send("UDP set OK! \r\n");   // send udp data

  }











