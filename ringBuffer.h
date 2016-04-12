/*
 * ringBuffer.h
 *
 *  Created on: 02.03.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_RINGBUFFER_H_
#define APPLICATION_USER_RINGBUFFER_H_

#define anzahlVektorBuf 3
#define anzahlFloatBuf 2
#define bufSize1 10		//Max Anzahl Vektoren im Buffer (lastVectors)
#define bufSize2 20		//Max Anzahl Vektoren im Buffer (open)
#define bufSize3 20		//Max Anzahl Vektoren im Buffer (closed)
#define bufSize4 5		//Max Anzahl floats im Buffer (accel)
#define bufSize5 5		//Max Anzahl floats im Buffer (accel_offset)

#if defined STM32F401xE || STM32F4051xE
#include "stm32f4xx_hal.h"
#endif
#if defined STM32L476xx
#include "stm32l4xx_hal.h"
#endif

#include "i2c_magn.h"

typedef struct
{
	magnVec_t *vectorArray;
	uint16_t index;			//Aktueller Eintrag
	uint16_t size;			//Anzahl Einträge
	uint16_t counter;		//So oft wurde in den Buffer geschrieben
}vectorRingBufHandle;

typedef struct
{
	float *floatArray;
	uint16_t index;			//Aktueller Eintrag
	uint16_t size;			//Anzahl Einträge
	uint16_t counter;		//So oft wurde in den Buffer geschrieben
}floatRingBufHandle;

//Vector Ringbuffers
vectorRingBufHandle * initVectorBuffer(uint16_t whichBuf);
void putInBuffer(vectorRingBufHandle *buf_handle, magnVec_t vector);
float getMeanValue(vectorRingBufHandle *buf_handle);
float getMeanAlpha(vectorRingBufHandle *buf_handle);
float getMeanBeta(vectorRingBufHandle *buf_handle);
float getMeanX(vectorRingBufHandle *buf_handle);
float getMeanY(vectorRingBufHandle *buf_handle);
float getMeanZ(vectorRingBufHandle *buf_handle);
void eraseVectorBuffer(vectorRingBufHandle *buf_handle);

// Float RingBuffers
floatRingBufHandle * initfloatBuffer(uint16_t whichFloatBuf);
void putInFloatBuffer(floatRingBufHandle *buf_Float_handle, float value);
void getMeanFloatValue(floatRingBufHandle *buf_Float_handle, float *result);
void eraseFloatBuffer(floatRingBufHandle *buf_Float_handle);

#endif /* APPLICATION_USER_RINGBUFFER_H_ */
