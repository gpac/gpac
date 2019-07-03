/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / MP3 reframer filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

typedef struct
{
	u64 pos;
	Double duration;
} MP3Idx;

typedef struct
{
	//filter args
	Double index;
	Bool expart;

	//only one input pid declared
	GF_FilterPid *ipid;
	//only one output pid declared
	GF_FilterPid *opid;

	GF_BitStream *bs;
	u64 file_pos, cts, prev_cts;
	u32 sr, nb_ch, codecid;
	GF_Fraction duration;
	Double start_range;
	Bool in_seek;
	u32 timescale;
	Bool is_playing;
	Bool is_file;
	Bool initial_play_done, file_loaded;

	u32 hdr;

	u8 *mp3_buffer;
	u32 mp3_buffer_size, mp3_buffer_alloc, resume_from;
	u64 byte_offset;

	GF_FilterPacket *src_pck;

	Bool recompute_cts;
	MP3Idx *indexes;
	u32 index_alloc_size, index_size;

	u32 tag_size;
	u8 *id3_buffer;
	u32 id3_buffer_size, id3_buffer_alloc;

	GF_FilterPid *vpid;
} GF_MP3DmxCtx;




GF_Err mp3_dmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		gf_filter_pid_remove(ctx->opid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) ctx->timescale = p->value.uint;

	p = gf_filter_pid_get_property_str(pid, "nocts");
	if (p && p->value.boolean) ctx->recompute_cts = GF_TRUE;
	else ctx->recompute_cts = GF_FALSE;

	if (ctx->timescale && !ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL);
	}
	return GF_OK;
}

static void mp3_dmx_check_dur(GF_Filter *filter, GF_MP3DmxCtx *ctx)
{
	FILE *stream;
	u64 duration, cur_dur;
	s32 prev_sr = -1;
	const GF_PropertyValue *p;
	if (!ctx->opid || ctx->timescale || ctx->file_loaded) return;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p || !p->value.string) {
		ctx->is_file = GF_FALSE;
		return;
	}
	ctx->is_file = GF_TRUE;

	stream = gf_fopen(p->value.string, "rb");
	if (!stream) return;

	ctx->index_size = 0;

	duration = 0;
	cur_dur = 0;
	while (1) {
		u32 sr, dur;
		u64 pos;
		u32 hdr = gf_mp3_get_next_header(stream);
		if (!hdr) break;
		sr = gf_mp3_sampling_rate(hdr);

		if ((prev_sr>=0) && (prev_sr != sr)) {
			duration *= sr;
			duration /= prev_sr;

			cur_dur *= sr;
			cur_dur /= prev_sr;
		}
		prev_sr = sr;
		dur = gf_mp3_window_size(hdr);
		duration += dur;
		cur_dur += dur;
		pos = gf_ftell(stream);
		if (cur_dur > ctx->index * prev_sr) {
			if (!ctx->index_alloc_size) ctx->index_alloc_size = 10;
			else if (ctx->index_alloc_size == ctx->index_size) ctx->index_alloc_size *= 2;
			ctx->indexes = gf_realloc(ctx->indexes, sizeof(MP3Idx)*ctx->index_alloc_size);
			ctx->indexes[ctx->index_size].pos = pos - 4;
			ctx->indexes[ctx->index_size].duration = (Double) duration;
			ctx->indexes[ctx->index_size].duration /= prev_sr;
			ctx->index_size ++;
			cur_dur = 0;
		}

		pos = gf_ftell(stream);
		gf_fseek(stream, pos + gf_mp3_frame_size(hdr) - 4, SEEK_SET);
	}
	gf_fclose(stream);

	if (!ctx->duration.num || (ctx->duration.num  * prev_sr != duration * ctx->duration.den)) {
		ctx->duration.num = (s32) duration;
		ctx->duration.den = prev_sr ;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILE_CACHED);
	if (p && p->value.boolean) ctx->file_loaded = GF_TRUE;
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CAN_DATAREF, & PROP_BOOL(GF_TRUE ) );
}


#include <gpac/utf.h>
static void mp3_dmx_id3_set_string(GF_MP3DmxCtx *ctx, char *name, u8 *buf)
{
	if ((buf[0]==0xFF) || (buf[0]==0xFE)) {
		const u16 *sptr = (u16 *) (buf+2);
		s32 len = (s32) ( UTF8_MAX_BYTES_PER_CHAR * gf_utf8_wcslen(sptr) );
		char *tmp = gf_malloc(len+1);
		len = (s32) gf_utf8_wcstombs(tmp, len, &sptr);
		if (len>=0) {
			tmp[len] = 0;
			gf_filter_pid_set_property_dyn(ctx->opid, name, &PROP_STRING(tmp) );
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[MP3Dmx] Corrupted ID3 text frame %s\n", name));
		}
		gf_free(tmp);
	} else {
		gf_filter_pid_set_property_dyn(ctx->opid, name, &PROP_STRING(buf) );
	}
}

static void mp3_dmx_flush_id3(GF_Filter *filter, GF_MP3DmxCtx *ctx)
{
	GF_BitStream *bs = gf_bs_new(ctx->id3_buffer, ctx->id3_buffer_size, GF_BITSTREAM_READ);
	char *buf=NULL;
	u32 buf_alloc=0;
	gf_bs_skip_bytes(bs, 3);
	/*u8 major = */gf_bs_read_u8(bs);
	/*u8 minor = */gf_bs_read_u8(bs);
	/*u8 unsync = */gf_bs_read_int(bs, 1);
	u8 ext_hdr = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 6);
	u32 size = gf_id3_read_size(bs);


	if (ext_hdr) {

	}

	while (size && (gf_bs_available(bs)>=10) ) {
		char szTag[1024];
		char *sep, *sep_desc;
		u32 pic_size;
		//u32 pic_type;
		u32 ftag = gf_bs_read_u32(bs);
		u32 fsize = gf_id3_read_size(bs);
		/*u16 fflags = */gf_bs_read_u16(bs);

		size -= 10;
		if (!fsize)
			break;

		if (size<fsize) {
			fprintf(stderr, "Broken ID3 frame tag %s, size %d but remaining bytes %d\n", gf_4cc_to_str(ftag), fsize, size);
			break;
		}

		if (buf_alloc<=fsize) {
			buf = gf_realloc(buf, fsize+2);
			buf_alloc = fsize+2;
		}

		gf_bs_read_data(bs, buf, fsize);
		buf[fsize]=0;
		buf[fsize+1]=0;

		switch (ftag) {
		case ID3V2_FRAME_TIT2:
			mp3_dmx_id3_set_string(ctx, "tag:title", buf+1);
			break;
		case ID3V2_FRAME_TPE1:
			mp3_dmx_id3_set_string(ctx, "tag:artist", buf+1);
			break;
		case ID3V2_FRAME_TALB:
			mp3_dmx_id3_set_string(ctx, "tag:album", buf+1);
			break;
		case ID3V2_FRAME_TCON:
			mp3_dmx_id3_set_string(ctx, "tag:genre", buf+1);
			break;
		case ID3V2_FRAME_TDRC:
			mp3_dmx_id3_set_string(ctx, "tag:recdate", buf+1);
			break;
		case ID3V2_FRAME_TRCK:
			mp3_dmx_id3_set_string(ctx, "tag:tracknum", buf+1);
			break;
		case ID3V2_FRAME_TSSE:
			mp3_dmx_id3_set_string(ctx, "tag:encoder", buf+1);
			break;
		case ID3V2_FRAME_TXXX:
			sep = memchr(buf, 0, fsize);
			if (sep) {
				strcpy(szTag, "tag:");
				strncat(szTag, buf+1, 1023);
				mp3_dmx_id3_set_string(ctx, szTag, sep+1);
			}
			break;

		//we forward as is the APIC tag for now
		case ID3V2_FRAME_APIC:
			if (ctx->expart) {
				//first char is text encoding
				//then mime
				sep = memchr(buf+1, 0, fsize-1);
				/*pic_type = sep[1];*/
				sep_desc = memchr(sep+2, 0, fsize-1);

				if (sep_desc) {
					GF_Err e;
					pic_size = (u32) ( (sep_desc + 1) - buf);
					pic_size = fsize - pic_size;

					e = gf_filter_pid_raw_new(filter, NULL, NULL, buf+1, NULL, sep_desc+1, pic_size, GF_FALSE, &ctx->vpid);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[MP3Dmx] error setting up video pid for cover art: %s\n", gf_error_to_string(e) ));
					}
					if (ctx->vpid) {
						u8 *out_buffer;
						GF_FilterPacket *dst_pck;
						gf_filter_pid_set_name(ctx->vpid, "CoverArt");
						dst_pck = gf_filter_pck_new_alloc(ctx->vpid, pic_size, &out_buffer);
						gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
						memcpy(out_buffer, sep_desc+1, pic_size);
						gf_filter_pck_send(dst_pck);
					}
				}
			}
			//fallthrough
		default:
			sprintf(szTag, "tag:%s", gf_4cc_to_str(ftag));

			if ((ftag>>24) == 'T') {
				mp3_dmx_id3_set_string(ctx, szTag, buf+1);
			} else {
				gf_filter_pid_set_property_str(ctx->opid, szTag, &PROP_DATA(buf, fsize) );
			}
			break;
		}
		size -= fsize;
	}

	gf_bs_del(bs);
	if (buf) gf_free(buf);
	ctx->id3_buffer_size = 0;
	if (ctx->vpid)
		gf_filter_pid_set_eos(ctx->vpid);

}

static void mp3_dmx_check_pid(GF_Filter *filter, GF_MP3DmxCtx *ctx)
{
	u32 sr;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		mp3_dmx_check_dur(filter, ctx);
	}

	if ((ctx->sr == gf_mp3_sampling_rate(ctx->hdr)) && (ctx->nb_ch == gf_mp3_num_channels(ctx->hdr) )
		&& (ctx->codecid == gf_mp3_object_type_indication(ctx->hdr) )
	)
		return;

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, & PROP_UINT( GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, NULL );
	if (ctx->is_file && ctx->index) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PLAYBACK_MODE, & PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD) );
	}
	if (ctx->duration.num)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DURATION, & PROP_FRAC(ctx->duration));

	if (!ctx->timescale) gf_filter_pid_set_name(ctx->opid, "audio");

	ctx->nb_ch = gf_mp3_num_channels(ctx->hdr);
	ctx->codecid = gf_mp3_object_type_indication(ctx->hdr);
	sr = gf_mp3_sampling_rate(ctx->hdr);

	if (!ctx->timescale) {
		//we change sample rate, change cts
		if (ctx->cts && ctx->sr && (ctx->sr != sr)) {
			ctx->cts *= sr;
			ctx->cts /= ctx->sr;
		}
	}
	ctx->sr = sr;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, & PROP_UINT(ctx->timescale ? ctx->timescale : sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, & PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, & PROP_UINT(ctx->nb_ch) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, & PROP_UINT(gf_mp3_object_type_indication(ctx->hdr) ) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, & PROP_UINT(gf_mp3_window_size(ctx->hdr) ) );

	if (ctx->id3_buffer_size)
		mp3_dmx_flush_id3(filter, ctx);

}

static Bool mp3_dmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_FilterEvent fevt;
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);

	if (evt->base.on_pid != ctx->opid) return GF_TRUE;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->is_playing) {
			ctx->is_playing = GF_TRUE;
		}
		if (! ctx->is_file) {
			if (evt->play.start_range || ctx->initial_play_done) {
				ctx->mp3_buffer_size = 0;
				ctx->resume_from = 0;
			}
			ctx->initial_play_done = GF_TRUE;
			return GF_FALSE;
		}
		mp3_dmx_check_dur(filter, ctx);

		ctx->start_range = evt->play.start_range;
		ctx->in_seek = GF_TRUE;
		ctx->file_pos = 0;
		if (ctx->start_range) {
			for (i=1; i<ctx->index_size; i++) {
				if (ctx->indexes[i].duration>ctx->start_range) {
					ctx->cts = (u64) (ctx->indexes[i-1].duration * ctx->sr);
					ctx->file_pos = ctx->indexes[i-1].pos;
					break;
				}
			}
		}
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (!ctx->file_pos)
				return GF_TRUE;
		}
		ctx->mp3_buffer_size = 0;
		ctx->resume_from = 0;
		//post a seek
		GF_FEVT_INIT(fevt, GF_FEVT_SOURCE_SEEK, ctx->ipid);
		fevt.seek.start_offset = ctx->file_pos;
		gf_filter_pid_send_event(ctx->ipid, &fevt);

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
		ctx->src_pck = NULL;
		//don't cancel event
		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static GFINLINE void mp3_dmx_update_cts(GF_MP3DmxCtx *ctx)
{
	u32 nb_samp = gf_mp3_window_size(ctx->hdr);
	if (ctx->timescale) {
		u64 inc = nb_samp;
		inc *= ctx->timescale;
		inc /= ctx->sr;
		ctx->cts += inc;
	} else {
		ctx->cts += nb_samp;
	}
}

GF_Err mp3_dmx_process(GF_Filter *filter)
{
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u8 *start;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		mp3_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->mp3_buffer_size) {
				gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->mp3_buffer_size;
	if (pck && !ctx->resume_from) {
		data = (char *) gf_filter_pck_get_data(pck, &pck_size);

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->mp3_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->mp3_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->mp3_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->mp3_buffer_size;
				}
			}
		}

		if (ctx->mp3_buffer_size + pck_size > ctx->mp3_buffer_alloc) {
			ctx->mp3_buffer_alloc = ctx->mp3_buffer_size + pck_size;
			ctx->mp3_buffer = gf_realloc(ctx->mp3_buffer, ctx->mp3_buffer_alloc);
		}
		memcpy(ctx->mp3_buffer + ctx->mp3_buffer_size, data, pck_size);
		ctx->mp3_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->mp3_buffer_size;
	start = ctx->mp3_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	while (remain) {
		u8 *sync;
		Bool skip_id3v1=GF_FALSE;
		u32 bytes_skipped=0, size, nb_samp, bytes_to_drop=0;;

		if (!ctx->tag_size && (remain>3)) {

			/* Did we read an ID3v2 ? */
			if (start[0] == 'I' && start[1] == 'D' && start[2] == '3') {
				if (remain<10)
					return GF_OK;

				ctx->tag_size = ((start[9] & 0x7f) + ((start[8] & 0x7f) << 7) + ((start[7] & 0x7f) << 14) + ((start[6] & 0x7f) << 21));

				bytes_to_drop = 10;
				if (ctx->id3_buffer_alloc < ctx->tag_size+10) {
					ctx->id3_buffer = gf_realloc(ctx->id3_buffer, ctx->tag_size+10);
					ctx->id3_buffer_alloc = ctx->tag_size+10;
				}
				memcpy(ctx->id3_buffer, start, 10);
				ctx->id3_buffer_size = 10;
				goto drop_byte;
			}
		}
		if (ctx->tag_size) {
			if (ctx->tag_size>remain) {
				bytes_to_drop = remain;
				ctx->tag_size-=remain;
			} else {
				bytes_to_drop = ctx->tag_size;
				ctx->tag_size = 0;
			}
			memcpy(ctx->id3_buffer + ctx->id3_buffer_size, start, bytes_to_drop);
			ctx->id3_buffer_size += bytes_to_drop;

			if (!ctx->tag_size && ctx->opid) {
				mp3_dmx_flush_id3(filter, ctx);
			}
			goto drop_byte;

		}

		ctx->hdr = gf_mp3_get_next_header_mem(start, remain, &bytes_skipped);

		//couldn't find sync byte in this packet
		if (!ctx->hdr) {
			break;
		}
		sync = start + bytes_skipped;

		size = gf_mp3_frame_size(ctx->hdr);


		//ready to send packet
		if (size + 1 < remain-bytes_skipped) {
			//make sure we are sync!
			if (sync[size] !=0xFF) {
				if ((sync[size]=='T') && (sync[size+1]=='A') && (sync[size+2]=='G')) {
					skip_id3v1=GF_TRUE;
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[MP3Dmx] invalid frame, resyncing\n"));
					goto drop_byte;
				}
			}
		}
		//otherwise wait for next frame, unless if end of stream
		else if (pck) {
			break;
		}
		//ready to send packet
		mp3_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) {
			ctx->resume_from = (u32) (sync - ctx->mp3_buffer + 1);
			return GF_OK;
		}

		nb_samp = gf_mp3_window_size(ctx->hdr);

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * ctx->sr);
			if (ctx->cts + nb_samp >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + nb_samp ) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		bytes_to_drop = bytes_skipped + size;
		if (ctx->timescale && !prev_pck_size && (cts != GF_FILTER_NO_TS) ) {
			ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			memcpy(output, sync, size);

			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, nb_samp);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset + bytes_skipped);
			}

			gf_filter_pck_send(dst_pck);
		}
		mp3_dmx_update_cts(ctx);

		//TODO, parse id3v1 ??
		if (skip_id3v1)
			bytes_to_drop+=128;

		//truncated last frame
		if (bytes_to_drop>remain) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[MP3Dmx] truncated frame!\n"));
			bytes_to_drop=remain;
		}

drop_byte:
		if (!bytes_to_drop) {
			bytes_to_drop = 1;
		}
		start += bytes_to_drop;
		remain -= bytes_to_drop;

		if (prev_pck_size) {
			if (prev_pck_size > bytes_to_drop) prev_pck_size -= bytes_to_drop;
			else {
				prev_pck_size=0;
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = pck;
				if (pck)
					gf_filter_pck_ref_props(&ctx->src_pck);
			}
		}
		if (ctx->byte_offset != GF_FILTER_NO_BO)
			ctx->byte_offset += bytes_to_drop;
	}

	if (!pck) {
		ctx->mp3_buffer_size = 0;
		return mp3_dmx_process(filter);
	} else {
		if (remain) {
			memmove(ctx->mp3_buffer, start, remain);
		}
		ctx->mp3_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static void mp3_dmx_finalize(GF_Filter *filter)
{
	GF_MP3DmxCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->bs) gf_bs_del(ctx->bs);
	if (ctx->indexes) gf_free(ctx->indexes);
	if (ctx->mp3_buffer) gf_free(ctx->mp3_buffer);
	if (ctx->id3_buffer) gf_free(ctx->id3_buffer);
	if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
}


static const char *mp3_dmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	u32 nb_frames=0;
	u32 pos=0;
	u32 prev_pos=0;

	//For new we assume id3 to be set only for mp3 files
	//FIXME this isn't really true, we have seen ADTS with ID3 header in front - we would need a bigger probe size
	//to detect the format ...
	if (size>= 10) {
		/* Did we read an ID3v2 ? */
		if (data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
			*score = GF_FPROBE_MAYBE_SUPPORTED;
			return "audio/mp3";
		}
	}

	while (1) {
		u32 hdr = gf_mp3_get_next_header_mem(data, size, &pos);
		if (!hdr) break;
		if (gf_mp3_version(hdr) > 3)
			break;
		u8 sampleRateIndex = (hdr >> 10) & 0x3;
		if (sampleRateIndex>2)
			break;
		u32 fsize = gf_mp3_frame_size(hdr);
		if (prev_pos && pos) {
			nb_frames=0;
			break;
		}

		if (fsize + pos > size) {
			nb_frames++;
			break;
		}

		prev_pos = pos;
		nb_frames++;
		if (nb_frames>4) break;
		if (size < fsize + pos) break;
		size -= fsize + pos;
		data += fsize + pos;
	}

	if (nb_frames>=2) {
		*score = GF_FPROBE_SUPPORTED;
		return "audio/mp3";
	}
	return NULL;
}

static const GF_FilterCapability MP3DmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/mp3|audio/x-mp3"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mp3"),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	//also declare generic file output for embedded files (cover art & co), but explicit to skip this cap in chain resolution
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT | GF_CAPFLAG_LOADED_FILTER ,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE)
};



#define OFFS(_n)	#_n, offsetof(GF_MP3DmxCtx, _n)
static const GF_FilterArgs MP3DmxArgs[] =
{
	{ OFFS(index), "indexing window length", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(expart), "expose pictures as a dedicated video pid", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister MP3DmxRegister = {
	.name = "rfmp3",
	GF_FS_SET_DESCRIPTION("MP3 reframer")
	GF_FS_SET_HELP("This filter parses MPEG-1/2 audio files/data and outputs corresponding audio PID and frames.")
	.private_size = sizeof(GF_MP3DmxCtx),
	.args = MP3DmxArgs,
	.finalize = mp3_dmx_finalize,
	SETCAPS(MP3DmxCaps),
	.configure_pid = mp3_dmx_configure_pid,
	.process = mp3_dmx_process,
	.probe_data = mp3_dmx_probe_data,
	.process_event = mp3_dmx_process_event
};


const GF_FilterRegister *mp3_dmx_register(GF_FilterSession *session)
{
	return &MP3DmxRegister;
}
