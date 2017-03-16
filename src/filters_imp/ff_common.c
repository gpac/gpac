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
#include <gpac/constants.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/dict.h>

static Bool ffmpeg_init = GF_FALSE;

void ffmpeg_initialize()
{
	if (ffmpeg_init) return;
	av_register_all();
	ffmpeg_init = GF_TRUE;
}

void ffmpeg_registry_free(GF_FilterSession *session, GF_FilterRegister *reg, u32 nb_skip_begin)
{
	u32 i;
	GF_List *all_filters = reg->udta;
	if (all_filters) {
		while (gf_list_count(all_filters)) {
			i=0;
			GF_FilterRegister *f = gf_list_pop_back(all_filters);
			gf_free(f->name);

			while (f->args) {
				GF_FilterArgs *arg = &f->args[i];
				if (!arg || !arg->arg_name) break;
				i++;
				if (arg->arg_default_val) gf_free(arg->arg_default_val);
				if (arg->min_max_enum) gf_free(arg->min_max_enum);
			}
			gf_free(f->args);
			gf_fs_remove_filter_registry(session, f);
			gf_free(f);
		}
		gf_list_del(all_filters);
	}
	i=nb_skip_begin;
	while (reg->args) {
		GF_FilterArgs *arg = &reg->args[i];
		if (!arg || !arg->arg_name) break;
		i++;
		if (arg->arg_default_val) gf_free(arg->arg_default_val);
		if (arg->min_max_enum) gf_free(arg->min_max_enum);
	}
	if (reg->args) gf_free(reg->args);

}

GF_FilterArgs ffmpeg_arg_translate(const struct AVOption *opt)
{
	char szDef[100];
	GF_FilterArgs arg;
	memset(&arg, 0, sizeof(GF_FilterArgs));
	arg.arg_name = opt->name;
	arg.arg_desc = opt->help;
	arg.offset_in_private=-1;
	arg.meta_arg = GF_TRUE;

	switch (opt->type) {
	case AV_OPT_TYPE_INT64:
	case AV_OPT_TYPE_INT:
		arg.arg_type = (opt->type==AV_OPT_TYPE_INT64) ? GF_PROP_LSINT : GF_PROP_SINT;
		sprintf(szDef, ""LLD, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		sprintf(szDef, "%d-%d", (s32) opt->min, (s32) opt->max);
		arg.min_max_enum = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_FLOAT:
		arg.arg_type = GF_PROP_FLOAT;
		sprintf(szDef, "%g", opt->default_val.dbl);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_DOUBLE:
		arg.arg_type = GF_PROP_DOUBLE;
		sprintf(szDef, "%g", opt->default_val.dbl);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_VIDEO_RATE:
	case AV_OPT_TYPE_RATIONAL:
		arg.arg_type = GF_PROP_FRACTION;
		sprintf(szDef, "%d/%d", opt->default_val.q.num, opt->default_val.q.den);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_STRING:
		arg.arg_type = GF_PROP_STRING;
		if (opt->default_val.str)
			arg.arg_default_val = gf_strdup(opt->default_val.str);
		break;
	case AV_OPT_TYPE_IMAGE_SIZE:
		arg.arg_type = GF_PROP_STRING;
		break;
	case AV_OPT_TYPE_CONST:
		arg.arg_type = GF_PROP_BOOL;
		arg.arg_default_val = gf_strdup("false");
		break;
	case AV_OPT_TYPE_FLAGS:
		arg.arg_type = GF_PROP_UINT;
		sprintf(szDef, ""LLD, opt->default_val.i64);
		arg.arg_default_val = gf_strdup(szDef);
		break;
	case AV_OPT_TYPE_BINARY:
	case AV_OPT_TYPE_DURATION:
		arg.arg_type = GF_PROP_UINT;
		break;
	case AV_OPT_TYPE_SAMPLE_FMT:
		arg.arg_type = GF_PROP_UINT;
		arg.arg_default_val = gf_strdup("s16");
		arg.min_max_enum = gf_strdup("u8|s16|s32|flt|dbl|u8p|s16p|s32p|fltp|dblp");
		break;
	case AV_OPT_TYPE_PIXEL_FMT:
		arg.arg_type = GF_PROP_UINT;
		arg.arg_default_val = gf_strdup(av_get_pix_fmt_name(AV_PIX_FMT_YUV420P) );
		{
		u32 i, all_len=1, def_size=1000;
		char *enum_val = gf_malloc(sizeof(char)*def_size);
		enum_val[0] = 0;
		for (i=0; i<AV_PIX_FMT_NB; i++) {
			u32 len;
			char *n = av_get_pix_fmt_name(i);
			if (!n) continue;

			len = strlen(n)+ i ? 2 : 1;
			if (len+all_len>def_size) {
				def_size+=1000;
				enum_val = gf_realloc(enum_val, sizeof(char)*def_size);
			}
			if (i) strcat(enum_val, "|");
			strcat(enum_val, n);
			all_len+=len-1;
		}
		arg.min_max_enum = enum_val;
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_FILTER, ("[FFDemux] Unhandled option type %d\n", opt->type));
	}
	return arg;
}

void ffmpeg_expand_registry(GF_FilterSession *session, GF_FilterRegister *orig_reg, u32 type)
{
	GF_FilterArgs *args;
	u32 i=0, idx=0, flags=0;
	const struct AVOption *opt;
	GF_List *all_filters = gf_list_new();
	AVInputFormat  *fmt = NULL;
	AVCodec *codec = NULL;

	const char *fname = "";
	if (type==0) fname = "ffdmx";
	else if (type==1) {
		fname = "ffdec";
		flags=AV_OPT_FLAG_DECODING_PARAM;
	}
	else return;


	orig_reg->udta = all_filters;

	while (1) {
		const AVClass *av_class=NULL;
		const char *subname = NULL;
		const char *description = NULL;
		char szDef[100];
		GF_FilterRegister *freg;
		i=0;
		if (type==0) {
			fmt = av_iformat_next(fmt);
			if (!fmt) break;
			av_class = fmt->priv_class;
			subname = fmt->name;
			description = fmt->long_name;
		} else if (type==1) {
			codec = av_codec_next(codec);
			if (!codec) break;
			av_class = codec->priv_class;
			subname = codec->name;
			description = codec->long_name;
		} else {
			break;
		}

		GF_SAFEALLOC(freg, GF_FilterRegister);
		if (!freg) continue;
		memcpy(freg, orig_reg, sizeof(GF_FilterRegister));
		freg->args = NULL;

		gf_list_add(all_filters, freg);

		sprintf(szDef, "%s:%s", fname, subname);
		freg->name = gf_strdup(szDef);
		freg->description = description;

		idx=0;
		i=0;
		while (av_class) {
			opt = &av_class->option[idx];
			if (!opt || !opt->name) break;
			if (!flags || (opt->flags & flags) )
				i++;
			idx++;
		}
		if (i) {
			GF_FilterArgs *args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
			memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
			freg->args = args;

			i=0;
			idx=0;
			while (av_class) {
				opt = &av_class->option[idx];
				if (!opt || !opt->name) break;

				if (!flags || (opt->flags & flags) ) {
					args[i] = ffmpeg_arg_translate(opt);
					i++;
				}
				idx++;
			}
		}

		gf_fs_add_filter_registry(session, freg);
	}
}



