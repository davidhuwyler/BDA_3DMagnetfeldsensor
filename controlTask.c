/*
 * controlTask.c
 *
 *  Created on: 07.03.2016
 *      Author: xxdav
 */
#include <math.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "controlTask.h"
#include "i2c_accel.h"
#include "uart.h"
#include "app.h"
#include "ringBuffer.h"



/*
 * Static Prototypes:
 */
static uint16_t getMinFromStartUp(void);
//static void getAccelWithoutG(void);
static err_door_t controlDoorstate(uint16_t *controllNr);
static void printOutLog(void);
static void writeToFlash(uint16_t *minsSinceStartUp, uint16_t *controllNr, err_door_t *error);
static void eraseFlash(void);


extern floatRingBufHandle *accelBuffer;
//floatRingBufHandle *accelOffsetBuffer;
//extern float z_accel;

extern device_mode_t device_mode;
extern door_state_t door_state;

#if FlashLogging
static FLASH_EraseInitTypeDef Flash_Eraser;
static uint32_t SectorEraseError;
#endif

void StartControlTask(void const * argument)
{
	volatile uint16_t mins;
	volatile static uint16_t controllNr = 0;
//	accelBuffer = initfloatBuffer(1);
//	accelOffsetBuffer = initfloatBuffer(2);
	for(;;)
	{
		volatile err_door_t err_door;
		mins = getMinFromStartUp();
//		getAccelWithoutG();

		if(device_mode == mode_init)
		{
			printOutLog();
		}

		else if(device_mode == mode_run)
		{
			err_door =controlDoorstate(&controllNr);
			if(err_door!=err_ok)
			{
				writeToFlash(&mins, &controllNr, &err_door);
			}
		}


		osDelay(100);
	}

}


static void printOutLog(void)
{
	uint32_t address = 0x08020000;
	static uint8_t alreadyPrinted = 0;
	uint16_t fehlerNr = 1;

	HAL_FLASH_Unlock();
	if( *((uint32_t *) address)!=0xFFFFFFFF  && !alreadyPrinted)
	{
		HAL_FLASH_Lock();
		while(FLASH_WaitForLastOperation(1000)!=HAL_OK){}
		alreadyPrinted = 1;
		char car_ret = '\n';
		char string1[73]="------------------------------------------------------------------------\n";
		char string2[73]="|                    Tuerendetektor Fehler Logbuch                     |\n";
		char string2_1[73]="|                                                                      |\n";
		char string2_2[73]="|                    Datum: 22.03.2016    FW Version 1.0               |\n";
		char string3[15]="\tKontrolle Nr: ";
		char string4[11]="Fehler Nr: ";
		char string5[13]="\tZeit [min]: ";
		char string6[17]="\tFehler: Türe zu\n";
		char string7[20]="\tFehler: Türe offen\n";

		println(&car_ret,sizeof(char));println(&car_ret,sizeof(char));
		println(string1,sizeof(string1));println(string2,sizeof(string2));
		println(string2_1,sizeof(string2_1));println(string2_2,sizeof(string2_2));
		println(string1,sizeof(string1));println(&car_ret,sizeof(char));
		HAL_FLASH_Unlock();
		while(*((uint32_t *) address)!=0xFFFFFFFF)
		{
			HAL_FLASH_Lock();
			uint64_t tempData;
			uint16_t mins, contrNr, errCode;

			HAL_FLASH_Unlock();
			tempData = *((uint64_t *) address);
			HAL_FLASH_Lock();
			mins = (uint16_t)tempData;
			contrNr = (uint16_t)(tempData>>16);
			errCode = (uint32_t)(tempData>>32);

			println(string4,sizeof(string4));
			printNumber(fehlerNr);
			println(string5,sizeof(string5));
			printNumber(mins);
			println(string3,sizeof(string3));
			printNumber(contrNr);


			if(errCode == (uint32_t)(0x00000001)) //== err_should_be_open -> Fehler Türe geschlossen
			{
				println(string6,sizeof(string6));
			}
			else //if(*((uint16_t *) address+4)  // == err_should_be_closed) -> Fehler Türe offen
			{
				println(string7,sizeof(string7));
			}
			address = address +8;
			fehlerNr++;
			HAL_FLASH_Unlock();
		}
		HAL_FLASH_Lock();

		println(&car_ret,sizeof(char));println(&car_ret,sizeof(char));
		char string10[28]="Press Button1 to Erase Flash\n";
		println(string10,sizeof(string10));
		vPortEnterCritical();
		//TODO alle LEDs anzünden!
		while(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)){}
		vPortExitCritical();
		eraseFlash();

	}

}



static void writeToFlash(uint16_t *minsSinceStartUp, uint16_t *controllNr, err_door_t *error)
{
#if FlashLogging
	static uint16_t adressLow = 0x0000;
	static uint16_t adressHigh = 0x0802;

	uint32_t adress = (uint32_t)((adressHigh<<16)|adressLow);
	uint32_t data1 = (uint32_t)(*controllNr<<16)|(*minsSinceStartUp);
	uint32_t data2 = (uint32_t)(*error);

	if(adress<0x08080000)
	{
		vPortEnterCritical();
		HAL_FLASH_Unlock();
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,adress,data1);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,adress+4,data2);
		HAL_FLASH_Lock();
		vPortExitCritical();
		adress = adress +8;
	}
	adressLow = (uint16_t)adress;
	adressHigh =(uint16_t)(adress>>16);
#endif
}

static void eraseFlash(void)
{
	vPortEnterCritical();

	Flash_Eraser.TypeErase = FLASH_TYPEERASE_SECTORS;
	Flash_Eraser.Banks = FLASH_BANK_1;
	Flash_Eraser.VoltageRange = FLASH_VOLTAGE_RANGE_3; 	//2.7 - 3.6 V
	Flash_Eraser.Sector = FLASH_SECTOR_5; 				//Erster Sektor, welcher zu löschen ist
	Flash_Eraser.NbSectors = 3;							//3 Sektoren Löschen (5,6 und 7)
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&Flash_Eraser,&SectorEraseError);

	while(SectorEraseError!=0xFFFFFFFFU){}
	HAL_FLASH_Lock();
	vPortExitCritical();
}


static uint16_t getMinFromStartUp(void)
{
	TickType_t ticksSinceLastCall;
	static TickType_t lastTicks=0;
	static uint16_t mins = 0;
	static float mins_f = 0;

	ticksSinceLastCall = xTaskGetTickCount()-lastTicks;
	mins_f = mins_f + (ticksSinceLastCall/60000.0f);
	mins = (uint16_t)(mins_f+0.5);
	lastTicks = xTaskGetTickCount();

	return mins;
}

static err_door_t controlDoorstate(uint16_t *controllNr)
{
	static uint16_t timer1 = 0; //Zeit bis sich die Türe öffen muss;
	static uint32_t timer2 = 0; //Zeit zwischen 2 Fehlern
	static uint8_t timerRunns1 = 0;
	static uint8_t accelStoppedSinceLastCall = 0;
	static TickType_t lastTicks=0;
	volatile err_door_t err_door;
	float mean_z_accel;
	getMeanFloatValue(accelBuffer,&mean_z_accel);


	// Timer unterhalten
	if(timerRunns1)
	{
		timer1 = timer1 + (xTaskGetTickCount()-lastTicks);
	}
	timer2 = timer2 + (xTaskGetTickCount()-lastTicks);
	lastTicks = xTaskGetTickCount();


	//Aufzug Fährt?
	if(fabs(mean_z_accel)>=accel_aufzug_runns_threshold)
	{
		if(accelStoppedSinceLastCall)
		{
			*controllNr = *controllNr +1;
			accelStoppedSinceLastCall = 0;
		}
		timerRunns1 = 1; //Timer Starten
		timer1 = 0;

		if(door_state==door_open && timer2>timeout_after_error)
		{
			timer2 = 0;
			err_door = err_should_be_closed;
			eraseFloatBuffer(accelBuffer);
		}
		else
		{
			err_door = err_ok;
		}
	}
	else if(timerRunns1 && timer1>timeout_after_accel && timer2>timeout_after_error) // Timer Abgelaufen
	{
		timer1 = 0;
		timer2 = 0;
		timerRunns1 = 0;
		err_door = err_should_be_open;
		accelStoppedSinceLastCall = 1;
	}
	else if(timerRunns1 && timer1<=timeout_after_accel && door_state==door_open) // Türe öffnete sich im Zeitfenster
	{
		timer1 = 0;
		timerRunns1 = 0;
		err_door = err_ok;
		accelStoppedSinceLastCall = 1;
	}
	else
	{
		err_door = err_ok;
		accelStoppedSinceLastCall = 1;
	}

	return err_door;

}
