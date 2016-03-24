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
#include "i2c_accel.h"
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
static void calcDoorState(drive_cycle_t *drive_cycle);
static void putVectorInRightBuffer(magnVec_t *vektor);
static void getAccelWithoutG(void);
static void bufferChangeRequest(void);
static void measureMovementTime(magnVec_t *vektor);


/*
 * Globale Variablen
 */
device_mode_t device_mode = mode_init;
door_state_t door_state;
static uint8_t door_State_percent;			//T�rstatus von 0-100% | 0% = Geschlossen    100% = Offen
static drive_cycle_t drive_cycle;

static vectorRingBufHandle *lastVectorsBuffer;
static vectorRingBufHandle *openBuffer;
static vectorRingBufHandle *closedBuffer;

floatRingBufHandle *accelBuffer;
floatRingBufHandle *accelOffsetBuffer;
float z_accel;

#if EnableLED_Output
static GPIO_InitTypeDef GPIOB_5;			//LED
#endif
static GPIO_InitTypeDef GPIOC_13;			//Button


extern float z_accel;
extern floatRingBufHandle *accelBuffer;


static uint16_t time_door_OtoC;				//Zeit welche die T�re braucht um zu schliessen
static uint16_t time_door_CtoO;				//Zeit welche die T�re braucht um zu �ffnen
static uint16_t meanTime_door_OtoC;			//Zeit welche die T�re braucht um zu schliessen �ber 1000 Werte gemittelt
static uint16_t meanTime_door_CtoO;			//Zeit welche die T�re braucht um zu �ffnen �ber 1000 Werte gemittelt

/*
 * Initialisierungen
 */
void app_init(void)
{
	//Init all Ringbuffers
	lastVectorsBuffer = initVectorBuffer(1);	//Ringbuffer mit den Neuesten Vektoren welche noch nicht zugeordnet wurden
	openBuffer = initVectorBuffer(2);			//Ringbuffer mit den Vektoren welche einer offenen T�re zugeordnet wurden
	closedBuffer = initVectorBuffer(3);			//Ringbuffer mit den Vektoren welche einer geschlossenen T�re zugeordnet wurden
	accelBuffer = initfloatBuffer(1);			//Ringbuffer mit den letzten Beschleunigungswerten
	accelOffsetBuffer = initfloatBuffer(2);		//Ringbuffer werlcher benutzt wird um den Beschleunigungsoffset zu ermitteln

#if EnableLED_Output
	//Init GPIO for LEDs
	GPIOB_5.Pin =  GPIO_PIN_5;
	GPIOB_5.Mode = GPIO_MODE_OUTPUT_PP;
	GPIOB_5.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB,&GPIOB_5);
#endif

	//Init GPIO for Buttons
	GPIOC_13.Pin = GPIO_PIN_13;
	GPIOC_13.Mode = GPIO_MODE_INPUT;
	GPIOC_13.Pull = GPIO_PULLUP;
	GPIOC_13.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC,&GPIOC_13);


	// Init der Fahrt Zustandsmaschine
	drive_cycle = drive_end_S4;
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
	vectorAnalyseState(&vektor);			//Magnetfelfvektor wird analysiert und einem T�rstatus zugewiesen
	putInBuffer(lastVectorsBuffer,vektor);	//Vektor in Ringbuffer speichern
	calcDoorState(&drive_cycle);			//Die Globale variable door_State_percent wird unterhalten
	measureMovementTime(&vektor);			//Die Zeiten um die T�re zu �ffnen und zu schliessen werden gemessen
	outputResult(&vektor);					//Die Resultate werden �ber LEDs und UART ausgegeben
	bufferChangeRequest();					//Wenn beide Tasten 2s gedr�ckt werden -> Buffer Vertauschen
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
		#if EnableLED_Output
		HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
		#endif
	}

	if(i==5)		//Nur jeder 5te Aufruf die Ausgabe T�tigen
	{
		//vPortEnterCritical();
		if(device_mode == mode_init)
		{
			// TODO LEDs blink!!!
			#if EnableLED_Output
			HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
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
			//wenn t�re weniger als 10% offen ist -> Geschlossen
			if(door_state==door_closed)
			{
				#if EnableUART_Output
				uint8_t string2[18]="Tuere geschlossen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LED
				#if EnableLED_Output
				HAL_GPIO_WritePin(GPIOB,&GPIOB_5,GPIO_PIN_SET);
				#endif
			}
			//wenn t�re mehr als 90% offen ist -> Offen
			else if(door_state==door_open)
			{
				#if EnableUART_Output
				uint8_t string2[12]="Tuere offen!";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LED
				#if EnableLED_Output
				HAL_GPIO_WritePin(GPIOB,&GPIOB_5,GPIO_PIN_RESET);
				#endif
			}
			else if(door_state==door_moving)
			{
				#if EnableUART_Output
				uint8_t string2[20]="Tuere in Bewegung...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LEDs aus
			}
			else
			{
				#if EnableUART_Output
				uint8_t string2[15]="Kabine f�hrt...";
				println(string2,sizeof(string2));
				println(&temp,sizeof(uint8_t));
				#endif
				// TODO LEDs aus
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

#if distance_assignment
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
	if(!HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13))
	{
		#if EnableLED_Output
		HAL_GPIO_TogglePin(GPIOB,&GPIOB_5);
		#endif
	}
}


/*
 * Entscheidet, ob der Vektor sich �nder oder nicht
 * wenn er sich �ndert (um dynThreshold im gesammten buffer) wird die
 * dynamic Variable des Vektors auf 1 gesetzt.
 * Nachher wird dem Vektor eine Geschlossener oder ein offener Zustand  der T�re zugewiesen
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

			//Bewegte sich der Letzte Vektor? Dann �ndere den Abzuf�llenden Buffer (Open<-->Closed)
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

			//Init Mode verlassen, wenn die Buffers gef�llt wurden und der Aufzug F�hrt
			if((fabs(mean_z_accel)>=accel_aufzug_accel_threshold) &&										//Aufzug F�hrt
			   (closedBuffer->counter >= closedBuffer->size && openBuffer->counter >= openBuffer->size))    //Buffer gef�llt
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

/*
 * Diese Funktion misst die Beschleunigung in Z Richtung und subtrahiert
 * den Offset durch die Erdanziehungskraft
 * Der Offset wird immer einmal neu berechnet, wenn die T�re vollst�ndig ge�ffnet ist.
 */
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
 * Vertauscht die Ringbuffer OpenBuffer und ClosedBuffer, wenn Beide Tasten 2s gedr�ckt wurden
 */
static void bufferChangeRequest(void)
{
	//TODO Beide taster verwenden nicht nur 1
	static uint8_t i = 0;
	if(i ==40)
	{
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
	//Ist der Aktuelle Vektor n�her bei den "offenen" Vektoren oder bei den "Geschlossenen"
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

/*
 * Diese Funktion entscheidet ob der Vektor sich bewegt oder Statisch ist
 * Dies wird anhand der dynThresholds ausgewertet
 */
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

		//Ausnahmen bei den �berg�ngen der Winkel;
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
 * Berechnet den T�rzustand in Prozent anhand des aktuellen Vektors
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
 * Der T�rstatus wird ermittelt. Weiter ist der
 * Zustandsautomat der Aufzugskabinen Fahrten hier
 * Implementiert
 */
static void calcDoorState(drive_cycle_t *driveCycle)
{
	float mean_z_accel;
	static uint8_t doorStateLocked = 0;
	static uint16_t timer = 0;		//Timeout f�r Kabinen Fahrt
	static TickType_t lastTicks=0;


	timer = timer + (xTaskGetTickCount()-lastTicks);
	lastTicks = xTaskGetTickCount();
	getMeanFloatValue(accelBuffer,&mean_z_accel);

	if(device_mode == mode_run)
	{

		switch (*driveCycle)
		{
		case drive_accel_S1:
			doorStateLocked = 1;
			door_state = cabin_drives;
			if(fabs(mean_z_accel)<=accel_aufzug_no_accel_threshold)
			{
				*driveCycle=drive_v_max_S2;
			}
			else if(timer>timeoutLockDoorState)
			{
				*driveCycle=drive_end_S4;
			}
		break;
		case drive_v_max_S2:
			doorStateLocked = 1;
			door_state = cabin_drives;
			if(fabs(mean_z_accel)>=accel_aufzug_accel_threshold)
			{
				*driveCycle=drive_deccel_S3;
			}
			else if(timer>timeoutLockDoorState)
			{
				*driveCycle=drive_end_S4;
			}
		break;
		case drive_deccel_S3:
			doorStateLocked = 1;
			door_state = cabin_drives;
			if(fabs(mean_z_accel)<=accel_aufzug_no_accel_threshold)
			{
				*driveCycle=drive_end_S4;
			}
			else if(timer>timeoutLockDoorState)
			{
				*driveCycle=drive_end_S4;
			}
		break;
		case drive_end_S4:
			doorStateLocked = 0;
			timer = 0;
			if(fabs(mean_z_accel)>=accel_aufzug_accel_threshold)
			{
				*driveCycle=drive_accel_S1;
			}
		break;
		}

		//T�rstatus aktualisieren, wenn der Status nicht eingefrohren ist.
		if(!doorStateLocked)
		{
			if(door_State_percent<=5)
			{
				door_state = door_closed;
			}
			else if(door_State_percent>=95)
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


static void measureMovementTime(magnVec_t *vektor)
{
#if EnableTimeMeasure
	static uint16_t counter_OtoC = 0;		//Anzahl Messungen von Offen --> Geschlossen
	static uint16_t counter_CtoO = 0;		//Anzahl Messungen von Geschlossen --> Offen
	static time_measure_t timeMeasure;
	static uint16_t timer = 0;				//Zeitmessung der T�re
	static TickType_t lastTicks=0;
	static uint8_t i;						//Z�hlvariable

	if(device_mode == mode_run)
	{

		switch (timeMeasure)
		{
		case time_ready_S1:
			timer = 0;
			if(door_state == door_moving)
			{
				timeMeasure = time_moving_S2;
				lastTicks = xTaskGetTickCount();
				i = 0;
			}
		break;
		case time_moving_S2:
			if(door_state == door_moving)
			{
				i++;
			}
			if(i==4)
			{
				timeMeasure = time_stillMoving_S3;
			}
			if(door_state != door_moving)
			{
				timeMeasure = time_ready_S1;
			}
		break;
		case time_stillMoving_S3:
			if(door_state == door_open)
			{
				timeMeasure = time_open_S4;
				timer =(xTaskGetTickCount()-lastTicks);
				i = 0;
			}
			if(door_state == door_closed)
			{
				timeMeasure = time_closed_S5;
				timer =(xTaskGetTickCount()-lastTicks);
				i = 0;
			}
		break;
		case time_open_S4:
			if(door_state == door_open)
			{
				i++;
			}
			if(i==4)
			{
				time_door_CtoO = timer;
				meanTime_door_CtoO = ((meanTime_door_CtoO*counter_CtoO)+time_door_CtoO)/(counter_CtoO+1);
				counter_CtoO ++;
				if(counter_CtoO>1000){counter_CtoO=1000;};
				timeMeasure = time_ready_S1;
			}
			if(door_state != door_open)
			{
				timeMeasure = time_ready_S1;
			}
		break;
		case time_closed_S5:
			if(door_state == door_closed)
			{
				i++;
			}
			if(i==4)
			{
				time_door_OtoC = timer;
				meanTime_door_OtoC = ((meanTime_door_OtoC*counter_OtoC)+time_door_OtoC)/(counter_OtoC+1);
				counter_OtoC ++;
				if(counter_OtoC>1000){counter_OtoC=1000;};
				timeMeasure = time_ready_S1;
			}
			if(door_state != door_closed)
			{
				timeMeasure = time_ready_S1;
			}
		break;
		}
	}
#endif
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
