/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / gpac application
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

#include <gpac/filters.h>
#include <gpac/events.h>

#ifndef GPAC_CONFIG_ANDROID

//gpac is loaded using sink compositor in base player mode (-mp4c)  with a custum event proc
#define LOAD_MP4C		1
//gpac is loaded with gui
#define LOAD_GUI		2
//gpac is loaded with gui from env var GPAC_GUI (OSX only for now)
#define LOAD_GUI_ENV	3
//gpac is loaded with gui but uses event proc (ios only for now)
#define LOAD_GUI_CBK	4

void load_compositor(GF_Filter *f);
void unload_compositor(void);
Bool mp4c_event_proc(void *ptr, GF_Event *evt);
Bool mp4c_parse_arg(char *arg, char *arg_val);
Bool mp4c_handle_prompt(u8 char_val);
Bool mp4c_task(void );
void mp4c_help(u32 argmode);

#endif


//if uncommented, check argument description matches our conventions - see filter.h
#define CHECK_DOC

#define SEP_LINK	5
#define SEP_FRAG	2
#define SEP_LIST	3

#include <gpac/main.h>

void gpac_filter_help(void);
void gpac_modules_help(void);
void gpac_alias_help(GF_SysArgMode argmode);
void gpac_core_help(GF_SysArgMode mode, Bool for_logs);
void gpac_usage(GF_SysArgMode argmode);
void gpac_config_help(void);
void gpac_suggest_arg(char *aname);
void gpac_suggest_filter(char *fname, Bool is_help, Bool filter_only);
void gpac_check_session_args(void);
int gpac_make_lang(char *filename);
Bool gpac_expand_alias(int o_argc, char **o_argv);
void gpac_load_suggested_filter_args(void);
void dump_all_props(char *pname);
void dump_all_colors(void);
void dump_all_audio_cicp(void);
void dump_all_codec(GF_SysArgMode argmode);
void write_core_options(void );
void write_file_extensions(void );
void write_filters_options(void);
Bool print_filters(int argc, char **argv, GF_SysArgMode argmode);
void parse_sep_set(const char *arg, Bool *override_seps);


#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)
void carbon_remove_hook(void);
void carbon_set_hook(void);
#endif

void gpac_open_urls(const char *urls);
