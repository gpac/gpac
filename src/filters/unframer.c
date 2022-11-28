/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2022
 *					All rights reserved
 *
 *  This file is part of GPAC / unframer filter
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
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
#include <gpac/mpeg4_odf.h>

static GF_Err unframer_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FilterPid *opid = gf_filter_pid_get_udta(pid);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		if (opid) {
			gf_filter_pid_set_udta(pid, NULL);
			gf_filter_pid_remove(opid);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!opid) {
		opid = gf_filter_pid_new(filter);
		if (!opid) return GF_OUT_OF_MEM;
		gf_filter_pid_set_udta(pid, opid);
	}
	//remove unframe property for streams/codecs not using them
	Bool no_unframe=GF_FALSE;
	gf_filter_pid_copy_properties(opid, pid);
	const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) {
		//for now we only have audio video or text using framing formats
		switch (p->value.uint) {
		case GF_STREAM_AUDIO:
		case GF_STREAM_TEXT:
		case GF_STREAM_VISUAL:
			p =gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
			if (!p || !gf_codecid_has_unframer(p->value.uint))
				no_unframe = GF_TRUE;
			break;
		default:
			no_unframe = GF_TRUE;
			break;
		}
	} else {
		//no stream type defined, cannot unframe
		no_unframe = GF_TRUE;
	}

	if (no_unframe)
		gf_filter_pid_set_property(opid, GF_PROP_PID_UNFRAMED, NULL);
	return GF_OK;
}

static GF_Err unframer_process(GF_Filter *filter)
{
	u32 i, count;

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		while (1) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(ipid);
			GF_FilterPid *opid = gf_filter_pid_get_udta(ipid);
			if (!pck) {
				if (opid && gf_filter_pid_is_eos(ipid))
					gf_filter_pid_set_eos(opid);
				break;
			}
			gf_filter_pck_forward(pck, opid);
			gf_filter_pid_drop_packet(ipid);
		}
	}
	return GF_OK;
}


static const GF_FilterCapability UnframerCaps[] =
{
	//all codecs with unframed formats
	CAP_UINT(GF_CAPS_INPUT_OUTPUT | GF_CAPFLAG_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT | GF_CAPFLAG_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_FORCE_UNFRAME, GF_TRUE),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_FALSE),
};

GF_FilterRegister UnframerRegister = {
	.name = "unframer",
	GF_FS_SET_DESCRIPTION("Stream unframer")
	GF_FS_SET_HELP("This filter is used to force reframing of input sources using the same internal framing as GPAC (e.g. ISOBMFF) but with broken framing or signaling.\n"
	"EX gpac -i src.mp4 unframer -o dst.mp4\n"
	"This will:\n"
	"- force input PIDs of unframer to be in serialized form (AnnexB, ADTS, ...)\n"
	"- trigger reframers to be instanciated after the `unframer` filter.\n"
	"Using the unframer filter avoids doing a dump to disk then reimport or other complex data piping."
	)
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_FORCE_REMUX,
	SETCAPS(UnframerCaps),
	.configure_pid = unframer_configure_pid,
	.process = unframer_process,
};

const GF_FilterRegister *unframer_register(GF_FilterSession *session)
{
	return (const GF_FilterRegister *) &UnframerRegister;
}
