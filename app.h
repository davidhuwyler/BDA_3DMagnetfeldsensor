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
#define value_angle_assignment			0		//Die Zuordnung der Vektoren geschiet über den Betrag und die Winkel
#define distance_assignment				1		//Die Zuordnung der Vektoren geschiet über den Abstand

#if value_angle_assignment
#define dynThreshold_value 				2		//Ab wieviel uT dynamik soll ein Vektor als Stillstehend gelten
#define dynThreshold_angle 				10		//Ab wieviel ° dynamik soll ein Vektor als Stillstehend gelten
#endif

#if distance_assignment
#define dynThreshold_distance 			3		//Ab wieviel uT dynamik soll ein Vektor als Stillstehend gelten
#endif

#define FlashLogging 					1		//Soll der ControlTask die Fehler im Flash Loggen?

#define Enable_LIS3MDL 					0		//Welcher Magnetometer wird verwendet?
#define Enable_LSM303DLHC 				1

#define accel_aufzug_runns_threshold 	0.4     //Welche gemittelte beschleunigung ist nötig, dass
												//der Aufzug als fahren eingeschätzt wird (mean von 0.5s | 5 Werten)

#define timeoutLockDoorState			5000	//5s Timeout wenn der Türstatus nicht entlockt wird
#define minimalDoorStateLockTime		1500	//1.5s Minimale Zeit bis die Türe entlockt werden kann
#define timeout_after_accel 			10000 	//10s Zeit bis sich die Aufzugstüre öffnen musss nach einer Beschleunigung
#define timeout_after_error 			2000 	// 2s Zeit die zwischen zwei Fehlern minimum Vergehen muss


typedef enum
{
	mode_init,					//Im init-Mode werden die Ringbuffer openBuffer und closedBuffer gefüllt
	mode_waitForDrive,			//Es wird gewartet bis die Aufzugskabine fährt, um die Buffer endgültig zuzuweisen
	mode_run					//Normaler Betrieb
}device_mode_t;

typedef enum
{
	door_moving,
	door_open,
	door_closed,
	cabin_drives
}door_state_t;

void app_init(void);

void app_run(void);

void vApplicationIdleHook();

#endif /* APPLICATION_USER_APP_H_ */
