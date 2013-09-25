/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Arash Shafiei
 *			Copyright (c) Telecom ParisTech 2000-2013
 *					All rights reserved
 *
 *  This file is part of GPAC / dashcast
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

#include "register.h"

int lock_call_back(void ** mutex, enum AVLockOp op) {
	switch (op) {
	case AV_LOCK_CREATE:
		{
			static int i = 0;
			char mxName[64];
			snprintf(mxName, 64, "AVLIB callback mutex %d", i++);
			*mutex = gf_mx_new(mxName);
			break;
		}
	case AV_LOCK_OBTAIN:
		gf_mx_p(*mutex);
		break;
	case AV_LOCK_RELEASE:
		gf_mx_v(*mutex);
		break;
	case AV_LOCK_DESTROY:
		gf_mx_del(*mutex);
		*mutex = NULL;
		break;
	}

	return 0;
}

void dc_register_libav() {

	av_register_all();
	avcodec_register_all();
	avdevice_register_all();
	avformat_network_init();

	av_lockmgr_register(&lock_call_back);
}

void dc_unregister_libav() {
	av_lockmgr_register(NULL);
}

