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


#include <gpac/tools.h>

#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#define SLEEP_ABS_SELECT		1


u32 gf_sys_clock()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000 + (now.tv_usec) / 1000);
}

u64 gf_sys_clock_ns()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000000 + now.tv_usec);
}


void gf_sleep(u32 ms)
{
	s32 sel_err;
	struct timeval tv;

#ifndef SLEEP_ABS_SELECT
	u32 prev, now, elapsed;
#endif

#ifdef SLEEP_ABS_SELECT
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
#else
	prev = gf_sys_clock();
#endif

	do {
		errno = 0;

#ifndef SLEEP_ABS_SELECT
		now = gf_sys_clock();
		elapsed = (now - prev);
		if ( elapsed >= ms ) {
			break;
		}
		prev = now;
		ms -= elapsed;
		tv.tv_sec = ms/1000;
		tv.tv_usec = (ms%1000)*1000;
#endif

		sel_err = select(0, NULL, NULL, NULL, &tv);
	} while ( sel_err && (errno == EINTR) );
}


/*
		Some NTP tools
*/

#define SECS_1900_TO_1970 2208988800ul
       

void gf_get_ntp(u32 *sec, u32 *frac)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	*sec = now.tv_sec + SECS_1900_TO_1970;
	*frac = (now.tv_usec << 12) + (now.tv_usec << 8) - ((now.tv_usec * 3650) >> 6);
}


void gf_delete_file(char *fileName)
{
	remove(fileName);
}


#ifndef GPAC_READ_ONLY
FILE *gf_temp_file_new()
{
	return tmpfile(); 
}
#endif



void gf_utc_time_since_1970(u32 *sec, u32 *msec)
{
	struct timeval tp;
	gettimeofday(&tp, NULL);
	*sec = tp.tv_sec;
	*msec = tp.tv_usec*1000;
}

void gf_rand_init(Bool Reset)
{
	if (Reset) {
		srand(1);
	} else {
		srand( (u32) time(NULL) );
	}
}

u32 gf_rand()
{
	return rand();
}



void gf_get_user_name(char *buf, u32 buf_size)
{
	strcpy(buf, "mpeg4-user");

#if 0
	struct passwd *pw;
	pw = getpwuid(getuid());
	strcpy(buf, "");
	if (pw && pw->pw_name) strcpy(name, pw->pw_name);
#endif

}



/*enumerate directories*/
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck)
{
	unsigned char ext[2];
	unsigned char path[GF_MAX_PATH];
	unsigned char filepath[GF_MAX_PATH];
	DIR *the_dir;
	struct dirent* the_file;
	struct stat st;

	if (!dir) return GF_BAD_PARAM;

	strcpy(path, dir);
	if (path[strlen(path)-1] != GF_PATH_SEPARATOR) {
		ext[0] = GF_PATH_SEPARATOR;
		ext[1] = 0;
		strcat(path, ext);
	}


	the_dir = opendir(path);
	if (the_dir == NULL) {
		return GF_IO_ERR;
	}

	the_file = readdir(the_dir);
	while (the_file) {
		if (!strcmp(the_file->d_name, "..")) goto next;
		if (the_file->d_name[0] == '.') goto next;

		strcpy(filepath, path);
		strcat(filepath, the_file->d_name);

		if (stat( filepath, &st ) != 0) {
			printf("stat err %s\n", filepath);
			goto next;
		}
		if (enum_directory && ( (st.st_mode & S_IFMT) != S_IFDIR)) goto next;
		if (!enum_directory && ((st.st_mode & S_IFMT) == S_IFDIR)) goto next;

		if (enum_dir_item(cbck, the_file->d_name, filepath)) break;

next:
		the_file = readdir(the_dir);
	}
	closedir(the_dir);
	return GF_OK;
}


char * my_str_upr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = toupper(str[i]);
	}
	return str;
}
char * my_str_lwr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = tolower(str[i]);
	}
	return str;
}



u64 gf_f64_tell(FILE *f)
{
#ifdef CONFIG_LINUX
	return (u64) ftello64(f);
#elif defined(CONFIG_FREEBSD)
	return (u64) ftell(f);
#else
	return (u64) ftell(f);
#endif
}

u64 gf_f64_seek(FILE *f, s64 pos, s32 whence)
{
#ifdef CONFIG_LINUX
	return fseeko64(f, (off64_t) pos, whence);
#elif defined(CONFIG_FREEBSD)
	return fseek(f, pos, whence);
#else
	return fseek(f, (s32) pos, whence);
#endif
}

FILE *gf_f64_open(const char *file_name, const char *mode)
{
#ifdef CONFIG_LINUX
	return fopen64(file_name, mode);
#elif defined(CONFIG_FREEBSD)
	return fopen(file_name, mode);
#else
	return fopen(file_name, mode);
#endif
}


#include <unistd.h>
#include <sys/times.h>
#include <sys/resource.h>

static Bool sys_init = 0;
static FILE *proc_f = NULL;
static GF_SystemRTInfo the_rti;
static u64 clock_res = 0;
static u32 last_update_time = 0;
static u64 last_cpu_u_k_time = 0;
static u64 last_cpu_idle_time = 0;
static u64 last_process_u_k_time = 0;

void gf_sys_init() 
{
  if (!sys_init) {
    proc_f = fopen("/proc/stat", "r");

    last_process_u_k_time = 0;
    last_cpu_u_k_time = last_cpu_idle_time = 0;
    last_update_time = 0;
    memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
    the_rti.pid = getpid();
    clock_res = CLOCKS_PER_SEC;
  }
  sys_init++;
}

void gf_sys_close() 
{
  if (sys_init) {
    sys_init--;
    if (sys_init) return;
    if (proc_f) fclose(proc_f);
  }
}


Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
  u32 entry_time;
  u64 process_u_k_time;
  u32 u_k_time, idle_time;
  struct rusage proctime;

  assert(sys_init);

  entry_time = gf_sys_clock();
  if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
    memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
    return 0;
  }

  u_k_time = idle_time = 0;
  if (proc_f) {
    char line[128];
    u32 k_time, nice_time, u_time;
    rewind(proc_f);
    fflush(proc_f);
    if (fgets(line, 128, proc_f) == NULL) {
      fclose(proc_f);
      proc_f = NULL;
      return 0;
    } else if (sscanf(line, "cpu  %u %u %u %u\n", &u_time, &k_time, &nice_time, &idle_time) != 4) {
      fclose(proc_f);
      proc_f = NULL;
      return 0;
    } else {
      u_k_time = u_time + k_time + nice_time;
   }
  } else {
    /*FIXME*/
  }

  /*FIXME and complete me - times() nor getrusage() seem to work properly on linux*/
#if 0
  getrusage(RUSAGE_SELF, &proctime);
  process_u_k_time = proctime.ru_utime.tv_sec + proctime.ru_utime.tv_usec/1000;
  process_u_k_time += proctime.ru_stime.tv_sec + proctime.ru_stime.tv_usec/1000;
  getrusage(RUSAGE_CHILDREN, &proctime);
  process_u_k_time += proctime.ru_utime.tv_sec + proctime.ru_utime.tv_usec/1000;
  process_u_k_time += proctime.ru_stime.tv_sec + proctime.ru_stime.tv_usec/1000;

  {
    struct tms vals;
    int status;
    wait(&status);
    times(&vals);
    process_u_k_time = vals.tms_utime + vals.tms_stime;
    process_u_k_time += vals.tms_cutime + vals.tms_cstime;
    fprintf(stdout, "times get %d %d - %d %d\n", vals.tms_utime , vals.tms_stime, vals.tms_cutime , vals.tms_cstime);
  }
#else
  process_u_k_time = 0;
#endif

  the_rti.sampling_instant = last_update_time;
  
  if (last_update_time) {
    the_rti.sampling_period_duration = (entry_time - last_update_time);
    the_rti.process_cpu_time_diff = process_u_k_time - last_process_u_k_time;

    /*oops, we have no choice but to assume 100% cpu usage during this period*/
    if (!u_k_time) {
      the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
      u_k_time = last_cpu_u_k_time + the_rti.sampling_period_duration;
      the_rti.cpu_idle_time = 0;
      the_rti.cpu_usage = 100;
    } else {
      u64 samp_sys_time;
      /*move to ms (/proc/stat gives times in 100 ms unit*/
      the_rti.total_cpu_time_diff = (u_k_time - last_cpu_u_k_time)*10;

      /*we're not that accurate....*/
      if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration)
      	the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;

   
      if (!idle_time) idle_time = (the_rti.sampling_period_duration - the_rti.total_cpu_time_diff)/10;
      samp_sys_time = u_k_time - last_cpu_u_k_time;
      the_rti.cpu_idle_time = idle_time - last_cpu_idle_time;
      the_rti.cpu_usage = (u32) ( 100 * samp_sys_time / (the_rti.cpu_idle_time + samp_sys_time ) );
      /*move to ms (/proc/stat gives times in 100 ms unit*/
      the_rti.cpu_idle_time *= 10;
    }
    if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
  }
  last_process_u_k_time = process_u_k_time;
  last_cpu_idle_time = idle_time;
  last_cpu_u_k_time = u_k_time;
  last_update_time = entry_time;
  
  /*  GlobalMemoryStatus(&ms);
  the_rti.physical_memory = ms.dwTotalPhys;
  the_rti.physical_memory_avail = ms.dwAvailPhys;
  */
  memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
  return 1;
}

