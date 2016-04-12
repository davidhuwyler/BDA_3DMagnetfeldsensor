/*
 * app.c
 *
 *  Created on: 23.02.2016
 *      Author: xxdav
 */
#include <stdlib.h>
#include <math.h>
#if defined STM32F401xE || STM32F4051xE
#include "stm32f4xx_hal.h"
#endif
#if defined STM32L476xx
#include "stm32l4xx_hal.h"
#endif
#include "cmsis_os.h"
#include "app.h"
#include "i2c_magn.h"
#include "i2c_accel.h"
#include "uart.h"
#include "ringBuffer.h"
#include "controlTask.h"
#include "output.h"
#include "tm_stm32f4_pcd8544.h"

/*
 * Static Prototypes:
 */
static void vectorAnalyseState(magnVec_t *vektor);
static void setDynamicFlag(magnVec_t *vektor);
static uint8_t getDoorstateInPercent(magnVec_t *vektor);
static void calcDoorState(drive_cycle_t *drive_cycle);
static void putVectorInRightBuffer(magnVec_t *vektor);
static void getAccelWithoutG(void);
static void measureMovementTime(magnVec_t *vektor);


/*
 * Globale Variablen
 */
device_mode_t device_mode = mode_init;
door_state_t door_state;
uint8_t door_State_percent;			//Türstatus von 0-100% | 0% = Geschlossen    100% = Offen
static drive_cycle_t drive_cycle;

vectorRingBufHandle *lastVectorsBuffer;
vectorRingBufHandle *openBuffer;
vectorRingBufHandle *closedBuffer;

floatRingBufHandle *accelBuffer;
floatRingBufHandle *accelOffsetBuffer;
float z_accel;


extern floatRingBufHandle *accelBuffer;


static uint16_t time_door_OtoC;				//Zeit welche die Türe braucht um zu schliessen
static uint16_t time_door_CtoO;				//Zeit welche die Türe braucht um zu öffnen
static uint16_t meanTime_door_OtoC;			//Zeit welche die Türe braucht um zu schliessen über 1000 Werte gemittelt
static uint16_t meanTime_door_CtoO;			//Zeit welche die Türe braucht um zu öffnen über 1000 Werte gemittelt

/*
 * Initialisierungen
 */
void app_init(void)
{

	//Init all Ringbuffers
	lastVectorsBuffer = initVectorBuffer(1);	//Ringbuffer mit den Neuesten Vektoren welche noch nicht zugeordnet wurden
	openBuffer = initVectorBuffer(2);			//Ringbuffer mit den Vektoren welche einer offenen Türe zugeordnet wurden
	closedBuffer = initVectorBuffer(3);			//Ringbuffer mit den Vektoren welche einer geschlossenen Türe zugeordnet wurden
	accelBuffer = initfloatBuffer(1);			//Ringbuffer mit den letzten Beschleunigungswerten
	accelOffsetBuffer = initfloatBuffer(2);		//Ringbuffer werlcher benutzt wird um den Beschleunigungsoffset zu ermitteln


	GPIO_InitTypeDef GPIOinitType;
#if EnableLED_Output
	//LED Init
	GPIOinitType.Mode = GPIO_MODE_OUTPUT_PP;
	GPIOinitType.Pull = GPIO_PULLDOWN;
	GPIOinitType.Pin =  LED_closed;
	HAL_GPIO_Init(GPIOB,&GPIOinitType);
	GPIOinitType.Pin =  LED_changing;
	HAL_GPIO_Init(GPIOB,&GPIOinitType);
	GPIOinitType.Pin =  LED_open;
	HAL_GPIO_Init(GPIOB,&GPIOinitType);
#endif

	//Init GPIO for Buttons
	GPIOinitType.Mode = GPIO_MODE_INPUT;
	GPIOinitType.Pull = GPIO_PULLUP;
	GPIOinitType.Speed = GPIO_SPEED_FREQ_LOW;
	GPIOinitType.Pin = button_S1;
	HAL_GPIO_Init(button_Port,&GPIOinitType);
	GPIOinitType.Pin = button_S2;
	HAL_GPIO_Init(button_Port,&GPIOinitType);



	// Init der Fahrt Zustandsmaschine
	drive_cycle = drive_end_S4;



	//LCD
#if EnableLCD
	PCD8544_Init(0x38);
#endif
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
	calcDoorState(&drive_cycle);			//Die Globale variable door_State_percent wird unterhalten
	measureMovementTime(&vektor);			//Die Zeiten um die Türe zu öffnen und zu schliessen werden gemessen
	outputResult(&vektor);					//Die Resultate werden über LEDs und UART ausgegeben
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
			if((fabs(mean_z_accel)>=accel_aufzug_accel_threshold) &&										//Aufzug Fährt
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

/*
 * Diese Funktion misst die Beschleunigung in Z Richtung und subtrahiert
 * den Offset durch die Erdanziehungskraft
 * Der Offset wird immer einmal neu berechnet, wenn die Türe vollständig geöffnet ist.
 */
static void getAccelWithoutG(void)
{
	static float offset = 0.0;
	static int i = 0;
	static door_state_t oldDoor_state;
	static device_mode_t oldDevice_mode;
	static uint8_t newOffset = 0;
	float z_accel_temp;

	static int j = 0;
	if(j<41)
	{
		j++;
	}


	if(((oldDoor_state!=door_open) && (door_state==door_open) && (device_mode==mode_run)) ||			//Neuer offset wenn Türe öffnet,
		//(!HAL_GPIO_ReadPin(GPIOB,button_S1) && !HAL_GPIO_ReadPin(GPIOB,button_S2))		  ||			//wenn beide Taster gedrückt wreden
		((oldDevice_mode==mode_waitForDrive) && (device_mode==mode_run))				  ||			//wenn in den Run Modus gewechselt wird
		(j==40))
	{
		newOffset = 1;
	}


	if(newOffset)
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

	oldDevice_mode = device_mode;
	oldDoor_state = door_state;
}

///*
// * Vertauscht die Ringbuffer OpenBuffer und ClosedBuffer, wenn Beide Tasten 2s gedrückt wurden
// */
//static void bufferChangeRequest(void)
//{
//	//TODO Beide taster verwenden nicht nur 1
//	static uint8_t i = 0;
//	if(i ==40)
//	{
//		magnVec_t *temp;
//		temp = (openBuffer->vectorArray);
//		openBuffer->vectorArray = closedBuffer->vectorArray;
//		closedBuffer->vectorArray = temp;
//
//		if(lastVectorsBuffer->vectorArray[lastVectorsBuffer->index].state == vek_door_open)
//		{
//
//		}
//	}
//	if(!(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_0))) //Anstelle C 13
//	{
//		i++;
//	}
//	else
//	{
//		i = 0;
//	}
//
//}
//

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
 * Der Türstatus wird ermittelt. Weiter ist der
 * Zustandsautomat der Aufzugskabinen Fahrten hier
 * Implementiert
 */
static void calcDoorState(drive_cycle_t *driveCycle)
{
	float mean_z_accel;
	static uint8_t doorStateLocked = 0;
	static uint16_t timer = 0;		//Timeout für Kabinen Fahrt
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
			door_State_percent = 0;
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
			door_State_percent = 0;
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
			door_State_percent = 0;
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

		//Türstatus aktualisieren, wenn der Status nicht eingefrohren ist.
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
	static uint16_t timer = 0;				//Zeitmessung der Türe
	static TickType_t lastTicks=0;
	static uint8_t i;						//Zählvariable

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

#if defined STM32F401xE || STM32F4051xE
	HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
#endif
#if defined STM32L476xx
	HAL_PWR_EnterSLEEPMode(PWR_LOWPOWERREGULATOR_ON, PWR_SLEEPENTRY_WFI);
#endif
}
