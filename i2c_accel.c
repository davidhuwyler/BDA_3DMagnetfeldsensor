/*
 * i2c_accel.c
 *
 *  Created on: 02.03.2016
 *      Author: xxdav
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "i2c_accel.h"

#define adress_LSM303DLHC_acel 0x32	//Slave Adress for the Accelerometer of the LSM303DLHC

static I2C_HandleTypeDef *i2c_Handle;


void init_i2c_accel(I2C_HandleTypeDef *hi2c)
{
	i2c_Handle = hi2c;
	uint8_t buf[2];

	volatile HAL_StatusTypeDef error;

	buf[0] = 0x20; 						//CTRL_REG1
	buf[1] = 0x34;						//Data

	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC_acel, (uint8_t*)buf,sizeof(buf),1000);
	if(error!=HAL_OK){} //TODO Error Handling
}

void get_accel(float *zValue)
{
	uint8_t buf;
	uint8_t zl;
	int8_t zh;
	volatile HAL_StatusTypeDef error;

	buf = 0x2C;						//Low Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC_acel, &buf,sizeof(buf),1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, (uint16_t)adress_LSM303DLHC_acel, &zl,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}

	buf = 0x2D;						//High Byte des Z-Anteils Abfragen
	error = HAL_I2C_Master_Transmit(i2c_Handle, (uint16_t)adress_LSM303DLHC_acel, &buf,sizeof(buf),1000);
	if(error==HAL_OK)
	{
		error = HAL_I2C_Master_Receive(i2c_Handle, (uint16_t)adress_LSM303DLHC_acel, &zh,1,500);
		if(error!=HAL_OK){} //TODO Error Handling
	}


	*zValue = (float)((int16_t)((zh<<8) | zl))*0.000649;
}



