/*
 * i2c_magn.c
 *
 *  Created on: 23.02.2016
 *      Author: xxdav
 */
#include <stdlib.h>
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
#include "cmsis_os.h"
#include "i2c_magn.h"
#include "app.h"

#include <math.h>
#include "arm_math.h"
int __errno;					//Compier error Verhindern für math.h

static I2C_HandleTypeDef *i2c_Handle;


#if Enable_LIS3MDL
#define adress_LIS3MDL 0x3C			//Slave Adresse  for LIS3MDL
#endif
#if Enable_LSM303DLHC
#define adress_LSM303DLHC 0x3C	//Slace Adress for the Magnetometer of the LSM303DLHC
#endif

void init_i2c_magn(I2C_HandleTypeDef *hi2c)
{
	i2c_Handle = hi2c;
	uint8_t buf[2];
	volatile HAL_StatusTypeDef error;

#if Enable_LIS3MDL
	buf[0] = 0x20; 						//CTRL_REG1
	buf[1] = 0x74;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling

	buf[0] = 0x21; 						//CTRL_REG2
	buf[1] = 0x00;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling

	buf[0] = 0x22; 						//CTRL_REG3
	buf[1] = 0x00;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling

	buf[0] = 0x23; 						//CTRL_REG4
	buf[1] = 0x0C;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling
#endif

#if Enable_LSM303DLHC
	buf[0] = 0x00; 						//CRA_REG_M
	buf[1] = 0x14;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling

	buf[0] = 0x01; 						//CRB_REG_M
	buf[1] = 0x20;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling

	buf[0] = 0x02; 						//MR_REG_M
	buf[1] = 0x00;						//Data
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,2,1000);
	if(error!=HAL_OK){} //TODO Error Handling
#endif

}

/**
 *
 * @param *vector Pointer to a Buffer in witch the 3 components of the magnetfield
 * 		  vector gets stored
 */
void get_magn(int16_t *vector)
{
	uint8_t buf[2];
	uint8_t xl,xh,yl,yh,zl,zh;
	volatile HAL_StatusTypeDef error;

#if Enable_LIS3MDL
	buf[0] = 0x28;						//Low Byte des X-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &xl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x29;						//High Byte des X-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &xh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x2A;						//Low Byte des Y-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &yl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x2B;						//High Byte des Y-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &yh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x2C;						//Low Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &zl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x2D;						//High Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LIS3MDL, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LIS3MDL, &zh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}
#endif

#if Enable_LSM303DLHC
	buf[0] = 0x04;						//Low Byte des X-Anteils Abfragen


	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &xl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x03;						//High Byte des X-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &xh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x08;						//Low Byte des Y-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &yl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x07;						//High Byte des Y-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &yh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x06;						//Low Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &zl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf[0] = 0x05;						//High Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC, (uint8_t*)buf,1,1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, adress_LSM303DLHC, &zh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}
#endif

	vector[0] = (xh<<8) | xl;
	vector[1] = (yh<<8) | yl;
	vector[2] = (zh<<8) | zl;
}



void get_magn_uT(float32_t *vector)
{
	int16_t vector_int[3];
	get_magn(vector_int);

#if Enable_LIS3MDL
	vector[0] = (float)vector_int[0]/68.4;
	vector[1] = (float)vector_int[1]/68.4;
	vector[2] = (float)vector_int[2]/68.4;
#endif

#if Enable_LSM303DLHC
	vector[0] = (float)vector_int[0]/11.0;
	vector[1] = (float)vector_int[1]/11.0;
	vector[2] = (float)vector_int[2]/9.8;
#endif
}


/*
 * Funktion gibt den Betrag des Vektors in uT, den Winkel alpha des auf die XY Ebene projektierten Vektors
 * bezüglich der X-Achse und den Winkel beta des Vektors bezüglich der XY Ebene zurück
 */
void get_vektor(magnVec_t *vector)
{
	float32_t vector_uT[3];
	float32_t temp,temp2;

	get_magn_uT(vector_uT);

#if distance_assignment
	vector->x_val = vector_uT[0];
	vector->y_val = vector_uT[1];
	vector->z_val = vector_uT[2];
#endif

	//Betrag berechnen:
	temp = fabs(vector_uT[0]*vector_uT[0])+(vector_uT[1]*vector_uT[1])+(vector_uT[2]*vector_uT[2]);
	arm_sqrt_f32(temp,&vector->value);

	//Winkel alpha muss angepasst werden nach Quadrant:
	if(vector_uT[0]>0 && vector_uT[1]>0)		//Vektor im 1. Quadranten
	{
		vector->alpha=((atanf(vector_uT[1]/vector_uT[0]))*180/PI);
		//*alpha=1;
	}
	else if(vector_uT[0]<0 && vector_uT[1]>0) //Vektor im 2. Quadranten
	{
		vector->alpha=((atanf(vector_uT[1]/vector_uT[0]))*180/PI)+180;
		//*alpha=2;
	}
	else if(vector_uT[0]<0 && vector_uT[1]<0) //Vektor im 3. Quadranten
	{
		vector->alpha=((atanf(vector_uT[1]/vector_uT[0]))*180/PI)+180;
		//*alpha=3;
	}
	else if(vector_uT[0]>0 && vector_uT[1]<0) //Vektor im 4. Quadranten
	{
		vector->alpha=((atanf(vector_uT[1]/vector_uT[0]))*180/PI)+360;
		//*alpha=4;
	}

	//beta berechnen:
	temp = fabs(vector_uT[0]*vector_uT[0]+vector_uT[1]*vector_uT[1]);
	arm_sqrt_f32(temp,&temp2);
	vector->beta=(atanf(vector_uT[2]/(temp2))*180/PI);
}
