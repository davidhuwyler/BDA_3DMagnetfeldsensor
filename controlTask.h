/*
 * controlTask.h
 *
 *  Created on: 07.03.2016
 *      Author: xxdav
 */

#ifndef APPLICATION_USER_CONTROLTASK_H_
#define APPLICATION_USER_CONTROLTASK_H_



typedef enum
{
	err_ok,
	err_should_be_open,
	err_should_be_closed
}err_door_t;


void StartControlTask(void const * argument);

#endif /* APPLICATION_USER_CONTROLTASK_H_ */
