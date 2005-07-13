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

void gf_sys_clock_start() {}

void gf_sys_clock_stop() {}

u32 gf_sys_clock()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000 + (now.tv_usec) / 1000);
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

u32 gf_get_ntp_frac(u32 sec, u32 frac)
{
	return ( ((sec  & 0x0000ffff) << 16) |  ((frac & 0xffff0000) >> 16));
}
         

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



u64 f64_tell(FILE *f)
{
#ifdef CONFIG_LINUX
	return (u64) ftello64(f);
#elif defined(CONFIG_FREEBSD)
	return (u64) ftell(f);
#else
	return (u64) ftell(f);
#endif
}

u64 f64_seek(FILE *f, s64 pos, s32 whence)
{
#ifdef CONFIG_LINUX
	return fseeko64(f, (off64_t) pos, whence);
#elif defined(CONFIG_FREEBSD)
	return fseek(f, pos, whence);
#else
	return fseek(f, (s32) pos, whence);
#endif
}

FILE *f64_open(const char *file_name, const char *mode)
{
#ifdef CONFIG_LINUX
	return fopen64(file_name, mode);
#elif defined(CONFIG_FREEBSD)
	return fopen(file_name, mode);
#else
	return fopen(file_name, mode);
#endif
}

