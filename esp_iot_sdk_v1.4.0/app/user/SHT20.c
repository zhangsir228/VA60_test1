#include "ets_sys.h"
#include "osapi.h"

#include "SHT20.h"
#include "i2c_master.h"
#include "TCPclient.h"

extern struct VA_saved_param	VA_param;

int16_t   SHT_humi=560;
int16_t   SHT_temp=340;

uint8 test_read[5];

void ICACHE_FLASH_ATTR
 SET_Resolution(void)
{
	i2c_master_start();
	i2c_master_writeByte(SHT20ADDR&0xfe);
	if(i2c_master_checkAck())
	{//os_printf("SET_Resolution addr!!! \r\n");
		i2c_master_writeByte(0xe6);
		if(i2c_master_checkAck())
		{//os_printf("SET_Resolution 0xe6!!! \r\n");
			i2c_master_writeByte(0x83);
			i2c_master_checkAck();
			//os_printf("SET_Resolution 0x83!!! \r\n");
		}
	}
	i2c_master_stop();
}

/***************************/
//TMPorRH:TMP 0xf3 RH 0xf5
//return value
//fail return 0
/*****************************/
float ICACHE_FLASH_ATTR
 ReadSHT20(char TMPorRH)
{
	 float temp;
	 uint8 MSB,LSB;
	 float Humidity,Temperature;

	 SET_Resolution();
	 i2c_master_start();
	 i2c_master_writeByte(SHT20ADDR&0xfe);
	 if(i2c_master_getAck()==NACK)
	 {
		 return 0;
	 }
	 i2c_master_writeByte(TMPorRH);
	 if(i2c_master_getAck()==NACK)
	 {
		 return 0;
	 }

     i2c_master_wait(4200);
	 i2c_master_start();
	i2c_master_writeByte(SHT20ADDR|0x01);
	while(i2c_master_getAck()==NACK)
	{
		i2c_master_wait(400);
		i2c_master_start();
		i2c_master_writeByte(SHT20ADDR|0x01);
		 //os_printf("waitting!!!\r\n");
	}

	 MSB = i2c_master_readByte();i2c_master_send_ack();i2c_master_wait(20);
	 LSB = i2c_master_readByte();i2c_master_send_ack();i2c_master_wait(20);
	  i2c_master_readByte();i2c_master_send_nack();i2c_master_wait(20);
	 i2c_master_stop();

	 //os_printf("MSB:%d !!!\r\n",MSB);
	 //os_printf("LSB:%d !!!\r\n",LSB);

	 i2c_master_stop();
	 i2c_master_wait(100);

	 LSB &= 0xfc;
	 temp = (uint16)(MSB*256) + LSB;

	 if (TMPorRH==((char)0xf5))
	 {
		  /*-- calculate relative humidity [%RH] --*/
			Humidity =(temp*125/65536-6)*10;
			return Humidity;
	 }
	 else
	 {
	   /*-- calculate temperature [°„C] --*/
			Temperature = temp*0.026813-468.5;
			return Temperature;
	 }
	 return 0;
}

bool ICACHE_FLASH_ATTR
 SHT20_read(void)
{
	SHT_temp = (int16_t)(ReadSHT20(0xf3))+VA_param.param_TCal;
	i2c_master_wait(8);
	SHT_humi = (int16_t)(ReadSHT20(0xf5))+VA_param.param_HCal;
	return SHT_humi;
}
