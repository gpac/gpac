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


#include <gpac/thread.h>

//#include <features.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>

typedef pthread_t TH_HANDLE ;


/*********************************************************************
						OS-Specific Thread Object
**********************************************************************/
struct __tag_thread
{

	u32 status;
	TH_HANDLE threadH;
	u32 stackSize;
	/* the thread procedure */
	u32 (*Run)(void *param);
	void *args;
	/* lock for signal */
	GF_Semaphore *_signal;
};

GF_Thread *gf_th_new()
{
	GF_Thread *tmp = malloc(sizeof(GF_Thread));
	memset(tmp, 0, sizeof(GF_Thread));
	tmp->status = GF_THREAD_STATUS_STOP;
	return tmp;
}

void *RunThread(void *ptr)
{
	u32 ret;
	GF_Thread *t = (GF_Thread *)ptr;

	/* Signal the caller */
	if (! t->_signal) goto exit;

	t->status = GF_THREAD_STATUS_RUN;
	
	gf_sema_notify(t->_signal, 1);
	/* Run our thread */
	ret = t->Run(t->args);

exit:
	/* kill the pthread*/
	t->status = GF_THREAD_STATUS_DEAD;
	t->Run = NULL;
	pthread_exit((void *)0);
	return (void *)0;
}

GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
	pthread_attr_t att;

	if (!t || t->Run || t->_signal) return GF_BAD_PARAM;
	t->Run = Run;
	t->args = param;
	t->_signal = gf_sema_new(1, 0);

	if ( pthread_attr_init(&att) != 0 ) return GF_IO_ERR;
	pthread_attr_setdetachstate(&att, PTHREAD_CREATE_JOINABLE);
	if ( pthread_create(&t->threadH, &att, RunThread, t) != 0 ) {
		t->status = GF_THREAD_STATUS_DEAD;
		return GF_IO_ERR;
	}
	/*wait for the child function to call us - do NOT return bedfore, otherwise the thread status would
	be unknown*/ 	
	gf_sema_wait(t->_signal);
	gf_sema_del(t->_signal);
	t->_signal = NULL;
	return GF_OK;
}


/* Stops a thread. If Destroy is not 0, thread is destroyed DANGEROUS as no cleanup */
void Thread_Stop(GF_Thread *t, Bool Destroy)
{
	if (gf_th_status(t) == GF_THREAD_STATUS_RUN) {
		if (Destroy) {
			pthread_cancel(t->threadH);
			t->threadH = 0;
		} else {
			/*gracefully wait for Run to finish*/
			pthread_join(t->threadH, NULL);			
		}
	}
	t->status = GF_THREAD_STATUS_DEAD;
}

void gf_th_stop(GF_Thread *t)
{
	Thread_Stop(t, 0);
}

void gf_th_del(GF_Thread *t)
{
	Thread_Stop(t, 0);
	free(t);
}


void gf_th_set_priority(GF_Thread *t, s32 priority)
{
	struct sched_param s_par;
	if (!t) return;

	/* consider this as real-time priority */
	if (priority > 200) {
		s_par.sched_priority = priority - 200;
		pthread_setschedparam(t->threadH, SCHED_RR, &s_par);
	} else {
		s_par.sched_priority = priority;
		pthread_setschedparam(t->threadH, SCHED_OTHER, &s_par);
	}
}

u32 gf_th_status(GF_Thread *t)
{
	if (!t) return 0;
	return t->status;
}


u32 gf_th_id()
{
	return ((u32) pthread_self());
}


/*********************************************************************
						OS-Specific Mutex Object
**********************************************************************/
struct __tag_mutex
{
	pthread_mutex_t hMutex;
	/* We filter recursive calls (1 thread calling Lock several times in a row only locks
	ONCE the mutex. Holder is the current ThreadID of the mutex holder*/
	u32 Holder, HolderCount;
};


GF_Mutex *gf_mx_new()
{
	pthread_mutexattr_t attr;
	GF_Mutex *tmp = malloc(sizeof(GF_Mutex));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Mutex));
	
	pthread_mutexattr_init(&attr);
	if ( pthread_mutex_init(&tmp->hMutex, &attr) != 0 ) {
		free(tmp);
		return NULL;
	}
	return tmp;
}

void gf_mx_del(GF_Mutex *mx)
{
	pthread_mutex_destroy(&mx->hMutex);
	free(mx);
}

void gf_mx_v(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return;
	caller = gf_th_id();
	/*only if we own*/
	if (caller != mx->Holder) return;

	if (mx->HolderCount) {
	  mx->HolderCount -= 1;
	} else {
		mx->Holder = 0;
		pthread_mutex_unlock(&mx->hMutex);
	}
}

u32 gf_mx_p(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return 0;
	
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return 1;
	}

	if (pthread_mutex_lock(&mx->hMutex) == 0 ) {
		mx->Holder = caller;
		mx->HolderCount = 0;
		return 1;
	}
	assert(0);
	mx->Holder = mx->HolderCount = 0;
	return 0;
}



/*********************************************************************
						OS-Specific Semaphore Object
**********************************************************************/
struct __tag_semaphore
{
	sem_t *hSemaphore;
	sem_t SemaData;
};


GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount)
{
	GF_Semaphore *tmp = malloc(sizeof(GF_Semaphore));



	if (!tmp) return NULL;
	if (sem_init(&tmp->SemaData, 0, InitCount) < 0 ) {
		free(tmp);
		return NULL;
	}
	tmp->hSemaphore = &tmp->SemaData;
	return tmp;
}

void gf_sema_del(GF_Semaphore *sm)
{
	sem_destroy(sm->hSemaphore);
	free(sm);
}

u32 gf_sema_notify(GF_Semaphore *sm, u32 NbRelease)
{
	u32 prevCount;
	
	if (!sm) return 0;

	sem_getvalue(sm->hSemaphore, (s32 *) &prevCount);
	while (NbRelease) {
		if (sem_post(sm->hSemaphore) < 0) return 0;
		NbRelease -= 1;
	}
	return prevCount;
}

void gf_sema_wait(GF_Semaphore *sm)
{
	sem_wait(sm->hSemaphore);
}

Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut)
{
	if (!TimeOut) {
		if (!sem_trywait(sm->hSemaphore)) return 1;
		return 0;
	}
	TimeOut += gf_sys_clock();
	do {
		if (!sem_trywait(sm->hSemaphore)) return 1;
		gf_sleep(1);	
	} while (gf_sys_clock() < TimeOut);
	return 0;
}

