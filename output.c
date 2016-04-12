/*
 * output.c
 *
 *  Created on: 12.04.2016
 *      Author: xxdav
 */
#include "output.h"
#include "app.h"
#include "controlTask.h"
#include "uart.h"
#include "tm_stm32f4_pcd8544.h"
#include "ringBuffer.h"
#include <stdlib.h>
#include <math.h>

extern uint8_t enableLEDOutput;
extern uint8_t door_State_percent;
extern vectorRingBufHandle *lastVectorsBuffer;
extern device_mode_t device_mode;
extern door_state_t door_state;
extern float z_accel;
/*
 * Druckt die Resultate auf die UART-Schnittstelle und dem LCD
 * und bringt die Entsprechenden LEDs zum Leuchten
 */

void outputResult(magnVec_t *vektor)
{
	static uint8_t i=0;

	if(vektor->state == vek_dynamic)
	{
		#if EnableLED_Output
		if(enableLEDOutput)
		{
			HAL_GPIO_TogglePin(GPIOB,LED_changing);
		}
		#endif
		#if EnableLCD
		if(device_mode == mode_init)
		{
			PCD8544_GotoXY(0, 40);
			PCD8544_Puts("Vektor dynam. ", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		}
		#endif
	}
	else
	{
		#if EnableLCD
		if(device_mode == mode_init)
		{
			PCD8544_GotoXY(0, 40);
			PCD8544_Puts("Vektor stat.  ", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		}
		#endif
	}
	if(device_mode == mode_waitForDrive)
	{
		#if EnableLED_Output
		if(enableLEDOutput)
		{
			HAL_GPIO_TogglePin(GPIOB,LED_changing);
		}
		#endif
		#if EnableLCD
		PCD8544_Clear();
		PCD8544_GotoXY(0, 0);
		PCD8544_Puts("Run Mode!", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_Refresh();
		#endif
	}

	if(i==5)		//Nur jeder 5te Aufruf die Ausgabe Tätigen
	{
#if EnableLCD
	PCD8544_Clear();
#endif
		//vPortEnterCritical();
		if(device_mode == mode_init)
		{
			// TODO LEDs blink!!!
			#if EnableLED_Output
			if(enableLEDOutput)
			{
				HAL_GPIO_TogglePin(GPIOB,LED_closed);
				HAL_GPIO_TogglePin(GPIOB,LED_open);
			}
			#endif
			#if EnableLCD
			PCD8544_GotoXY(0, 0);
			PCD8544_Puts("Tuerendetektor", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(0, 13);
			PCD8544_Puts("BDA D.Huwyler", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(0, 30);
			PCD8544_Puts("Init-Mode...", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			#endif

		}

		else if(device_mode == mode_run)
		{
			#if EnableUART_Output
			uint8_t string1[45]="---------------------------------------------";
			uint8_t temp = '\n';
			println(&temp,sizeof(uint8_t));println(&temp,sizeof(uint8_t));println(&temp,sizeof(uint8_t));
			println(string1,sizeof(string1));
			println(&temp,sizeof(uint8_t));
			#endif
			//wenn türe weniger als 10% offen ist -> Geschlossen
			if(door_state==door_closed)
			{
				#if EnableUART_Output
				uint8_t string2[18]="Tuere geschlossen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LED
				#if EnableLED_Output
				if(enableLEDOutput)
				{
					HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
				}
				#endif

				#if EnableLCD
			    PCD8544_GotoXY(0, 0);
			    PCD8544_Puts("Tuere geschl.!", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
				#endif
			}
			//wenn türe mehr als 90% offen ist -> Offen
			else if(door_state==door_open)
			{
				#if EnableUART_Output
				uint8_t string2[12]="Tuere offen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LED
				#if EnableLED_Output
				if(enableLEDOutput)
				{
					HAL_GPIO_WritePin(GPIOB,LED_closed,GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOB,LED_open,GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOB,LED_changing,GPIO_PIN_RESET);
				}
				#endif
				#if EnableLCD
				PCD8544_GotoXY(0, 0);
				PCD8544_Puts("Tuere offen!", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
				#endif
			}
			else if(door_state==door_moving)
			{
				#if EnableUART_Output
				uint8_t string2[20]="Tuere in Bewegung...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				#if EnableLCD
				PCD8544_GotoXY(0, 0);
				PCD8544_Puts("Tuerbewegung...", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
				#endif
			}
			else
			{
				#if EnableUART_Output
				uint8_t string2[15]="Kabine fährt...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				#if EnableLCD
				PCD8544_GotoXY(0, 0);
				PCD8544_Puts("Kabine faehrt...", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
				#endif
			}

			#if EnableUART_Output
			uint8_t string3[15]="Oeffnung in %: ";
			uint8_t string4[19]="VektorBetrag [uT]: ";
			uint8_t string5[9]="  alpha: ";
			uint8_t string6[8]="  beta: ";
			uint8_t string7[25]="Beschleunigung [dm/s^2]: ";
			uint8_t string8[15]="Status(Debug): ";

			println(string3,sizeof(string3));
			printNumber(door_State_percent);
			println(&temp,sizeof(uint8_t));
			println(string4,sizeof(string4));
			printNumber((uint16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].value+0.5));
			println(string5,sizeof(string5));
			printNumber((uint16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].alpha+0.5));
			println(string6,sizeof(string6));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].beta+0.5));
			println(&temp,sizeof(uint8_t));
			#endif

			#if EnableLCD
			//Print % on lcd
			PCD8544_GotoXY(0, 10);
			PCD8544_Puts("[%]: ", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(50, 10);
			char *buf[3];
			itoa(door_State_percent,(char *)buf,10);
			PCD8544_Puts((char *)buf, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			//Print Accel on lcd
			PCD8544_GotoXY(0, 41);
			PCD8544_Puts("Beschl.: ", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(50, 41);
			itoa(z_accel*10,(char *)buf,10);
			PCD8544_Puts((char *)buf, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			#endif


#if distance_assignment
#if EnableUART_Output
			uint8_t string9[19]="Komponente[uT]: x: ";
			uint8_t string10[5]="  y: ";
			uint8_t string11[5]="  z: ";
			println(string9,sizeof(string9));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].x_val+0.5));
			println(string10,sizeof(string10));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].y_val+0.5));
			println(string11,sizeof(string11));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].z_val+0.5));
			println(&temp,sizeof(uint8_t));
#endif
#if EnableLCD
			//Print x y z on lcd
			PCD8544_GotoXY(0, 25);
			PCD8544_Puts("X", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(28, 25);
			PCD8544_Puts("Y", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(56, 25);
			PCD8544_Puts("Z", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(8, 25);
			itoa((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].x_val+0.5),(char *)buf,10);
			PCD8544_Puts((char *)buf, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(36, 25);
			itoa((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].y_val+0.5),(char *)buf,10);
			PCD8544_Puts((char *)buf, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
			PCD8544_GotoXY(64, 25);
			itoa((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].z_val+0.5),(char *)buf,10);
			PCD8544_Puts((char *)buf, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
#endif

#endif
#if EnableUART_Output
			println(string7,sizeof(string7));
			printNumber(z_accel*10);
			println(&temp,sizeof(char));
			println(string8,sizeof(string8));
			printNumber(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].state);
			println(&temp,sizeof(char));

#if EnableTimeMeasure
			println(&temp,sizeof(char));
			uint8_t stringT1[30]="Zeitmessung der Tuerbewegung:\n";
		    uint8_t stringT2[18]="Closed->Open[ms]: ";
			uint8_t stringT3[19]="\tOpen->Closed[ms]: ";
			uint8_t stringT4[18]="Mean_C->O[ms]:    ";
			uint8_t stringT5[19]="\tMean_O->C[ms]:    ";
			println(stringT1,sizeof(stringT1));
			println(stringT2,sizeof(stringT2));
			printNumber(time_door_CtoO);
			println(stringT3,sizeof(stringT3));
			printNumber(time_door_OtoC);println(&temp,sizeof(char));
			println(stringT4,sizeof(stringT4));
			printNumber(meanTime_door_CtoO);
			println(stringT5,sizeof(stringT5));
			printNumber(meanTime_door_OtoC);
			println(&temp,sizeof(char));
#endif
			println(string1,sizeof(string1));
			#endif
		}
		i = 0;
		//vPortExitCritical();
	}
	else
	{
		i++;
	}

	//ButtonTest
	if(!HAL_GPIO_ReadPin(button_Port,button_S1))	//Anstelle C 13
	{
		#if EnableLED_Output
		if(enableLEDOutput)
		{
			HAL_GPIO_TogglePin(GPIOB,LED_closed);
		}
		#endif
	}
#if EnableLCD
	PCD8544_Refresh();
#endif
}
