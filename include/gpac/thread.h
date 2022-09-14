/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2021
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

/*!
\file <gpac/thread.h>
\brief Threading and Mutual Exclusion
*/

/*!
\addtogroup thr_grp
\brief Threading and Mutual Exclusion

This section documents the threading of the GPAC framework. These provide an easy way to implement
safe multithreaded tools.

Available tools are thread, mutex and semaphore

\defgroup thread_grp Thread
\ingroup thr_grp
\defgroup mutex_grp Mutex
\ingroup thr_grp
\defgroup sema_grp Semaphore
\ingroup thr_grp
*/

/*!
\addtogroup thread_grp
\brief Thread processing

The thread object allows executing some code independently of the main process of your application.
@{
*/

#include <gpac/tools.h>


 //atomic ref_count++ / ref_count--
#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#include <windows.h>
#include <winbase.h>


#ifdef GPAC_BUILD_FOR_WINXP

/* Addendum by M. Lackner: Wrap InterlockedExchangeAdd64() around
   InterlockedCompareExchange64() and name it specifically as an XP
   function for 32-bit builds only */
#ifndef GPAC_64_BITS
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64xp _InterlockedCompareExchange64
static inline LONGLONG InterlockedExchangeAdd64xp(_Inout_ LONGLONG volatile *Addend, _In_ LONGLONG Value)
{
	LONGLONG Old;
	do {
		Old = *Addend;
	} while (InterlockedCompareExchange64xp(Addend, Old + Value, Old) != Old);
	return Old;
}
#endif
/* End of addendum */


/*! atomic integer increment */
#define safe_int_inc(__v) InterlockedIncrement((int *) (__v))
/*! atomic integer decrement */
#define safe_int_dec(__v) InterlockedDecrement((int *) (__v))
/*! atomic integer addition */

/* Modified by M. Lackner for XP & XP x64 compatibility */
/* InterlockedAdd and InterlockedAdd64 do not exist on Win XP, so we use
the older InterlockedExchangeAdd and a newly written InterlockedExchangeAdd64xp
static inline function.
See:
https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedexchangeadd
https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedexchangeadd64
*/

#define safe_int_add(__v, inc_val) InterlockedExchangeAdd((int *) (__v), inc_val)
#define safe_int_sub(__v, dec_val) InterlockedExchangeAdd((int *) (__v), -dec_val)
#ifdef GPAC_64_BITS
#define safe_int64_add(__v, inc_val) InterlockedExchangeAdd64((LONGLONG *) (__v), inc_val)
#define safe_int64_sub(__v, dec_val) InterlockedExchangeAdd64((LONGLONG *) (__v), -dec_val)
#else
#define safe_int64_add(__v, inc_val) InterlockedExchangeAdd64xp((LONGLONG *) (__v), inc_val)
#define safe_int64_sub(__v, dec_val) InterlockedExchangeAdd64xp((LONGLONG *) (__v), -dec_val)
#endif
/* End of modification by M. Lackner */

#else //not winxp

/*! atomic integer increment */
#define safe_int_inc(__v) InterlockedIncrement((int *) (__v))
/*! atomic integer decrement */
#define safe_int_dec(__v) InterlockedDecrement((int *) (__v))
/*! atomic integer addition */
#define safe_int_add(__v, inc_val) InterlockedAdd((int *) (__v), inc_val)
/*! atomic integer subtraction */
#define safe_int_sub(__v, dec_val) InterlockedAdd((int *) (__v), -dec_val)
/*! atomic large integer addition */
#define safe_int64_add(__v, inc_val) InterlockedAdd64((LONG64 *) (__v), inc_val)
/*! atomic large integer subtraction */
#define safe_int64_sub(__v, dec_val) InterlockedAdd64((LONG64 *) (__v), -dec_val)

#endif //winxp

#else //not windows

#ifdef GPAC_NEED_LIBATOMIC

/*! atomic integer increment */
#define safe_int_inc(__v) __atomic_add_fetch((int *) (__v), 1, __ATOMIC_SEQ_CST)
/*! atomic integer decrement */
#define safe_int_dec(__v) __atomic_sub_fetch((int *) (__v), 1, __ATOMIC_SEQ_CST)
/*! atomic integer addition */
#define safe_int_add(__v, inc_val) __atomic_add_fetch((int *) (__v), inc_val, __ATOMIC_SEQ_CST)
/*! atomic integer subtraction */
#define safe_int_sub(__v, dec_val) __atomic_sub_fetch((int *) (__v), dec_val, __ATOMIC_SEQ_CST)
/*! atomic large integer addition */
#define safe_int64_add(__v, inc_val) __atomic_add_fetch((int64_t *) (__v), inc_val, __ATOMIC_SEQ_CST)
/*! atomic large integer subtraction */
#define safe_int64_sub(__v, dec_val) __atomic_sub_fetch((int64_t *) (__v), dec_val, __ATOMIC_SEQ_CST)

#else

/*! atomic integer increment */
#define safe_int_inc(__v) __sync_add_and_fetch((int *) (__v), 1)
/*! atomic integer decrement */
#define safe_int_dec(__v) __sync_sub_and_fetch((int *) (__v), 1)
/*! atomic integer addition */
#define safe_int_add(__v, inc_val) __sync_add_and_fetch((int *) (__v), inc_val)
/*! atomic integer subtraction */
#define safe_int_sub(__v, dec_val) __sync_sub_and_fetch((int *) (__v), dec_val)
/*! atomic large integer addition */
#define safe_int64_add(__v, inc_val) __sync_add_and_fetch((int64_t *) (__v), inc_val)
/*! atomic large integer subtraction */
#define safe_int64_sub(__v, dec_val) __sync_sub_and_fetch((int64_t *) (__v), dec_val)

#endif //GPAC_NEED_LIBATOMIC

#endif


/*!
\brief Thread states
 *
 *Indicates the execution status of a thread
 */
enum
{
	/*! the thread has been initialized but is not started yet*/
	GF_THREAD_STATUS_STOP = 0,
	/*! the thread is running*/
	GF_THREAD_STATUS_RUN = 1,
	/*! the thread has exited its body function*/
	GF_THREAD_STATUS_DEAD = 2
};

/*!
\brief abstracted thread object
*/
typedef struct __tag_thread GF_Thread;

/*!
\brief thread constructor

Constructs a new thread object
\param name log name of the thread if any
\return new thread object
 */
GF_Thread *gf_th_new(const char *name);
/*!
\brief thread destructor

Kills the thread if running and destroys the object
\param th the thread object
 */
void gf_th_del(GF_Thread *th);

/*!
\brief thread run function callback

The gf_thread_run type is the type for the callback of the \ref gf_thread_run function
\param par opaque user data
\return exit code of the thread, usually 1 for error and 0 if normal execution
 */
typedef u32 (*gf_thread_run)(void *par);

/*!
\brief thread execution

Executes the thread with the given function
\note A thread may be run several times but cannot be run twice in the same time.
\param th the thread object
\param run the function this thread will call
\param par the argument to the function the thread will call
\return error if any
 */
GF_Err gf_th_run(GF_Thread *th, gf_thread_run run, void *par);
/*!
\brief thread stopping

Waits for the thread exit until return
\param th the thread object
 */
void gf_th_stop(GF_Thread *th);
/*!
\brief thread status query

Gets the thread status
\param th the thread object
\return thread status
 */
u32 gf_th_status(GF_Thread *th);

/*! thread priorities */
enum
{
	/*!Idle Priority*/
	GF_THREAD_PRIORITY_IDLE=0,
	/*!Less Idle Priority*/
	GF_THREAD_PRIORITY_LESS_IDLE,
	/*!Lowest Priority*/
	GF_THREAD_PRIORITY_LOWEST,
	/*!Low Priority*/
	GF_THREAD_PRIORITY_LOW,
	/*!Normal Priority (the default one)*/
	GF_THREAD_PRIORITY_NORMAL,
	/*!High Priority*/
	GF_THREAD_PRIORITY_HIGH,
	/*!Highest Priority*/
	GF_THREAD_PRIORITY_HIGHEST,
	/*!First real-time priority*/
	GF_THREAD_PRIORITY_REALTIME,
	/*!Last real-time priority*/
	GF_THREAD_PRIORITY_REALTIME_END=255
};

/*!
\brief thread priority

Sets the thread execution priority level.
\param th the thread object
\param priority the desired priority
\note this should be used with caution, especially use of real-time priorities.
 */
void gf_th_set_priority(GF_Thread *th, s32 priority);
/*!
\brief current thread ID

Gets the ID of the current thread the caller is in.
\return thread ID
*/
u32 gf_th_id();

#ifdef GPAC_CONFIG_ANDROID
/*! Register a function that will be called before pthread_exist is called */
GF_Err gf_register_before_exit_function(GF_Thread *t, u32 (*toRunBeforePthreadExit)(void *param));

/*! Get the current Thread if any. May return NULL
 */
GF_Thread * gf_th_current();

#endif /* GPAC_CONFIG_ANDROID */

/*! @} */

/*!
\addtogroup mutex_grp
\brief Mutual exclusion

The mutex object allows ensuring that portions of the code (typically access to variables) cannot be executed by two threads (or a thread and the main process) at the same time.

@{
*/

/*!
\brief abstracted mutex object*/
typedef struct __tag_mutex GF_Mutex;
/*!
\brief mutex constructor

Contructs a new mutex object
\param name log name of the thread if any
\return new mutex
*/
GF_Mutex *gf_mx_new(const char *name);
/*!
\brief mutex denstructor

Destroys a mutex object. This will wait for the mutex to be released if needed.
\param mx the mutex object, may be NULL
*/
void gf_mx_del(GF_Mutex *mx);
/*!
\brief mutex locking

Locks the mutex object, making sure that another thread locking this mutex cannot execute until the mutex is unlocked.
\param mx the mutex object, may be NULL
\return 1 if success or mutex is NULL, 0 if error locking the mutex (which should never happen)
*/
u32 gf_mx_p(GF_Mutex *mx);
/*!
\brief mutex unlocking

Unlocks the mutex object, allowing other threads waiting on this mutex to continue their execution
\param mx the mutex object, may be NULL
*/
void gf_mx_v(GF_Mutex *mx);
/*!
\brief mutex non-blocking lock

Attemps to lock the mutex object without blocking until the object is released.
\param mx the mutex object, may be NULL
\return GF_TRUE if the mutex has been successfully locked or if the mutex is NULL, in which case it shall then be unlocked, or GF_FALSE if the mutex is locked by another thread.
*/
Bool gf_mx_try_lock(GF_Mutex *mx);

/*!
\brief get mutex number of locks

Returns the number of locks on the mutex if the caller thread is holding the mutex.
\param mx the mutex object, may be NULL
\return -1 if the mutex is not hold by the calling thread, or the number of locks (possibly 0) otherwise. Returns 0 if mutex is NULL
 */
s32 gf_mx_get_num_locks(GF_Mutex *mx);

/*! @} */

/*!
\addtogroup sema_grp
\brief Semaphore


The semaphore object allows controlling how portions of the code (typically access to variables) are
 executed by two threads (or a thread and the main process) at the same time. The best image for a semaphore is a limited set
 of money coins (always easy to understand hmm?). If no money is in the set, nobody can buy anything until a coin is put back in the set.
 When the set is full, the money is wasted (call it "the bank"...).

@{
*/

/*********************************************************************
					Semaphore Object
**********************************************************************/
/*!
\brief abstracted semaphore object
*/
typedef struct __tag_semaphore GF_Semaphore;
/*!
\brief semaphore constructor

Constructs a new semaphore object
\param MaxCount the maximum notification count of this semaphore
\param InitCount the initial notification count of this semaphore upon construction
\return the semaphore object
 */
GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount);
/*!
\brief semaphore destructor

Destructs the semaphore object. This will wait for the semaphore to be released if needed.
\param sm the semaphore object
 */
void gf_sema_del(GF_Semaphore *sm);
/*!
\brief semaphore notification.

Notifies the semaphore of a certain amount of releases.
\param sm the semaphore object
\param nb_rel sm the number of release to notify
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_sema_notify(GF_Semaphore *sm, u32 nb_rel);
/*!
\brief semaphore wait

Waits for the semaphore to be accessible (eg, may wait an infinite time).
\param sm the semaphore object
\return GF_TRUE if successfull wait, GF_FALSE if wait failed
*/
Bool gf_sema_wait(GF_Semaphore *sm);
/*!
\brief semaphore time wait

Waits for a certain for the semaphore to be accessible, and returns when semaphore is accessible or wait time has passed.
\param sm the semaphore object
\param time_out the amount of time to wait for the release in milliseconds
\return returns 1 if the semaphore was released before the timeout, 0 if the semaphore is still not released after the timeout.
*/
Bool gf_sema_wait_for(GF_Semaphore *sm, u32 time_out);


/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_THREAD_H_*/
