/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2022-2023
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

#if defined(__DARWIN__) || defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif

#include <gpac/setup.h>

#ifndef GPAC_DISABLE_COMPOSITOR

//declare prototype, don't include gpac.h due to conflict in Fixed type between gpac and OSX
void carbon_remove_hook(void);
void carbon_set_hook(void);
void gpac_open_urls(const char *urls);

int gf_dynstrcat(char **str, const char *to_append, const char *sep);

static AEEventHandlerUPP open_doc_UPP;

void carbon_remove_hook()
{
	AERemoveEventHandler(kCoreEventClass, kAEOpenDocuments, open_doc_UPP, false);
	DisposeAEEventHandlerUPP(open_doc_UPP);
}

static pascal OSErr sdl_ae_open_doc(const AppleEvent *ae_event, AppleEvent *ae_reply, long ae_ref_count)
{
	char path[4096];
	OSErr err;
	AEDescList docList;
	long i, count;
	char *urls = NULL;

	err = AEGetParamDesc(ae_event, keyDirectObject, typeAEList, &docList);
	if (err) goto exit;

	err = AECountItems(&docList, &count);
	if (err) goto exit;
	for (i=0; i<count; i++) {
		FSRef ref;
		err = AEGetNthPtr(&docList, i+1, typeFSRef, NULL, NULL, &ref, sizeof(FSRef), NULL);
		if (err) continue;
		FSRefMakePath(&ref, (UInt8 *) path, 4096);
		path[4095]=0;
		gf_dynstrcat(&urls, path, ",");
	}
	if (urls) {
		gpac_open_urls(urls);
		gf_free(urls);
	}

	AEDisposeDesc(&docList);

exit:
	return (noErr);
}

void carbon_set_hook()
{
	open_doc_UPP = NewAEEventHandlerUPP(sdl_ae_open_doc);
	AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, open_doc_UPP, 0L, false);
}


#endif // GPAC_DISABLE_COMPOSITOR
