/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Stream Management sub-project
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

#ifndef _GF_TERMINAL_DEV_H_
#define _GF_TERMINAL_DEV_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <gpac/thread.h>
#include <gpac/filters.h>

#include <gpac/terminal.h>
#include <gpac/term_info.h>
#include <gpac/internal/compositor_dev.h>

/*forwards all clocks of the given amount of time. Can only be used when terminal is in paused mode
this is mainly designed for movie dumping in MP4Client*/
GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff);

u32 gf_term_sample_clocks(GF_Terminal *term);


struct _tag_terminal
{
	GF_User *user;
	GF_Compositor *compositor;
	GF_FilterSession *fsess;
};



#ifdef __cplusplus
}
#endif


#endif	/*_GF_TERMINAL_DEV_H_*/


