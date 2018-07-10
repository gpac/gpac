/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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
#include <gpac/constants.h>

typedef struct
{
	//codec ID
	u32 codecid;
	//not defined (0) if codecid>255
	u8 mpeg4_oti;
	//stream type
	u32 stream_type;
	 //log name
	const char *name;
	//short name for identifying codec
	const char *sname;
	const char *rfc_4cc;
} CodecIDReg;

CodecIDReg CodecRegistry [] = {
	{GF_CODECID_BIFS, GF_CODECID_BIFS, GF_STREAM_SCENE, "MPEG-4 BIFS v1 Scene Description", "bifs", "mp4s"},
	{GF_CODECID_BIFS_V2, GF_CODECID_BIFS_V2, GF_STREAM_SCENE, "MPEG-4 BIFS v2 Scene Description", "bifs2", "mp4s" },
	{GF_CODECID_BIFS_EXTENDED, GF_CODECID_BIFS_EXTENDED, GF_STREAM_SCENE, "MPEG-4 BIFS Extended Scene Description", "bifsX", "mp4s"},
	{GF_CODECID_OD_V1, GF_CODECID_OD_V1, GF_STREAM_OD, "MPEG-4 ObjectDescriptor v1", "od", "mp4s"},
	{GF_CODECID_OD_V2, GF_CODECID_OD_V2, GF_STREAM_OD, "MPEG-4 ObjectDescriptor v2", "od2", "mp4s"},
	{GF_CODECID_INTERACT, GF_CODECID_INTERACT, GF_STREAM_INTERACT, "MPEG-4 Interaction Stream", "interact", "mp4s"},
	{GF_CODECID_AFX, GF_CODECID_AFX, GF_STREAM_SCENE, "MPEG-4 AFX Stream", "afx", "mp4s"},
	{GF_CODECID_FONT, GF_CODECID_FONT, GF_STREAM_FONT, "MPEG-4 Font Stream", "font", "mp4s"},
	{GF_CODECID_SYNTHESIZED_TEXTURE, GF_CODECID_SYNTHESIZED_TEXTURE, GF_STREAM_VISUAL, "MPEG-4 Synthetized Texture", "syntex", "mp4s"},
	{GF_CODECID_TEXT_MPEG4, GF_CODECID_TEXT_MPEG4, GF_STREAM_TEXT, "MPEG-4 Streaming Text", "m4txt", "text"},
	{GF_CODECID_LASER, GF_CODECID_LASER, GF_STREAM_SCENE, "MPEG-4 LASeR", "laser", "lsr1"},
	{GF_CODECID_SAF, GF_CODECID_SAF, GF_STREAM_SCENE, "MPEG-4 Simple Aggregation Format", "saf", "mp4s"},
	{GF_CODECID_MPEG4_PART2, GF_CODECID_MPEG4_PART2, GF_STREAM_VISUAL, "MPEG-4 Visual part 2", "m4vp2", "mp4v"},
	{GF_CODECID_AVC, GF_CODECID_AVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video", "avc|h264", "avc1"},
	{GF_CODECID_AVC_PS, GF_CODECID_AVC_PS, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video Parameter Sets", "avcps", "avcp"},
	{GF_CODECID_SVC, GF_CODECID_SVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Scalable Video Coding", "svc1"},
	{GF_CODECID_MVC, GF_CODECID_MVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Multiview Video Coding", "mvc1"},
	{GF_CODECID_HEVC, GF_CODECID_HEVC, GF_STREAM_VISUAL, "HEVC Video", "hevc|h265", "hvc1"},
	{GF_CODECID_LHVC, GF_CODECID_LHVC, GF_STREAM_VISUAL, "HEVC Video Layered Extensions", "lhvc|shvc|mhvc", "lhc1"},
	{GF_CODECID_MPEG2_SIMPLE, GF_CODECID_MPEG2_SIMPLE, GF_STREAM_VISUAL, "MPEG-2 Visual Simple", "m2vs", "mp2v"},
	{GF_CODECID_MPEG2_MAIN, GF_CODECID_MPEG2_MAIN, GF_STREAM_VISUAL, "MPEG-2 Visual Main", "m2v", "mp2v"},
	{GF_CODECID_MPEG2_SNR, GF_CODECID_MPEG2_SNR, GF_STREAM_VISUAL, "MPEG-2 Visual SNR", "m2vsnr", "mp2v"},
	{GF_CODECID_MPEG2_SPATIAL, GF_CODECID_MPEG2_SPATIAL, GF_STREAM_VISUAL, "MPEG-2 Visual Spatial", "m2vspat", "mp2v"},
	{GF_CODECID_MPEG2_HIGH, GF_CODECID_MPEG2_HIGH, GF_STREAM_VISUAL, "MPEG-2 Visual High", "m2vh", "mp2v"},
	{GF_CODECID_MPEG2_422, GF_CODECID_MPEG2_422, GF_STREAM_VISUAL, "MPEG-2 Visual 422", "m2v4", "mp2v"},
	{GF_CODECID_MPEG1, GF_CODECID_MPEG1, GF_STREAM_VISUAL, "MPEG-1 Video", "m1v", "mp1v"},
	{GF_CODECID_JPEG, GF_CODECID_JPEG, GF_STREAM_VISUAL, "JPEG Image", "jpg|jpeg", "jpeg"},
	{GF_CODECID_PNG, GF_CODECID_PNG, GF_STREAM_VISUAL, "PNG Image", "png", "png "},
	{GF_CODECID_J2K, GF_CODECID_J2K, GF_STREAM_VISUAL, "JPEG200 Image", "j2k", "mjp2"},
	{GF_CODECID_AAC_MPEG4, GF_CODECID_AAC_MPEG4, GF_STREAM_AUDIO, "MPEG-4 AAC Audio", "aac", "mp4a"},
	{GF_CODECID_AAC_MPEG2_MP, GF_CODECID_AAC_MPEG2_MP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Main", "aac2m", "mp4a"},
	{GF_CODECID_AAC_MPEG2_LCP, GF_CODECID_AAC_MPEG2_LCP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Low Complexity", "aac2l", "mp4a"},
	{GF_CODECID_AAC_MPEG2_SSRP, GF_CODECID_AAC_MPEG2_SSRP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Scalable Sampling Rate", "aac2s", "mp4a"},
	{GF_CODECID_MPEG2_PART3, GF_CODECID_MPEG2_PART3, GF_STREAM_AUDIO, "MPEG-2 Audio", "m2a", ".mp3"},
	{GF_CODECID_MPEG_AUDIO, GF_CODECID_MPEG_AUDIO, GF_STREAM_AUDIO, "MPEG-1 Audio", "m1a", ".mp3"},
	{GF_CODECID_S263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "s263"},
	{GF_CODECID_H263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "h263"},
	{GF_CODECID_HEVC_TILES, 0, GF_STREAM_VISUAL, "HEVC tiles Video", "hvt1", "hvt1"},

	{GF_CODECID_EVRC, 0xA0, GF_STREAM_AUDIO, "EVRC Voice", "evrc", "sevc"},
	{GF_CODECID_SMV, 0xA1, GF_STREAM_AUDIO, "SMV Voice", "smv", "ssmv"},
	{GF_CODECID_QCELP, 0xE1, GF_STREAM_AUDIO, "QCELP Voice", "qcelp", "sqcp"},
	{GF_CODECID_AMR, 0, GF_STREAM_AUDIO, "AMR Audio", "amr", "samr"},
	{GF_CODECID_AMR_WB, 0, GF_STREAM_AUDIO, "AMR WideBand Audio", "amrwb", "sawb"},
	{GF_CODECID_EVRC_PV, 0, GF_STREAM_AUDIO, "EVRC (PacketVideo MUX) Audio", "evrcpv", "sevc"},
	{GF_CODECID_SMPTE_VC1, 0xA3, GF_STREAM_VISUAL, "SMPTE VC-1 Video", "vc1", "svc1"},
	{GF_CODECID_DIRAC, 0xA4, GF_STREAM_VISUAL, "Dirac Video", "dirac", NULL},
	{GF_CODECID_AC3, 0xA5, GF_STREAM_AUDIO, "AC3 Audio", "ac3", "ac-3"},
	{GF_CODECID_EAC3, 0xA6, GF_STREAM_AUDIO, "Enhanced AC3 Audio", "eac3", "ec-3"},
	{GF_CODECID_DRA, 0xA7, GF_STREAM_AUDIO, "DRA Audio", "dra", NULL},
	{GF_CODECID_G719, 0xA8, GF_STREAM_AUDIO, "G719 Audio", "g719", NULL},
	{GF_CODECID_DTS_CA, 0xA9, GF_STREAM_AUDIO, "DTS Coherent Acoustics Audio", "dstca", NULL},
	{GF_CODECID_DTS_HD_HR, 0xAA, GF_STREAM_AUDIO, "DTS-HD High Resolution Audio", "dtsh", NULL},
	{GF_CODECID_DTS_HD_MASTER, 0xAB, GF_STREAM_AUDIO, "DTS-HD Master Audio", "dstm", NULL},
	{GF_CODECID_DTS_LBR, 0xAC, GF_STREAM_AUDIO, "DTS Express low bit rate Audio", "dtsl", NULL},
	{GF_CODECID_OPUS, 0xAD, GF_STREAM_AUDIO, "Opus Audio", "opus", NULL},
	{GF_CODECID_DVB_EIT, 0, GF_STREAM_PRIVATE_SCENE, "DVB Event Information", "eti", NULL},
	{GF_CODECID_SVG, 0, GF_STREAM_PRIVATE_SCENE, "SVG over RTP", "svgr", NULL},
	{GF_CODECID_SVG_GZ, 0, GF_STREAM_PRIVATE_SCENE, "SVG+gz over RTP", "svgzr", NULL},
	{GF_CODECID_DIMS, 0, GF_STREAM_PRIVATE_SCENE, "3GPP DIMS Scene", "dims", NULL},
	{GF_CODECID_WEBVTT, 0, GF_STREAM_TEXT, "WebVTT Text", "vtt", "wvtt"},
	{GF_CODECID_SIMPLE_TEXT, 0, GF_STREAM_TEXT, "Simple Text Stream", "txt", "stxt"},
	{GF_CODECID_META_TEXT, 0, GF_STREAM_METADATA, "Metadata Text Stream", "mtxt", "mett"},
	{GF_CODECID_META_XML, 0, GF_STREAM_METADATA, "Metadata XML Stream", "mxml", "metx"},
	{GF_CODECID_SUBS_TEXT, 0, GF_STREAM_TEXT, "Subtitle text Stream", "subs", "sbtt"},
	{GF_CODECID_SUBS_XML, 0, GF_STREAM_TEXT, "Subtitle XML Stream", "subx", "stpp"},
	{GF_CODECID_TX3G, 0, GF_STREAM_TEXT, "Subtitle/text 3GPP/Apple Stream", "tx3g", "tx3g"},
	{GF_CODECID_THEORA, 0xDF, GF_STREAM_VISUAL, "Theora Video", "theora", NULL},
	{GF_CODECID_VORBIS, 0, GF_STREAM_AUDIO, "Vorbis Audio", "vorbis", NULL},
	{GF_CODECID_FLAC, 0, GF_STREAM_AUDIO, "Flac Audio", "flac", NULL},
	{GF_CODECID_SPEEX, 0, GF_STREAM_AUDIO, "Speex Audio", "speex", NULL},
	{GF_CODECID_SUBPIC, 0xE0, GF_STREAM_TEXT, "VobSub Subtitle", "vobsub", NULL},
	{GF_CODECID_PCM, 0, GF_STREAM_AUDIO, "PCM", "pcm", NULL},
	{GF_CODECID_ADPCM, 0, GF_STREAM_AUDIO, "AD-PCM", "adpcm", NULL},
	{GF_CODECID_IBM_CVSD, 0, GF_STREAM_AUDIO, "IBM CSVD", "csvd", NULL},
	{GF_CODECID_ALAW, 0, GF_STREAM_AUDIO, "ALAW", "alaw", NULL},
	{GF_CODECID_MULAW, 0, GF_STREAM_AUDIO, "MULAW", "mulaw", NULL},
	{GF_CODECID_OKI_ADPCM, 0, GF_STREAM_AUDIO, "OKI ADPCM", "okiadpcm", NULL},
	{GF_CODECID_DVI_ADPCM, 0, GF_STREAM_AUDIO, "DVI ADPCM", "dviadpcm", NULL},
	{GF_CODECID_DIGISTD, 0, GF_STREAM_AUDIO, "DIGISTD", "digistd", NULL},
	{GF_CODECID_YAMAHA_ADPCM, 0, GF_STREAM_AUDIO, "YAMAHA ADPCM", "yamadpcm", NULL},
	{GF_CODECID_DSP_TRUESPEECH, 0, GF_STREAM_AUDIO, "DSP TrueSpeech", "truespeech", NULL},
	{GF_CODECID_GSM610, 0, GF_STREAM_AUDIO, "GSM 610", "g610", NULL},
	{GF_CODECID_IBM_MULAW, 0, GF_STREAM_AUDIO, "IBM MULAW", "imulaw", NULL},
	{GF_CODECID_IBM_ALAW, 0, GF_STREAM_AUDIO, "IBM ALAW", "ialaw", NULL},
	{GF_CODECID_IBM_ADPCM, 0, GF_STREAM_AUDIO, "IBM ADPCL", "iadpcl", NULL},
	{GF_CODECID_FLASH, 0, GF_STREAM_SCENE, "Adobe Flash", "swf", NULL},
	{GF_CODECID_RAW, 0, GF_STREAM_UNKNOWN, "Raw media", "raw", NULL},

	{GF_CODECID_AV1, 0, GF_STREAM_VISUAL, "AOM AV1 Video", "ivf|obu|av1b", NULL},

};


GF_EXPORT
u32 gf_codec_parse(const char *cname)
{
	u32 len = (u32) strlen(cname);
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		char *sep;
		if (!strcmp(CodecRegistry[i].sname, cname)) return CodecRegistry[i].codecid;
		if (!strchr(CodecRegistry[i].sname, '|') ) continue;
		sep = strstr(CodecRegistry[i].sname, cname);
		if (sep && (!sep[len] || (sep[len]=='|'))) return CodecRegistry[i].codecid;
	}
	return GF_CODECID_NONE;
}



static CodecIDReg *gf_codecid_reg_find(u32 codecid)
{
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		if (CodecRegistry[i].codecid==codecid) return &CodecRegistry[i];
	}
	return NULL;
}

static CodecIDReg *gf_codecid_reg_find_oti(u32 stream_type, u32 oti)
{
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		if (CodecRegistry[i].stream_type != stream_type) continue;
		if (CodecRegistry[i].mpeg4_oti != oti) continue;
		return &CodecRegistry[i];
	}
	return NULL;
}

GF_EXPORT
const char *gf_codecid_name(u32 codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return "Codec Not Supported";
	return r->name;
}

GF_EXPORT
u32 gf_codecid_enum(u32 idx, const char **short_name, const char **long_name)
{
	u32 count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	if (idx >= count) return GF_CODECID_NONE;
	if (short_name) *short_name = CodecRegistry[idx].sname;
	if (long_name) *long_name = CodecRegistry[idx].name;
	return CodecRegistry[idx].codecid;
}


GF_EXPORT
u32 gf_codecid_type(u32 codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return GF_STREAM_UNKNOWN;
	return r->stream_type;
}

GF_EXPORT
const char *gf_codecid_name_oti(u32 stream_type, u32 oti)
{
	CodecIDReg *r = gf_codecid_reg_find_oti(stream_type, oti);
	if (!r) return "Codec Not Supported";
	return r->name;
}

GF_EXPORT
u8 gf_codecid_oti(u32 codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return 0;
	return r->mpeg4_oti;
}

GF_EXPORT
u32 gf_codecid_4cc_type(u32 codecid)
{
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		if (CodecRegistry[i].codecid==codecid) {
			const char *n = CodecRegistry[i].rfc_4cc;
			if (!n) return 0;
			return GF_4CC(n[0], n[1], n[2], n[3]);
		}
	}
	return 0;
}

GF_EXPORT
const char *gf_stream_type_name(u32 streamType)
{
	switch (streamType) {
	case GF_STREAM_OD:
		return "ObjectDescriptor";
	case GF_STREAM_OCR:
		return "ClockReference";
	case GF_STREAM_SCENE:
		return "SceneDescription";
	case GF_STREAM_VISUAL:
		return "Visual";
	case GF_STREAM_AUDIO:
		return "Audio";
	case GF_STREAM_MPEG7:
		return "MPEG7";
	case GF_STREAM_IPMP:
		return "IPMP";
	case GF_STREAM_OCI:
		return "OCI";
	case GF_STREAM_MPEGJ:
		return "MPEGJ";
	case GF_STREAM_INTERACT:
		return "Interaction";
	case GF_STREAM_FONT:
		return "Font";
	case GF_STREAM_TEXT:
		return "Text_Subtitle";
	case GF_STREAM_FILE:
		return "File";
	case GF_STREAM_ENCRYPTED:
		return "Encrypted";
	default:
		return "Unknown";
	}
}

GF_EXPORT
const char *gf_stream_type_afx_name(u8 afx_code)
{
	switch (afx_code) {
	case GPAC_AFX_3DMC:
		return "AFX 3D Mesh Compression";
	case GPAC_AFX_WAVELET_SUBDIVISION:
		return "AFX Wavelet Subdivision Surface";
	case GPAC_AFX_MESHGRID:
		return "AFX Mesh Grid";
	case GPAC_AFX_COORDINATE_INTERPOLATOR:
		return "AFX Coordinate Interpolator";
	case GPAC_AFX_ORIENTATION_INTERPOLATOR:
		return "AFX Orientation Interpolator";
	case GPAC_AFX_POSITION_INTERPOLATOR:
		return "AFX Position Interpolator";
	case GPAC_AFX_OCTREE_IMAGE:
		return "AFX Octree Image";
	case GPAC_AFX_BBA:
		return "AFX BBA";
	case GPAC_AFX_POINT_TEXTURE:
		return "AFX Point Texture";
	case GPAC_AFX_3DMC_EXT:
		return "AFX 3D Mesh Compression Extension";
	case GPAC_AFX_FOOTPRINT:
		return "AFX FootPrint Representation";
	case GPAC_AFX_ANIMATED_MESH:
		return "AFX Animated Mesh Compression";
	case GPAC_AFX_SCALABLE_COMPLEXITY:
		return "AFX Scalable Complexity Representation";
	default:
		break;
	}
	return "AFX Unknown";
}


typedef struct
{
	u32 sfmt;
	const char *name; //as used in gpac
	const char *sname; //short name, as used in gpac
} GF_AudioFmt;

static const GF_AudioFmt GF_AudioFormats[] =
{
	{GF_AUDIO_FMT_U8, "u8", "pc8"},
	{GF_AUDIO_FMT_S16, "s16", "pcm"},
	{GF_AUDIO_FMT_S32, "s24"},
	{GF_AUDIO_FMT_S32, "s32"},
	{GF_AUDIO_FMT_FLT, "flt"},
	{GF_AUDIO_FMT_DBL, "dbl"},
	{GF_AUDIO_FMT_U8P, "u8p", "pc8p"},
	{GF_AUDIO_FMT_S16P, "s16p", "pcmp"},
	{GF_AUDIO_FMT_S32P, "s24p"},
	{GF_AUDIO_FMT_S32P, "s32p"},
	{GF_AUDIO_FMT_FLTP, "fltp"},
	{GF_AUDIO_FMT_DBLP, "dblp"},
	{0}
};


GF_EXPORT
u32 gf_audio_fmt_parse(const char *af_name)
{
	u32 i=0;
	if (!af_name || !strcmp(af_name, "none")) return 0;
	while (!i || GF_AudioFormats[i].sfmt) {
		if (!strcmp(GF_AudioFormats[i].name, af_name))
			return GF_AudioFormats[i].sfmt;
		if (GF_AudioFormats[i].sname && !strcmp(GF_AudioFormats[i].sname, af_name))
			return GF_AudioFormats[i].sfmt;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported audio format %s\n", af_name));
	return 0;
}

GF_EXPORT
const char *gf_audio_fmt_name(u32 sfmt)
{
	u32 i=0;
	while (!i || GF_AudioFormats[i].sfmt) {
		if (GF_AudioFormats[i].sfmt==sfmt) return GF_AudioFormats[i].name;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported audio format %d\n", sfmt ));
	return "unknown";
}
GF_EXPORT
const char *gf_audio_fmt_sname(u32 sfmt)
{
	u32 i=0;
	while (!i || GF_AudioFormats[i].sfmt) {
		if (GF_AudioFormats[i].sfmt==sfmt) {
			if (GF_AudioFormats[i].sname)
				return GF_AudioFormats[i].sname;
			return GF_AudioFormats[i].name;
		}
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported audio format %d\n", sfmt ));
	return "unknown";
}

static char szAllAudioFormats[500] = {0};

GF_EXPORT
const char *gf_audio_fmt_all_names()
{
	if (!szAllAudioFormats[0]) {
		u32 i=0;
		u32 tot_len=4;
		strcpy(szAllAudioFormats, "none");
		while (!i || GF_AudioFormats[i].sfmt) {
			u32 len = (u32) strlen(GF_AudioFormats[i].name);
			if (len+tot_len+2>=500) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Not enough memory to hold all audio formats!!\n"));
				break;
			}
			strcat((char *)szAllAudioFormats, ",");
			tot_len += 1;
			strcat((char *)szAllAudioFormats, GF_AudioFormats[i].name);
			tot_len += len;
			i++;
		}
	}
	return szAllAudioFormats;
}

static char szAllShortAudioFormats[500] = {0};

GF_EXPORT
const char *gf_audio_fmt_all_shortnames()
{
	if (!szAllShortAudioFormats[0]) {
		u32 i=0;
		u32 tot_len=0;
		memset(szAllShortAudioFormats, 0, sizeof(char)*500);
		while (!i || GF_AudioFormats[i].sfmt) {
			const char * n = GF_AudioFormats[i].sname ? GF_AudioFormats[i].sname : GF_AudioFormats[i].name;
			u32 len = (u32) strlen(n);
			if (len+tot_len+1>=500) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Not enough memory to hold all audio formats!!\n"));
				break;
			}
			if (i) {
				strcat((char *)szAllShortAudioFormats, "|");
				tot_len += 1;
				strcat((char *)szAllShortAudioFormats, n);
			} else {
				strcpy((char *)szAllShortAudioFormats, n);
			}
			tot_len += len;
			i++;
		}
		szAllShortAudioFormats[tot_len] = 0;
	}
	return szAllShortAudioFormats;
}

GF_EXPORT
u32 gf_audio_fmt_bit_depth(u32 audio_fmt)
{
	switch (audio_fmt) {
	case GF_AUDIO_FMT_U8P:
	case GF_AUDIO_FMT_U8: return 8;

	case GF_AUDIO_FMT_S16P:
	case GF_AUDIO_FMT_S16: return 16;

	case GF_AUDIO_FMT_S32P:
	case GF_AUDIO_FMT_S32: return 32;

	case GF_AUDIO_FMT_FLTP:
	case GF_AUDIO_FMT_FLT: return 32;

	case GF_AUDIO_FMT_DBLP:
	case GF_AUDIO_FMT_DBL: return 64;

	case GF_AUDIO_FMT_S24P:
	case GF_AUDIO_FMT_S24:  return 24;

	default:
		break;
	}
	return 0;
}

GF_EXPORT
Bool gf_audio_fmt_is_planar(u32 audio_fmt)
{
	switch (audio_fmt) {
	case GF_AUDIO_FMT_U8P:
	case GF_AUDIO_FMT_S16P:
	case GF_AUDIO_FMT_S32P:
	case GF_AUDIO_FMT_FLTP:
	case GF_AUDIO_FMT_DBLP:
	case GF_AUDIO_FMT_S24P:
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

typedef struct
{
	u32 pixfmt;
	const char *name; //as used in gpac
	const char *sname; //short name, as used in gpac
} GF_PixFmt;

static const GF_PixFmt GF_PixelFormats[] =
{
	{GF_PIXEL_YUV, "yuv420", "yuv"},
	{GF_PIXEL_YUV_10, "yuv420_10", "yuvl"},
	{GF_PIXEL_YUV422, "yuv422", "yuv2"},
	{GF_PIXEL_YUV422_10, "yuv422_10", "yp2l"},
	{GF_PIXEL_YUV444, "yuv444", "yuv4"},
	{GF_PIXEL_YUV444_10, "yuv444_10", "yp4l"},
	{GF_PIXEL_UYVY, "uyvy"},
	{GF_PIXEL_UYVY, "vyuy"},
	{GF_PIXEL_YUYV, "yuyv"},
	{GF_PIXEL_YUYV, "yvyu"},
	{GF_PIXEL_NV12, "nv12"},
	{GF_PIXEL_NV12_10, "nv1l"},
	{GF_PIXEL_NV21_10, "nv2l"},
	{GF_PIXEL_YUVA, "yuva"},
	{GF_PIXEL_YUVD, "yuvd"},
	{GF_PIXEL_GREYSCALE, "grey"},
	{GF_PIXEL_ALPHAGREY, "gral"},
	{GF_PIXEL_RGB_444, "rgb4"},
	{GF_PIXEL_RGB_555, "rgb5"},
	{GF_PIXEL_RGB_565, "rgb6"},
	{GF_PIXEL_RGBA, "rgba"},
	{GF_PIXEL_ARGB, "argb"},
	{GF_PIXEL_RGB, "rgb"},
	{GF_PIXEL_BGR, "bgr"},
	{GF_PIXEL_XRGB, "xrgb"},
	{GF_PIXEL_RGBX, "rgbx"},
	{GF_PIXEL_XBGR, "xbgr"},
	{GF_PIXEL_BGRX, "bgrx"},
	{GF_PIXEL_RGBD, "rgbd"},
	{GF_PIXEL_RGBDS, "rgbds"},
	{GF_PIXEL_RGBS, "rgbs"},
	{GF_PIXEL_RGBAS, "rgbas"},
	{0}
};

GF_EXPORT
u32 gf_pixel_fmt_enum(u32 *idx, const char **out_name)
{
	u32 pf, c=sizeof(GF_PixelFormats) / sizeof(GF_PixFmt);
	if (!idx) return 0;
	if (*idx >= c) return 0;
	if (! GF_PixelFormats[*idx].pixfmt) return 0;
	if (out_name) *out_name = GF_PixelFormats[*idx].name;
	pf = GF_PixelFormats[*idx].pixfmt;
	(*idx) ++;
	return pf;
}

GF_EXPORT
u32 gf_pixel_fmt_parse(const char *pf_name)
{
	u32 i=0;
	if (!pf_name || !strcmp(pf_name, "none")) return 0;
	while (GF_PixelFormats[i].pixfmt) {
		if (!strcmp(GF_PixelFormats[i].name, pf_name))
			return GF_PixelFormats[i].pixfmt;
		if (GF_PixelFormats[i].sname && !strcmp(GF_PixelFormats[i].sname, pf_name))
			return GF_PixelFormats[i].pixfmt;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported pixel format %s\n", pf_name));
	return 0;
}
GF_EXPORT
const char *gf_pixel_fmt_name(u32 pfmt)
{
	u32 i=0;
	while (GF_PixelFormats[i].pixfmt) {
		if (GF_PixelFormats[i].pixfmt==pfmt) return GF_PixelFormats[i].name;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported pixel format %d (%s)\n", pfmt, gf_4cc_to_str(pfmt) ));
	return "unknown";
}
GF_EXPORT
const char *gf_pixel_fmt_sname(u32 pfmt)
{
	u32 i=0;
	while (GF_PixelFormats[i].pixfmt) {
		if (GF_PixelFormats[i].pixfmt==pfmt) {
			if (GF_PixelFormats[i].sname)
				return GF_PixelFormats[i].sname;
			return GF_PixelFormats[i].name;
		}
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported pixel format %d (%s)\n", pfmt, gf_4cc_to_str(pfmt) ));
	return "unknown";

}

static char szAllPixelFormats[5000] = {0};

GF_EXPORT
const char *gf_pixel_fmt_all_names()
{
	if (!szAllPixelFormats[0]) {
		u32 i=0;
		u32 tot_len=4;
		strcpy(szAllPixelFormats, "none");
		while (GF_PixelFormats[i].pixfmt) {
			u32 len = (u32) strlen(GF_PixelFormats[i].name);
			if (len+tot_len+2>=5000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Not enough memory to hold all pixel formats!!\n"));
				break;
			}
			strcat((char *)szAllPixelFormats, ",");
			tot_len += 1;
			strcat((char *)szAllPixelFormats, GF_PixelFormats[i].name);
			tot_len += len;
			i++;
		}
	}
	return szAllPixelFormats;
}

static char szAllShortPixelFormats[5000] = {0};

GF_EXPORT
const char *gf_pixel_fmt_all_shortnames()
{
	if (!szAllShortPixelFormats[0]) {
		u32 i=0;
		u32 tot_len=0;
		while (GF_PixelFormats[i].pixfmt) {
			const char * n = GF_PixelFormats[i].sname ? GF_PixelFormats[i].sname : GF_PixelFormats[i].name;
			u32 len = (u32) strlen(n);
			if (len+tot_len+1>=5000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Not enough memory to hold all pixel formats!!\n"));
				break;
			}
			if (i) {
				strcat((char *)szAllShortPixelFormats, "|");
				tot_len += 1;
				strcat((char *)szAllShortPixelFormats, n);
			} else {
				strcpy((char *)szAllShortPixelFormats, n);
			}
			tot_len += len;
			i++;
		}
	}
	return szAllShortPixelFormats;
}

GF_EXPORT
Bool gf_pixel_get_size_info(GF_PixelFormat pixfmt, u32 width, u32 height, u32 *out_size, u32 *out_stride, u32 *out_stride_uv, u32 *out_planes, u32 *out_plane_uv_height)
{
	u32 stride=0, stride_uv=0, size=0, planes=0, uv_height=0;
	Bool no_in_stride = (!out_stride || (*out_stride==0)) ? GF_TRUE : GF_FALSE;
	Bool no_in_stride_uv = (!out_stride_uv || (*out_stride_uv==0)) ? GF_TRUE : GF_FALSE;

	switch (pixfmt) {
	case GF_PIXEL_GREYSCALE:
		stride = no_in_stride ? width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_ALPHAGREY:
		stride = no_in_stride ? 2*width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		stride = no_in_stride ? 2*width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGBDS:
	case GF_PIXEL_RGBAS:
		stride = no_in_stride ? 4*width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_RGB_DEPTH:
		stride = no_in_stride ? 3*width : *out_stride;
		stride_uv = no_in_stride_uv ? width : *out_stride_uv;
		size = 4 * width * height;
		planes=1;
		break;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
	case GF_PIXEL_RGBS:
		stride = no_in_stride ? 3*width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_YUV:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height / 2;
		size = 3*stride * height / 2;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height / 2;
		size = 3*stride * height / 2 + width*height;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		planes=4;
		break;
	case GF_PIXEL_YUV_10:
		stride = no_in_stride ? 2*width : *out_stride;
		size = 3*stride * height / 2;
		uv_height = height / 2;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_YUV422:
		stride = no_in_stride ? width : *out_stride;
		size = stride * height * 2;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_YUV422_10:
		stride = no_in_stride ? 2*width : *out_stride;
		size = stride * height * 2;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_YUV444:
		stride = no_in_stride ? width : *out_stride;
		size = stride * height * 3;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_YUV444_10:
		stride = no_in_stride ? 2*width : *out_stride;
		size = stride * height * 3;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=3;
		break;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		stride = no_in_stride ? width : *out_stride;
		size = 3 * stride * height / 2;
		uv_height = height/2;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=2;
		break;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		stride = no_in_stride ? 2*width : *out_stride;
		size = 3 * stride * height / 2;
		uv_height = height/2;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=2;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
		stride = no_in_stride ? 2*width : *out_stride;
		size = height * stride;
		planes=1;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported pixel format %s, cannot get size info\n", gf_pixel_fmt_name(pixfmt) ));
		return GF_FALSE;
	}
	if (out_size) *out_size = size;
	if (out_stride) *out_stride = stride;
	if (out_stride_uv) *out_stride_uv = stride_uv;
	if (out_planes) *out_planes = planes;
	if (out_plane_uv_height) *out_plane_uv_height = uv_height;
	return GF_TRUE;
}

GF_EXPORT
u32 gf_pixel_get_bytes_per_pixel(GF_PixelFormat pixfmt)
{
	switch (pixfmt) {
	case GF_PIXEL_GREYSCALE:
		return 1;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		return 2;
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGBD:
	case GF_PIXEL_RGBDS:
	case GF_PIXEL_RGBAS:
	case GF_PIXEL_RGB_DEPTH:
		return 4;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
	case GF_PIXEL_RGBS:
		return 3;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
		return 1;
	case GF_PIXEL_YUV_10:
		return 2;
	case GF_PIXEL_YUV422:
		return 1;
	case GF_PIXEL_YUV422_10:
		return 2;
	case GF_PIXEL_YUV444:
		return 1;
	case GF_PIXEL_YUV444_10:
		return 2;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		return 1;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		return 2;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
		return 1;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("Unsupported pixel format %s, cannot get bytes per pixel info\n", gf_pixel_fmt_name(pixfmt) ));
		break;
	}
	return 0;
}



