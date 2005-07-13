/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#ifndef _GF_THREAD_H_
#define _GF_THREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/tools.h>


/*********************************************************************
					Thread Object
**********************************************************************/

#define GF_THREAD_STATUS_STOP		0
#define GF_THREAD_STATUS_RUN		1
#define GF_THREAD_STATUS_DEAD		2

typedef struct __tag_thread GF_Thread;

/* creates a new thread object */
GF_Thread *gf_th_new();
/* kill the thread if active and delete the object */
void gf_th_del(GF_Thread *t);
/* run the thread */
GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param);
/* stop the thread and wait for its exit */
void gf_th_stop(GF_Thread *t);
/*get the thread status */
u32 gf_th_status(GF_Thread *t);

/* thread priority */

enum
{
	GF_THREAD_PRIORITY_IDLE=0,
	GF_THREAD_PRIORITY_LESS_IDLE,
	GF_THREAD_PRIORITY_LOWEST,
	GF_THREAD_PRIORITY_LOW,
	GF_THREAD_PRIORITY_NORMAL,
	GF_THREAD_PRIORITY_HIGH,
	GF_THREAD_PRIORITY_HIGHEST,
	GF_THREAD_PRIORITY_REALTIME,
	/*this is where real-time priorities stop*/
	GF_THREAD_PRIORITY_REALTIME_END=255
};

void gf_th_set_priority(GF_Thread *t, s32 priority);
/* get the current threadID */
u32 gf_th_id();

/*********************************************************************
					Mutex Object
**********************************************************************/
typedef struct __tag_mutex GF_Mutex;

GF_Mutex *gf_mx_new();
void gf_mx_del(GF_Mutex *mx);
u32 gf_mx_p(GF_Mutex *mx);
void gf_mx_v(GF_Mutex *mx);


/*********************************************************************
					Semaphore Object
**********************************************************************/
typedef struct __tag_semaphore GF_Semaphore;


GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount);
void gf_sema_del(GF_Semaphore *sm);

/* return the number of previous count in the semaphore */
u32 gf_sema_notify(GF_Semaphore *sm, u32 NbRelease);
/*infinite wait on a semaphore */
void gf_sema_wait(GF_Semaphore *sm);
/* timely wait on a semaphore, return 0 if couldn't get control before TimeOut
	NOTE: On POSIX, the timeout is implemented via a gf_sleep() till the notif is done
	or an error occurs
*/
Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut);

#ifdef __cplusplus
}
#endif


#endif		/*_GF_THREAD_H_*/

