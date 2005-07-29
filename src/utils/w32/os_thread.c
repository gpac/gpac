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

#include <windows.h>

typedef HANDLE TH_HANDLE ;


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

DWORD WINAPI RunThread(void *ptr)
{
	DWORD ret = 0;
	GF_Thread *t = (GF_Thread *)ptr;

	/* Signal the caller */
	if (! t->_signal) goto exit;

	t->status = GF_THREAD_STATUS_RUN;
	
	gf_sema_notify(t->_signal, 1);
	/* Run our thread */
	ret = t->Run(t->args);

exit:
	t->status = GF_THREAD_STATUS_DEAD;
	t->Run = NULL;
	CloseHandle(t->threadH);
	t->threadH = NULL;
	return ret;
}

GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
	DWORD id;
	if (!t || t->Run || t->_signal) return GF_BAD_PARAM;
	t->Run = Run;
	t->args = param;
	t->_signal = gf_sema_new(1, 0);

	t->threadH = CreateThread(NULL,  t->stackSize, &(RunThread), (void *)t, 0, &id);
	if (t->threadH == NULL) {
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
			DWORD dw = 1;
			TerminateThread(t->threadH, dw);
			t->threadH = NULL;
		} else {
			WaitForSingleObject(t->threadH, INFINITE);			
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
	if (t->threadH) CloseHandle(t->threadH);
	free(t);
}


void gf_th_set_priority(GF_Thread *t, s32 priority)
{
	SetThreadPriority(t ? t->threadH : GetCurrentThread(), priority);
}

u32 gf_th_status(GF_Thread *t)
{
	if (!t) return 0;
	return t->status;
}


u32 gf_th_id()
{
	return ((u32) GetCurrentThreadId());
}


/*********************************************************************
						OS-Specific Mutex Object
**********************************************************************/
struct __tag_mutex
{
	HANDLE hMutex;
	/* We filter recursive calls (1 thread calling Lock several times in a row only locks
	ONCE the mutex. Holder is the current ThreadID of the mutex holder*/
	u32 Holder, HolderCount;
};


GF_Mutex *gf_mx_new()
{
	GF_Mutex *tmp = malloc(sizeof(GF_Mutex));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Mutex));
	
	tmp->hMutex = CreateMutex(NULL, FALSE, NULL);
	if (!tmp->hMutex) {
		free(tmp);
		return NULL;
	}
	return tmp;
}

void gf_mx_del(GF_Mutex *mx)
{
	CloseHandle(mx->hMutex);
	free(mx);
}

void gf_mx_v(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return;
	caller = gf_th_id();

	/*only if we own*/
	assert(caller == mx->Holder);
	if (caller != mx->Holder) return;

	assert(mx->HolderCount > 0);
	mx->HolderCount -= 1;

	if (mx->HolderCount == 0) {
		mx->Holder = 0;
		caller = ReleaseMutex(mx->hMutex);
		assert(caller);
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

	switch (WaitForSingleObject(mx->hMutex, INFINITE)) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		return 0;
	default:
		mx->HolderCount = 1;
		mx->Holder = caller;
		return 1;
	}
}

Bool gf_mx_try_lock(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return 0;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return 1;
	}

	/*wait for 1 ms (I can't figure out from MS doc if 0 timeout only "tests the state" or also lock the mutex ... */
	switch (WaitForSingleObject(mx->hMutex, 1)) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		return 0;
	default:
		mx->HolderCount = 1;
		mx->Holder = caller;
		return 1;
	}
}



/*********************************************************************
						OS-Specific Semaphore Object
**********************************************************************/
struct __tag_semaphore
{
	HANDLE hSemaphore;
};


GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount)
{
	GF_Semaphore *tmp = malloc(sizeof(GF_Semaphore));

	if (!tmp) return NULL;
	tmp->hSemaphore = CreateSemaphore(NULL, InitCount, MaxCount, NULL);
	if (!tmp->hSemaphore) {
		free(tmp);
		return NULL;
	}
	return tmp;
}

void gf_sema_del(GF_Semaphore *sm)
{
	CloseHandle(sm->hSemaphore);
	free(sm);
}

u32 gf_sema_notify(GF_Semaphore *sm, u32 NbRelease)
{
	LONG prevCount;
	
	if (!sm) return 0;

	ReleaseSemaphore(sm->hSemaphore, NbRelease, &prevCount);
	return (u32) prevCount;
}

void gf_sema_wait(GF_Semaphore *sm)
{
	WaitForSingleObject(sm->hSemaphore, INFINITE);
}

Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut)
{
	if (WaitForSingleObject(sm->hSemaphore, TimeOut) == WAIT_TIMEOUT) return 0;
	return 1;
}

