/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifdef GPAC_CONFIG_ANDROID
#include <jni.h>
#endif

#include <gpac/thread.h>

#ifndef GPAC_DISABLE_THREADS


#if defined(WIN32) || defined(_WIN32_WCE)

/*win32 threads*/
#include <windows.h>
typedef HANDLE TH_HANDLE;

#else
/*pthreads*/
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
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
	Bool blocking;

	Bool no_kill;

#ifndef GPAC_DISABLE_LOG
	u32 id;
	char *log_name;
#endif
#ifdef GPAC_CONFIG_ANDROID
	u32 (*RunBeforeExit)(void *param);
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
		GF_Thread *t = (GF_Thread*)gf_list_get(thread_bank, i);
		if (t->id == id) return t->log_name;
	}
	return "Main Process";
}

#endif


GF_EXPORT
GF_Thread *gf_th_new(const char *name)
{
	if (gf_opts_get_bool("core", "no-mx")) return NULL;

	GF_Thread *tmp = (GF_Thread*)gf_malloc(sizeof(GF_Thread));
	memset(tmp, 0, sizeof(GF_Thread));
	tmp->status = GF_THREAD_STATUS_STOP;
#ifdef GPAC_CONFIG_EMSCRIPTEN
	tmp->blocking = GF_FALSE;
#else
	tmp->blocking = GF_TRUE;
#endif

#ifndef GPAC_DISABLE_LOG
	if (name) {
		tmp->log_name = gf_strdup(name);
	} else {
		char szN[20];
		sprintf(szN, "%p", (void*)tmp);
		tmp->log_name = gf_strdup(szN);
	}
	log_add_thread(tmp);
#endif
	return tmp;
}


#ifdef GPAC_CONFIG_ANDROID
#include <pthread.h>

static pthread_key_t currentThreadInfoKey = 0;

/* Unique allocation of key */
static pthread_once_t currentThreadInfoKey_once = PTHREAD_ONCE_INIT;

GF_Err gf_register_before_exit_function(GF_Thread *t, u32 (*toRunBeforePthreadExit)(void *param) )
{
	if (!t)
		return GF_BAD_PARAM;
	t->RunBeforeExit = toRunBeforePthreadExit;
	return GF_OK;
}

/* Unique key allocation */
static void currentThreadInfoKey_alloc()
{
	int err;
	/* We do not use any destructor */
	err = pthread_key_create(&currentThreadInfoKey, NULL);
	if (err) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] pthread_key_create() failed with error %d\n", err));
	}
}

GF_Thread * gf_th_current() {
	return pthread_getspecific(currentThreadInfoKey);
}

#endif /* GPAC_CONFIG_ANDROID */


#ifdef WIN32
DWORD WINAPI RunThread(void *ptr)
{
	DWORD ret = 0;
#else
void * RunThread(void *ptr)
{
	long int ret = 0;
#endif
	GF_Thread *t = (GF_Thread *)ptr;


//not activated by default, not portable
#if 0 && !defined(GPAC_DISABLE_LOG)
	if (t->log_name) {
#ifdef WIN32
#elif defined(__DARWIN__) || defined(__APPLE__)
		pthread_setname_np(t->log_name);
#else
		pthread_setname_np(t->threadH, t->log_name);
#endif
	}
#endif

	/* Signal the caller */
	if (t->blocking && !t->_signal) goto exit;

#ifdef GPAC_CONFIG_ANDROID
	if (pthread_once(&currentThreadInfoKey_once, &currentThreadInfoKey_alloc) || pthread_setspecific(currentThreadInfoKey, t))
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't run thread %s, ID 0x%08x\n", t->log_name, t->id));
#endif /* GPAC_CONFIG_ANDROID */
	t->status = GF_THREAD_STATUS_RUN;

	if (t->blocking)
		gf_sema_notify(t->_signal, 1);

#ifndef GPAC_DISABLE_LOG
	t->id = gf_th_id();
	GF_LOG(GF_LOG_INFO, GF_LOG_MUTEX, ("[Thread %s] At %d Entering thread proc - thread ID 0x%08x\n", t->log_name, gf_sys_clock(), t->id));
#endif

	/* Each thread has its own seed */
	gf_rand_init(GF_FALSE);

	/* Run our thread */
	ret = t->Run(t->args);

exit:

	if (t->no_kill) {
		t->no_kill = 0;
		return (void *)ret;
	}
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_INFO, GF_LOG_MUTEX, ("[Thread %s] At %d Exiting thread proc, return code %d\n", t->log_name, gf_sys_clock(), ret));
#endif
	t->status = GF_THREAD_STATUS_DEAD;
	t->Run = NULL;
#ifdef WIN32
	if (!CloseHandle(t->threadH)) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Thread %s] Couldn't close handle when exiting thread proc, error code: %d\n", t->log_name, err));
	}
	t->threadH = NULL;
	return ret;
#else

#ifdef GPAC_CONFIG_ANDROID
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_INFO, GF_LOG_MUTEX, ("[Thread %s] RunBeforeExit=%p\n", t->log_name, t->RunBeforeExit));
#endif
	if (t->RunBeforeExit)
		t->RunBeforeExit(t->args);
#endif /* GPAC_CONFIG_ANDROID */
	pthread_exit((void *)0);
	return (void *)ret;
#endif
}

GF_EXPORT
GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
#ifdef WIN32
	DWORD id;
#else
	pthread_attr_t att;
#endif
	if (!t || t->Run || t->_signal) {
		return GF_BAD_PARAM;
	}
	t->Run = Run;
	t->args = param;
	if (t->blocking) {
		t->_signal = gf_sema_new(1, 0);
		if (!t->_signal) return GF_IO_ERR;
	}


	GF_LOG(GF_LOG_INFO, GF_LOG_MUTEX, ("[Thread %s] Starting\n", t->log_name));

#ifdef WIN32
	t->threadH = CreateThread(NULL, t->stackSize, &(RunThread), (void *)t, 0, &id);
	if (t->threadH != NULL) {
#ifdef _MSC_VER
		/*add thread name for the msvc debugger*/
#pragma pack(push,8)
		typedef struct {
			DWORD dwType;
			LPCSTR szName;
			DWORD dwThreadID;
			DWORD dwFlags;
		} THREADNAME_INFO;
#pragma pack(pop)
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = t->log_name;
		info.dwThreadID = id;
		info.dwFlags = 0;
		__try {
			RaiseException(0x406D1388, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
		} __except (EXCEPTION_CONTINUE_EXECUTION) {
		}
#endif
	} else {
#else
	if ( pthread_attr_init(&att) != 0 ) return GF_IO_ERR;
	pthread_attr_setdetachstate(&att, PTHREAD_CREATE_JOINABLE);
	if ( pthread_create(&t->threadH, &att, RunThread, t) != 0 ) {
#endif
		t->status = GF_THREAD_STATUS_DEAD;
		return GF_IO_ERR;
	}

	if (t->blocking) {
		/*wait for the child function to call us - do NOT return before, otherwise the thread status would be unknown*/
		gf_sema_wait(t->_signal);
		gf_sema_del(t->_signal);
		t->_signal = NULL;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MUTEX, ("[Thread %s] Started\n", t->log_name));
	return GF_OK;
}

#if defined(GPAC_CONFIG_EMSCRIPTEN) && !defined(GPAC_DISABLE_THREADS)
#include <emscripten/threading.h>

GF_Err gf_th_async_call(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
	t->no_kill = 1;
	emscripten_dispatch_to_thread_async_(t->threadH, EM_FUNC_SIG_II, Run, NULL, param);
	return GF_OK;
}
#endif


/* Stops a thread. If Destroy is not 0, thread is destroyed DANGEROUS as no cleanup */
void Thread_Stop(GF_Thread *t, Bool Destroy)
{
	if (gf_th_status(t) == GF_THREAD_STATUS_RUN) {
#ifdef WIN32
		if (Destroy) {
			DWORD dw = 1;
			BOOL ret = TerminateThread(t->threadH, dw);
			if (!ret) {
				DWORD err = GetLastError();
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Thread %s] Couldn't stop thread ID 0x%08x, error code %d\n", t->log_name, t->id, err));
			}
			t->threadH = NULL;
		} else {
			WaitForSingleObject(t->threadH, INFINITE);
		}
#else
		if (Destroy) {
#if defined(GPAC_CONFIG_ANDROID) || defined(PTHREAD_HAS_NO_CANCEL)
			if (pthread_kill(t->threadH, SIGQUIT))
#else
			if (pthread_cancel(t->threadH))
#endif
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Thread %s] Couldn't kill thread ID 0x%08x\n", t->log_name, t->id));
			t->threadH = 0;
		} else {
			/*gracefully wait for Run to finish*/
#ifdef GPAC_CONFIG_EMSCRIPTEN
			//only destroy if in main application thread (otherwise emscripten assert on cleanupThread()  fails)
			if (emscripten_is_main_runtime_thread()) {
				int pthread_tryjoin_np(pthread_t, void **);
				void *rval=NULL;
				if (pthread_tryjoin_np(t->threadH, &rval))
					GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Thread %s] pthread_join() returned an error with thread ID 0x%08x\n", t->log_name, t->id));
			}
#else
			if (pthread_join(t->threadH, NULL))
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Thread %s] pthread_join() returned an error with thread ID 0x%08x\n", t->log_name, t->id));
#endif

		}
#endif
	}
	t->status = GF_THREAD_STATUS_DEAD;
}

GF_EXPORT
void gf_th_stop(GF_Thread *t)
{
	if (t) Thread_Stop(t, GF_FALSE);
}

GF_EXPORT
void gf_th_del(GF_Thread *t)
{
	Thread_Stop(t, GF_FALSE);
#ifdef WIN32
//	if (t->threadH) CloseHandle(t->threadH);
#else
	/* It is necessary to free pthread handle */
	if (t->threadH)
		pthread_detach(t->threadH);
	t->threadH = 0;
#endif

#ifndef GPAC_DISABLE_LOG
	gf_free(t->log_name);
	log_del_thread(t);
#endif
	gf_free(t);
}

GF_EXPORT
void gf_th_set_priority(GF_Thread *t, s32 priority)
{
#ifdef WIN32
	/*!! in WinCE, changin thread priority is extremely dangerous, it may freeze threads randomly !!*/
#ifndef _WIN32_WCE
	int _prio;
	BOOL ret;
	switch (priority) {
	case GF_THREAD_PRIORITY_IDLE:
		_prio = THREAD_PRIORITY_IDLE;
		break;
	case GF_THREAD_PRIORITY_LESS_IDLE:
		_prio = THREAD_PRIORITY_IDLE;
		break;
	case GF_THREAD_PRIORITY_LOWEST:
		_prio = THREAD_PRIORITY_LOWEST;
		break;
	case GF_THREAD_PRIORITY_LOW:
		_prio = THREAD_PRIORITY_BELOW_NORMAL;
		break;
	case GF_THREAD_PRIORITY_NORMAL:
		_prio = THREAD_PRIORITY_NORMAL;
		break;
	case GF_THREAD_PRIORITY_HIGH:
		_prio = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
	case GF_THREAD_PRIORITY_HIGHEST:
		_prio = THREAD_PRIORITY_HIGHEST;
		break;
	default: /*GF_THREAD_PRIORITY_REALTIME -> GF_THREAD_PRIORITY_REALTIME_END*/
		_prio = THREAD_PRIORITY_TIME_CRITICAL;
		break;
	}
	ret = SetThreadPriority(t ? t->threadH : GetCurrentThread(), _prio);
	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_WARNING, GF_LOG_MUTEX, ("[Thread %s] Couldn't set priority for thread ID 0x%08x, error %d\n", t->log_name, t->id, err));
	}
#endif

#elif !defined(GPAC_CONFIG_EMSCRIPTEN)

	struct sched_param s_par;
	if (!t) return;

	/* consider this as real-time priority */
	if (priority > 200) {
		s_par.sched_priority = priority - 200;
		if (pthread_setschedparam(t->threadH, SCHED_RR, &s_par))
			GF_LOG(GF_LOG_WARNING, GF_LOG_MUTEX, ("[Thread %s] Couldn't set priority(1) for thread ID 0x%08x\n", t->log_name, t->id));
	} else {
		s_par.sched_priority = priority;
		if (pthread_setschedparam(t->threadH, SCHED_OTHER, &s_par))
			GF_LOG(GF_LOG_WARNING, GF_LOG_MUTEX, ("[Thread %s] Couldn't set priority(2) for thread ID 0x%08x\n", t->log_name, t->id));
	}

#endif
}

GF_EXPORT
u32 gf_th_status(GF_Thread *t)
{
	if (!t) return 0;
	return t->status;
}


GF_EXPORT
u32 gf_th_id()
{
#ifdef WIN32
	return ((u32) GetCurrentThreadId());
#else
	return ((u32) (PTR_TO_U_CAST(pthread_self())));
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


GF_EXPORT
GF_Mutex *gf_mx_new(const char *name)
{
	if (gf_opts_get_bool("core", "no-mx")) return NULL;

#ifndef WIN32
	pthread_mutexattr_t attr;
#endif
	GF_Mutex *tmp = (GF_Mutex*)gf_malloc(sizeof(GF_Mutex));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Mutex));

#ifdef WIN32
	tmp->hMutex = CreateMutex(NULL, FALSE, NULL);
	if (!tmp->hMutex) {
#else
	pthread_mutexattr_init(&attr);
	if ( pthread_mutex_init(&tmp->hMutex, &attr) != 0 ) {
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't create mutex %s\n", strlen(name) ? name : ""));
		gf_free(tmp);
		return NULL;
	}

#ifndef GPAC_DISABLE_LOG
	if (name) {
		if (stricmp(name, "Logs")) {
			tmp->log_name = gf_strdup(name);
		}
	} else {
		char szN[20];
		sprintf(szN, "%p", (void*)tmp);
		tmp->log_name = gf_strdup(szN);
	}
#endif

	return tmp;
}

GF_EXPORT
void gf_mx_del(GF_Mutex *mx)
{
#ifndef WIN32
	int err;
#endif
	if (!mx) return;

#ifndef GPAC_DISABLE_LOG
	if (mx->Holder && (gf_th_id() != mx->Holder) && mx->log_name) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MUTEX, ("[Mutex %s] Destroying mutex from thread %s but hold by thread %s\n", mx->log_name, log_th_name(gf_th_id() ), log_th_name(mx->Holder) ));
	}
#endif

#ifdef WIN32
	if (!CloseHandle(mx->hMutex)) {
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			DWORD err = GetLastError();
			GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex %s] CloseHandle when deleting mutex failed with error code %d\n", mx->log_name, err));
		}
#endif
	}
#else
	err = pthread_mutex_destroy(&mx->hMutex);
	if (err) {
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex %s] pthread_mutex_destroy failed with error code %d\n", mx->log_name, err));
		}
#endif
	}

#endif
#ifndef GPAC_DISABLE_LOG
	if (mx->log_name) {
		gf_free(mx->log_name);
		mx->log_name = NULL;
	}
#endif
	gf_free(mx);
}

GF_EXPORT
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
		if (mx->log_name) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Released by thread %s\n", mx->log_name, gf_sys_clock(), log_th_name(mx->Holder) ));
		}
#endif
		mx->Holder = 0;
#ifdef WIN32
		{
			BOOL ret = ReleaseMutex(mx->hMutex);
			if (!ret) {
#ifndef GPAC_DISABLE_LOG
				if (mx->log_name) {
					DWORD err = GetLastError();
					GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't release mutex (thread %s, error %d)\n", log_th_name(mx->Holder), err));
				}
#endif

			}
		}
#else
		if (pthread_mutex_unlock(&mx->hMutex)) {
#ifndef GPAC_DISABLE_LOG
			if (mx->log_name) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't release mutex (thread %s)\n", log_th_name(mx->Holder)));
			}
#endif
		}
#endif
	}
}

GF_EXPORT
u32 gf_mx_p(GF_Mutex *mx)
{
#ifndef WIN32
	int retCode;
#endif
	u32 caller;
#ifndef GPAC_DISABLE_LOG
	const char *mx_holder_name;
#endif

	if (!mx) return 1;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return 1;
	}

#ifndef GPAC_DISABLE_LOG
	mx_holder_name = mx->Holder ? log_th_name(mx->Holder) : "none";
	if (mx->Holder && mx->log_name)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Thread %s waiting a release from thread %s\n", mx->log_name, gf_sys_clock(), log_th_name(caller), mx_holder_name ));
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
	retCode = pthread_mutex_lock(&mx->hMutex);
	if (retCode != 0 ) {
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			if (retCode == EINVAL)
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex %p=%s] Not properly initialized.\n", mx, mx->log_name));
			if (retCode == EDEADLK)
				GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex %p=%s] Deadlock detected.\n", mx, mx->log_name));
		}
#endif /* GPAC_DISABLE_LOG */
		assert(0);
		return 0;
	}
#endif /* NOT WIN32 */
	mx->HolderCount = 1;
	mx->Holder = caller;
#ifndef GPAC_DISABLE_LOG
	if (mx->log_name) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Grabbed by thread %s\n", mx->log_name, gf_sys_clock(), log_th_name(mx->Holder) ));
	}
#endif
	return 1;
}


GF_EXPORT
s32 gf_mx_get_num_locks(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return 0;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		return mx->HolderCount;
	}
	return -1;
}

GF_EXPORT
Bool gf_mx_try_lock(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return GF_TRUE;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return GF_TRUE;
	}

#ifdef WIN32
	/*is the object signaled?*/
	switch (WaitForSingleObject(mx->hMutex, 0)) {
	case WAIT_OBJECT_0:
		break;
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Couldn't be locked by thread %s (grabbed by thread %s)\n", mx->log_name, gf_sys_clock(), log_th_name(caller), log_th_name(mx->Holder) ));
		}
#endif
		return GF_FALSE;
	case WAIT_FAILED:
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex %s] At %d WaitForSingleObject failed\n", mx->log_name, gf_sys_clock()));
			return GF_FALSE;
		}
#endif
	default:
		assert(0);
		return GF_FALSE;
	}
#else
	if (pthread_mutex_trylock(&mx->hMutex) != 0 ) {
#ifndef GPAC_DISABLE_LOG
		if (mx->log_name) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Couldn't release it for thread %s (grabbed by thread %s)\n", mx->log_name, gf_sys_clock(), log_th_name(caller), log_th_name(mx->Holder) ));
		}
#endif
		return GF_FALSE;
	}
#endif
	mx->Holder = caller;
	mx->HolderCount = 1;
#ifndef GPAC_DISABLE_LOG
	if (mx->log_name) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MUTEX, ("[Mutex %s] At %d Grabbed by thread %s\n", mx->log_name, gf_sys_clock(), log_th_name(mx->Holder) ));
	}
#endif
	return GF_TRUE;
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
	char *SemName;
#endif
#endif
};

GF_EXPORT
GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount)
{
	if (gf_opts_get_bool("core", "no-mx")) return NULL;

	GF_Semaphore *tmp = (GF_Semaphore*)gf_malloc(sizeof(GF_Semaphore));
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("Couldn't allocate semaphore\n"));
		return NULL;
	}

#if defined(WIN32)
	MaxCount = MIN(MaxCount, GF_INT_MAX);
	tmp->hSemaphore = CreateSemaphore(NULL, InitCount, MIN(MaxCount, GF_INT_MAX), NULL);
	if (!tmp->hSemaphore) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("Couldn't create semaphore\n"));
		gf_free(tmp);
		return NULL;
	}

#elif defined(__DARWIN__) || defined(__APPLE__)
	/* sem_init isn't supported on Mac OS X 10.3 & 10.4; it returns ENOSYS
	To get around this, a NAMED semaphore needs to be used
	sem_t *sem_open(const char *name, int oflag, ...);
	http://users.evitech.fi/~hannuvl/ke04/reaaliaika_ohj/memmap_named_semaphores.c
	*/
	{
		char semaName[40];
		u64 add = (u64) tmp ^ gf_net_get_utc() ^ gf_rand();
		sprintf(semaName,"GPAC_SEM"LLU, add);
		tmp->SemName = gf_strdup(semaName);
	}
	sem_unlink(tmp->SemName);
	tmp->hSemaphore = sem_open(tmp->SemName, O_CREAT, S_IRUSR|S_IWUSR, InitCount);
	if (tmp->hSemaphore==SEM_FAILED) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("Couldn't init semaphore: error %d\n", errno));
		gf_free(tmp);
		return NULL;
	}
#else
	if (sem_init(&tmp->SemaData, 0, InitCount) < 0 ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("Couldn't init semaphore: error %d\n", errno));
		gf_free(tmp);
		return NULL;
	}
	tmp->hSemaphore = &tmp->SemaData;
#endif
	return tmp;
}

GF_EXPORT
void gf_sema_del(GF_Semaphore *sm)
{
	if (!sm) return;
#if defined(WIN32)
	if (!CloseHandle(sm->hSemaphore)) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] CloseHandle when deleting semaphore failed with error code %d\n", err));
	}
#elif defined(__DARWIN__) || defined(__APPLE__)
	sem_close(sm->hSemaphore);
	sem_unlink(sm->SemName);
	gf_free(sm->SemName);
#else
	sem_destroy(sm->hSemaphore);
#endif
	gf_free(sm);
}

GF_EXPORT
Bool gf_sema_notify(GF_Semaphore *sm, u32 NbRelease)
{
#ifndef WIN32
	sem_t *hSem;
#else
	u32 prevCount=0;
#endif

	if (!sm) return GF_FALSE;

#if defined(WIN32)
	ReleaseSemaphore(sm->hSemaphore, NbRelease, (LPLONG) &prevCount);
#else

	hSem = sm->hSemaphore;

	while (NbRelease) {
		if (sem_post(hSem) < 0) return GF_FALSE;
		NbRelease -= 1;
	}
#endif
	return GF_TRUE;
}

GF_EXPORT
Bool gf_sema_wait(GF_Semaphore *sm)
{
	if (!sm) return GF_FALSE;
#ifdef WIN32
	switch (WaitForSingleObject(sm->hSemaphore, INFINITE) ) {
	case WAIT_FAILED:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Semaphore] failed to wait: %d\n", GetLastError() ));
		return GF_FALSE;
	case WAIT_ABANDONED:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Semaphore] failed to wait: owner thread exit\n"));
		return GF_FALSE;
	default:
		break;
	}

#else
	if (sem_wait(sm->hSemaphore) < 0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Semaphore] failed to wait for semaphore: %d\n", errno));
		return GF_FALSE;
	}
#endif
	return GF_TRUE;
}

GF_EXPORT
Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut)
{
#ifdef WIN32
	if (!sm) return GF_FALSE;
	if (WaitForSingleObject(sm->hSemaphore, TimeOut) == WAIT_TIMEOUT) return GF_FALSE;
	return GF_TRUE;
#else
	sem_t *hSem;
	if (!sm) return GF_FALSE;
	hSem = sm->hSemaphore;

	if (!TimeOut) {
		if (!sem_trywait(hSem)) return GF_TRUE;
		return GF_FALSE;
	}

#if defined(__DARWIN__) || defined(__APPLE__) || defined(GPAC_CONFIG_IOS) || defined(GPAC_CONFIG_EMSCRIPTEN)

	TimeOut += gf_sys_clock();
	do {
		if (!sem_trywait(hSem)) return GF_TRUE;
		//OSX/ios don't support sem_timedwait, so we sleep until the sem is notified or the timeout is done
		//don't be too greedy, use 5ms sleep
		//another approach would be to spawn a thread, use sem_wait and send an interrupt on the sema after the timeout ...
		gf_sleep(1);
	} while (gf_sys_clock() < TimeOut);
	return GF_FALSE;
#else
	struct timespec tv;
	u32 secs;
	if (clock_gettime(CLOCK_REALTIME, &tv) == -1) return GF_FALSE;
	secs = TimeOut/1000;
	tv.tv_sec += secs;
	tv.tv_nsec += (u64) ((TimeOut-secs*1000)) * 1000000;
	if (tv.tv_nsec>1000000000) {
		tv.tv_nsec -= 1000000000;
		tv.tv_sec += 1;
	}
	if (!sem_timedwait(hSem, &tv)) return GF_TRUE;

	return GF_FALSE;
#endif

#endif
}


#endif //GPAC_DISABLE_THREADS
