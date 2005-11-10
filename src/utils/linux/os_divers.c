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
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck, const char *filter)
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

		if (filter) {
			char ext[30];
			char *sep = strrchr(the_file->d_name[0], '.');
			if (!sep) goto next;
			strcpy(ext, sep+1);
			strlwr(ext);
			if (!strstr(filter, sep+1)) goto next;
		}
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
static GF_SystemRTInfo the_rti;
static u32 last_update_time = 0;
static u64 last_cpu_u_k_time = 0;
static u64 last_cpu_idle_time = 0;
static u64 last_process_u_k_time = 0;
static u64 mem_at_startup = 0;

void gf_sys_init() 
{
  if (!sys_init) {
    last_process_u_k_time = 0;
    last_cpu_u_k_time = last_cpu_idle_time = 0;
    last_update_time = 0;
    memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
    the_rti.pid = getpid();
  }
  sys_init++;
}

void gf_sys_close() 
{
  if (sys_init) {
    sys_init--;
    if (sys_init) return;
  }
}


Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
  u32 entry_time;
  u64 process_u_k_time;
  u32 u_k_time, idle_time;
  char szProc[100], line[2048];
  FILE *f;

  assert(sys_init);

  entry_time = gf_sys_clock();
  if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
    memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
    return 0;
  }

  u_k_time = idle_time = 0;
  f = fopen("/proc/stat", "r");
  if (f) {
    u32 k_time, nice_time, u_time;
    if (fgets(line, 128, f) != NULL) {
      if (sscanf(line, "cpu  %u %u %u %u\n", &u_time, &k_time, &nice_time, &idle_time) == 4) {
	u_k_time = u_time + k_time + nice_time;
      }
    }
    fclose(f);
  }

  process_u_k_time = 0;
  the_rti.process_memory = 0;

  /*FIXME? under LinuxThreads this will only fetch stats for the calling thread, we would have to enumerate /proc to get
   the complete CPU usage of all therads of the process...*/
#if 0
  sprintf(szProc, "/proc/%d/stat", the_rti.pid);
  f = fopen(szProc, "r");
  if (f) {
    fflush(f);
    if (fgets(line, 2048, f) != NULL) {
      char state;
      char *start;
      long cutime, cstime, priority, nice, itrealvalue, rss;
      int exit_signal, processor;
      unsigned long flags, minflt, cminflt, majflt, cmajflt, utime, stime,starttime, vsize, rlim, startcode, endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore, sigcatch, wchan, nswap, cnswap, rem;
      int ppid, pgrp ,session, tty_nr, tty_pgrp, res;
      start = strchr(line, ')');
      if (start) start += 2;
      else {
	start = strchr(line, ' ');
	start++;
      }
      res = sscanf(start,"%c %d %d %d %d %d %lu %lu %lu %lu \
%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu \
%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu \
%lu %lu %lu %lu %lu %d %d",
		   &state, &ppid, &pgrp, &session, &tty_nr, &tty_pgrp, &flags, &minflt, &cminflt, &majflt,
		   &cmajflt, &utime, &stime, &cutime, &cstime, &priority, &nice, &itrealvalue, &rem, &starttime,
		   &vsize, &rss, &rlim, &startcode, &endcode, &startstack, &kstkesp, &kstkeip, &signal, &blocked,
		   &sigignore, &sigcatch, &wchan, &nswap, &cnswap, &exit_signal, &processor);
 
      if (res) process_u_k_time = (u64) (cutime + cstime);
      else fprintf(stdout, "parse error\n");
    } else {
      fprintf(stdout, "error reading pid/stat\n");
    }
    fclose(f);
  } else {
    fprintf(stdout, "cannot open %s\n", szProc);
  }
  sprintf(szProc, "/proc/%d/status", the_rti.pid);
  f = fopen(szProc, "r");
  if (f) {
    while (fgets(line, 1024, f) != NULL) {
      if (!strnicmp(line, "VmSize:", 7)) {
	sscanf(line, "VmSize: %lld kB",  &the_rti.process_memory);
	the_rti.process_memory *= 1024;
      }
    }
    fclose(f);
  } else {
    fprintf(stdout, "cannot open %s\n", szProc);
  }
#endif


  the_rti.physical_memory = the_rti.physical_memory_avail = 0;
  f = fopen("/proc/meminfo", "r");
  if (f) {
    while (fgets(line, 1024, f) != NULL) {
      if (!strnicmp(line, "MemTotal:", 9)) {
	sscanf(line, "MemTotal: %lld kB",  &the_rti.physical_memory);
	the_rti.physical_memory *= 1024;
      }else if (!strnicmp(line, "MemFree:", 8)) {
	sscanf(line, "MemFree: %lld kB",  &the_rti.physical_memory_avail);
	the_rti.physical_memory_avail *= 1024;
	break;
      }
    }
    fclose(f);
  } else {
    fprintf(stdout, "cannot open /proc/meminfo\n");
  }


  the_rti.sampling_instant = last_update_time;
  
  if (last_update_time) {
    the_rti.sampling_period_duration = (entry_time - last_update_time);
    the_rti.process_cpu_time_diff = (process_u_k_time - last_process_u_k_time) * 10;

    /*oops, we have no choice but to assume 100% cpu usage during this period*/
    if (!u_k_time) {
      the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
      u_k_time = last_cpu_u_k_time + the_rti.sampling_period_duration;
      the_rti.cpu_idle_time = 0;
      the_rti.total_cpu_usage = 100;
      if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
      the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / the_rti.sampling_period_duration);
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
      the_rti.total_cpu_usage = (u32) ( 100 * samp_sys_time / (the_rti.cpu_idle_time + samp_sys_time ) );
      /*move to ms (/proc/stat gives times in 100 ms unit*/
      the_rti.cpu_idle_time *= 10;
      if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
      the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / (the_rti.cpu_idle_time + 10*samp_sys_time ) );
    }
  } else {
    mem_at_startup = the_rti.physical_memory_avail;
  }
  the_rti.process_memory = mem_at_startup - the_rti.physical_memory_avail;

  last_process_u_k_time = process_u_k_time;
  last_cpu_idle_time = idle_time;
  last_cpu_u_k_time = u_k_time;
  last_update_time = entry_time;
  memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
  return 1;
}

