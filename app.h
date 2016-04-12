/*
 * app.h
 *
 *  Created on: 23.02.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_APP_H_
#define APPLICATION_USER_APP_H_

#include "i2c_magn.h"

/*
 * Konfiguratonen:
 */
#define value_angle_assignment			0		//Die Zuordnung der Vektoren geschieht über den Betrag und die Winkel
#define distance_assignment				1		//Die Zuordnung der Vektoren geschieht über den Abstand

#define Enable_LIS3MDL 					0		//Welcher Magnetometer wird verwendet?
#define Enable_LSM303DLHC 				1


#define EnableControlTask				1
#define FlashLogging 					1		//Soll der ControlTask die Fehler im Flash Loggen?
#define EnableTimeMeasure				1		//Aktivierung der Zeitmessung der Türbewegungnen
#define EnableUART_Output				0
#define EnableLED_Output				1
#define EnableLCD						0

/*
 * Konstanten:
 */
#if value_angle_assignment
#define dynThreshold_value 				2		//Ab wieviel uT dynamik soll ein Vektor als Stillstehend gelten
#define dynThreshold_angle 				10		//Ab wieviel ° dynamik soll ein Vektor als Stillstehend gelten
#endif

#if distance_assignment
#define dynThreshold_distance 			2		//Ab wieviel uT dynamik soll ein Vektor als Stillstehend gelten
#endif

#define accel_aufzug_accel_threshold 	0.3     //Welche gemittelte beschleunigung muss überschritten werden
												//der Aufzug als fahrend eingeschätzt wird (mean von 0.5s | 5 Werten)
#define accel_aufzug_no_accel_threshold 0.15	//Welche gemittelte beschleunigung muss unterschritten werden
												//der Aufzug als fahrend eingeschätzt wird (mean von 0.5s | 5 Werten)
#define accel_aufzug_bigAccel_threshold 1.2     //Welche gemittelte beschleunigung muss überschritten werden
												//damit der Beschleunigungsfehler errkantt wird (mean von 0.5s | 5 Werten)

#define timeoutLockDoorState			10000	//5s Timeout wenn der Türstatus nicht entlockt wird
#define timeout_after_accel 			10000 	//10s Zeit bis sich die Aufzugstüre öffnen musss nach einer Beschleunigung
#define timeout_after_error 			2000 	// 2s Zeit die zwischen zwei Fehlern minimum Vergehen muss



/*
 * GPIO
 */
//LEDs
#if defined STM32F401xE
#define LED_closed GPIO_PIN_5
#define LED_changing GPIO_PIN_5
#define LED_open GPIO_PIN_5
#endif
#if defined STM32L476xx || STM32F4051xE
#define LED_closed GPIO_PIN_14
#define LED_changing GPIO_PIN_13
#define LED_open GPIO_PIN_12
#endif

//Buttons
#if defined STM32F401xE
#define button_Port GPIOC
#define button_S1 GPIO_PIN_13
#define button_S2 GPIO_PIN_1
#endif
#if defined STM32L476xx || STM32F4051xE
#define button_Port GPIOB
#define button_S1 GPIO_PIN_0
#define button_S2 GPIO_PIN_1
#endif



/*
 * Enumerations
 */
typedef enum
{
	mode_init,					//Im init-Mode werden die Ringbuffer openBuffer und closedBuffer gefüllt
	mode_waitForDrive,			//Es wird gewartet bis die Aufzugskabine fährt, um die Buffer endgültig zuzuweisen
	mode_run					//Normaler Betrieb
}device_mode_t;

typedef enum
{
	door_moving,				//Türe in Bewegung
	door_open,					//Türe offen
	door_closed,				//Türe geschlossen
	cabin_drives				//Aufzug fährt
}door_state_t;

typedef enum					//Türstatus Zustandsautomat
{
	drive_accel_S1,				//Zustand S1: beschleunigen
	drive_v_max_S2,				//Zustand S2: Geschwindigkeit erreicht
	drive_deccel_S3,			//Zustand S3: bremsen
	drive_end_S4				//Zustand S4: Fahrt zuende
}drive_cycle_t;

typedef enum					//Zeitmessungs Zustandsautomat
{
	time_ready_S1,				//Zustand S1: Bereit für die Neue Messung
	time_moving_S2,				//Zustand S2: Türe in Bewegung
	time_stillMoving_S3,		//Zustand S3: Türe noch immer in Bewegung
	time_open_S4,				//Zustand S4: Türe offen
	time_closed_S5				//Zustand S5: Türe geschlossen
}time_measure_t;

void app_init(void);

void app_run(void);

void vApplicationIdleHook();

#endif /* APPLICATION_USER_APP_H_ */
