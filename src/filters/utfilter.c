/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017
 *					All rights reserved
 *
 *  This file is part of GPAC / filters sub-project
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
#include <gpac/list.h>

typedef struct
{
	GF_FilterPid *src_pid;
	GF_FilterPid *dst_pid;
	GF_SHA1Context *sha_ctx;
	u32 nb_packets, pck_del;
} PIDCtx;

typedef struct
{
	GF_List *pids;

	//0: source, 1: sink, 2: filter
	u32 mode;
	u32 max_pck;
	s32 max_out;
	const char *pid_att;
	Bool alloc;
	u32 nb_pids;
	u32 fwd;
	u32 framing;
	Bool opt_dump;
	Bool norecfg;
	const char *update;

	u64 dummy1;
} GF_UnitTestFilter;

static void test_pck_del(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	PIDCtx *stack = (PIDCtx *) gf_filter_pid_get_udta(pid);
	stack->pck_del++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("%s: Packet deleted - %d out there\n", gf_filter_get_name(filter), stack->nb_packets - stack->pck_del));
}

static void dump_property(GF_FilterPacket *pck, u32 nb_recv, u32 p4cc, const char *pname, const GF_PropertyValue *att)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\tPacket %d Prop %s: ", nb_recv, pname ? pname : gf_4cc_to_str(p4cc)));
	switch (att->type) {
	case GF_PROP_SINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(sint) %d", att->value.sint));
		break;
	case GF_PROP_UINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(uint) %u", att->value.uint));
		break;
	case GF_PROP_LONGSINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(slongint) "LLD, att->value.longsint));
		break;
	case GF_PROP_LONGUINT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(ulongint) "LLU, att->value.longuint));
		break;
	case GF_PROP_FRACTION:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(frac) %d/%d", att->value.frac.num, att->value.frac.den));
		break;
	case GF_PROP_BOOL:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(bool) %s", att->value.boolean ? "true" : "fale"));
		break;
	case GF_PROP_FLOAT:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(float) %f", FIX2FLT(att->value.fnumber) ));
		break;
	case GF_PROP_DOUBLE:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(double) %g", att->value.number));
		break;
	case GF_PROP_NAME:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(const) %s", att->value.string));
		break;
	case GF_PROP_STRING:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(string) %s", att->value.string));
		break;
	case GF_PROP_DATA:
	case GF_PROP_CONST_DATA:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(data) %d bytes", att->data_len));
		break;
	case GF_PROP_POINTER:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(pointer) %p", att->value.ptr));
		break;
	case GF_PROP_FORBIDEN:
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("(forbiden)"));
		break;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("\n"));
}

void dump_properties(GF_FilterPacket *pck, u32 nb_pck)
{
	u32 idx = 0;
	while (1) {
		u32 p4cc;
		const char *pname;
		const GF_PropertyValue *p = gf_filter_pck_enum_properties(pck, &idx, &p4cc, &pname);
		if (!p) break;
		dump_property(pck, nb_pck, p4cc, pname, p);
	}
	if (nb_pck==1) {
		gf_filter_pck_get_property(pck, GF_4CC('c','u','s','t'));
		gf_filter_pck_get_property_str(pck, "custom");
	}
}

static void ut_filter_finalize(GF_Filter *filter)
{
	u32 i, count;
	u8 digest[GF_SHA1_DIGEST_SIZE];
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	count = gf_list_count(stack->pids);
	for (i=0; i<count; i++) {
		PIDCtx *pidctx = gf_list_get(stack->pids, i);
		if (pidctx->sha_ctx) {
			gf_sha1_finish(pidctx->sha_ctx, digest);

			if (!pidctx->src_pid) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[%s] Pid %d Source PID not available while dumping SHA1\n", gf_filter_get_name(filter), i+1 ));
			} else {
				const GF_PropertyValue *p = gf_filter_pid_get_property(pidctx->src_pid, GF_4CC('s','h','a','1') );
				if (!p) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[%s] Pid %d sha1 property not found on input pid\n", gf_filter_get_name(filter), i+1 ));
				} else if (p->data_len != GF_SHA1_DIGEST_SIZE) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[%s] Pid %d wrong size for sha1 property\n", gf_filter_get_name(filter), i+1 ));
				} else if (memcmp(p->value.data, digest, p->data_len )) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[%s] Pid %d wrong hash after execution\n", gf_filter_get_name(filter), i+1 ));
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_FILTER, ("[%s] Pid %d hash OK after execution :)\n", gf_filter_get_name(filter), i+1 ));
				}
			}
		}
		gf_free(pidctx);
	}
	gf_list_del(stack->pids);
}

static void ut_filter_send_update(GF_Filter *filter, u32 nb_pck)
{
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->update && (nb_pck==stack->max_pck/2) ) {
		char *sep, *fid, *name, *val;
		char *cmd = gf_strdup(stack->update);
		fid = cmd;
		sep = strchr(cmd, ',');
		if (sep) {
			sep[0]=0;
			sep+=1;
			name=sep;
			sep = strchr(name, ',');
			if (sep) {
				sep[0]=0;
				val = sep+1;
			} else {
				val=NULL;
			}
			gf_filter_send_update(filter, fid, name, val);
		}
		gf_free(cmd);
	}
}


static GF_Err ut_filter_process_filter(GF_Filter *filter)
{
	const char *data;
	u32 size, i, j, count, nb_loops;
	u32 fwd;
	GF_FilterPacket *pck_dst;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	count = gf_list_count(stack->pids);
	for (i=0; i<count; i++) {
		PIDCtx *pidctx = gf_list_get(stack->pids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pidctx->src_pid);
		if (!pck)
			return GF_OK;

		if ((stack-> max_out>=0) && (pidctx->nb_packets - pidctx->pck_del >= (u32) stack->max_out) ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: No packets to emit, waiting for destruction\n"));
			return GF_OK;
		}
	}

	//loop on each PID
	for (i=0; i<count; i++) {
	u32 pck_size;
	char *data_ptr;
	PIDCtx *pidctx = gf_list_get(stack->pids, i);
	GF_FilterPacket *pck = gf_filter_pid_get_packet(pidctx->src_pid);
	assert (pck);
	assert(pidctx == gf_filter_pid_get_udta(pidctx->src_pid));

	data = gf_filter_pck_get_data(pck, &size);
	gf_sha1_update(pidctx->sha_ctx, (u8*)data, size);

	nb_loops = stack->framing ? 3 : 1;
	pck_size = stack->framing ? size/nb_loops : size;
	data_ptr = (char *)data;

	for (j=0; j<nb_loops; j++) {

	//adjust last packet size
	if ((j+1) == nb_loops) {
		pck_size = size - j*pck_size;
	}

	fwd = stack->fwd;
	if (fwd==3) fwd = pidctx->nb_packets % 3;

	//shared memory
	if (fwd==0) {
		pck_dst = gf_filter_pck_new_shared(pidctx->dst_pid, data_ptr, pck_size, test_pck_del);
	}
	//copy memory
	else if (fwd==1) {
		char *data_dst;
		pck_dst = gf_filter_pck_new_alloc(pidctx->dst_pid, pck_size, &data_dst);
		if (pck_dst) {
			memcpy(data_dst, data_ptr, pck_size);
		}
	}
	//packet reference
	else {
		if (stack->framing) {
			pck_dst = gf_filter_pck_new_ref(pidctx->dst_pid, data_ptr, pck_size, pck);
		} else {
			pck_dst = gf_filter_pck_new_ref(pidctx->dst_pid, NULL, 0, pck);
		}
	}


	if (pck_dst) {
		Bool is_start, is_end;
		//get source packet framing
		gf_filter_pck_get_framing(pck, &is_start, &is_end);
		//adjust flags given our framing
		if (is_start && j) is_start = GF_FALSE;
		if (is_end && (j+1 < nb_loops) ) is_end = GF_FALSE;
		if (stack->framing==2) is_start = GF_FALSE;
		if (stack->framing==3) is_end = GF_FALSE;

		gf_filter_pck_set_framing(pck_dst, is_start, is_end);
		
		pidctx->nb_packets++;
		//copy over src props to dst
		gf_filter_pck_merge_properties(pck, pck_dst);
		gf_filter_pck_send(pck_dst);
	}
	//move our data pointer
	data_ptr += pck_size;

	} //end framing loop

	gf_filter_pid_drop_packet(pidctx->src_pid);

	} //end PID loop

	return GF_OK;

}

static GF_Err ut_filter_process_source(GF_Filter *filter)
{
	GF_PropertyValue p;
	GF_FilterPacket *pck;
	u32 i, count;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	count = gf_list_count(stack->pids);
	for (i=0; i<count; i++) {
	PIDCtx *pidctx=gf_list_get(stack->pids, i);

	if (pidctx->nb_packets==stack->max_pck) {
		return GF_EOS;
	}

	if ((stack->max_out>=0) && (pidctx->nb_packets - pidctx->pck_del >= (u32) stack->max_out) ) {
		GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: No packets to emit, waiting for destruction\n"));
		return GF_OK;
	}
	pidctx->nb_packets++;

	if (stack->alloc) {
		char *data;
		pck = gf_filter_pck_new_alloc(pidctx->dst_pid, 10, &data);
		memcpy(data, "PacketCopy", 10);
		gf_sha1_update(pidctx->sha_ctx, "PacketCopy", 10);
	} else {
		pck = gf_filter_pck_new_shared(pidctx->dst_pid, "PacketShared", 12, test_pck_del);
		gf_sha1_update(pidctx->sha_ctx, "PacketShared", 12);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSource: pck %d PacketShared\n", pidctx->nb_packets));

	p.type = GF_PROP_NAME;
	p.value.string = "custom_value";
	gf_filter_pck_set_property(pck, GF_4CC('c','u','s','t'), &p);

	//try all our properties
	if (pidctx->nb_packets==1) {
		p.type = GF_PROP_BOOL;
		p.value.boolean = GF_TRUE;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','1'), &p);

		p.type = GF_PROP_SINT;
		p.value.sint = -1;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','2'), &p);

		p.type = GF_PROP_UINT;
		p.value.uint = 1;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','3'), &p);

		p.type = GF_PROP_LONGSINT;
		p.value.longsint = -1;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','4'), &p);

		p.type = GF_PROP_LONGUINT;
		p.value.longuint = 1;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','5'), &p);

		p.type = GF_PROP_FLOAT;
		p.value.fnumber = 1.0f;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','6'), &p);

		p.type = GF_PROP_DOUBLE;
		p.value.fnumber = 1.0;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','7'), &p);

		p.type = GF_PROP_FRACTION;
		p.value.frac.den = 1;
		p.value.frac.num = 1;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','8'), &p);

		p.type = GF_PROP_POINTER;
		p.value.ptr = pck;
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','9'), &p);

		p.type = GF_PROP_DATA;
		p.value.data = (char *) pidctx;
		p.data_len = sizeof(pidctx);
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','a'), &p);

		p.type = GF_PROP_CONST_DATA;
		p.value.data = (char *) pidctx;
		p.data_len = sizeof(pidctx);
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','b'), &p);

		p.type = GF_PROP_STRING;
		p.value.string = "custom";
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','c'), &p);

		p.type = GF_PROP_BOOL;
		p.value.boolean = GF_TRUE;
		gf_filter_pck_set_property_str(pck, "cusd", &p);

		p.type = GF_PROP_BOOL;
		p.value.boolean = GF_TRUE;
		gf_filter_pck_set_property_dyn(pck, "cuse", &p);
	}

	ut_filter_send_update(filter, pidctx->nb_packets);

	if (pidctx->nb_packets==stack->max_pck) {
		if (pidctx->sha_ctx) {
			u8 digest[GF_SHA1_DIGEST_SIZE];
			gf_sha1_finish(pidctx->sha_ctx, digest);
			pidctx->sha_ctx = NULL;
			p.type = GF_PROP_DATA;
			p.data_len = GF_SHA1_DIGEST_SIZE;
			p.value.data = (char *) digest;
			//with this we test both:
			//- SHA of send data is correct at the receiver side
			//- property update on a PID
			gf_filter_pid_set_property(pidctx->dst_pid, GF_4CC('s','h','a','1'), &p);
		}
	}
	//just for coverage: check keeping a reference to the packet
	gf_filter_pck_ref(pck);

	gf_filter_pck_send(pck);

	//and destroy the reference it
	gf_filter_pck_unref(pck);

	} //end PID loop

	return GF_OK;
}


static GF_Err ut_filter_process_sink(GF_Filter *filter)
{
	u32 size, i, count;
	const char *data;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	count = gf_list_count(stack->pids);
	for (i=0; i<count; i++) {
	PIDCtx *pidctx=gf_list_get(stack->pids, i);

	GF_FilterPacket *pck = gf_filter_pid_get_packet(pidctx->src_pid);
	if (!pck)
		return GF_OK;

	data = gf_filter_pck_get_data(pck, &size);

	if (stack->opt_dump && !pidctx->nb_packets) {
		GF_PropertyValue p;
		Bool old_strict = gf_log_set_strict_error(GF_FALSE);
		gf_filter_pck_send(pck);
		gf_filter_pck_set_property(pck, GF_4CC('c','u','s','t'), &p);
		gf_filter_pck_merge_properties(pck, pck);
		gf_filter_pck_set_framing(pck, GF_TRUE, GF_FALSE);
		gf_log_set_strict_error(old_strict);
	}

	gf_sha1_update(pidctx->sha_ctx, (u8*)data, size);

	pidctx->nb_packets++;
	GF_LOG(GF_LOG_INFO, GF_LOG_FILTER, ("TestSink: Consuming packet %d bytes\n", size));

	dump_properties(pck, pidctx->nb_packets);

	gf_filter_pid_drop_packet(pidctx->src_pid);

	} //end PID loop
	return GF_OK;
}


static GF_Err ut_filter_process(GF_Filter *filter)
{
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->mode==0) {
		return ut_filter_process_source(filter);
	}
	else if (stack->mode==1) {
		return ut_filter_process_sink(filter);
	}
	else if (stack->mode==2) {
		return ut_filter_process_filter(filter);
	} else {
		return GF_BAD_PARAM;
	}

}

static GF_Err ut_filter_config_input(GF_Filter *filter, GF_FilterPid *pid, GF_PID_ConfigState state)
{
	const GF_PropertyValue *format;
	GF_PropertyValue p;
	PIDCtx *pidctx;
	u32 i, count;
	GF_UnitTestFilter  *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	if (stack->mode==0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error: Attempt to connect PID on source filter\n"));
		return GF_BAD_PARAM;
	}
	//for both filter and sink modes, check input format

	count = gf_list_count(stack->pids);
	for (i=0; i<count; i++) {
	pidctx = gf_list_get(stack->pids, i);

	//something is being reconfigured. We check we have the same custum arg, otherwise we do not support
	if (pidctx->src_pid == pid) {
		format = gf_filter_pid_get_property(pidctx->src_pid, GF_4CC('c','u','s','t') );
		if (!format || !format->value.string || strcmp(format->value.string, stack->pid_att)) {
			return GF_NOT_SUPPORTED;
		}
		//filter mode, set properties on output
		if (stack->mode==2) {
			//this is not needed since copy_properties does that, used for coverage/tests
			gf_filter_pid_reset_properties(pidctx->dst_pid);
			gf_filter_pid_copy_properties(pidctx->dst_pid, pidctx->src_pid);
		}
		return GF_OK;
	}

	}

	//new PID
	GF_SAFEALLOC(pidctx, PIDCtx);
	pidctx->src_pid = pid;
	gf_list_add(stack->pids, pidctx);

	//check our functions
	format = gf_filter_pid_get_property_str(pidctx->src_pid, "custom1");
	if (!format) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("%s: expecting property string custom1 on PID\n", gf_filter_get_name(filter) ));
	}
	format = gf_filter_pid_get_property_str(pidctx->src_pid, "custom2");
	if (!format) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("%s: expecting property string custom2 on PID\n", gf_filter_get_name(filter) ));
	}

	format = gf_filter_pid_get_property(pidctx->src_pid, GF_4CC('c','u','s','t') );
	if (!format || !format->value.string || strcmp(format->value.string, stack->pid_att)) {
		return GF_NOT_SUPPORTED;
	}
	if (stack->opt_dump) {
		char *data;
		Bool old_strict = gf_log_set_strict_error(GF_FALSE);
		gf_filter_pid_set_property(pidctx->src_pid, GF_4CC('s','h','a','1'), format);
		gf_filter_pid_reset_properties(pidctx->src_pid);
		gf_filter_pck_new_alloc(pidctx->src_pid, 20, &data);
		gf_filter_pck_new_shared(pidctx->src_pid, "foo", 3, NULL);
		gf_filter_pck_new_ref(pidctx->src_pid, "foo", 3, NULL);
		gf_log_set_strict_error(old_strict);
	}

	//filter mode, setup output
	if (stack->mode==2) {
		pidctx->dst_pid = gf_filter_pid_new(filter);
		p.type=GF_PROP_NAME;
		p.value.string = (char *) stack->pid_att;
		gf_filter_pid_copy_properties(pidctx->dst_pid, pidctx->src_pid);

		if (stack->opt_dump) {
			Bool old_strict = gf_log_set_strict_error(GF_FALSE);
			gf_filter_pid_copy_properties(pidctx->src_pid, pidctx->dst_pid);
			gf_filter_pid_get_packet(pidctx->dst_pid);
			gf_filter_pid_drop_packet(pidctx->dst_pid);
			gf_filter_pid_drop_packet(pidctx->src_pid);
			gf_log_set_strict_error(old_strict);
		}

		gf_filter_pid_set_property(pidctx->dst_pid, GF_4CC('c','u','s','t'), &p);

		gf_filter_pid_set_udta(pidctx->dst_pid, pidctx);
		gf_filter_pid_set_udta(pidctx->src_pid, pidctx);

		gf_filter_pid_set_framing_mode(pidctx->src_pid, GF_TRUE);
		pidctx->sha_ctx = gf_sha1_starts();
	}
	//sink mode, request full reconstruction of input blocks or not depending on framing mode 
	else {
		gf_filter_pid_set_framing_mode(pidctx->src_pid, stack->framing ? GF_FALSE : GF_TRUE);
		pidctx->sha_ctx = gf_sha1_starts();
	}

	return GF_OK;
}


static GF_Err ut_filter_config_source(GF_Filter *filter)
{
	GF_PropertyValue p;
	PIDCtx *pidctx;
	u32 i;
	GF_UnitTestFilter *stack = (GF_UnitTestFilter *) gf_filter_get_udta(filter);

	for (i=0; i<stack->nb_pids; i++) {
	char szName[10];

	//create a pid
	GF_SAFEALLOC(pidctx, PIDCtx);
	gf_list_add(stack->pids, pidctx);
	pidctx->dst_pid = gf_filter_pid_new(filter);
	gf_filter_pid_set_udta(pidctx->dst_pid, pidctx);

	//set a custum property
	p.type = GF_PROP_NAME;
	p.value.string = (char *) stack->pid_att;
	gf_filter_pid_set_property(pidctx->dst_pid, GF_4CC('c','u','s','t'), &p);

	//for coverage
	gf_filter_pid_set_property_str(pidctx->dst_pid, "custom1", &p);
	gf_filter_pid_set_property_dyn(pidctx->dst_pid, "custom2", &p);

	sprintf(szName, "PID%d", i+1);
	p.type = GF_PROP_STRING;
	p.value.string = szName;
	gf_filter_pid_set_property(pidctx->dst_pid, GF_FILTER_PID_NAME, &p);

	if (stack->opt_dump) {
		Bool old_strict = gf_log_set_strict_error(GF_FALSE);
		gf_filter_pid_set_framing_mode(pidctx->dst_pid, GF_TRUE);
		gf_log_set_strict_error(old_strict);
	}

	pidctx->sha_ctx = gf_sha1_starts();

	}//end pid loop
	return GF_OK;
}


static GF_Err ut_filter_update_arg(GF_Filter *filter, const char *arg_name, GF_PropertyValue *arg_val)
{
	return GF_OK;
}

GF_Err utfilter_initialize(GF_Filter *filter)
{
	GF_PropertyValue p;
	GF_UnitTestFilter *stack = gf_filter_get_udta(filter);

	stack->pids = gf_list_new();

	if (stack->opt_dump) {
		Bool old_strict;
		char szFmt[40];
		s64 val;
		p = gf_props_parse_value(GF_PROP_BOOL, "prop", "true", NULL);
		if (p.value.boolean != GF_TRUE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing boolean value"));
		}
		p = gf_props_parse_value(GF_PROP_BOOL, "prop", "yes", NULL);
		if (p.value.boolean != GF_TRUE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing boolean value"));
		}
		p = gf_props_parse_value(GF_PROP_BOOL, "prop", "no", NULL);
		if (p.value.boolean != GF_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing boolean value"));
		}
		p = gf_props_parse_value(GF_PROP_BOOL, "prop", "false", NULL);
		if (p.value.boolean != GF_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing boolean value"));
		}
		p = gf_props_parse_value(GF_PROP_SINT, "prop", "-1", NULL);
		if (p.value.sint != -1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing sint value"));
		}
		p = gf_props_parse_value(GF_PROP_UINT, "prop", "1", NULL);
		if (p.value.uint != 1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing uint value"));
		}
		val = 0xFFFFFFFF;
		val *= 2;
		sprintf(szFmt, ""LLD, -val);
		p = gf_props_parse_value(GF_PROP_LONGSINT, "prop", szFmt, NULL);
		if (p.value.longsint != -val) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing longsint value"));
		}
		sprintf(szFmt, ""LLU, val);
		p = gf_props_parse_value(GF_PROP_LONGUINT, "prop", szFmt, NULL);
		if (p.value.longuint != val) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing longuint value"));
		}
		p = gf_props_parse_value(GF_PROP_FLOAT, "prop", "1.0", NULL);
		if (p.value.fnumber != FIX_ONE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing float value"));
		}
		p = gf_props_parse_value(GF_PROP_DOUBLE, "prop", "1.0", NULL);
		if (p.value.number != 1.0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing double value"));
		}
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", "1000/1", NULL);
		if ((p.value.frac.den != 1) || (p.value.frac.num != 1000)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing fraction value"));
		}
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", "1000", NULL);
		if ((p.value.frac.den != 1) || (p.value.frac.num != 1000)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing fraction value"));
		}
		p = gf_props_parse_value(GF_PROP_STRING, "prop", "test", NULL);
		sprintf(szFmt, "%p:%d", stack, (u32) sizeof(stack));
		p = gf_props_parse_value(GF_PROP_DATA, "prop", szFmt, NULL);
		if ((p.value.data != (char *) stack) || (p.data_len != (u32) sizeof(stack))) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing data value"));
		}
		p = gf_props_parse_value(GF_PROP_CONST_DATA, "prop", szFmt, NULL);
		if ((p.value.data != (char *) stack) || (p.data_len != (u32) sizeof(stack))) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing data value"));
		}
		sprintf(szFmt, "%p", stack);
		p = gf_props_parse_value(GF_PROP_POINTER, "prop", szFmt, NULL);
		if (p.value.ptr != stack) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Error parsing data value"));
		}

		old_strict = gf_log_set_strict_error(GF_FALSE);

		p = gf_props_parse_value(GF_PROP_BOOL, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_SINT, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_UINT, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_LONGSINT, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_LONGUINT, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_FLOAT, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_DOUBLE, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_STRING, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_DATA, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_CONST_DATA, "prop", "", NULL);
		p = gf_props_parse_value(GF_PROP_POINTER, "prop", "", NULL);

		p = gf_props_parse_value(GF_PROP_BOOL, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_SINT, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_UINT, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_LONGSINT, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_LONGUINT, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_FLOAT, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_DOUBLE, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_FRACTION, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_STRING, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_DATA, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_CONST_DATA, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_POINTER, "prop", NULL, NULL);
		p = gf_props_parse_value(GF_PROP_UINT, "prop", "test", "foo|bar");
		p = gf_props_parse_value(100, "prop", "test", NULL);

		gf_log_set_strict_error(old_strict);

		gf_props_get_type_name(GF_PROP_BOOL);
		gf_props_get_type_name(GF_PROP_SINT);
		gf_props_get_type_name(GF_PROP_UINT);
		gf_props_get_type_name(GF_PROP_LONGSINT);
		gf_props_get_type_name(GF_PROP_LONGUINT);
		gf_props_get_type_name(GF_PROP_FRACTION);
		gf_props_get_type_name(GF_PROP_FLOAT);
		gf_props_get_type_name(GF_PROP_DOUBLE);
		gf_props_get_type_name(GF_PROP_DATA);
		gf_props_get_type_name(GF_PROP_CONST_DATA);
		gf_props_get_type_name(GF_PROP_NAME);
		gf_props_get_type_name(GF_PROP_STRING);
		gf_props_get_type_name(GF_PROP_POINTER);
	}


	switch (stack->mode) {
	case 1:// "sink"
		gf_filter_set_name(filter, "UTSink");
		return GF_OK;
	case 2:// "filter"
		gf_filter_set_name(filter, "UTFilter");
		return GF_OK;
	case 0: // "source"
		gf_filter_set_name(filter, "UTSource");
		return ut_filter_config_source(filter);
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[UTFilter] Unrecognized filter mode %d\n", stack->mode));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(GF_UnitTestFilter, _n)
static const GF_FilterArgs UTFilterArgs[] =
{
	{ OFFS(mode), "Set filter operation mode", GF_PROP_UINT, "source", "source|sink|filter", GF_FALSE},
	{ OFFS(pid_att), "Sets default value for PID \"cust\" attribute", GF_PROP_NAME, "UTSourceData", NULL, GF_FALSE},
	{ OFFS(max_pck), "Maximum number of packets to send in source mode", GF_PROP_UINT, "1000", NULL, GF_FALSE},
	{ OFFS(nb_pids), "Number of PIDs in source mode", GF_PROP_UINT, "1", "1-+I", GF_FALSE},
	{ OFFS(max_out), "Maximum number of shared packets not yet released in source/filter mode, no limit if -1", GF_PROP_SINT, "-1", NULL, GF_TRUE},
	{ OFFS(alloc), "Uses allocated memory packets in source mode", GF_PROP_BOOL, "false", NULL, GF_TRUE},
	{ OFFS(fwd), "Indicates packet forward mode for filter.\n\t\tshared: uses shared memory (dangerous)\n\t\tcopy: uses copy\n\t\tref: uses references to source packet\n\t\tmix: change mode at each packet sent", GF_PROP_UINT, "shared", "shared|copy|ref|mix", GF_TRUE},
	{ OFFS(framing), "Divides packets in 3 for filter mode, or allows partial blocks for sink mode", GF_PROP_UINT, "none", "none|default|nostart|noend", GF_TRUE},
	{ OFFS(update), "sends update message after half packet send. Update format is FID,argname,argval", GF_PROP_STRING, NULL, NULL, GF_TRUE},
	{ OFFS(opt_dump), "Dumps options and exercise error cases for code coverage", GF_PROP_BOOL, "false", NULL, GF_TRUE},
	{ OFFS(norecfg), "Disabled reconfig on input pid in filter/sink mode", GF_PROP_BOOL, "false", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_LONGSINT, "0", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_LONGUINT, "0", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_FLOAT, "0", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_DOUBLE, "0", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_FRACTION, "0", NULL, GF_TRUE},
	{ OFFS(dummy1), "dummy for coverage", GF_PROP_POINTER, "0", NULL, GF_TRUE},
	{ NULL }
};

const GF_FilterRegister UTFilterRegister = {
	.name = "UTFilter",
	.description = "Unit Test Filter, only used for unit testing of filter framework",
	.private_size = sizeof(GF_UnitTestFilter),
	.args = UTFilterArgs,
	.initialize = utfilter_initialize,
	.finalize = ut_filter_finalize,
	.process = ut_filter_process,
	.configure_pid = ut_filter_config_input,
	.update_arg = ut_filter_update_arg
};


const GF_FilterRegister *ut_filter_register()
{
	return &UTFilterRegister;
}



