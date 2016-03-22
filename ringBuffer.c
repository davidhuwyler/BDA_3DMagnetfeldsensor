/*
 * ringBuffer.c
 *
 *  Created on: 02.03.2016
 *      Author: xxdav
 */

#include "stm32f4xx_hal.h"
#include "ringBuffer.h"
#include "i2c_magn.h"
#include "cmsis_os.h"

static vectorRingBufHandle handles[anzahlVektorBuf];
static floatRingBufHandle floatHandle[anzahlFloatBuf];

static magnVec_t vectorBuf1[bufSize1];
static magnVec_t vectorBuf2[bufSize2];
static magnVec_t vectorBuf3[bufSize3];
static float floatBuf1[bufSize4];
static float floatBuf2[bufSize5];

vectorRingBufHandle * initVectorBuffer(uint16_t whichBuf)
{
	handles[whichBuf-1].index = 0;
	handles[whichBuf-1].counter = 0;
	switch(whichBuf)
	{
	case 1: handles[whichBuf-1].vectorArray = vectorBuf1;
			handles[whichBuf-1].size = bufSize1;
	break;
	case 2: handles[whichBuf-1].vectorArray = vectorBuf2;
			handles[whichBuf-1].size = bufSize2;
	break;
	case 3: handles[whichBuf-1].vectorArray = vectorBuf3;
			handles[whichBuf-1].size = bufSize3;
	break;
	}
	return &(handles[whichBuf-1]);
}

void putInBuffer(vectorRingBufHandle *buf_handle, magnVec_t vector)
{
	buf_handle->vectorArray[buf_handle->index]=vector;
	buf_handle->index++;
	if(buf_handle->index==buf_handle->size){buf_handle->index = 0;}
	buf_handle->counter++;
}


float getMeanValue(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].value;
	}
	return temp/buf_handle->size;
}
float getMeanAlpha(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].alpha;
	}
	return temp/buf_handle->size;
}
float getMeanBeta(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].beta;
	}
	return temp/buf_handle->size;
}

#if distance_assignment
float getMeanX(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].x_val;
	}
	return temp/buf_handle->size;
}
float getMeanY(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].y_val;
	}
	return temp/buf_handle->size;
}
float getMeanZ(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
		temp = temp+buf_handle->vectorArray[i].z_val;
	}
	return temp/buf_handle->size;
}
#endif


void eraseVectorBuffer(vectorRingBufHandle *buf_handle)
{
	int i;
	float temp = 0;
	for(i=0;i<buf_handle->size;i++)
	{
#if distance_assignment
		buf_handle->vectorArray[i].x_val = 0;
		buf_handle->vectorArray[i].y_val = 0;
		buf_handle->vectorArray[i].z_val = 0;
#endif
#if value_angle_assignment
		buf_handle->vectorArray[i].value = 0;
		buf_handle->vectorArray[i].alpha = 0;
		buf_handle->vectorArray[i].beta = 0;
#endif
	}

}

floatRingBufHandle * initfloatBuffer(uint16_t whichFloatBuf)
{
	floatHandle[whichFloatBuf-1].index = 0;
	floatHandle[whichFloatBuf-1].counter = 0;
	switch(whichFloatBuf)
	{
	case 1: floatHandle[whichFloatBuf-1].floatArray = floatBuf1;
			floatHandle[whichFloatBuf-1].size = bufSize4;
	break;
	case 2: floatHandle[whichFloatBuf-1].floatArray = floatBuf2;
			floatHandle[whichFloatBuf-1].size = bufSize5;
	break;
	}
	return &(floatHandle[whichFloatBuf-1]);
}

void putInFloatBuffer(floatRingBufHandle *buf_Float_handle, float value)
{
	buf_Float_handle->floatArray[buf_Float_handle->index]=value;
	buf_Float_handle->index++;
	if(buf_Float_handle->index==buf_Float_handle->size){buf_Float_handle->index = 0;}
	buf_Float_handle->counter++;
}


void getMeanFloatValue(floatRingBufHandle *buf_Float_handle, float *result)
{
	vPortEnterCritical();
	int i;
	float temp = 0, temp2;
	for(i=0;i<buf_Float_handle->size;i++)
	{
		temp = temp+buf_Float_handle->floatArray[i];
	}
	vPortExitCritical();
	*result = (float)(temp/(float)(buf_Float_handle->size));

}

void eraseFloatBuffer(floatRingBufHandle *buf_Float_handle)
{
	int i;
	for(i=0;i<buf_Float_handle->size;i++)
	{
		buf_Float_handle->floatArray[i] = 0;
	}
}

