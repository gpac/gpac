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

#if defined(WIN32) || defined(_WIN32_WCE)

/*win32 threads*/
#include <windows.h>
typedef HANDLE TH_HANDLE;

#else
/*pthreads*/
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
typedef pthread_t TH_HANDLE ;

#endif



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
#ifndef GPAC_DISABLE_LOG
	u32 id;
	char *log_name;
#endif
};


#ifndef GPAC_DISABLE_LOG
#include <gpac/list.h>
static GF_List *thread_bank = NULL;

static void log_add_thread(GF_Thread *t)
{
	if (!thread_bank) thread_bank = gf_list_new();
	gf_list_add(thread_bank, t);
}
static void log_del_thread(GF_Thread *t)
{
	gf_list_del_item(thread_bank, t);
	if (!gf_list_count(thread_bank)) {
		gf_list_del(thread_bank);
		thread_bank = NULL;
	}
}
static const char *log_th_name(u32 id)
{
	u32 i, count;
	
	if (!id) id = gf_th_id();
	count = gf_list_count(thread_bank);
	for (i=0; i<count; i++) {
		GF_Thread *t = gf_list_get(thread_bank, i);
		if (t->id == id) return t->log_name;
	}
	return "Main Process";
}

#endif


GF_Thread *gf_th_new(const char *name)
{
	GF_Thread *tmp = malloc(sizeof(GF_Thread));
	memset(tmp, 0, sizeof(GF_Thread));
	tmp->status = GF_THREAD_STATUS_STOP;

#ifndef GPAC_DISABLE_LOG
	if (name) {
		tmp->log_name = strdup(name);
	} else {
		char szN[20];
		sprintf(szN, "0x%08x", (u32) tmp);
		tmp->log_name = strdup(szN);
	}
	log_add_thread(tmp);
#endif
	return tmp;
}

#ifdef WIN32
DWORD WINAPI RunThread(void *ptr)
{
	DWORD ret = 0;
#else
void *RunThread(void *ptr)
{
	u32 ret = 0;
#endif
	GF_Thread *t = (GF_Thread *)ptr;

	/* Signal the caller */
	if (! t->_signal) goto exit;

	t->status = GF_THREAD_STATUS_RUN;
	gf_sema_notify(t->_signal, 1);

#ifndef GPAC_DISABLE_LOG
	t->id = gf_th_id();
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Entering thread proc - thread ID 0x%08x\n", t->log_name, t->id));
#endif
	/* Run our thread */
	ret = t->Run(t->args);

exit:
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Exiting thread proc\n", t->log_name));
#endif
	t->status = GF_THREAD_STATUS_DEAD;
	t->Run = NULL;
#ifdef WIN32
	CloseHandle(t->threadH);
	t->threadH = NULL;
	return ret;
#else
	pthread_exit((void *)0);
	return (void *)ret;
#endif
}

GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
#ifdef WIN32
	DWORD id;
#else
	pthread_attr_t att;
#endif
	if (!t || t->Run || t->_signal) return GF_BAD_PARAM;
	t->Run = Run;
	t->args = param;
	t->_signal = gf_sema_new(1, 0);

#ifdef WIN32
	t->threadH = CreateThread(NULL,  t->stackSize, &(RunThread), (void *)t, 0, &id);
	if (t->threadH == NULL) {
#else
	if ( pthread_attr_init(&att) != 0 ) return GF_IO_ERR;
	pthread_attr_setdetachstate(&att, PTHREAD_CREATE_JOINABLE);
	if ( pthread_create(&t->threadH, &att, RunThread, t) != 0 ) {
#endif
		t->status = GF_THREAD_STATUS_DEAD;
		return GF_IO_ERR;
	}

	/*wait for the child function to call us - do NOT return before, otherwise the thread status would
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
#ifdef WIN32
		if (Destroy) {
			DWORD dw = 1;
			TerminateThread(t->threadH, dw);
			t->threadH = NULL;
		} else {
			WaitForSingleObject(t->threadH, INFINITE);			
		}
#else
		if (Destroy) {
			pthread_cancel(t->threadH);
			t->threadH = 0;
		} else {
			/*gracefully wait for Run to finish*/
			pthread_join(t->threadH, NULL);			
		}
#endif
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
#ifdef WIN32
//	if (t->threadH) CloseHandle(t->threadH);
#endif

#ifndef GPAC_DISABLE_LOG
	free(t->log_name);
	log_del_thread(t);
#endif
	free(t);
}


void gf_th_set_priority(GF_Thread *t, s32 priority)
{
#ifdef WIN32
	/*!! in WinCE, changin thread priority is extremely dangerous, it may freeze threads randomly !!*/
#ifndef _WIN32_WCE
	SetThreadPriority(t ? t->threadH : GetCurrentThread(), priority);
#endif

#else

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

#endif
}

u32 gf_th_status(GF_Thread *t)
{
	if (!t) return 0;
	return t->status;
}


u32 gf_th_id()
{
#ifdef WIN32
	return ((u32) GetCurrentThreadId());
#else
	return ((u32) pthread_self());
#endif
}


/*********************************************************************
						OS-Specific Mutex Object
**********************************************************************/
struct __tag_mutex
{
#ifdef WIN32
	HANDLE hMutex;
#else
	pthread_mutex_t hMutex;
#endif
	/* We filter recursive calls (1 thread calling Lock several times in a row only locks
	ONCE the mutex. Holder is the current ThreadID of the mutex holder*/
	u32 Holder, HolderCount;
#ifndef GPAC_DISABLE_LOG
	char *log_name;
#endif
};


GF_Mutex *gf_mx_new(const char *name)
{
#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	GF_Mutex *tmp = malloc(sizeof(GF_Mutex));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Mutex));
	
#ifdef WIN32
	tmp->hMutex = CreateMutex(NULL, FALSE, NULL);
	if (!tmp->hMutex) {
#else
	pthread_mutexattr_init(&attr);
	if ( pthread_mutex_init(&tmp->hMutex, &attr) != 0 ) {
#endif
		free(tmp);
		return NULL;
	}

#ifndef GPAC_DISABLE_LOG
	if (name) {
		tmp->log_name = strdup(name);
	} else {
		char szN[20];
		sprintf(szN, "0x%08x", (u32) tmp);
		tmp->log_name = strdup(szN);
	}
#endif

	return tmp;
}

void gf_mx_del(GF_Mutex *mx)
{
#ifdef WIN32
	CloseHandle(mx->hMutex);
#else
	pthread_mutex_destroy(&mx->hMutex);
#endif
#ifndef GPAC_DISABLE_LOG
	free(mx->log_name);
#endif
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
#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Released by thread %s\n", mx->log_name, log_th_name(mx->Holder) ));
#endif
		mx->Holder = 0;
#ifdef WIN32
		ReleaseMutex(mx->hMutex);
#else
		pthread_mutex_unlock(&mx->hMutex);
#endif
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

#ifndef GPAC_DISABLE_LOG
	if (mx->Holder)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Thread %s waiting a release from thread %s\n", mx->log_name, log_th_name(caller), log_th_name(mx->Holder) ));
#endif

#ifdef WIN32
	switch (WaitForSingleObject(mx->hMutex, INFINITE)) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		return 0;
	default:
		break;
	}
#else
	if (pthread_mutex_lock(&mx->hMutex) != 0 ) {
		assert(0);
		return 0;
	}
#endif
	mx->HolderCount = 1;
	mx->Holder = caller;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Grabbed by thread %s\n", mx->log_name, log_th_name(mx->Holder) ));
	return 1;
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

#ifdef WIN32
	/*wait for 1 ms (I can't figure out from MS doc if 0 timeout only "tests the state" or also lock the mutex ... */
	switch (WaitForSingleObject(mx->hMutex, 1)) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Couldn't release it for thread %s (grabbed by thread %s)\n", mx->log_name, log_th_name(caller), log_th_name(mx->Holder) ));
		return 0;
	default:
		break;
	}
#else
	if (pthread_mutex_trylock(&mx->hMutex) != 0 ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Couldn't release it for thread %s (grabbed by thread %s)\n", mx->log_name, log_th_name(caller), log_th_name(mx->Holder) ));
		return 0;
	}
#endif
	mx->Holder = caller;
	mx->HolderCount = 1;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Mutex %s] Grabbed by thread %s\n", mx->log_name, log_th_name(mx->Holder) ));
	return 1;
}


/*********************************************************************
						OS-Specific Semaphore Object
**********************************************************************/
struct __tag_semaphore
{
#ifdef WIN32
	HANDLE hSemaphore;
#else
	sem_t *hSemaphore;
	sem_t SemaData;
#if defined(__DARWIN__) || defined(__APPLE__)
	const char *SemName;
#endif
#endif
};


GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount)
{
	GF_Semaphore *tmp = malloc(sizeof(GF_Semaphore));

	if (!tmp) return NULL;
#if defined(WIN32)
	tmp->hSemaphore = CreateSemaphore(NULL, InitCount, MaxCount, NULL);
#elif defined(__DARWIN__) || defined(__APPLE__)
	/* sem_init isn't supported on Mac OS X 10.3 & 10.4; it returns ENOSYS
	To get around this, a NAMED semaphore needs to be used
	sem_t *sem_open(const char *name, int oflag, ...);
	http://users.evitech.fi/~hannuvl/ke04/reaaliaika_ohj/memmap_named_semaphores.c
	*/
	{
		char semaName[40];
		sprintf(semaName,"GPAC_SEM%d", (u32) tmp);
		tmp->SemName = strdup(semaName);
	}
	tmp->hSemaphore = sem_open(tmp->SemName, O_CREAT, S_IRUSR|S_IWUSR, InitCount);
#else
	if (sem_init(&tmp->SemaData, 0, InitCount) < 0 ) {
		free(tmp);
		return NULL;
	}
	tmp->hSemaphore = &tmp->SemaData;
#endif

	if (!tmp->hSemaphore) {
		free(tmp);
		return NULL;
	}
	return tmp;
}

void gf_sema_del(GF_Semaphore *sm)
{
#if defined(WIN32)
	CloseHandle(sm->hSemaphore);
#elif defined(__DARWIN__) || defined(__APPLE__)
	sem_t *sema = sem_open(sm->SemName, 0);
	sem_destroy(sema);
	free(sm->SemName);
#else
	sem_destroy(sm->hSemaphore);
#endif
	free(sm);
}

u32 gf_sema_notify(GF_Semaphore *sm, u32 NbRelease)
{
	u32 prevCount;
#ifndef WIN32
	sem_t *hSem;
#endif

	if (!sm) return 0;

#if defined(WIN32)
	ReleaseSemaphore(sm->hSemaphore, NbRelease, (LPLONG) &prevCount);
#else

#if defined(__DARWIN__) || defined(__APPLE__)
	hSem = sem_open(sm->SemName, 0);
#else
	hSem = sm->hSemaphore;
#endif
	sem_getvalue(hSem, (s32 *) &prevCount);
	while (NbRelease) {
		if (sem_post(hSem) < 0) return 0;
		NbRelease -= 1;
	}
#endif
	return (u32) prevCount;
}

void gf_sema_wait(GF_Semaphore *sm)
{
#ifdef WIN32
	WaitForSingleObject(sm->hSemaphore, INFINITE);
#else
	sem_t *hSem;
#if defined(__DARWIN__) || defined(__APPLE__)
	hSem = sem_open(sm->SemName, 0);
#else
	hSem = sm->hSemaphore;
#endif
	sem_wait(hSem);
#endif
}

Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut)
{
#ifdef WIN32
	if (WaitForSingleObject(sm->hSemaphore, TimeOut) == WAIT_TIMEOUT) return 0;
	return 1;
#else
	sem_t *hSem;
#if defined(__DARWIN__) || defined(__APPLE__)
	hSem = sem_open(sm->SemName, 0);
#else
	hSem = sm->hSemaphore;
#endif
	if (!TimeOut) {
		if (!sem_trywait(hSem)) return 1;
		return 0;
	}
	TimeOut += gf_sys_clock();
	do {
		if (!sem_trywait(hSem)) return 1;
		gf_sleep(1);	
	} while (gf_sys_clock() < TimeOut);
	return 0;
#endif
}

