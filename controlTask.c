/*
 * controlTask.c
 *
 *  Created on: 07.03.2016
 *      Author: xxdav
 */
#include <math.h>
#if defined STM32F401xE || STM32F405xE
#include "stm32f4xx_hal.h"
#endif
#if defined STM32L476xx
#include "stm32l4xx_hal.h"
#endif
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
static err_door_t checkButtons(void);

extern floatRingBufHandle *accelBuffer;

extern device_mode_t device_mode;
extern door_state_t door_state;

uint8_t enableLEDOutput = 1;

void StartControlTask(void const * argument)
{
	uint16_t mins;
	static uint16_t controllNr;
	controllNr = 0;
	uint32_t PreviousWakeTime = osKernelSysTick();
	for(;;)
	{
		err_door_t err_door;
		mins = getMinFromStartUp();

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
		//osDelay(100);
		osDelayUntil(&PreviousWakeTime, 100);
	}

}


static void printOutLog(void)
{
#if defined STM32F401xE
	uint32_t address = 0x08020000;
#endif
#if defined STM32L476xx
	uint32_t address = 0x08080000;
#endif

	static uint8_t alreadyPrinted = 0,loggingFinished = 0;
	uint16_t fehlerNr = 1;

	HAL_FLASH_Unlock();
	if( *((uint32_t *) address)!=0xFFFFFFFF  && !alreadyPrinted)
	{
		HAL_FLASH_Lock();
		alreadyPrinted = 1;
		uint8_t car_ret = '\n';
		uint8_t string1[73]="------------------------------------------------------------------------\n";
		uint8_t string2[73]="|                    Tuerendetektor Fehler Logbuch                     |\n";
		uint8_t string2_1[73]="|                                                                      |\n";
		uint8_t string2_2[73]="|                    Datum: 05.04.2016    FW Version 2.1               |\n";
		uint8_t string3[15]="\tKontrolle Nr: ";
		uint8_t string4[11]="Fehler Nr: ";
		uint8_t string5[13]="\tZeit [min]: ";
		uint8_t string6[17]="\tFehler: Türe zu\n";
		uint8_t string7[20]="\tFehler: Türe offen\n";
		uint8_t string8[33]="\tFehler: Beschleunigung zu gross\n";
		uint8_t string9[73]="-----------------------Marker! Button S2 Pressed!-----------------------\n";

		println(&car_ret,sizeof(uint8_t));println(&car_ret,sizeof(uint8_t));
		println(string1,sizeof(string1));println(string2,sizeof(string2));
		println(string2_1,sizeof(string2_1));println(string2_2,sizeof(string2_2));
		println(string1,sizeof(string1));println(&car_ret,sizeof(uint8_t));
		HAL_FLASH_Unlock();
		while(*((uint32_t *) address)!=0xFFFFFFFF && !loggingFinished)
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

			if(errCode == logging_finish)
			{
				loggingFinished = 1;
				float errRate;
				errRate = (100.0*(fehlerNr-1))/(float)contrNr;
				println(&car_ret,sizeof(uint8_t));
				println(string1,sizeof(string1));
				uint8_t string10_0[73]="|                    Fehler Logbuch abgeschlossen                      |\n";
				println(string10_0,sizeof(string10_0));
				println(string2_1,sizeof(string2_1));
				uint8_t string10_1[15]="Anzahl Fehler: ";
				println(string10_1,sizeof(string10_1));
				printNumber(fehlerNr-1);
				uint8_t string10_2[22]="   Anzahl Kontrollen: ";
				println(string10_2,sizeof(string10_2));
				printNumber(contrNr);
				uint8_t string10_3[23]="   Fehlerrate [%*100]: ";
				println(string10_3,sizeof(string10_3));
				printNumber((int16_t)((errRate)*100)+0.5);
				println(&car_ret,sizeof(uint8_t));
				uint8_t string10_4[15]="   Zeit [min]: ";
				println(string10_4,sizeof(string10_4));
				printNumber(mins);
				println(&car_ret,sizeof(uint8_t));
				println(string1,sizeof(string1));
				println(&car_ret,sizeof(uint8_t));
			}
			else
			{
				if(errCode == err_should_be_open)
				{
					println(string4,sizeof(string4));
					printNumber(fehlerNr);
					println(string5,sizeof(string5));
					printNumber(mins);
					println(string3,sizeof(string3));
					printNumber(contrNr);
					println(string6,sizeof(string6));
				}
				else if(errCode == err_should_be_closed)
				{
					println(string4,sizeof(string4));
					printNumber(fehlerNr);
					println(string5,sizeof(string5));
					printNumber(mins);
					println(string3,sizeof(string3));
					printNumber(contrNr);
					println(string7,sizeof(string7));
				}
				else if(errCode == err_accel_too_big)
				{
					println(string4,sizeof(string4));
					printNumber(fehlerNr);
					println(string5,sizeof(string5));
					printNumber(mins);
					println(string3,sizeof(string3));
					printNumber(contrNr);
					println(string8,sizeof(string8));
				}
				else if(errCode == set_marker)
				{
					println(string9,sizeof(string9));
					fehlerNr--;
				}
			}

			address = address +8;
			fehlerNr++;
			HAL_FLASH_Unlock();
		}
		HAL_FLASH_Lock();

		println(&car_ret,sizeof(uint8_t));println(&car_ret,sizeof(uint8_t));
		uint8_t string10[29]="Press Button1 to Erase Flash\n";
		println(string10,sizeof(string10));

		vPortEnterCritical();
		//TODO alle LEDs anzünden!
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_SET);
		while(HAL_GPIO_ReadPin(GPIOB,button_S1) || HAL_GPIO_ReadPin(GPIOB,button_S2)){}  //Antelle C 13
		eraseFlash();
		//TODO alle LEDs auslöschen!
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
		vPortExitCritical();

	}

}



static void writeToFlash(uint16_t *minsSinceStartUp, uint16_t *controllNr, err_door_t *error)
{
#if FlashLogging

#if defined STM32F401xE
	static uint16_t adressLow = 0x0000;
	static uint16_t adressHigh = 0x0802;
#endif
#if defined STM32L476xx
	static uint16_t adressLow = 0x0000;
	static uint16_t adressHigh = 0x0808;
#endif
	uint32_t adress = (uint32_t)((adressHigh<<16)|adressLow);
	uint32_t data1 = (uint32_t)(*controllNr<<16)|(*minsSinceStartUp);
	uint32_t data2 = (uint32_t)(*error);


#if defined STM32F401xE
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
#endif

#if defined STM32L476xx
	if(adress<=0x080FFFFF)
	{
		uint64_t data = (uint64_t)(((uint64_t)data2<<32)|(data1));
		vPortEnterCritical();
		HAL_FLASH_Unlock();
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, adress, data);
		HAL_FLASH_Lock();
		vPortExitCritical();
		adress = adress +8;
	}
#endif

	adressLow = (uint16_t)adress;
	adressHigh =(uint16_t)(adress>>16);
#endif
}

static void eraseFlash(void)
{
#if defined STM32F401xE
	FLASH_EraseInitTypeDef Flash_Eraser;
	uint32_t SectorEraseError;
	Flash_Eraser.TypeErase = FLASH_TYPEERASE_SECTORS;
	Flash_Eraser.Banks = FLASH_BANK_1;
	Flash_Eraser.VoltageRange = FLASH_VOLTAGE_RANGE_3; 	//2.7 - 3.6 V
	Flash_Eraser.Sector = FLASH_SECTOR_5; 				//Erster Sektor, welcher zu löschen ist
	Flash_Eraser.NbSectors = 3;							//3 Sektoren Löschen (5,6 und 7)
	HAL_FLASH_Unlock();
	HAL_FLASHEx_Erase(&Flash_Eraser,&SectorEraseError);
//	while(SectorEraseError!=0xFFFFFFFFU){}
	HAL_FLASH_Lock();
#endif

#if defined STM32L476xx
	HAL_FLASH_Unlock();
	uint32_t eraseError;
	FLASH_EraseInitTypeDef Flash_Eraser;
	Flash_Eraser.TypeErase = FLASH_TYPEERASE_MASSERASE;
	Flash_Eraser.Banks = FLASH_BANK_2;
	HAL_FLASHEx_Erase(&Flash_Eraser, &eraseError);
	//while(eraseError!=0xFFFFFFFF){}
	HAL_FLASH_Lock();
#endif
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
	volatile err_door_t err_door;
#if FlashLogging
	static uint16_t timer1 = 0; //Zeit bis sich die Türe öffen muss;
	static uint32_t timer2 = 0; //Zeit zwischen 2 Fehlern
	static uint8_t timerRunns1 = 0;
	static uint8_t accelStoppedSinceLastCall = 0;
	static TickType_t lastTicks=0;
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
	if(fabs(mean_z_accel)>=accel_aufzug_accel_threshold)
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

#if Enable_err_accelToBig
		//Der Beschleunigungsfehler wird Ausgelöst:
		if(fabs(mean_z_accel)>=accel_aufzug_bigAccel_threshold)
		{
			err_door = err_accel_too_big;
		}
#endif
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

	if(err_door==err_ok)
	{
		err_door = checkButtons();
	}
#endif
	return err_door;

}

static err_door_t checkButtons(void)
{
	static uint8_t button_S1_counter = 0;
	static uint8_t button_S2_counter = 0;
	static uint8_t button_S1_S2_counter = 0;
	err_door_t error = err_ok;

	if(!(HAL_GPIO_ReadPin(GPIOB,button_S1)) && !(HAL_GPIO_ReadPin(GPIOB,button_S2)))
	{
		button_S1_S2_counter++;
	}
	else if(!(HAL_GPIO_ReadPin(GPIOB,button_S1)))
	{
		button_S1_counter++;
	}
	else if(!(HAL_GPIO_ReadPin(GPIOB,button_S2)))
	{
		button_S2_counter++;
	}

	if(button_S1_counter >= 10)
	{
		button_S1_counter = 0;
		button_S2_counter = 0;
		button_S1_S2_counter = 0;
		enableLEDOutput = !enableLEDOutput;

		volatile uint32_t i;
		i = 0;
		vPortEnterCritical();
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_SET);
		while(i<8000000){i++;}
		i = 0;
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
		while(i<8000000){i++;}
		vPortExitCritical();
	}

	if(button_S2_counter >= 10)
	{
		button_S1_counter = 0;
		button_S2_counter = 0;
		button_S1_S2_counter = 0;
		error = set_marker;

		volatile uint32_t i;
		i = 0;
		vPortEnterCritical();
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_SET);
		while(i<8000000){i++;}
		i = 0;
		HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
		while(i<8000000){i++;}
		vPortExitCritical();
	}

	if(button_S1_S2_counter >= 10)
	{
		button_S1_S2_counter = 0;
		button_S1_counter = 0;
		button_S2_counter = 0;

		error = logging_finish;

		volatile uint32_t i;
		uint8_t j;
		for(j = 0 ; j < 3; j++)
		{
			i = 0;
			vPortEnterCritical();
			HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_SET);
			while(i<8000000){i++;}
			i = 0;
			HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
			while(i<8000000){i++;}
			vPortExitCritical();
		}
	}

	return error;
}
