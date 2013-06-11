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

#include "task.h"


void dc_task_init(TaskList * list) {

	list->tasks = gf_list_new();
	list->size = 0;
}

void dc_task_destroy(TaskList * list) {

	gf_list_del(list->tasks);
}

void dc_task_add(TaskList * list, int source_number, char * id, time_t start, time_t end) {

	Task * task = malloc(sizeof(Task));

	task->source_number = source_number;
	strncpy(task->id, id, MAX_ID_SIZE);
	//memcpy(&task->start_time, start, sizeof(struct tm));
	//memcpy(&task->end_time, end, sizeof(struct tm));
	task->start_time_t = start;
	task->end_time_t = end;
	gf_list_add(list->tasks, task);
	list->size ++;
}

int dc_task_get_current(TaskList * list, Task * task) {

	int i;

	time_t now_time = time(NULL);

	for (i = 0; i<list->size; i++) {
		Task * cur_task = gf_list_get(list->tasks, i);


		if(now_time > cur_task->start_time_t  && now_time < cur_task->end_time_t) {

			//strncpy(task->id, cur_task->id, MAX_ID_SIZE);
			//memcpy(&task->start_time, &cur_task->start_time, sizeof(struct tm));
			//memcpy(&task->end_time, &cur_task->end_time, sizeof(struct tm));
			task->source_number = cur_task->source_number;
			return 0;
		}
	}

	task->source_number = 0;
	return -1;
}


