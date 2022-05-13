/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / command-line client
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

#if defined(__DARWIN__) || defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif


int gf_sys_set_args(int argc, const char **argv);
void send_open_url(const char *url);

void RunApplicationEventLoop(void);
void QuitApplicationEventLoop(void);

char *my_argv[2];

static int main_evt_loop_run = 1;

static AEEventHandlerUPP	open_app_UPP, open_doc_UPP;

static pascal OSErr ae_open_app (const AppleEvent *ae_event, AppleEvent *ae_reply, long ae_ref_count)
{
	if (main_evt_loop_run) {
		QuitApplicationEventLoop();
		main_evt_loop_run = 0;
	}
	return (noErr);
}

static pascal OSErr ae_open_doc (const AppleEvent *ae_event, AppleEvent *ae_reply, long ae_ref_count)
{
	OSErr err;
	FSRef	ref;
	AEDescList	docList;
	long	count;

	err = AEGetParamDesc(ae_event, keyDirectObject, typeAEList, &docList);
	if (err) {
		return (noErr);
	}
	err = AECountItems(&docList, &count);
	if (err == noErr) {
		err = AEGetNthPtr(&docList, 1, typeFSRef, NULL, NULL, &ref, sizeof(FSRef), NULL);
		if (err == noErr) {
			char path[4096];
			FSRefMakePath(&ref, (UInt8 *) path, 4096);
			if (main_evt_loop_run) {
				my_argv[1] = strdup(path);
				gf_sys_set_args(2, (const char **) my_argv);
			} else {
				send_open_url(path);
			}
		}
	}
	AEDisposeDesc(&docList);

	if (main_evt_loop_run) {
		QuitApplicationEventLoop();
		main_evt_loop_run = 0;
	}
	return (noErr);
}

void carbon_init ()
{
	my_argv[0] = "GPAC";
	my_argv[1] = NULL;

	open_app_UPP = NewAEEventHandlerUPP(ae_open_app);
	AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, open_app_UPP, 0L, false);
	open_doc_UPP = NewAEEventHandlerUPP(ae_open_doc);
	AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, open_doc_UPP, 0L, false);

	main_evt_loop_run = 1;
	RunApplicationEventLoop();
}

void carbon_uninit()
{

	DisposeAEEventHandlerUPP(open_app_UPP);
	DisposeAEEventHandlerUPP(open_doc_UPP);

	if (my_argv[1]) free(my_argv[1]);
}


