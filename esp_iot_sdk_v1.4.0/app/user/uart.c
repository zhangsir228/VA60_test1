/*
 * File	: uart.c
 * Copyright (C) 2013 - 2016, Espressif Systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ets_sys.h"
#include "osapi.h"
#include "uart.h"
#include "osapi.h"
#include "uart_register.h"
#include "mem.h"
#include "os_type.h"
#include "TCPclient.h"
#include "string.h"
#include "stdlib.h"
#include "UDPsend.h"
#include "user_main.h"
#include "Mysntp.h"

#include "user_interface.h"

// UartDev is defined and initialized in rom code.
extern UartDevice    			UartDev;
extern ATT7053_data_type    	ATT7053_data;
extern struct VA_saved_param	VA_param;
extern uint8 					HoldFlag;//程序内部使用 =1 不休眠 等待      ；=0 正常工作循环
extern uint8 					HoldSoft;//上位机主动停止模块标志  =1 不休眠 等待      ；=0 正常工作循环
extern uint32 					interval_timer_cnt;
extern uint32 					WitchSec;//当前使用的是哪个参数区域

extern 	int32		RTimeSec;//需要读取的timesec
extern 	int32		RDataSec;//
extern	os_timer_t 	read_timer;//用于串口读取数据延时以及回调执行
void 							read_timer_cb				();

void ICACHE_FLASH_ATTR 			esp_VA_save_param			(uint32 sec);

LOCAL struct UartBuffer* pTxBuffer = NULL;
LOCAL struct UartBuffer* pRxBuffer = NULL;

uint8 VAHold[]=		"VAHold:";
uint8 VASetSN[]=  	"VASetSN:";
uint8 VASetINTERVAL[]="VASetINTERVAL:";
uint8 VASetSSID[]=	"VASetSSID:";
uint8 VASetPSW[]=	"VASetPSW:";
uint8 VASetMySSID[]="VASetMySSID:";//当前模块的ssid和psw配置
uint8 VASetMyPSW[]=	"VASetMyPSW:";
uint8 VATCal[]=		"VATCal:";
uint8 VAHCal[]=		"VAHCal:";
uint8 VAResetSAVE[]="VAResetSAVE";
uint8 VASetFUNC[]=	"VASetFUNC:";//设置模块当前功能
uint8 VARestart[]=	"VARestart";
uint8 VARead[]=		"VARead:";
uint8 VASecDATA[]=	"VASecDATA:";//读取指定sec的数据
uint8 VASecTIME[]=	"VASecTIME:";//读取指定sec的时间戳
//uint8 READHIS[]=	"READ:HIS";



/*uart demo with a system task, to output what uart receives*/
/*this is a example to process uart data from task,please change the priority to fit your application task if exists*/
/*it might conflict with your task, if so,please arrange the priority of different task,  or combine it to a different event in the same task. */
#define uart_recvTaskPrio        0
#define uart_recvTaskQueueLen    10
os_event_t    uart_recvTaskQueue[uart_recvTaskQueueLen];

#define DBG  
#define DBG1 uart1_sendStr_no_wait
#define DBG2 os_printf


LOCAL void uart0_rx_intr_handler(void *para);

/******************************************************************************
 * FunctionName : uart_config
 * Description  : Internal used function
 *                UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled
 *                UART1 just used for debug output
 * Parameters   : uart_no, use UART0 or UART1 defined ahead
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart_config(uint8 uart_no)
{
    if (uart_no == UART1){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    }else{
        /* rcv_buff size if 0x100 */
        ETS_UART_INTR_ATTACH(uart0_rx_intr_handler,  &(UartDev.rcv_buff));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
	#if UART_HW_RTS
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);   //HW FLOW CONTROL RTS PIN
        #endif
	#if UART_HW_CTS
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_U0CTS);   //HW FLOW CONTROL CTS PIN
        #endif
    }
    uart_div_modify(uart_no, UART_CLK_FREQ / (UartDev.baut_rate));//SET BAUDRATE
    
    WRITE_PERI_REG(UART_CONF0(uart_no), ((UartDev.exist_parity & UART_PARITY_EN_M)  <<  UART_PARITY_EN_S) //SET BIT AND PARITY MODE
                                                                        | ((UartDev.parity & UART_PARITY_M)  <<UART_PARITY_S )
                                                                        | ((UartDev.stop_bits & UART_STOP_BIT_NUM) << UART_STOP_BIT_NUM_S)
                                                                        | ((UartDev.data_bits & UART_BIT_NUM) << UART_BIT_NUM_S));
    
    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);    //RESET FIFO
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    
    if (uart_no == UART0){
        //set rx fifo trigger
        WRITE_PERI_REG(UART_CONF1(uart_no),
        ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
        #if UART_HW_RTS
        ((110 & UART_RX_FLOW_THRHD) << UART_RX_FLOW_THRHD_S) |
        UART_RX_FLOW_EN |   //enbale rx flow control
        #endif
        (0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S |
        UART_RX_TOUT_EN|
        ((0x10 & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S));//wjl 
        #if UART_HW_CTS
        SET_PERI_REG_MASK( UART_CONF0(uart_no),UART_TX_FLOW_EN);  //add this sentense to add a tx flow control via MTCK( CTS )
        #endif
        SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_TOUT_INT_ENA |UART_FRM_ERR_INT_ENA);
    }else{
        WRITE_PERI_REG(UART_CONF1(uart_no),((UartDev.rcv_buff.TrigLvl & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S));//TrigLvl default val == 1
    }
    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(uart_no), 0xffff);
    //enable rx_interrupt
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_OVF_INT_ENA);
}

/******************************************************************************
 * FunctionName : uart1_tx_one_char
 * Description  : Internal used function
 *                Use uart1 interface to transfer one char
 * Parameters   : uint8 TxChar - character to tx
 * Returns      : OK
*******************************************************************************/
 STATUS uart_tx_one_char(uint8 uart, uint8 TxChar)
{
    while (true){
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(uart)) & (UART_TXFIFO_CNT<<UART_TXFIFO_CNT_S);
        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126) {
            break;
        }
    }
    WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    return OK;
}

/******************************************************************************
 * FunctionName : uart1_write_char
 * Description  : Internal used function
 *                Do some special deal while tx char is '\r' or '\n'
 * Parameters   : char c - character to tx
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
uart1_write_char(char c)
{
    if (c == '\n'){
        uart_tx_one_char(UART1, '\r');
        uart_tx_one_char(UART1, '\n');
    }else if (c == '\r'){
    
    }else{
        uart_tx_one_char(UART1, c);
    }
}

//os_printf output to fifo or to the tx buffer
 void ICACHE_FLASH_ATTR
uart0_write_char_no_wait(char c)
{
#if UART_BUFF_EN    //send to uart0 fifo but do not wait 
    uint8 chr;
    if (c == '\n'){
        chr = '\r';
        tx_buff_enq(&chr, 1);
        chr = '\n';
        tx_buff_enq(&chr, 1);
    }else if (c == '\r'){
    
    }else{
        tx_buff_enq(&c,1);
    }
#else //send to uart tx buffer
    if (c == '\n'){
        uart_tx_one_char_no_wait(UART0, '\r');
        uart_tx_one_char_no_wait(UART0, '\n');
    }else if (c == '\r'){
    
    }
    else{
        uart_tx_one_char_no_wait(UART0, c);
    }
#endif
}

/******************************************************************************
 * FunctionName : uart0_tx_buffer
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_tx_buffer(uint8 *buf, uint16 len)
{
    uint16 i;
    for (i = 0; i < len; i++)
    {
        uart_tx_one_char(UART0, buf[i]);
    }
}

/******************************************************************************
 * FunctionName : uart0_sendStr
 * Description  : use uart0 to transfer buffer
 * Parameters   : uint8 *buf - point to send buffer
 *                uint16 len - buffer len
 * Returns      :
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart0_sendStr(const char *str)
{
    while(*str){
        uart_tx_one_char(UART0, *str++);
    }
}
void at_port_print(const char *str) __attribute__((alias("uart0_sendStr")));
/******************************************************************************
 * FunctionName : uart0_rx_intr_handler
 * Description  : Internal used function
 *                UART0 interrupt handler, add self handle code inside
 * Parameters   : void *para - point to ETS_UART_INTR_ATTACH's arg
 * Returns      : NONE
*******************************************************************************/
LOCAL void
uart0_rx_intr_handler(void *para)
{
    /* uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    * uart1 and uart0 respectively
    */
    uint8 RcvChar;
    uint8 uart_no = UART0;//UartDev.buff_uart_no;
    uint8 fifo_len = 0;
    uint8 buf_idx = 0;
    uint8 temp,cnt;
    RcvMsgBuff *pRxBuff = (RcvMsgBuff *)para;
    os_printf("uart0 INT handler! \r\n");
    os_printf("received data:%s! \r\n",pRxBuff);
    	/*ATTENTION:*/
	/*IN NON-OS VERSION SDK, DO NOT USE "ICACHE_FLASH_ATTR" FUNCTIONS IN THE WHOLE HANDLER PROCESS*/
	/*ALL THE FUNCTIONS CALLED IN INTERRUPT HANDLER MUST BE DECLARED IN RAM */
	/*IF NOT , POST AN EVENT AND PROCESS IN SYSTEM TASK */
    if(UART_FRM_ERR_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_FRM_ERR_INT_ST)){
        DBG2("FRM_ERR\r\n");
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_FRM_ERR_INT_CLR);
    }else if(UART_RXFIFO_FULL_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_FULL_INT_ST)){
        DBG2("f\r\n");
        uart_rx_intr_disable(UART0);
        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
        system_os_post(uart_recvTaskPrio, 0, 0);
    }else if(UART_RXFIFO_TOUT_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_TOUT_INT_ST)){
        DBG2("t\r\n");
        uart_rx_intr_disable(UART0);
        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
        system_os_post(uart_recvTaskPrio, 0, 0);
    }else if(UART_TXFIFO_EMPTY_INT_ST == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_TXFIFO_EMPTY_INT_ST)){
        DBG2("e\r\n");
	/* to output uart data from uart buffer directly in empty interrupt handler*/
	/*instead of processing in system event, in order not to wait for current task/function to quit */
	/*ATTENTION:*/
	/*IN NON-OS VERSION SDK, DO NOT USE "ICACHE_FLASH_ATTR" FUNCTIONS IN THE WHOLE HANDLER PROCESS*/
	/*ALL THE FUNCTIONS CALLED IN INTERRUPT HANDLER MUST BE DECLARED IN RAM */
	CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
	#if UART_BUFF_EN
		tx_start_uart_buffer(UART0);
	#endif
        //system_os_post(uart_recvTaskPrio, 1, 0);
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_TXFIFO_EMPTY_INT_CLR);
        
    }else if(UART_RXFIFO_OVF_INT_ST  == (READ_PERI_REG(UART_INT_ST(uart_no)) & UART_RXFIFO_OVF_INT_ST)){
        WRITE_PERI_REG(UART_INT_CLR(uart_no), UART_RXFIFO_OVF_INT_CLR);
        DBG2("RX OVF!!\r\n");
    }

}


#if UART_SELFTEST&UART_BUFF_EN
os_timer_t buff_timer_t;
void ICACHE_FLASH_ATTR
uart_test_rx()
{
    uint8 uart_buf[128]={0};
    uint16 len = 0;
    len = rx_buff_deq(uart_buf, 128 );
    tx_buff_enq(uart_buf,len);
}
#endif

LOCAL void ICACHE_FLASH_ATTR ///////
uart_recvTask(os_event_t *events)
{
    if(events->sig == 0){
    #if  UART_BUFF_EN  
        Uart_rx_buff_enq();
    #else
        uint8 fifo_len = (READ_PERI_REG(UART_STATUS(UART0))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
        uint8 d_tmp = 0;
        uint8 idx=0;
        uint8 data_temp[128],*data_tab,*data_buf;
        uint8 *pStr,i=0;

        //os_memset(data_temp,0,128);
        for(idx=0;idx<fifo_len;idx++) {
        	data_temp[idx] = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            //uart_tx_one_char(UART0, data_temp[idx]);
        }
        data_temp[fifo_len]=0;
        os_printf("%s \r\n",data_temp);
        if((strstr(data_temp,"VA"))&&(strstr(data_temp,"\r\n")))//确认指令格式
        {
        	if(data_tab=strstr(data_temp,VAHold))//模块上电接受到此命令将不进入休眠
        	{
        		data_buf = data_tab+(strlen(VAHold));
        		if((*data_buf)==0x31){HoldSoft=1;os_printf("set hold:%d\r\n",HoldSoft);}
        		else if((*data_buf)==0x30){HoldSoft=0;}
        	}
        	else if(data_tab=strstr(data_temp,VASetSN))
			{
				data_buf = data_tab+(strlen(VASetSN));
														//|----------|
				if((strlen(data_buf))==12)   //if(VASetSN:00xxxxxxxx\r\n)==12
				{
					*(data_buf+10) = 0;	//claer \r \n
					os_strcpy(VA_param.param_SN,data_tab+(strlen(VASetSN)));
					os_printf("get sn:%s \r\n",VA_param.param_SN);
					esp_VA_save_param(WitchSec);
				}
				else
				{
					os_printf("sn number error%d ! \r\n",(strlen(data_tab+(strlen(VASetSN)))));
				}
			}
        	//VASetINTERVAL
			else if(data_tab=strstr(data_temp,VASetINTERVAL))//Restart the module
			{
				data_buf = data_tab+(strlen(VASetINTERVAL));
				VA_param.param_interval=*data_buf;
				switch(VA_param.param_interval){
					case 0x31: 	interval_timer_cnt=(1*60-6)*1000000;break;//1min
					case 0x32: 	interval_timer_cnt=(5*60-6)*1000000;break;//5min
					case 0x33: 	interval_timer_cnt=(10*60-6)*1000000;break;//10min
					case 0x34: 	interval_timer_cnt=(30*60-6)*1000000;break;//30min
					case 0x35: 	interval_timer_cnt=3594000000UL;break;//(60*60-6)*100000;break;//60min  over flow
					default:	interval_timer_cnt=(5*60-6)*1000000;break;
					}
					os_printf("VASetINTERVAL:%d S!\r\n",interval_timer_cnt/1000000+6);
			}
        	else if(data_tab=strstr(data_temp,VASetSSID))
			{
        		data_buf = data_tab+(strlen(VASetSSID));data_tab=data_buf;
        		while(((*data_tab)!='\r')){data_tab++;}*data_tab = 0;
        		strcpy(VA_param.param_ap_ssid,data_buf);
        		os_printf("VA_param.param_ap_ssid:%s\r\n",VA_param.param_ap_ssid);
			}
        	else if(data_tab=strstr(data_temp,VASetPSW))
			{
        		data_buf = data_tab+(strlen(VASetPSW));data_tab=data_buf;
				while(((*data_tab)!='\r')){data_tab++;}*data_tab = 0;
				strcpy(VA_param.param_ap_psw,data_buf);
				os_printf("VA_param.param_ap_psw:%s\r\n",VA_param.param_ap_psw);
			}
        	else if(data_tab=strstr(data_temp,VASetMySSID))
			{
        		data_buf = data_tab+(strlen(VASetMySSID));data_tab=data_buf;
				while(((*data_tab)!='\r')){data_tab++;}*data_tab = 0;
				strcpy(VA_param.param_my_ssid,data_buf);
				os_printf("VA_param.param_my_ssid:%s\r\n",VA_param.param_my_ssid);
			}
        	else if(data_tab=strstr(data_temp,VASetMyPSW))
			{
        		data_buf = data_tab+(strlen(VASetMyPSW));data_tab=data_buf;
				while(((*data_tab)!='\r')){data_tab++;}*data_tab = 0;
				strcpy(VA_param.param_my_psw,data_buf);
				os_printf("VA_param.param_my_psw:%s\r\n",VA_param.param_my_psw);
			}
        	else if(data_tab=strstr(data_temp,VATCal))
        	{
        		data_buf = data_tab+(strlen(VATCal));
        		data_tab=data_buf;
        		while((*data_tab>0x29)&&(*data_tab<0x40)){data_tab++;}
        		VA_param.param_TCal = atoi(data_buf);
        		os_printf("%d\r\n",VA_param.param_TCal);
        	}
        	else if(data_tab=strstr(data_temp,VAHCal))
			{
				data_buf = data_tab+(strlen(VAHCal));
				data_tab=data_buf;
				while((*data_tab>0x29)&&(*data_tab<0x40)){data_tab++;}
				VA_param.param_HCal = atoi(data_buf);
				os_printf("%d\r\n",VA_param.param_HCal);
			}
			else if(data_tab=strstr(data_temp,VAResetSAVE))//New Record!
			{
				os_printf("VAResetSAVE start new record!!!\r\n-------------------\r\n");
				VA_param.param_SaveSection = 0;
			}
        	else if(data_tab=strstr(data_temp,VASetFUNC))//Set the Function
			{
				data_buf = data_tab+(strlen(VASetFUNC));
				data_tab=data_buf;
				while((*data_tab>0x29)&&(*data_tab<0x40)){data_tab++;}
				VA_param.param_ModelFunc = atoi(data_buf);
				os_printf("VA_param.param_ModelFunc:%d\r\n",VA_param.param_ModelFunc);
			}
        	else if(data_tab=strstr(data_temp,VARestart))//Restart the module
			{
				os_printf("Model Restart!!!\r\n-------------------\r\n");
				system_restart();
			}
        	else if(data_tab=strstr(data_temp,VARead))//Restart the module
			{
        		data_buf = data_tab+(strlen(VARead));
				switch(*data_buf)
				{
					case 0x31: os_printf("S:%s P:%s\r\n",VA_param.param_ap_ssid,VA_param.param_ap_psw);break;
					case 0x32: os_printf("MyS:%s MyP:%s\r\n",VA_param.param_my_ssid,VA_param.param_my_psw);break;
					case 0x33: os_printf("SN:%s\r\n",VA_param.param_SN);break;
					case 0x34: os_printf("Interval:%d S\r\n",interval_timer_cnt/1000000);break;
					case 0x35:  os_printf("SaveSection:%d    ModelFunc%d\r\n",VA_param.param_SaveSection,VA_param.param_ModelFunc);break;
					case 0x36:  os_printf("TCal:%d HCal%d\r\n",VA_param.param_TCal,VA_param.param_HCal);break;

					default:break;
				}
			}
        	//uint8 VASecDATA[]=	"VASecDATA:";//读取指定sec的数据
        	//uint8 VASecTIME[]=	"VASecTIME:";//读取指定sec的时间戳
        	//int32	RTimeSec=-1;//需要读取的timesec
        	//int32	RDataSec=-1;//
        	else if(data_tab=strstr(data_temp,VASecDATA))
			{
				data_buf = data_tab+(strlen(VASecDATA));
				data_tab=data_buf;
				while((*data_tab>0x29)&&(*data_tab<0x40)){data_tab++;}
				//Read_data(atoi(data_buf));
				RDataSec=atoi(data_buf);
				os_timer_arm(&read_timer,20,1);
			}
        	else if(data_tab=strstr(data_temp,VASecTIME))
			{
        		//user_time_t my_time;
				data_buf = data_tab+(strlen(VASecTIME));
				data_tab=data_buf;
				while((*data_tab>0x29)&&(*data_tab<0x40)){data_tab++;}
				RTimeSec=atoi(data_buf);
				os_timer_arm(&read_timer,20,1);
				/*my_time=ReadTime(atoi(data_buf));os_printf("1t\r\n");
				if(my_time.Savesec<=VA_param.param_SaveSection)
				{
					os_printf("2t\r\n");my_time.Timenc[0]=0;//防止字符串调用溢出。
					os_printf("3t\r\n");os_printf("TIME:%s\r\n",my_time.Time24.TimeTab);
				}*/

			}
        	else if(data_tab=strstr(data_temp,"READ:HIS"))
        	{
        		uint16 i4K,i,j,k;
        		uint16 N4K=0,Nowpage,NowSec=0;
        		typedef Mix32 SECData[INITFLAG_MEM_NUM]; //
        		SECData Readtemp[SECTION_NUM_PerPAGE];

        		//ThisPage = SENSOR_DATA_STARTADDR + (VA_param.param_SaveSection / SECTION_NUM_PerPAGE);
        		N4K 	= VA_param.param_SaveSection / SECTION_NUM_PerPAGE;
        		NowSec 	= VA_param.param_SaveSection % SECTION_NUM_PerPAGE;

        		os_printf("N4K:%d\r\n",N4K);
        		for(i4K=0;i4K<N4K;i4K++)
        		{
        			//Nowpage=SENSOR_DATA_STARTADDR+i
        			spi_flash_read( (SENSOR_DATA_STARTADDR+i4K) * SPI_FLASH_SEC_SIZE,(uint32 *)&Readtemp,sizeof( Readtemp ) );
        			for(i=0;i<SECTION_NUM_PerPAGE+1;i++)
					{
						for(j=0;j<INITFLAG_MEM_NUM;j++)
						{
							for(k=0;k<2;k++)
							{
								os_printf("%d",Readtemp[i][j].shortB[k]);
							}
							os_printf("-");
						}
					}
        		}
        		os_printf("-------------------\r\n");
        		os_printf("NowSec:%d\r\n",NowSec);
        		spi_flash_read( (SENSOR_DATA_STARTADDR+N4K) * SPI_FLASH_SEC_SIZE,(uint32 *)&Readtemp,sizeof( Readtemp ) );
        		for(i=0;i<NowSec;i++)
				{
					for(j=0;j<INITFLAG_MEM_NUM;j++)
					{
						for(k=0;k<2;k++)
						{
							os_printf("%d",Readtemp[i][j].shortB[k]);
						}
						os_printf("-");
					}
					//os_printf("***%d****\r\n",i);
				}
        	}

        	esp_VA_save_param(WitchSec);
        }
       /* uint8 VASetSN[]=  	"VASetSN:";
        uint8 VASetSSID[]=	"VASetSSID:";
        uint8 VASetPSW[]=	"VASetPSW:";
        uint8 VASetMySSID[]="VASetMySSID:";//当前模块的ssid和psw配置
        uint8 VASetMyPSW[]=	"VASetMyPSW:";
        uint8 VATCal[]=		"VATCal:";
        uint8 VAHCal[]=		"VAHCal:";
*/


        //get data
        /*data_tab=strstr(data_temp,":[");
        if(data_tab != NULL )
        {
        	data_tab+=7;
        	sscanf(data_tab,"%f , %f , %f , %f , %f",\
        							ATT7053_data.Vol,\
									ATT7053_data.Cur,\
									ATT7053_data.Pow,\
									ATT7053_data.Kwh,\
									ATT7053_data.nKwh);
        	printf("ATT7053_data.Vol:%s \r\n",ATT7053_data.Vol);
        	printf("ATT7053_data.Cur:%s \r\n",ATT7053_data.Cur);
        	printf("ATT7053_data.Pow:%s \r\n",ATT7053_data.Pow);
        	printf("ATT7053_data.Kwh:%s \r\n",ATT7053_data.Kwh);
        	printf("ATT7053_data.nKwh:%s \r\n",ATT7053_data.nKwh);

        }*/

        WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR|UART_RXFIFO_TOUT_INT_CLR);
        uart_rx_intr_enable(UART0);
    #endif
    }else if(events->sig == 1){
    #if UART_BUFF_EN
	 //already move uart buffer output to uart empty interrupt
        //tx_start_uart_buffer(UART0);
    #else 
    
    #endif
    }
}

/******************************************************************************
 * FunctionName : uart_init
 * Description  : user interface for init uart
 * Parameters   : UartBautRate uart0_br - uart0 bautrate
 *                UartBautRate uart1_br - uart1 bautrate
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_init(UartBautRate uart0_br, UartBautRate uart1_br)
{
    /*this is a example to process uart data from task,please change the priority to fit your application task if exists*/
    system_os_task(uart_recvTask, uart_recvTaskPrio, uart_recvTaskQueue, uart_recvTaskQueueLen);  //demo with a task to process the uart data
    
    UartDev.baut_rate = uart0_br;
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();
    
    #if UART_BUFF_EN
    pTxBuffer = Uart_Buf_Init(UART_TX_BUFFER_SIZE);
    pRxBuffer = Uart_Buf_Init(UART_RX_BUFFER_SIZE);
    #endif


    /*option 1: use default print, output from uart0 , will wait some time if fifo is full */
    //do nothing...

    /*option 2: output from uart1,uart1 output will not wait , just for output debug info */
    /*os_printf output uart data via uart1(GPIO2)*/
    //os_install_putc1((void *)uart1_write_char);    //use this one to output debug information via uart1 //

    /*option 3: output from uart0 will skip current byte if fifo is full now... */
    /*see uart0_write_char_no_wait:you can output via a buffer or output directly */
    /*os_printf output uart data via uart0 or uart buffer*/
    //os_install_putc1((void *)uart0_write_char_no_wait);  //use this to print via uart0
    
    #if UART_SELFTEST&UART_BUFF_EN
    os_timer_disarm(&buff_timer_t);
    os_timer_setfn(&buff_timer_t, uart_test_rx , NULL);   //a demo to process the data in uart rx buffer
    os_timer_arm(&buff_timer_t,10,1);
    #endif
}

void ICACHE_FLASH_ATTR
uart_reattach()
{
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
}

/******************************************************************************
 * FunctionName : uart_tx_one_char_no_wait
 * Description  : uart tx a single char without waiting for fifo 
 * Parameters   : uint8 uart - uart port
 *                uint8 TxChar - char to tx
 * Returns      : STATUS
*******************************************************************************/
STATUS uart_tx_one_char_no_wait(uint8 uart, uint8 TxChar)
{
    uint8 fifo_cnt = (( READ_PERI_REG(UART_STATUS(uart))>>UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);
    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(uart) , TxChar);
    }
    return OK;
}

STATUS uart0_tx_one_char_no_wait(uint8 TxChar)
{
    uint8 fifo_cnt = (( READ_PERI_REG(UART_STATUS(UART0))>>UART_TXFIFO_CNT_S)& UART_TXFIFO_CNT);
    if (fifo_cnt < 126) {
        WRITE_PERI_REG(UART_FIFO(UART0) , TxChar);
    }
    return OK;
}


/******************************************************************************
 * FunctionName : uart1_sendStr_no_wait
 * Description  : uart tx a string without waiting for every char, used for print debug info which can be lost
 * Parameters   : const char *str - string to be sent
 * Returns      : NONE
*******************************************************************************/
void uart1_sendStr_no_wait(const char *str)
{
    while(*str){
        uart_tx_one_char_no_wait(UART1, *str++);
    }
}


#if UART_BUFF_EN
/******************************************************************************
 * FunctionName : Uart_Buf_Init
 * Description  : tx buffer enqueue: fill a first linked buffer 
 * Parameters   : char *pdata - data point  to be enqueue
 * Returns      : NONE
*******************************************************************************/
struct UartBuffer* ICACHE_FLASH_ATTR
Uart_Buf_Init(uint32 buf_size)
{
    uint32 heap_size = system_get_free_heap_size();
    if(heap_size <=buf_size){
        DBG1("no buf for uart\n\r");
        return NULL;
    }else{
        DBG("test heap size: %d\n\r",heap_size);
        struct UartBuffer* pBuff = (struct UartBuffer* )os_malloc(sizeof(struct UartBuffer));
        pBuff->UartBuffSize = buf_size;
        pBuff->pUartBuff = (uint8*)os_malloc(pBuff->UartBuffSize);
        pBuff->pInPos = pBuff->pUartBuff;
        pBuff->pOutPos = pBuff->pUartBuff;
        pBuff->Space = pBuff->UartBuffSize;
        pBuff->BuffState = OK;
        pBuff->nextBuff = NULL;
        pBuff->TcpControl = RUN;
        return pBuff;
    }
}

//copy uart buffer
LOCAL void Uart_Buf_Cpy(struct UartBuffer* pCur, char* pdata , uint16 data_len)
{
    if(data_len == 0) return ;
    
    uint16 tail_len = pCur->pUartBuff + pCur->UartBuffSize - pCur->pInPos ;
    if(tail_len >= data_len){  //do not need to loop back  the queue
        os_memcpy(pCur->pInPos , pdata , data_len );
        pCur->pInPos += ( data_len );
        pCur->pInPos = (pCur->pUartBuff +  (pCur->pInPos - pCur->pUartBuff) % pCur->UartBuffSize );
        pCur->Space -=data_len;
    }else{
        os_memcpy(pCur->pInPos, pdata, tail_len);
        pCur->pInPos += ( tail_len );
        pCur->pInPos = (pCur->pUartBuff +  (pCur->pInPos - pCur->pUartBuff) % pCur->UartBuffSize );
        pCur->Space -=tail_len;
        os_memcpy(pCur->pInPos, pdata+tail_len , data_len-tail_len);
        pCur->pInPos += ( data_len-tail_len );
        pCur->pInPos = (pCur->pUartBuff +  (pCur->pInPos - pCur->pUartBuff) % pCur->UartBuffSize );
        pCur->Space -=( data_len-tail_len);
    }
    
}

/******************************************************************************
 * FunctionName : uart_buf_free
 * Description  : deinit of the tx buffer
 * Parameters   : struct UartBuffer* pTxBuff - tx buffer struct pointer
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
uart_buf_free(struct UartBuffer* pBuff)
{
    os_free(pBuff->pUartBuff);
    os_free(pBuff);
}


//rx buffer dequeue
uint16 ICACHE_FLASH_ATTR
rx_buff_deq(char* pdata, uint16 data_len )
{
    uint16 buf_len =  (pRxBuffer->UartBuffSize- pRxBuffer->Space);
    uint16 tail_len = pRxBuffer->pUartBuff + pRxBuffer->UartBuffSize - pRxBuffer->pOutPos ;
    uint16 len_tmp = 0;
    len_tmp = ((data_len > buf_len)?buf_len:data_len);
    if(pRxBuffer->pOutPos <= pRxBuffer->pInPos){
        os_memcpy(pdata, pRxBuffer->pOutPos,len_tmp);
        pRxBuffer->pOutPos+= len_tmp;
        pRxBuffer->Space += len_tmp;
    }else{
        if(len_tmp>tail_len){
            os_memcpy(pdata, pRxBuffer->pOutPos, tail_len);
            pRxBuffer->pOutPos += tail_len;
            pRxBuffer->pOutPos = (pRxBuffer->pUartBuff +  (pRxBuffer->pOutPos- pRxBuffer->pUartBuff) % pRxBuffer->UartBuffSize );
            pRxBuffer->Space += tail_len;
            
            os_memcpy(pdata+tail_len , pRxBuffer->pOutPos, len_tmp-tail_len);
            pRxBuffer->pOutPos+= ( len_tmp-tail_len );
            pRxBuffer->pOutPos= (pRxBuffer->pUartBuff +  (pRxBuffer->pOutPos- pRxBuffer->pUartBuff) % pRxBuffer->UartBuffSize );
            pRxBuffer->Space +=( len_tmp-tail_len);                
        }else{
            //os_printf("case 3 in rx deq\n\r");
            os_memcpy(pdata, pRxBuffer->pOutPos, len_tmp);
            pRxBuffer->pOutPos += len_tmp;
            pRxBuffer->pOutPos = (pRxBuffer->pUartBuff +  (pRxBuffer->pOutPos- pRxBuffer->pUartBuff) % pRxBuffer->UartBuffSize );
            pRxBuffer->Space += len_tmp;
        }
    }
    if(pRxBuffer->Space >= UART_FIFO_LEN){
        uart_rx_intr_enable(UART0);
    }
    return len_tmp; 
}


//move data from uart fifo to rx buffer
void Uart_rx_buff_enq()
{
    uint8 fifo_len,buf_idx;
    uint8 fifo_data;
    #if 1
    fifo_len = (READ_PERI_REG(UART_STATUS(UART0))>>UART_RXFIFO_CNT_S)&UART_RXFIFO_CNT;
    if(fifo_len >= pRxBuffer->Space){
        os_printf("buf full!!!\n\r");            
    }else{
        buf_idx=0;
        while(buf_idx < fifo_len){
            buf_idx++;
            fifo_data = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
            *(pRxBuffer->pInPos++) = fifo_data;
            if(pRxBuffer->pInPos == (pRxBuffer->pUartBuff + pRxBuffer->UartBuffSize)){
                pRxBuffer->pInPos = pRxBuffer->pUartBuff;
            }            
        }
        pRxBuffer->Space -= fifo_len ;
        if(pRxBuffer->Space >= UART_FIFO_LEN){
            //os_printf("after rx enq buf enough\n\r");
            uart_rx_intr_enable(UART0);
        }
    }
    #endif
}


//fill the uart tx buffer
void ICACHE_FLASH_ATTR
tx_buff_enq(char* pdata, uint16 data_len )
{
    CLEAR_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);

    if(pTxBuffer == NULL){
        DBG1("\n\rnull, create buffer struct\n\r");
        pTxBuffer = Uart_Buf_Init(UART_TX_BUFFER_SIZE);
        if(pTxBuffer!= NULL){
            Uart_Buf_Cpy(pTxBuffer ,  pdata,  data_len );
        }else{
            DBG1("uart tx MALLOC no buf \n\r");
        }
    }else{
        if(data_len <= pTxBuffer->Space){
        Uart_Buf_Cpy(pTxBuffer ,  pdata,  data_len);
        }else{
            DBG1("UART TX BUF FULL!!!!\n\r");
        }
    }
    #if 0
    if(pTxBuffer->Space <= URAT_TX_LOWER_SIZE){
	    set_tcp_block();        
    }
    #endif
    SET_PERI_REG_MASK(UART_CONF1(UART0), (UART_TX_EMPTY_THRESH_VAL & UART_TXFIFO_EMPTY_THRHD)<<UART_TXFIFO_EMPTY_THRHD_S);
    SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
}


//--------------------------------
LOCAL void tx_fifo_insert(struct UartBuffer* pTxBuff, uint8 data_len,  uint8 uart_no)
{
    uint8 i;
    for(i = 0; i<data_len;i++){
        WRITE_PERI_REG(UART_FIFO(uart_no) , *(pTxBuff->pOutPos++));
        if(pTxBuff->pOutPos == (pTxBuff->pUartBuff + pTxBuff->UartBuffSize)){
            pTxBuff->pOutPos = pTxBuff->pUartBuff;
        }
    }
    pTxBuff->pOutPos = (pTxBuff->pUartBuff +  (pTxBuff->pOutPos - pTxBuff->pUartBuff) % pTxBuff->UartBuffSize );
    pTxBuff->Space += data_len;
}

/******************************************************************************
 * FunctionName : tx_start_uart_buffer
 * Description  : get data from the tx buffer and fill the uart tx fifo, co-work with the uart fifo empty interrupt
 * Parameters   : uint8 uart_no - uart port num
 * Returns      : NONE
*******************************************************************************/
void tx_start_uart_buffer(uint8 uart_no)
{
    uint8 tx_fifo_len = (READ_PERI_REG(UART_STATUS(uart_no))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT;
    uint8 fifo_remain = UART_FIFO_LEN - tx_fifo_len ;
    uint8 len_tmp;
    uint16 tail_ptx_len,head_ptx_len,data_len;
    //struct UartBuffer* pTxBuff = *get_buff_prt();
    
    if(pTxBuffer){      
        data_len = (pTxBuffer->UartBuffSize - pTxBuffer->Space);
        if(data_len > fifo_remain){
            len_tmp = fifo_remain;
            tx_fifo_insert( pTxBuffer,len_tmp,uart_no);
            SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
        }else{
            len_tmp = data_len;
            tx_fifo_insert( pTxBuffer,len_tmp,uart_no);
        }
    }else{
        DBG1("pTxBuff null \n\r");
    }
}

#endif


void uart_rx_intr_disable(uint8 uart_no)
{
#if 1
    CLEAR_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
#else
    ETS_UART_INTR_DISABLE();
#endif
}

void uart_rx_intr_enable(uint8 uart_no)
{
#if 1
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), UART_RXFIFO_FULL_INT_ENA|UART_RXFIFO_TOUT_INT_ENA);
#else
    ETS_UART_INTR_ENABLE();
#endif
}


//========================================================
LOCAL void
uart0_write_char(char c)
{
    if (c == '\n') {
        uart_tx_one_char(UART0, '\r');
        uart_tx_one_char(UART0, '\n');
    } else if (c == '\r') {
    } else {
        uart_tx_one_char(UART0, c);
    }
}

void ICACHE_FLASH_ATTR
UART_SetWordLength(uint8 uart_no, UartBitsNum4Char len) 
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_BIT_NUM,len,UART_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetStopBits(uint8 uart_no, UartStopBitsNum bit_num) 
{
    SET_PERI_REG_BITS(UART_CONF0(uart_no),UART_STOP_BIT_NUM,bit_num,UART_STOP_BIT_NUM_S);
}

void ICACHE_FLASH_ATTR
UART_SetLineInverse(uint8 uart_no, UART_LineLevelInverse inverse_mask) 
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_LINE_INV_MASK);
    SET_PERI_REG_MASK(UART_CONF0(uart_no), inverse_mask);
}

void ICACHE_FLASH_ATTR
UART_SetParity(uint8 uart_no, UartParityMode Parity_mode) 
{
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_PARITY |UART_PARITY_EN);
    if(Parity_mode==NONE_BITS){
    }else{
        SET_PERI_REG_MASK(UART_CONF0(uart_no), Parity_mode|UART_PARITY_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_SetBaudrate(uint8 uart_no,uint32 baud_rate)
{
    uart_div_modify(uart_no, UART_CLK_FREQ /baud_rate);
}

void ICACHE_FLASH_ATTR
UART_SetFlowCtrl(uint8 uart_no,UART_HwFlowCtrl flow_ctrl,uint8 rx_thresh)
{
    if(flow_ctrl&USART_HardwareFlowControl_RTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_U0RTS);
        SET_PERI_REG_BITS(UART_CONF1(uart_no),UART_RX_FLOW_THRHD,rx_thresh,UART_RX_FLOW_THRHD_S);
        SET_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF1(uart_no), UART_RX_FLOW_EN);
    }
    if(flow_ctrl&USART_HardwareFlowControl_CTS){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_UART0_CTS);
        SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }else{
        CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_TX_FLOW_EN);
    }
}

void ICACHE_FLASH_ATTR
UART_WaitTxFifoEmpty(uint8 uart_no , uint32 time_out_us) //do not use if tx flow control enabled
{
    uint32 t_s = system_get_time();
    while (READ_PERI_REG(UART_STATUS(uart_no)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S)){
		
        if(( system_get_time() - t_s )> time_out_us){
            break;
        }
	WRITE_PERI_REG(0X60000914, 0X73);//WTD

    }
}


bool ICACHE_FLASH_ATTR
UART_CheckOutputFinished(uint8 uart_no, uint32 time_out_us)
{
    uint32 t_start = system_get_time();
    uint8 tx_fifo_len;
    uint32 tx_buff_len;
    while(1){
        tx_fifo_len =( (READ_PERI_REG(UART_STATUS(uart_no))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT);
        if(pTxBuffer){
            tx_buff_len = ((pTxBuffer->UartBuffSize)-(pTxBuffer->Space));
        }else{
            tx_buff_len = 0;
        }
		
        if( tx_fifo_len==0 && tx_buff_len==0){
            return TRUE;
        }
        if( system_get_time() - t_start > time_out_us){
            return FALSE;
        }
	WRITE_PERI_REG(0X60000914, 0X73);//WTD
    }    
}


void ICACHE_FLASH_ATTR
UART_ResetFifo(uint8 uart_no)
{
    SET_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uart_no), UART_RXFIFO_RST | UART_TXFIFO_RST);
}

void ICACHE_FLASH_ATTR
UART_ClearIntrStatus(uint8 uart_no,uint32 clr_mask)
{
    WRITE_PERI_REG(UART_INT_CLR(uart_no), clr_mask);
}

void ICACHE_FLASH_ATTR
UART_SetIntrEna(uint8 uart_no,uint32 ena_mask)
{
    SET_PERI_REG_MASK(UART_INT_ENA(uart_no), ena_mask);
}


void ICACHE_FLASH_ATTR
UART_SetPrintPort(uint8 uart_no)
{
    if(uart_no==1){
        os_install_putc1(uart1_write_char);
    }else{
        /*option 1: do not wait if uart fifo is full,drop current character*/
        os_install_putc1(uart0_write_char_no_wait);
	/*option 2: wait for a while if uart fifo is full*/
	os_install_putc1(uart0_write_char);
    }
}

//========================================================
/*test code*/
void ICACHE_FLASH_ATTR
uart_init_2(UartBautRate uart0_br, UartBautRate uart1_br)
{
    // rom use 74880 baut_rate, here reinitialize
    UartDev.baut_rate = uart0_br;
    UartDev.exist_parity = STICK_PARITY_EN;
    UartDev.parity = EVEN_BITS;
    UartDev.stop_bits = ONE_STOP_BIT;
    UartDev.data_bits = EIGHT_BITS;
	
    uart_config(UART0);
    UartDev.baut_rate = uart1_br;
    uart_config(UART1);
    ETS_UART_INTR_ENABLE();

    // install uart1 putc callback
    os_install_putc1((void *)uart1_write_char);//print output at UART1
}


