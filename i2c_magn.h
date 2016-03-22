/*
 * i2c_magn.h
 *
 *  Created on: 23.02.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_I2C_MAGN_H_
#define APPLICATION_USER_I2C_MAGN_H_

#include "stm32f4xx_hal.h"
#include "app.h"

typedef enum
{
	vek_dynamic,	//Änderndes Magnetfeld
	vek_idle,		//Statisches Magnetfeld
	vek_door_open,	//Vektor wurde einer geschlossenen Türe zugeordnet
	vek_door_closed	//Vektor wurde einer offenen Türe zugeordnet
}vektor_state;

typedef struct
{
//#if distance_assignment //TODO Diese // Entfernen ohne Kompier errors :-)
	float x_val;	//X Komponente des Magnetfeldvektors
	float y_val;	//Y Komponente des Magnetfeldvektors
	float z_val;	//Z Komponente des Magnetfeldvektors
//#endif
	float value;	//Betrag des Magnetvektors
	float alpha;	//Winkel der Vektorprojektion auf XY-Ebene zur X-Achse
	float beta;		//Winkel des Vektors Bezüglich der XY-Ebene
	vektor_state state;
}magnVec_t;





void init_i2c_magn(I2C_HandleTypeDef *hi2c);

void get_magn(int16_t *vector);

void get_magn_uT(float *vector);

void get_vektor(magnVec_t *vector);

#endif /* APPLICATION_USER_I2C_MAGN_H_ */
