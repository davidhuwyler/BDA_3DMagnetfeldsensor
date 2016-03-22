/*
 * app.c
 *
 *  Created on: 23.02.2016
 *      Author: xxdav
 */
#include <stdlib.h>
#include <math.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "app.h"
#include "i2c_magn.h"
#include "uart.h"
#include "ringBuffer.h"
#include "controlTask.h"

/*
 * Static Prototypes:
 */
static void outputResult(magnVec_t *vektor);
static void vectorAnalyseState(magnVec_t *vektor);
static void setDynamicFlag(magnVec_t *vektor);
static uint8_t getDoorstateInPercent(magnVec_t *vektor);
static void calcDoorState(void);
static void putVectorInRightBuffer(magnVec_t *vektor);
static void getAccelWithoutG(void);
static void bufferChangeRequest(void);

device_mode_t device_mode = mode_init;
door_state_t door_state;
static uint8_t door_State_percent;			//Türstatus von 0-100% | 0% = Geschlossen    100% = Offen


static vectorRingBufHandle *lastVectorsBuffer;
static vectorRingBufHandle *openBuffer;
static vectorRingBufHandle *closedBuffer;

floatRingBufHandle *accelBuffer;
floatRingBufHandle *accelOffsetBuffer;
float z_accel;



static GPIO_InitTypeDef GPIOB_5;			//LED
static GPIO_InitTypeDef GPIOC_13;			//Button

extern float z_accel;
extern floatRingBufHandle *accelBuffer;

void app_init(void)
{
	//Init all Ringbuffers
	lastVectorsBuffer = initVectorBuffer(1);	//Ringbuffer mit den Neuesten Vektoren welche noch nicht zugeordnet wurden
	openBuffer = initVectorBuffer(2);			//Ringbuffer mit den Vektoren welche einer offenen Türe zugeordnet wurden
	closedBuffer = initVectorBuffer(3);			//Ringbuffer mit den Vektoren welche einer geschlossenen Türe zugeordnet wurden
	accelBuffer = initfloatBuffer(1);			//Ringbuffer mit den letzten Beschleunigungswerten
	accelOffsetBuffer = initfloatBuffer(2);		//Ringbuffer werlcher benutzt wird um den Beschleunigungsoffset zu ermitteln


	//Init GPIO for LEDs
	GPIOB_5.Pin =  GPIO_PIN_5;
	GPIOB_5.Mode = GPIO_MODE_OUTPUT_PP;
	GPIOB_5.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB,&GPIOB_5);


	//Init GPIO for Buttons
	GPIOC_13.Pin = GPIO_PIN_13;
	GPIOC_13.Mode = GPIO_MODE_INPUT;
	GPIOC_13.Pull = GPIO_PULLUP;
	GPIOC_13.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC,&GPIOC_13);
}


/*
 * Dies ist die Funtion des Haupttasks des Betriebssystems
 * Diese Funktion wird mit 20Hz aufgerufen
 */
void app_run(void)
{
	magnVec_t vektor;
	vPortEnterCritical();
	getAccelWithoutG();						//Misst die Beschleunigung in Z Richtung ohne G
	get_vektor(&vektor);					//Misst das Magnetfeld mit dem Magnetometer
	vPortExitCritical();
	vectorAnalyseState(&vektor);			//Magnetfelfvektor wird analysiert und einem Türstatus zugewiesen
	putInBuffer(lastVectorsBuffer,vektor);	//Vektor in Ringbuffer speichern
	calcDoorState();						//Die Globale variable door_State_percent wird unterhalten
	outputResult(&vektor);					//Die Resultate werden über LEDs und UART ausgegeben
	bufferChangeRequest();					//Wenn beide Tasten 2s gedrückt werden -> Buffer Vertauschen
}


/*
 * Druckt die Resultate auf die UART-Schnittstelle
 * TODO und bringt die Entsprechenden LEDs zum Leuchten
 */

static void outputResult(magnVec_t *vektor)
{
	static uint8_t i=0;

	if(vektor->state == vek_dynamic || device_mode == mode_waitForDrive)
	{
		HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
	}

	if(i==5)		//Nur jeder 5te Aufruf die Ausgabe Tätigen
	{
		vPortEnterCritical();
		if(device_mode == mode_init)
		{
			// TODO LEDs blink!!!
			HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
		}

		else if(device_mode == mode_run)
		{
			char string1[45]="---------------------------------------------";
			char temp = '\n';
			println(&temp,sizeof(char));println(&temp,sizeof(char));println(&temp,sizeof(char));
			println(string1,sizeof(string1));
			println(&temp,sizeof(char));

			//wenn türe weniger als 10% offen ist -> Geschlossen
			if(door_state==door_closed)
			{
				char string2[18]="Tuere geschlossen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(char));
				// TODO LED
				HAL_GPIO_WritePin(GPIOB,&GPIOB_5,GPIO_PIN_SET);
			}
			//wenn türe mehr als 90% offen ist -> Offen
			else if(door_state==door_open)
			{
				char string2[12]="Tuere offen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(char));
				// TODO LED
				HAL_GPIO_WritePin(GPIOB,&GPIOB_5,GPIO_PIN_RESET);
			}
			else if(door_state==door_moving)
			{
				char string2[20]="Tuere in Bewegung...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(char));
				// TODO LEDs aus
			}
			else
			{
				char string2[15]="Kabine fährt...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(char));
				// TODO LEDs aus
			}
			char string3[15]="Oeffnung in %: ";
			char string4[19]="VektorBetrag [uT]: ";
			char string5[9]="  alpha: ";
			char string6[8]="  beta: ";
			char string7[25]="Beschleunigung [dm/s^2]: ";
			char string8[15]="Status(Debug): ";



			println(string3,sizeof(string3));
			printNumber(door_State_percent);
			println(&temp,sizeof(char));
			println(string4,sizeof(string4));
			printNumber((uint16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].value+0.5));
			println(string5,sizeof(string5));
			printNumber((uint16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].alpha+0.5));
			println(string6,sizeof(string6));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].beta+0.5));
			println(&temp,sizeof(char));

#if distance_assignment
			char string9[19]="Komponente[uT]: x: ";
			char string10[5]="  y: ";
			char string11[5]="  z: ";
			println(string9,sizeof(string9));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].x_val+0.5));
			println(string10,sizeof(string10));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].y_val+0.5));
			println(string11,sizeof(string11));
			printNumber((int16_t)(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].z_val+0.5));
			println(&temp,sizeof(char));
#endif
			println(string7,sizeof(string7));
			printNumber(z_accel*10);
			println(&temp,sizeof(char));
			println(string8,sizeof(string8));
			printNumber(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].state);
			println(&temp,sizeof(char));
			println(string1,sizeof(string1));
		}
		i = 0;
		vPortExitCritical();
	}
	else
	{
		i++;
	}

	//ButtonTest
	if(!HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13))
	{
		HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
	}
}


/*
 * Entscheidet, ob der Vektor sich änder oder nicht
 * wenn er sich ändert (um dynThreshold im gesammten buffer) wird die
 * dynamic Variable des Vektors auf 1 gesetzt.
 * Nachher wird dem Vektor eine Geschlossener oder ein offener Zustand  der Türe zugewiesen
 */
static void vectorAnalyseState(magnVec_t *vektor)
{
	static uint8_t actualVektorIsOpen = 0;
	setDynamicFlag(vektor);

	//Der Vektro ist Statisch:
	if (vektor->state == vek_idle)
	{
		if (device_mode == mode_init)
		{
			float mean_z_accel;
			getMeanFloatValue(accelBuffer,&mean_z_accel);

			//Bewegte sich der Letzte Vektor? Dann ändere den Abzufüllenden Buffer (Open<-->Closed)
			if(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].state == vek_dynamic)
			{
				if(actualVektorIsOpen==0){actualVektorIsOpen=1;}
				else {actualVektorIsOpen=0;}
			}

			if(actualVektorIsOpen)
			{
				vektor->state = vek_door_open;
				putInBuffer(openBuffer, *vektor);
			}
			else
			{
				vektor->state = vek_door_closed;
				putInBuffer(closedBuffer, *vektor);
			}

			//Init Mode verlassen, wenn die Buffers gefüllt wurden und der Aufzug Fährt
			if((fabs(mean_z_accel)>=accel_aufzug_runns_threshold) &&										//Aufzug Fährt
			   (closedBuffer->counter >= closedBuffer->size && openBuffer->counter >= openBuffer->size))    //Buffer gefüllt
			{
				if(actualVektorIsOpen) //open und closed Buffer vertauschen
				{
					magnVec_t *temp;
					temp = (openBuffer->vectorArray);
					openBuffer->vectorArray = closedBuffer->vectorArray;
					closedBuffer->vectorArray = temp;
				}
				eraseFloatBuffer(accelBuffer);
				door_state = door_closed;
				device_mode = mode_waitForDrive;
			}
		}
		else if (device_mode == mode_waitForDrive) //Zwischen Modus...
		{
			static uint8_t counter = 0;
			if(counter<(2*bufSize2))
			{
				counter++;
			}
			else if(counter==(2*bufSize2))
			{
				device_mode = mode_run;
			}


		}
		else if (device_mode == mode_run)
		{
			putVectorInRightBuffer(vektor);
		}
	}
	//Der Vektor ist in Bewegung:
	else
	{
		door_State_percent=getDoorstateInPercent(vektor);
	}

}

static void getAccelWithoutG(void)
{
	static float offset = 0.0;
	static int i = 0;
	static door_state_t oldDoor_state;
	static uint8_t newOffset = 0;
	float z_accel_temp;


	if((oldDoor_state!=door_open) && (door_state==door_open) && (device_mode==mode_run))
	{
		newOffset = 1;
	}


	if(newOffset || offset == 0.0)
	{
		if(i<bufSize4)
		{
			get_accel(&z_accel_temp);
			putInFloatBuffer(accelOffsetBuffer,z_accel_temp);
			i++;
		}
		else if(i==bufSize4)
		{
			float temp;
			getMeanFloatValue(accelOffsetBuffer,&temp);
			offset = temp;
			i = 0;
			newOffset = 0;
		}
	}
	else
	{
		vPortEnterCritical();
		get_accel(&z_accel_temp);
		vPortExitCritical();
		z_accel = z_accel_temp-offset;
		putInFloatBuffer(accelBuffer,z_accel);
	}

	oldDoor_state = door_state;
}

/*
 * Vertauscht die Ringbuffer OpenBuffer und ClosedBuffer, wenn Beide Tasten 2s gedrückt wurden
 */
static void bufferChangeRequest(void)
{
	//TODO Beide taster verwenden nicht nur 1
	static uint8_t i = 0;
	if(i ==40)
	{
//		eraseVectorBuffer(closedBuffer);
//		eraseVectorBuffer(openBuffer);
		// Buffer Vertauschen
		magnVec_t *temp;
		temp = (openBuffer->vectorArray);
		openBuffer->vectorArray = closedBuffer->vectorArray;
		closedBuffer->vectorArray = temp;

		if(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].state == vek_door_open)
		{

		}
	}
	if(!(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)))
	{
		i++;
	}
	else
	{
		i = 0;
	}

}


static void putVectorInRightBuffer(magnVec_t *vektor)
{
	//Ist der Aktuelle Vektor näher bei den "offenen" Vektoren oder bei den "Geschlossenen"
#if value_angle_assignment
	if ((fabs(getMeanValue(openBuffer) - vektor->value) +
		 fabs(getMeanAlpha(openBuffer) - vektor->alpha) +
		 fabs(getMeanBeta(openBuffer) - vektor->beta))
		 >
		(fabs(getMeanValue(closedBuffer) - vektor->value) +
		 fabs(getMeanAlpha(closedBuffer) - vektor->alpha) +
		 fabs(getMeanBeta(closedBuffer) - vektor->beta)))
#endif
#if distance_assignment
	if ((fabs(getMeanX(openBuffer) - vektor->x_val) +
		 fabs(getMeanY(openBuffer) - vektor->y_val) +
		 fabs(getMeanZ(openBuffer) - vektor->z_val))
		 >
		(fabs(getMeanX(closedBuffer) - vektor->x_val) +
		 fabs(getMeanY(closedBuffer) - vektor->y_val) +
		 fabs(getMeanZ(closedBuffer) - vektor->z_val)))
#endif
	{
		vektor->state = vek_door_closed;
		putInBuffer(closedBuffer, *vektor);
		door_State_percent = 0;
	} else
	{
		vektor->state = vek_door_open;
		putInBuffer(openBuffer, *vektor);
		door_State_percent = 100;
	}
}


static void setDynamicFlag(magnVec_t *vektor)
{
	int i;
	vektor->state = vek_idle;

#if value_angle_assignment
	//Umgehung eines Rundungsfehlers, welcher falsche Werte ergibt...
	if(vektor->alpha == 0)
	{
		vektor->alpha = lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].alpha;
	}
#endif


	//Analyse ob sich der Vektor bewegt
	for (i = 0; i < lastVectorsBuffer->size; i++)
	{
#if value_angle_assignment
		float diffValue, diffAlpha, diffBeta;
		diffValue =fabs(lastVectorsBuffer->vectorArray[i].value - vektor->value);
		diffAlpha =fabs(lastVectorsBuffer->vectorArray[i].alpha - vektor->alpha);
		diffBeta  =fabs(lastVectorsBuffer->vectorArray[i].beta - vektor->beta);

		//Ausnahmen bei den Übergängen der Winkel;
		if ((int16_t)(lastVectorsBuffer->vectorArray[i].alpha+0.5) == 0 ||
			(int16_t)(lastVectorsBuffer->vectorArray[i].alpha+0.5) == 90 ||
			(int16_t)(lastVectorsBuffer->vectorArray[i].alpha+0.5) == 180 ||
			(int16_t)(lastVectorsBuffer->vectorArray[i].alpha+0.5) == 270 ||
			(int16_t)(lastVectorsBuffer->vectorArray[i].alpha+0.5) == 360 ||
			(int16_t)(vektor->alpha+0.5)== 0 ||
			(int16_t)(vektor->alpha+0.5)== 90 ||
			(int16_t)(vektor->alpha+0.5)== 180 ||
			(int16_t)(vektor->alpha+0.5)== 270 ||
			(int16_t)(vektor->alpha+0.5)== 360)
		{
			diffAlpha = 0;
		}
		if ((uint16_t)(lastVectorsBuffer->vectorArray[i].beta+0.5) == -90 ||
			(uint16_t)(lastVectorsBuffer->vectorArray[i].beta+0.5) == 90 ||
			(uint16_t)(vektor->beta+0.5)== -90 ||
			(uint16_t)(vektor->beta+0.5)== 90)
		{
			diffBeta = 0;
		}

		if (diffValue>dynThreshold_value)
		{
			vektor->state = vek_dynamic;
		} else if (diffAlpha>dynThreshold_angle)
		{
			vektor->state = vek_dynamic;
		} else if (diffBeta>dynThreshold_angle)
		{
			vektor->state = vek_dynamic;
		}
#endif
#if distance_assignment
		float diffDistanceX,diffDistanceY,diffDistanceZ;
		diffDistanceX =fabs(lastVectorsBuffer->vectorArray[i].x_val - vektor->x_val);
		diffDistanceY =fabs(lastVectorsBuffer->vectorArray[i].y_val - vektor->y_val);
		diffDistanceZ =fabs(lastVectorsBuffer->vectorArray[i].z_val - vektor->z_val);

		if (diffDistanceX>dynThreshold_distance || diffDistanceY>dynThreshold_distance || diffDistanceZ>dynThreshold_distance)
		{
			vektor->state = vek_dynamic;
		}
#endif
	}
}


/*
 * Berechnet den Türzustand in Prozent anhand des aktuellen Vektors
 */
static uint8_t getDoorstateInPercent(magnVec_t *vektor)
{
	float diff,meanOpen, meanClosed;
	static uint8_t prozent;

	if(door_state!=cabin_drives)
	{
		meanOpen	= getMeanValue(openBuffer);
		meanClosed	= getMeanValue(closedBuffer);

		diff=fabs(meanOpen-meanClosed);
		if(meanOpen>meanClosed)
		{
			prozent = (((100*(vektor->value-meanClosed))/diff)+0.5);
		}
		else
		{
			prozent = 100-(((100*(vektor->value-meanOpen))/diff)+0.5);
		}

		if (prozent>100){prozent=100;}
	}
	return prozent;
}

/*
 *
 */
static void calcDoorState(void)
{
	float mean_z_accel;
	static uint8_t doorStateLocked = 0;
	static uint16_t timer1 = 0;
	static uint16_t timer2 = 0;
	static TickType_t lastTicks=0;

	if(device_mode == mode_run)
	{

		// Timer1 unterhalten
		if(doorStateLocked)
		{
			timer1 = timer1 + (xTaskGetTickCount()-lastTicks);
			timer2 = timer2 + (xTaskGetTickCount()-lastTicks);
			lastTicks = xTaskGetTickCount();
		}


		getMeanFloatValue(accelBuffer,&mean_z_accel);

		//Türstatus einfrieren, wenn  Aufzug anfährt
		if(!doorStateLocked && (fabs(mean_z_accel)>=accel_aufzug_runns_threshold))
		{
			doorStateLocked = 1;
			timer1 = 0;
			timer2 = 0;
			door_state = cabin_drives;
		}

		//Türstatus freigeben, wenn...
		if((timer2>minimalDoorStateLockTime) && 												//die minimale Lockzeit abgelaufen ist und...
			doorStateLocked	&&   																//der Türstatus im Moment eingefrohren ist und...
		   ((fabs(mean_z_accel)>=accel_aufzug_runns_threshold) || timer1>timeoutLockDoorState))	// der Aufzug bremst oder das Lock Timeout erreicht ist
		{
			doorStateLocked = 0;
			timer1 = 0;
			timer2 = 0;
		}

		//Türstatus aktualisieren, wenn der Status nicht eingefrohren ist.
		if(!doorStateLocked)
		{
			if(door_State_percent<=10)
			{
				door_state = door_closed;
			}
			else if(door_State_percent>=90)
			{
				door_state = door_open;
			}
			else
			{
				door_state = door_moving;
			}
		}
	}
}

/*
 * Diese Funktion wird vom FreeRTOS Betriebssystem im Idle Task aufgerufen und versetzt
 * den Prozessor in den Sleep-Mode
 * Aufgeweckt wird der Prozessor mit jedem Interrupt
 */
void vApplicationIdleHook()
{
	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}
