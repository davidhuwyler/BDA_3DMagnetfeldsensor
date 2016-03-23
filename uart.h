/*
 * uart.h
 *
 *  Created on: 24.02.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_UART_H_
#define APPLICATION_USER_UART_H_


void init_uart(UART_HandleTypeDef *huart);

void println(uint8_t *pData, uint16_t Size);

void printNumber(int16_t num);


#endif /* APPLICATION_USER_UART_H_ */
