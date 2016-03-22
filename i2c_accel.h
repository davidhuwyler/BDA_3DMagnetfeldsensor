/*
 * i2c_accel.h
 *
 *  Created on: 02.03.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_I2C_ACCEL_H_
#define APPLICATION_USER_I2C_ACCEL_H_

void init_i2c_accel(I2C_HandleTypeDef *hi2c);

void get_accel(float *zValue);

#endif /* APPLICATION_USER_I2C_ACCEL_H_ */
