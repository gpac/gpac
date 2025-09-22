/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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

#include <gpac/tools.h>
#include <gpac/network.h>

#ifdef __SYMBIAN32__

#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/times.h>
#include <sys/resource.h>
/*symbian stdlib*/
#include <e32std.h>
/*symbian core (for scheduler & trap cleanup)*/
#include <e32base.h>
/*hardware abstraction layer*/
#include <hal.h>

/*gpac module internals*/
#include "module_wrap.h"
#include <gpac/thread.h>


//  Exported Functions (DLL entry point)
#ifndef EKA2 // for EKA1 only
GLDEF_C TInt E32Dll(TDllReason /*aReason*/)
// Called when the DLL is loaded and unloaded. Note: have to define
// epoccalldllentrypoints in MMP file to get this called in THUMB.
{
	return KErrNone;
}

#endif



#define SLEEP_ABS_SELECT		1

static u32 sys_start_time = 0;

GF_EXPORT
u32 gf_sys_clock()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000 + (now.tv_usec) / 1000) - sys_start_time;
}


GF_EXPORT
void gf_sleep(u32 ms)
{
	TTimeIntervalMicroSeconds32 inter;
	inter = (TInt) (1000*ms);
#ifdef __SERIES60_3X__
	User::AfterHighRes(inter);
#else
	User::After(inter);
#endif

#if 0
	TInt error;
	CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);

	RTimer timer;
	TRequestStatus timerStatus;
	timer.CreateLocal();

	timer.After(timerStatus,ms*1000);
	User::WaitForRequest(timerStatus);
#endif
}

GF_EXPORT
void gf_delete_file(char *fileName)
{
	remove(fileName);
}


GF_EXPORT
void gf_rand_init(Bool Reset)
{
	if (Reset) {
		srand(1);
	} else {
		srand( (u32) time(NULL) );
	}
}

GF_EXPORT
u32 gf_rand()
{
	return rand();
}


#ifndef GPAC_READ_ONLY
GF_EXPORT
FILE *gf_temp_file_new()
{
	return tmpfile();
}
#endif


GF_EXPORT
void gf_utc_time_since_1970(u32 *sec, u32 *msec)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*sec = tv.tv_sec;
	*msec = tv.tv_usec/1000;
}

GF_EXPORT
void gf_get_user_name(char *buf, u32 buf_size)
{
	strcpy(buf, "mpeg4-user");

#if 0
	s32 len;
	char *t;
	strcpy(buf, "");
	len = 1024;
	GetUserName(buf, &len);
	if (!len) {
		t = getenv("USER");
		if (t) strcpy(buf, t);
	}
#endif
#if 0
	struct passwd *pw;
	pw = getpwuid(getuid());
	strcpy(buf, "");
	if (pw && pw->pw_name) strcpy(name, pw->pw_name);
#endif
}

GF_EXPORT
char * my_str_upr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = toupper(str[i]);
	}
	return str;
}
GF_EXPORT
char * my_str_lwr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = tolower(str[i]);
	}
	return str;
}

/*enumerate directories*/
GF_EXPORT
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir_fct, void *cbck, const char *filter)
{
	unsigned char item_path[GF_MAX_PATH];
	unsigned char path[GF_MAX_PATH], *file;

	DIR *the_dir;
	struct dirent* the_file;
	struct stat st;

	if (!dir || !enum_dir_fct) return GF_BAD_PARAM;

	strcpy((char*)path, dir);
	if (path[strlen((const char*)path)-1] != '\\') strcat((char*)path, "\\");


	the_dir = opendir((char*)path);
	if (the_dir == NULL) return GF_IO_ERR;

	the_file = readdir(the_dir);
	while (the_file) {
		if (!strcmp(the_file->d_name, "..")) goto next;
		if (the_file->d_name[0] == '.') goto next;

		if (filter) {
			char ext[30];
			char *sep = strrchr(the_file->d_name, '.');
			if (!sep) goto next;
			strcpy(ext, sep+1);
			strlwr(ext);
			if (!strstr(filter, sep+1)) goto next;
		}

		strcpy((char*)item_path, (const char*)path);
		strcat((char*)item_path, the_file->d_name);
		if (stat( (const char*)item_path, &st ) != 0) goto next;
		if (enum_directory && ( (st.st_mode & S_IFMT) != S_IFDIR)) goto next;
		if (!enum_directory && ((st.st_mode & S_IFMT) == S_IFDIR)) goto next;
		file = (unsigned char*)the_file->d_name;

		if (enum_dir_fct(cbck, (char *)file, (char *)item_path)) {
			break;
		}

next:
		the_file = readdir(the_dir);
	}
	return GF_OK;
}


GF_EXPORT
u64 gf_ftell(FILE *fp)
{
	return (u64) ftell(fp);
}

GF_EXPORT
u64 gf_fseek(FILE *fp, s64 offset, s32 whence)
{
	return fseek(fp, (s32) offset, whence);
}

GF_EXPORT
FILE *gf_fopen(const char *file_name, const char *mode)
{
	return fopen(file_name, mode);
}


/*symbian thread*/
typedef RThread* TH_HANDLE;

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
#ifndef GPAC_DISABLE_LOG
	char *log_name;
#endif
	/* lock for signal */
	GF_Semaphore *_signal;
};

GF_EXPORT
GF_Thread *gf_th_new(const char *name)
{
	GF_Thread *tmp = (GF_Thread *) gf_malloc(sizeof(GF_Thread));
	memset((void *)tmp, 0, sizeof(GF_Thread));
	tmp->status = GF_THREAD_STATUS_STOP;
#ifndef GPAC_DISABLE_LOG
	if (name) {
		tmp->log_name = gf_strdup(name);
	} else {
		char szN[20];
		sprintf(szN, "0x%08x", (u32) tmp);
		tmp->log_name = gf_strdup(szN);
	}
#endif

	return tmp;
}

static void *RunThread(void *ptr)
{
	TInt err;
	u32 ret = 0;
	CTrapCleanup * cleanup = NULL;
	GF_Thread *t = (GF_Thread *)ptr;


	CActiveScheduler * scheduler = new CActiveScheduler();
	if (scheduler == NULL) {
		t->status = GF_THREAD_STATUS_DEAD;
		gf_sema_notify(t->_signal, 1);
		goto exit;
	}
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Installing ActiveScheduler\n", t->log_name));
#endif
	CActiveScheduler::Install(scheduler);

#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Creating cleanup trap\n", t->log_name));
#endif
	cleanup = CTrapCleanup::New();
	if( cleanup == NULL ) {
		t->status = GF_THREAD_STATUS_DEAD;
		gf_sema_notify(t->_signal, 1);
		delete CActiveScheduler::Current();
		goto exit;
	}
#if 0
	CActiveScheduler::Start();
#endif
	/* OK , signal the caller */
	t->status = GF_THREAD_STATUS_RUN;
	gf_sema_notify(t->_signal, 1);
	/* Run our thread */
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Entering thread proc\n", t->log_name));
#endif
	TRAP(err, ret=t->Run(t->args) );
	//ret = t->Run(t->args);

	delete CActiveScheduler::Current();
	delete cleanup;

exit:
#ifndef GPAC_DISABLE_LOG
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Thread %s] Exit\n", t->log_name ));
#endif
	t->status = GF_THREAD_STATUS_DEAD;
	t->Run = NULL;
	t->threadH->Close();
	t->threadH = NULL;
	return (void *)ret;
}

GF_EXPORT
GF_Err gf_th_run(GF_Thread *t, u32 (*Run)(void *param), void *param)
{
	TBuf<32>	threadName;

	const TUint KThreadMinHeapSize = 0x1000;
	const TUint KThreadMaxHeapSize = 0x10000;

	if (!t || t->Run || t->_signal) return GF_BAD_PARAM;
	t->Run = Run;
	t->args = param;
	t->_signal = gf_sema_new(1, 0);
	if (! t->_signal) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Unable to create thread start-up semaphore\n"));
		t->status = GF_THREAD_STATUS_DEAD;
		return GF_IO_ERR;
	}

	threadName.Format(_L("GTH%d"), (u32) t);
	t->threadH = new RThread();
	if ( t->threadH->Create(threadName, (TThreadFunction)RunThread, KDefaultStackSize, KThreadMinHeapSize, KThreadMaxHeapSize, (void *)t, EOwnerProcess) != KErrNone) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Unable to create thread\n"));
		t->status = GF_THREAD_STATUS_DEAD;
		return GF_IO_ERR;
	}
	t->threadH->Resume();

	/*wait for the child function to call us - do NOT return before, otherwise the thread status would
	be unknown*/
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Waiting for thread to start\n"));
	gf_sema_wait(t->_signal);
	gf_sema_del(t->_signal);
	t->_signal = NULL;
	return GF_OK;
}


/* Stops a thread. If Destroy is not 0, thread is destroyed DANGEROUS as no cleanup */
static void Thread_Stop(GF_Thread *t, Bool Destroy)
{
	if (gf_th_status(t) == GF_THREAD_STATUS_RUN) {
		if (Destroy) {
			t->threadH->Terminate(0);
			t->threadH = NULL;
		}
		else {
			t->threadH->Suspend();
		}
	}
	t->status = GF_THREAD_STATUS_DEAD;
}

GF_EXPORT
void gf_th_stop(GF_Thread *t)
{
	Thread_Stop(t, 0);
}

GF_EXPORT
void gf_th_del(GF_Thread *t)
{
	Thread_Stop(t, 0);
#ifndef GPAC_DISABLE_LOG
	gf_free(t->log_name);
#endif
	gf_free(t);
}


GF_EXPORT
void gf_th_set_priority(GF_Thread *t, s32 priority)
{
	/*FIXEME: tune priorities on symbian*/
#if 0
	if (priority > 200)
		t->threadH->SetPriority(EPriorityRealTime);
	else
		t->threadH->SetPriority(EPriorityNormal);
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
	return RThread().Id();
}



/*********************************************************************
						OS-Specific Mutex Object
**********************************************************************/
struct __tag_mutex
{
	RMutex *hMutex;
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
	GF_Mutex *tmp = (GF_Mutex *)gf_malloc(sizeof(GF_Mutex));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Mutex));

	tmp->hMutex = new RMutex();
	if( tmp->hMutex->CreateLocal() != KErrNone) {
		gf_free(tmp);
		return NULL;
	}
#ifndef GPAC_DISABLE_LOG
	if (name) {
		tmp->log_name = gf_strdup(name);
	} else {
		char szN[20];
		sprintf(szN, "0x%08x", (u32) tmp);
		tmp->log_name = gf_strdup(szN);
	}
#endif
	return tmp;
}

GF_EXPORT
void gf_mx_del(GF_Mutex *mx)
{
	mx->hMutex->Close();
	gf_free(mx);
}

GF_EXPORT
void gf_mx_v(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return;
	caller = gf_th_id();

	/*only if we own*/
	if (caller != mx->Holder) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Invalid mutex release - owner PID %d - caller PID %d\n", mx->Holder, caller));
		return;
	}
	if (!mx->HolderCount) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Invalid mutex release - mutex not locked\n"));
		return;
	}
	mx->HolderCount -= 1;

	if (mx->HolderCount == 0) {
		mx->Holder = 0;
		mx->hMutex->Signal();
	}
}

GF_EXPORT
u32 gf_mx_p(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return 0;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return 1;
	}
	mx->hMutex->Wait();
	mx->Holder = caller;
	mx->HolderCount = 1;
	return 1;
}

GF_EXPORT
Bool gf_mx_try_lock(GF_Mutex *mx)
{
	u32 caller;
	if (!mx) return 0;
	caller = gf_th_id();
	if (caller == mx->Holder) {
		mx->HolderCount += 1;
		return 1;
	}
	/*FIXME !! WE MUST HAVE tryLock*/
	gf_mx_p(mx);
	return 1;
}




/*********************************************************************
						OS-Specific Semaphore Object
**********************************************************************/
struct __tag_semaphore
{
	RSemaphore *hSemaphore;
};


GF_EXPORT
GF_Semaphore *gf_sema_new(u32 MaxCount, u32 InitCount)
{
	GF_Semaphore *tmp = (GF_Semaphore *) gf_malloc(sizeof(GF_Semaphore));

	if (!tmp) return NULL;
	tmp->hSemaphore = new RSemaphore();
	if (!tmp->hSemaphore) {
		gf_free(tmp);
		return NULL;
	}
	TBuf<32>	semaName;
	semaName.Format(_L("GPAC_SEM%d"), (u32) tmp);
	tmp->hSemaphore->CreateGlobal(semaName, InitCount);

	return tmp;
}

GF_EXPORT
void gf_sema_del(GF_Semaphore *sm)
{
	sm->hSemaphore->Close();
	gf_free(sm);
}

GF_EXPORT
Bool gf_sema_notify(GF_Semaphore *sm, u32 NbRelease)
{
    if (!sm) return GF_FALSE;
	sm->hSemaphore->Signal(NbRelease);
	return GF_TRUE;
}

GF_EXPORT
void gf_sema_wait(GF_Semaphore *sm)
{
	sm->hSemaphore->Wait();
}

GF_EXPORT
Bool gf_sema_wait_for(GF_Semaphore *sm, u32 TimeOut)
{
	return 0;
}





static u32 sys_init = 0;
GF_SystemRTInfo the_rti;

GF_EXPORT
void gf_sys_init()
{
	if (!sys_init) {
		memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
		the_rti.pid = getpid();
		sys_start_time = gf_sys_clock();
	}
	sys_init += 1;
}

GF_EXPORT
void gf_sys_close()
{
	if (sys_init > 0) {
		sys_init --;
		if (sys_init) return;
	}
}

#ifdef GPAC_MEMORY_TRACKING
extern size_t gpac_allocated_memory;
#endif

/*CPU and Memory Usage*/
GF_EXPORT
Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	TInt ram, ram_free;
	u32 now, time;
#ifdef __SERIES60_3X__
	TModuleMemoryInfo mi;
#endif
	TTimeIntervalMicroSeconds tims;
	RProcess cur_process;
	RThread cur_th;

	now = gf_sys_clock();
	if (!rti->sampling_instant) {
		rti->sampling_instant = now;
		if (cur_th.GetCpuTime(tims) != KErrNone) {
			return 0;
		}
#ifdef __SERIES60_3X__
		rti->process_cpu_time = (u32) (tims.Int64() / 1000);
#else
		rti->process_cpu_time = (u32) ( TInt64(tims.Int64() / 1000).GetTInt() );
#endif
		return 0;
	}
	if (rti->sampling_instant + refresh_time_ms > now) return 0;
	rti->sampling_period_duration = now - rti->sampling_instant;
	rti->sampling_instant = now;

	if (cur_th.Process(cur_process) != KErrNone) {
		return 0;
	}
#ifdef __SERIES60_3X__
	if (cur_process.GetMemoryInfo(mi) != KErrNone) {
		return 0;
	}
	rti->process_memory = mi.iCodeSize + mi.iConstDataSize + mi.iInitialisedDataSize + mi.iUninitialisedDataSize;
#endif
	if (cur_th.GetCpuTime(tims) != KErrNone) {
		return 0;
	}
#ifdef __SERIES60_3X__
	time = (u32) (tims.Int64() / 1000);
#else
	time = (u32) ( TInt64(tims.Int64() / 1000).GetTInt() );
#endif
	rti->process_cpu_time_diff = time - rti->process_cpu_time;
	rti->process_cpu_time = time;
	rti->process_cpu_usage = 100*rti->process_cpu_time_diff / rti->sampling_period_duration;
	if (rti->process_cpu_usage > 100) rti->process_cpu_usage = 100;

	HAL::Get(HALData::EMemoryRAM, ram);
	HAL::Get(HALData::EMemoryRAMFree, ram_free);
	rti->physical_memory = ram;
	rti->physical_memory_avail = ram_free;
#ifdef GPAC_MEMORY_TRACKING
	rti->gpac_memory = gpac_allocated_memory;
#endif
	return 1;
}

GF_EXPORT
Bool gf_sys_get_battery_state(Bool *onBattery, u32 *state, u32*level)
{
	return 1;
}


/*delete all interfaces loaded on object*/
void gf_modules_free_module(ModuleInstance *inst)
{
	while (gf_list_count(inst->interfaces)) {
		void *objinterface = gf_list_get(inst->interfaces, 0);
		gf_list_rem(inst->interfaces, 0);
		inst->destroy_func(objinterface);
	}
	if (inst->lib_handle) {
		RLibrary* pLibrary = (RLibrary *) inst->lib_handle;
		pLibrary->Close();
	}
	gf_list_del(inst->interfaces);
	gf_free(inst);
}

Bool gf_modules_load_library(ModuleInstance *inst)
{
	const TUid KGPACModuleUid= {0x10000080};
	char s_path[GF_MAX_PATH];
	HBufC *path;
	TInt e;

	if (inst->lib_handle) return 1;

	sprintf(s_path, "%s%c%s", inst->plugman->dir, GF_PATH_SEPARATOR, inst->szName);

	path = HBufC::NewL( User::StringLength( ( TUint8* ) s_path) + 1 );
	path->Des().Copy( TPtrC8(( TText8* ) s_path) );

	RLibrary* pLibrary = new RLibrary();
	e = pLibrary->Load(*path);
	delete path;

	if (e != KErrNone) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Cannot load library %s: %d", s_path, e));
		delete pLibrary;
		goto err_exit;
	}
	/*check UID 2 is GPAC's identifier*/
	if (pLibrary->Type()[1] != KGPACModuleUid) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Invalid library UID %x", (u32) pLibrary->Type()[1].iUid));
		pLibrary->Close();
		delete pLibrary;
		goto err_exit;
	}
	inst->query_func = (QueryInterfaces) pLibrary->Lookup(1);
	inst->load_func = (LoadInterface) pLibrary->Lookup(2);
	inst->destroy_func = (ShutdownInterface) pLibrary->Lookup(3);

	if ((inst->query_func==NULL) || (inst->load_func==NULL) || (inst->destroy_func==NULL) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Library %s has invalid interfaces", inst->szName));
		pLibrary->Close();
		delete pLibrary;
		goto err_exit;
	}

	//store library handle
	inst->lib_handle = (void*) pLibrary;
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Module %s loaded\n", inst->szName));
	return 1;

err_exit:
	gf_cfg_set_key(inst->plugman->cfg, "SymbianDLLs", inst->szName, "no");
	return 0;
}

void gf_modules_unload_library(ModuleInstance *inst)
{
	if (!inst->lib_handle || gf_list_count(inst->interfaces)) return;

	RLibrary* pLibrary = (RLibrary *) inst->lib_handle;
	pLibrary->Close();
	delete pLibrary;

	inst->lib_handle = NULL;
	inst->load_func = NULL;
	inst->destroy_func = NULL;
	inst->query_func = NULL;
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Module %s unloaded\n", inst->szName));
}


static Bool enum_modules(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	ModuleInstance *inst;

	GF_ModuleManager *pm = (GF_ModuleManager *) cbck;

	if (strncmp(item_name, "gm_", 3)) return 0;
	if (gf_module_is_loaded(pm, item_name) ) return 0;

	/*TODO FIXME: add module check for symbian */

	GF_SAFEALLOC(inst, ModuleInstance);
	inst->interfaces = gf_list_new();
	inst->plugman = pm;
	strcpy((char*)inst->szName, item_name);
	gf_list_add(pm->plug_list, inst);
	return 0;
}

/*refresh modules - note we don't check for deleted modules but since we've open them the OS should forbid delete*/
GF_EXPORT
u32 gf_modules_refresh(GF_ModuleManager *pm)
{
	if (!pm) return 0;
	//!! symbian 9.1 doesn't allow for DLL browsing in /sys/bin, and can only load DLLs in /sys/bin !!
#if 0
	gf_enum_directory((char*)pm->dir, 0, enum_modules, pm, ".dll");
#else
	u32 i, mod_count;

	mod_count = gf_cfg_get_key_count(pm->cfg, "SymbianDLLs");
	for (i=0; i<mod_count; i++) {
		const char *mod = gf_cfg_get_key_name(pm->cfg, "SymbianDLLs", i);
		if (stricmp(gf_cfg_get_key(pm->cfg, "SymbianDLLs", mod), "yes")) continue;
		enum_modules(pm, (char*)mod, NULL);
	}
#endif
	return gf_list_count(pm->plug_list);
}

#endif

