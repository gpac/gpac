/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Arash Shafiei
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef TASK_H_
#define TASK_H_

#define MAX_ID_SIZE 512

#include <stdio.h>
#include <time.h>
#include <gpac/list.h>


typedef struct {
	char id[MAX_ID_SIZE];
	int source_number;
	time_t start_time_t;
	time_t end_time_t;
} Task;

typedef struct {
	GF_List *tasks;
	int size;
} TaskList;

/**
 * initialize a list of task
 */
void dc_task_init(TaskList *list);

/**
 * destroy the list of task
 */
void dc_task_destroy(TaskList *list);

/**
 * audio_input_data a task to the list
 */
void dc_task_add(TaskList *list, int source_number, char *id_name, time_t start, time_t end);

/**
 * give the task which corresponds to the time=NOW
 * note: in the case of infering tasks, the first one is picked
 */
int dc_task_get_current(TaskList *list, Task *task);

#endif /* TASK_H_ */
