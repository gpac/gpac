/*
*			GPAC - Multimedia Framework C SDK
*
*           Author: Romain Bouqueau
*			Copyright (c) Telecom ParisTech 2010-20XX
*					All rights reserved
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
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>

/*interface query*/
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
		GF_VIDEO_OUTPUT_INTERFACE,
		GF_AUDIO_OUTPUT_INTERFACE,
		0
	};
	return si; 
}
/*interface create*/
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	/*Hack: return invalid pointers*/
	if (InterfaceType == GF_VIDEO_OUTPUT_INTERFACE) return 0x1;
	if (InterfaceType == GF_AUDIO_OUTPUT_INTERFACE) return 0x2;
	return NULL;
}
/*interface destroy*/
void ShutdownInterface(GF_BaseInterface *ifce)
{
	/*Hack: don't clean anything*/
	switch (ifce->InterfaceType) {
		case GF_VIDEO_OUTPUT_INTERFACE:
			/*SDL_DeleteVideo(ifce);*/
			break;
		case GF_AUDIO_OUTPUT_INTERFACE:
			/*SDL_DeleteAudio(ifce);*/
			break;
	}
}