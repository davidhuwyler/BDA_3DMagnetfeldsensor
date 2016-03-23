/*
 * uart.c
 *
 *  Created on: 24.02.2016
 *      Author: xxdav
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "uart.h"

#include <stdio.h>
#include <stdlib.h>


static UART_HandleTypeDef *uartHandle;

void init_uart(UART_HandleTypeDef *huart)
{
	uartHandle = huart;
}


void println(uint8_t *pData, uint16_t Size)
{

	//UART_WaitOnFlagUntilTimeout(uartHandle,UART_FLAG_LBD);
	vPortEnterCritical();
	HAL_UART_Transmit(uartHandle, pData, Size, 1000);
	vPortExitCritical();
}

void printNumber(int16_t num)
{
	uint8_t buf[8];
	uint8_t size;
	if(num<10 && num>-1){size = 1;}
	else if(num<100 && num>-10){size = 2;}
	else if(num<1000 && num>-100){size = 3;}
	else if(num<10000 && num>-1000){size = 4;}
	else if(num<100000 && num>-10000){size = 5;}
	else if(num<1000000 && num>-100000){size = 6;}
	else if(num<10000000 && num>-1000000){size = 7;}
	else {size = 8;}

	itoa(num,(char *)buf,10);
	HAL_UART_Transmit(uartHandle, buf, size, 1000);
}

