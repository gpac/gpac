/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2010-20xx
 *					All rights reserved
 *
 *  This file is part of GPAC / Hybrid Media input module
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


/*hybrid media interface implementation using MMB Tools from the Communication Research Center Canada (http://mmbtools.crc.ca/)*/

#include <gpac/thread.h>

#if 0


GF_HYBMEDIA master_fm_mmbtools = {
};


typedef struct {
	Bool clock_found_time; /*simulate the discovery of a RDS clock: arrives within the first minutes with a 100ms jitter*/

	GF_Thread *fm_th;
	volatile u32 fm_th_state;
} FM_MMBTOOLS;



static Bool FM_MMBTOOLS_CanHandleURL(const char *url)
{
	if (!strnicmp(url, "fm://", 5))
		return 1;

	return 0;
}

static GF_ObjectDescriptor* FM_MMBTOOLS_GetOD()
{

	return od;
}

static GF_Err HYB_FM_Close(GF_HYB_In *hyb_in)
{
	/*stop the audio_generation thread*/
	hyb_in->fm_th_state = HYB_STATE_STOPPING;
	while (hyb_in->fm_th_state != HYB_STATE_STOPPED)
		gf_sleep(10);

	return GF_OK;
}

static GF_Err HYB_FM_Connect(GF_HYB_In *hyb_in, const char *url)
{
	FM_MMBTOOLS* audio_gen;
	if (hyb_in->fm_thread)
		return GF_ERR;
	GF_SAFEALLOC(audio_gen, FM_MMBTOOLS);
	hyb_in->fm_thread = gf_th_new("HYB-FM fake audio generation thread");
	gf_th_run(hyb_in->fm_thread, audio_gen_th, audio_gen);

	return GF_OK;
}

static u32 audio_gen_th(void *par) 
{
	FM_MMBTOOLS audio_gen = (FM_MMBTOOLS*) par;

	/*RDS clock discovery time*/
	clock_found_time = rand() % XXX; //radom in the 1st minute

	while (!audio_gen->state) /*FM_MMBTOOLS_STATE_RUNNING*/
	{
		gf_sleep (remaining);

		if (clock > clock_found_time && ) {
			gf_term_clock clock();
		} else {
			gf_term_clock utc_clock();
		}
	}

	audio_gen->state = FM_MMBTOOLS_STATE_STOPPED;
}

#endif
