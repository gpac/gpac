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
	//short name(s) for identifying codec - first value is used as default file ext
	const char *sname;
	//default name for isobmff/RFC6381
	const char *rfc_4cc;
	//mime type of raw format
	const char *mime_type;
} CodecIDReg;

CodecIDReg CodecRegistry [] = {
	{GF_CODECID_BIFS, GF_CODECID_BIFS, GF_STREAM_SCENE, "MPEG-4 BIFS v1 Scene Description", "bifs", "mp4s", "application/bifs"},
	{GF_CODECID_BIFS_V2, GF_CODECID_BIFS_V2, GF_STREAM_SCENE, "MPEG-4 BIFS v2 Scene Description", "bifs2", "mp4s", "application/bifs"},
	{GF_CODECID_BIFS_EXTENDED, GF_CODECID_BIFS_EXTENDED, GF_STREAM_SCENE, "MPEG-4 BIFS Extended Scene Description", "bifsX", "mp4s", "application/bifs"},
	{GF_CODECID_OD_V1, GF_CODECID_OD_V1, GF_STREAM_OD, "MPEG-4 ObjectDescriptor v1", "od", "mp4s", "application/od"},
	{GF_CODECID_OD_V2, GF_CODECID_OD_V2, GF_STREAM_OD, "MPEG-4 ObjectDescriptor v2", "od2", "mp4s", "application/od"},
	{GF_CODECID_INTERACT, GF_CODECID_INTERACT, GF_STREAM_INTERACT, "MPEG-4 Interaction Stream", "interact", "mp4s", "application/x-mpeg4-interact"},
	{GF_CODECID_AFX, GF_CODECID_AFX, GF_STREAM_SCENE, "MPEG-4 AFX Stream", "afx", "mp4s", "application/x-mpeg4-afx"},
	{GF_CODECID_FONT, GF_CODECID_FONT, GF_STREAM_FONT, "MPEG-4 Font Stream", "font", "mp4s", "application/x-mpeg4-font"},
	{GF_CODECID_SYNTHESIZED_TEXTURE, GF_CODECID_SYNTHESIZED_TEXTURE, GF_STREAM_VISUAL, "MPEG-4 Synthetized Texture", "syntex", "mp4s", "application/x-mpeg4-synth"},
	{GF_CODECID_TEXT_MPEG4, GF_CODECID_TEXT_MPEG4, GF_STREAM_TEXT, "MPEG-4 Streaming Text", "m4txt", "text", "application/x-mpeg4-text"},
	{GF_CODECID_LASER, GF_CODECID_LASER, GF_STREAM_SCENE, "MPEG-4 LASeR", "laser", "lsr1", "application/x-laser"},
	{GF_CODECID_SAF, GF_CODECID_SAF, GF_STREAM_SCENE, "MPEG-4 Simple Aggregation Format", "saf", "mp4s", "application/saf"},
	{GF_CODECID_MPEG4_PART2, GF_CODECID_MPEG4_PART2, GF_STREAM_VISUAL, "MPEG-4 Visual part 2", "cmp|m4vp2", "mp4v", "video/mp4v-es"},
	{GF_CODECID_AVC, GF_CODECID_AVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video", "264|avc|h264", "avc1", "video/avc"},
	{GF_CODECID_AVC_PS, GF_CODECID_AVC_PS, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video Parameter Sets", "avcps", "avcp", "video/avc"},
	{GF_CODECID_SVC, GF_CODECID_SVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Scalable Video Coding", "svc|avc|264|h264", "svc1", "video/svc"},
	{GF_CODECID_MVC, GF_CODECID_MVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Multiview Video Coding", "mvc", "mvc1", "video/mvc"},
	{GF_CODECID_HEVC, GF_CODECID_HEVC, GF_STREAM_VISUAL, "HEVC Video", "hvc|hevc|h265", "hvc1", "video/hevc"},
	{GF_CODECID_LHVC, GF_CODECID_LHVC, GF_STREAM_VISUAL, "HEVC Video Layered Extensions", "lhvc|shvc|mhvc", "lhc1", "video/lhvc"},
	{GF_CODECID_MPEG2_SIMPLE, GF_CODECID_MPEG2_SIMPLE, GF_STREAM_VISUAL, "MPEG-2 Visual Simple", "m2vs", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG2_MAIN, GF_CODECID_MPEG2_MAIN, GF_STREAM_VISUAL, "MPEG-2 Visual Main", "m2v", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG2_SNR, GF_CODECID_MPEG2_SNR, GF_STREAM_VISUAL, "MPEG-2 Visual SNR", "m2v|m2vsnr", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG2_SPATIAL, GF_CODECID_MPEG2_SPATIAL, GF_STREAM_VISUAL, "MPEG-2 Visual Spatial", "m2v|m2vspat", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG2_HIGH, GF_CODECID_MPEG2_HIGH, GF_STREAM_VISUAL, "MPEG-2 Visual High", "m2v|m2vh", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG2_422, GF_CODECID_MPEG2_422, GF_STREAM_VISUAL, "MPEG-2 Visual 422", "m2v|m2v4", "mp2v", "video/mp2v-es"},
	{GF_CODECID_MPEG1, GF_CODECID_MPEG1, GF_STREAM_VISUAL, "MPEG-1 Video", "m1v", "mp1v", "video/mp1v-es"},
	{GF_CODECID_JPEG, GF_CODECID_JPEG, GF_STREAM_VISUAL, "JPEG Image", "jpg|jpeg", "jpeg", "image/jpeg"},
	{GF_CODECID_PNG, GF_CODECID_PNG, GF_STREAM_VISUAL, "PNG Image", "png", "png ", "image/png"},
	{GF_CODECID_J2K, GF_CODECID_J2K, GF_STREAM_VISUAL, "JPEG200 Image", "jp2|j2k", "mjp2", "image/jp2"},
	{GF_CODECID_AAC_MPEG4, GF_CODECID_AAC_MPEG4, GF_STREAM_AUDIO, "MPEG-4 AAC Audio", "aac", "mp4a", "audio/aac"},
	{GF_CODECID_AAC_MPEG2_MP, GF_CODECID_AAC_MPEG2_MP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Main", "aac|aac2m", "mp4a", "audio/aac"},
	{GF_CODECID_AAC_MPEG2_LCP, GF_CODECID_AAC_MPEG2_LCP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Low Complexity", "aac|aac2l", "mp4a", "audio/aac"},
	{GF_CODECID_AAC_MPEG2_SSRP, GF_CODECID_AAC_MPEG2_SSRP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Scalable Sampling Rate", "aac|aac2s", "mp4a", "audio/aac"},
	{GF_CODECID_MPEG2_PART3, GF_CODECID_MPEG2_PART3, GF_STREAM_AUDIO, "MPEG-2 Audio", "mp3|m2a", ".mp3", "audio/mp3"},
	{GF_CODECID_MPEG_AUDIO, GF_CODECID_MPEG_AUDIO, GF_STREAM_AUDIO, "MPEG-1 Audio", "mp3|m1a", ".mp3", "audio/mp3"},
	{GF_CODECID_S263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "s263", "video/h263"},
	{GF_CODECID_H263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "h263", "video/h263"},
	{GF_CODECID_HEVC_TILES, 0, GF_STREAM_VISUAL, "HEVC tiles Video", "hvt1", "hvt1", "video/x-hevc-tiles"},

	{GF_CODECID_EVRC, 0xA0, GF_STREAM_AUDIO, "EVRC Voice", "evc|evrc", "sevc", "audio/evrc"},
	{GF_CODECID_SMV, 0xA1, GF_STREAM_AUDIO, "SMV Voice", "smv", "ssmv", "audio/smv"},
	{GF_CODECID_QCELP, 0xE1, GF_STREAM_AUDIO, "QCELP Voice", "qcp|qcelp", "sqcp", "audio/qcelp"},
	{GF_CODECID_AMR, 0, GF_STREAM_AUDIO, "AMR Audio", "amr", "samr", "audio/amr"},
	{GF_CODECID_AMR_WB, 0, GF_STREAM_AUDIO, "AMR WideBand Audio", "amr|amrwb", "sawb", "audio/amr"},
	{GF_CODECID_EVRC_PV, 0, GF_STREAM_AUDIO, "EVRC (PacketVideo MUX) Audio", "qcp|evrcpv", "sevc", "audio/evrc"},
	{GF_CODECID_SMPTE_VC1, 0xA3, GF_STREAM_VISUAL, "SMPTE VC-1 Video", "vc1", "vc1 ", "video/vc1"},
	{GF_CODECID_DIRAC, 0xA4, GF_STREAM_VISUAL, "Dirac Video", "dirac", NULL, "video/dirac"},
	{GF_CODECID_AC3, 0xA5, GF_STREAM_AUDIO, "AC3 Audio", "ac3", "ac-3", "audio/ac3"},
	{GF_CODECID_EAC3, 0xA6, GF_STREAM_AUDIO, "Enhanced AC3 Audio", "eac3", "ec-3", "audio/eac3"},
	{GF_CODECID_DRA, 0xA7, GF_STREAM_AUDIO, "DRA Audio", "dra", NULL, "audio/dra"},
	{GF_CODECID_G719, 0xA8, GF_STREAM_AUDIO, "G719 Audio", "g719", NULL, "audio/g719"},
	{GF_CODECID_DTS_CA, 0xA9, GF_STREAM_AUDIO, "DTS Coherent Acoustics Audio", "dstca", NULL, "audio/dts"},
	{GF_CODECID_DTS_HD_HR, 0xAA, GF_STREAM_AUDIO, "DTS-HD High Resolution Audio", "dtsh", NULL, "audio/dts"},
	{GF_CODECID_DTS_HD_MASTER, 0xAB, GF_STREAM_AUDIO, "DTS-HD Master Audio", "dstm", NULL, "audio/dts"},
	{GF_CODECID_DTS_LBR, 0xAC, GF_STREAM_AUDIO, "DTS Express low bit rate Audio", "dtsl", NULL, "audio/dts"},
	{GF_CODECID_OPUS, 0xAD, GF_STREAM_AUDIO, "Opus Audio", "opus", NULL, "audio/opus"},
	{GF_CODECID_DVB_EIT, 0, GF_STREAM_PRIVATE_SCENE, "DVB Event Information", "eti", NULL, "application/x-dvb-eit"},
	{GF_CODECID_SVG, 0, GF_STREAM_PRIVATE_SCENE, "SVG over RTP", "svgr", NULL, "application/x-svg-rtp"},
	{GF_CODECID_SVG_GZ, 0, GF_STREAM_PRIVATE_SCENE, "SVG+gz over RTP", "svgzr", NULL, "application/x-svgz-rtp"},
	{GF_CODECID_DIMS, 0, GF_STREAM_PRIVATE_SCENE, "3GPP DIMS Scene", "dims", NULL, "application/3gpp-dims"},
	{GF_CODECID_WEBVTT, 0, GF_STREAM_TEXT, "WebVTT Text", "vtt", "wvtt", "text/webtvv"},
	{GF_CODECID_SIMPLE_TEXT, 0, GF_STREAM_TEXT, "Simple Text Stream", "txt", "stxt", "text/subtitle"},
	{GF_CODECID_META_TEXT, 0, GF_STREAM_METADATA, "Metadata Text Stream", "mtxt", "mett", "application/text"},
	{GF_CODECID_META_XML, 0, GF_STREAM_METADATA, "Metadata XML Stream", "mxml", "metx", "application/text+xml"},
	{GF_CODECID_SUBS_TEXT, 0, GF_STREAM_TEXT, "Subtitle text Stream", "subs", "sbtt", "text/text"},
	{GF_CODECID_SUBS_XML, 0, GF_STREAM_TEXT, "Subtitle XML Stream", "subx", "stpp", "text/text+xml"},
	{GF_CODECID_TX3G, 0, GF_STREAM_TEXT, "Subtitle/text 3GPP/Apple Stream", "tx3g", "tx3g", "quicktime/text"},
	{GF_CODECID_THEORA, 0xDF, GF_STREAM_VISUAL, "Theora Video", "theo|theora", NULL, "video/theora"},
	{GF_CODECID_VORBIS, 0xDD, GF_STREAM_AUDIO, "Vorbis Audio", "vorb|vorbis", NULL, "audio/vorbis"},
	{GF_CODECID_OPUS, 0xDE, GF_STREAM_AUDIO, "Opus  Audio", "opus", NULL, "audio/opus"},
	{GF_CODECID_FLAC, 0, GF_STREAM_AUDIO, "Flac Audio", "flac", "fLaC", "audio/flac"},
	{GF_CODECID_SPEEX, 0, GF_STREAM_AUDIO, "Speex Audio", "spx|speex", NULL, "audio/speex"},
	{GF_CODECID_SUBPIC, 0xE0, GF_STREAM_TEXT, "VobSub Subtitle", "vobsub", NULL, "text/x-vobsub"},
	{GF_CODECID_SUBPIC, 0xE0, GF_STREAM_ND_SUBPIC, "VobSub Subtitle", "vobsub", NULL, "text/x-vobsub"},
	{GF_CODECID_PCM, 0, GF_STREAM_AUDIO, "PCM", "pcm", NULL, "audio/pcm"},
	{GF_CODECID_ADPCM, 0, GF_STREAM_AUDIO, "AD-PCM", "adpcm", NULL, "audio/pcm"},
	{GF_CODECID_IBM_CVSD, 0, GF_STREAM_AUDIO, "IBM CSVD", "csvd", NULL, "audio/pcm"},
	{GF_CODECID_ALAW, 0, GF_STREAM_AUDIO, "ALAW", "alaw", NULL, "audio/pcm"},
	{GF_CODECID_MULAW, 0, GF_STREAM_AUDIO, "MULAW", "mulaw", NULL, "audio/pcm"},
	{GF_CODECID_OKI_ADPCM, 0, GF_STREAM_AUDIO, "OKI ADPCM", "okiadpcm", NULL, "audio/pcm"},
	{GF_CODECID_DVI_ADPCM, 0, GF_STREAM_AUDIO, "DVI ADPCM", "dviadpcm", NULL, "audio/pcm"},
	{GF_CODECID_DIGISTD, 0, GF_STREAM_AUDIO, "DIGISTD", "digistd", NULL, "audio/pcm"},
	{GF_CODECID_YAMAHA_ADPCM, 0, GF_STREAM_AUDIO, "YAMAHA ADPCM", "yamadpcm", NULL, "audio/pcm"},
	{GF_CODECID_DSP_TRUESPEECH, 0, GF_STREAM_AUDIO, "DSP TrueSpeech", "truespeech", NULL, "audio/pcm"},
	{GF_CODECID_GSM610, 0, GF_STREAM_AUDIO, "GSM 610", "g610", NULL, "audio/pcm"},
	{GF_CODECID_IBM_MULAW, 0, GF_STREAM_AUDIO, "IBM MULAW", "imulaw", NULL, "audio/pcm"},
	{GF_CODECID_IBM_ALAW, 0, GF_STREAM_AUDIO, "IBM ALAW", "ialaw", NULL, "audio/pcm"},
	{GF_CODECID_IBM_ADPCM, 0, GF_STREAM_AUDIO, "IBM ADPCL", "iadpcl", NULL, "audio/pcm"},
	{GF_CODECID_FLASH, 0, GF_STREAM_SCENE, "Adobe Flash", "swf", NULL, "audio/pcm"},
	{GF_CODECID_RAW, 0, GF_STREAM_UNKNOWN, "Raw media", "raw", NULL, "audio/pcm"},

	{GF_CODECID_AV1, 0, GF_STREAM_VISUAL, "AOM AV1 Video", "av1|ivf|obu|av1b", NULL, "video/av1"},
	{GF_CODECID_VP8, 0, GF_STREAM_VISUAL, "VP8 Video", "vp8|ivf", NULL, "video/vp8"},
	{GF_CODECID_VP9, 0, GF_STREAM_VISUAL, "VP9 Video", "vp9|ivf", NULL, "video/vp9"},
	{GF_CODECID_VP10, 0, GF_STREAM_VISUAL, "VP10 Video", "vp10|ivf", NULL, "video/vp10"},

	{GF_CODECID_APCH, 0, GF_STREAM_VISUAL, "ProRes Video HQ", "apch", NULL, "video/prores"},
	{GF_CODECID_APCO, 0, GF_STREAM_VISUAL, "ProRes Video Proxy", "apco", NULL, "video/prores"},
	{GF_CODECID_APCN, 0, GF_STREAM_VISUAL, "ProRes Video STD", "apcn", NULL, "video/prores"},
	{GF_CODECID_APCS, 0, GF_STREAM_VISUAL, "ProRes Video LT", "apcs", NULL, "video/prores"},
	{GF_CODECID_APCF, 0, GF_STREAM_VISUAL, "ProRes Video", "apcf", NULL, "video/prores"},
	{GF_CODECID_AP4X, 0, GF_STREAM_VISUAL, "ProRes Video XQ", "ap4x", NULL, "video/prores"},
	{GF_CODECID_AP4H, 0, GF_STREAM_VISUAL, "ProRes Video 4444", "ap4h", NULL, "video/prores"},
	{GF_CODECID_FFMPEG, 0, GF_STREAM_UNKNOWN, "FFMPEG unmapped codec", "ffmpeg", NULL, NULL},

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


#include <gpac/internal/isomedia_dev.h>

GF_EXPORT
u32 gf_codec_id_from_isobmf(u32 isobmftype)
{
	switch (isobmftype) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
		return GF_CODECID_AMR;
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		return GF_CODECID_AMR_WB;
	case GF_ISOM_SUBTYPE_3GP_H263:
	case GF_ISOM_SUBTYPE_H263:
		return GF_CODECID_H263;
	case GF_ISOM_SUBTYPE_XDVB:
		return GF_CODECID_MPEG2_MAIN;
	case GF_ISOM_MEDIA_FLASH:
		return GF_CODECID_FLASH;
	case GF_ISOM_SUBTYPE_AC3:
		return GF_CODECID_AC3;
	case GF_ISOM_SUBTYPE_EC3:
		return GF_CODECID_EAC3;
	case GF_ISOM_SUBTYPE_FLAC:
		return GF_CODECID_FLAC;
	case GF_ISOM_SUBTYPE_MP3:
		return GF_CODECID_MPEG_AUDIO;
	case GF_ISOM_SUBTYPE_JPEG:
		return GF_CODECID_JPEG;
	case GF_ISOM_SUBTYPE_PNG:
		return GF_CODECID_PNG;
	case GF_ISOM_SUBTYPE_JP2K:
		return GF_CODECID_J2K;
	case GF_ISOM_SUBTYPE_STXT:
		return GF_CODECID_SIMPLE_TEXT;
	case GF_ISOM_SUBTYPE_METT:
		return GF_CODECID_META_TEXT;
	case GF_ISOM_SUBTYPE_METX:
		return GF_CODECID_META_XML;
	case GF_ISOM_SUBTYPE_SBTT:
		return GF_CODECID_SUBS_TEXT;
	case GF_ISOM_SUBTYPE_STPP:
		return GF_CODECID_SUBS_XML;
	case GF_ISOM_SUBTYPE_WVTT:
		return GF_CODECID_WEBVTT;
	case GF_ISOM_SUBTYPE_3GP_DIMS:
		return GF_CODECID_DIMS;
	case GF_ISOM_SUBTYPE_HVT1:
		return GF_CODECID_HEVC_TILES;
	case GF_ISOM_SUBTYPE_TEXT:
	case GF_ISOM_SUBTYPE_TX3G:
		return GF_CODECID_TX3G;

	case GF_QT_SUBTYPE_APCH:
		return GF_CODECID_APCH;
	case GF_QT_SUBTYPE_APCO:
		return GF_CODECID_APCO;
	case GF_QT_SUBTYPE_APCN:
		return GF_CODECID_APCN;
	case GF_QT_SUBTYPE_APCS:
		return GF_CODECID_APCS;
	case GF_QT_SUBTYPE_APCF:
		return GF_CODECID_APCF;
	case GF_QT_SUBTYPE_AP4X:
		return GF_CODECID_AP4X;
	case GF_QT_SUBTYPE_AP4H:
		return GF_CODECID_AP4H;

	case GF_QT_SUBTYPE_TWOS:
		return GF_CODECID_RAW;
	case GF_QT_SUBTYPE_SOWT:
		return GF_CODECID_RAW;
	case GF_QT_SUBTYPE_FL32:
		return GF_CODECID_RAW;
	case GF_QT_SUBTYPE_FL64:
		return GF_CODECID_RAW;
	case GF_QT_SUBTYPE_IN24:
		return GF_CODECID_RAW;
	case GF_QT_SUBTYPE_IN32:
		return GF_CODECID_RAW;
	default:
		break;
	}
	return 0;
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
const char *gf_codecid_file_ext(u32 codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	u32 global_ext_count = gf_opts_get_key_count("file_extensions");
	if (r && r->mime_type && global_ext_count) {
		const char *name = gf_opts_get_key("file_extensions", r->mime_type);
		if (name) return name;
	}
	if (r && r->sname) return r->sname;
	if (r && r->rfc_4cc) return r->rfc_4cc;
	return "raw";
}

GF_EXPORT
const char *gf_codecid_mime(u32 codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (r && r->mime_type) return r->mime_type;
	return "application/octet-string";
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
u32 gf_codecid_from_oti(u32 stream_type, u32 oti)
{
	CodecIDReg *r = gf_codecid_reg_find_oti(stream_type, oti);
	if (!r) return GF_CODECID_NONE;
	return r->codecid;
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
		return "Text";
	case GF_STREAM_METADATA:
		return "Metadata";
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
	const char *desc;
	const char *sname; //short name, as used in gpac
} GF_AudioFmt;

static const GF_AudioFmt GF_AudioFormats[] =
{
	{GF_AUDIO_FMT_U8, "u8", "8 bit PCM", "pc8"},
	{GF_AUDIO_FMT_S16, "s16", "16 bit PCM Little Endian", "pcm"},
	{GF_AUDIO_FMT_S24, "s24", "24 bit PCM"},
	{GF_AUDIO_FMT_S32, "s32", "32 bit PCM Little Endian"},
	{GF_AUDIO_FMT_FLT, "flt", "32-bit floating point PCM"},
	{GF_AUDIO_FMT_DBL, "dbl", "64-bit floating point PCM"},
	{GF_AUDIO_FMT_U8P, "u8p", "8 bit PCM planar", "pc8p"},
	{GF_AUDIO_FMT_S16P, "s16p", "16 bit PCM Little Endian planar", "pcmp"},
	{GF_AUDIO_FMT_S24P, "s24p", "24 bit PCM planar"},
	{GF_AUDIO_FMT_S32P, "s32p", "32 bit PCM Little Endian planar"},
	{GF_AUDIO_FMT_FLTP, "fltp", "32-bit floating point PCM planar"},
	{GF_AUDIO_FMT_DBLP, "dblp", "64-bit floating point PCM planar"},
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
u32 gf_audio_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **desc)
{
	u32 nb_afmt = sizeof(GF_AudioFormats) / sizeof(GF_AudioFmt);
	if (*idx >= nb_afmt) return 0;
	if (!GF_AudioFormats[*idx].sfmt) return 0;

	*name = GF_AudioFormats[*idx].name;
	*desc = GF_AudioFormats[*idx].desc;

	*fileext = GF_AudioFormats[*idx].sname;
	if (! *fileext) *fileext = *name;
	nb_afmt = GF_AudioFormats[*idx].sfmt;
	(*idx)++;
	return nb_afmt;
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

GF_EXPORT
u32 gf_audio_fmt_from_isobmf(u32 msubtype)
{
	switch (msubtype) {
	case GF_QT_SUBTYPE_TWOS:
		return GF_AUDIO_FMT_S16;
	case GF_QT_SUBTYPE_SOWT:
		return GF_AUDIO_FMT_S16;
	case GF_QT_SUBTYPE_FL32:
		return GF_AUDIO_FMT_FLT;
	case GF_QT_SUBTYPE_FL64:
		return GF_AUDIO_FMT_DBL;
	case GF_QT_SUBTYPE_IN24:
		return GF_AUDIO_FMT_S24;
	case GF_QT_SUBTYPE_IN32:
		return GF_AUDIO_FMT_S32;
	}
	return 0;
}


typedef struct
{
	u32 pixfmt;
	const char *name; //as used in gpac
	const char *desc; //as used in gpac
	const char *sname; //short name, as used in gpac
} GF_PixFmt;

static const GF_PixFmt GF_PixelFormats[] =
{
	{GF_PIXEL_YUV, "yuv420", "Planar YUV 420 8 bit", "yuv"},
	{GF_PIXEL_YUV_10, "yuv420_10", "Planar YUV 420 10 bit", "yuvl"},
	{GF_PIXEL_YUV422, "yuv422", "Planar YUV 422 8 bit", "yuv2"},
	{GF_PIXEL_YUV422_10, "yuv422_10", "Planar YUV 422 10 bit", "yp2l"},
	{GF_PIXEL_YUV444, "yuv444", "Planar YUV 444 8 bit", "yuv4"},
	{GF_PIXEL_YUV444_10, "yuv444_10", "Planar YUV 444 10 bit", "yp4l"},
	{GF_PIXEL_UYVY, "uyvy", "Planar UYVY 422 8 bit"},
	{GF_PIXEL_VYUY, "vyuy", "Planar VYUV 422 8 bit"},
	{GF_PIXEL_YUYV, "yuyv", "Planar YUYV 422 8 bit"},
	{GF_PIXEL_YVYU, "yvyu", "Planar YVYU 422 8 bit"},
	{GF_PIXEL_NV12, "nv12", "Semi-planar YUV 420 8 bit, Y plane and UV plane"},
	{GF_PIXEL_NV21, "nv21", "Semi-planar YUV 420 8 bit, Y plane and VU plane"},
	{GF_PIXEL_NV12_10, "nv1l", "Semi-planar YUV 420 10 bit, Y plane and UV plane"},
	{GF_PIXEL_NV21_10, "nv2l", "Semi-planar YUV 420 8 bit, Y plane and VU plane"},
	{GF_PIXEL_YUVA, "yuva", "Planar YUV+alpha 420 8 bit"},
	{GF_PIXEL_YUVD, "yuvd", "Planar YUV+depth  420 8 bit"},
	{GF_PIXEL_GREYSCALE, "grey", "Greyscale 8 bit"},
	{GF_PIXEL_ALPHAGREY, "algr", "Alpha+Grey 8 bit"},
	{GF_PIXEL_GREYALPHA, "gral", "Grey+Alpha 8 bit"},
	{GF_PIXEL_RGB_444, "rgb4", "RGB 444, 12 bits (16 stored) / pixel"},
	{GF_PIXEL_RGB_555, "rgb5", "RGB 555, 15 bits (16 stored) / pixel"},
	{GF_PIXEL_RGB_565, "rgb6", "RGB 555, 16 bits / pixel"},
	{GF_PIXEL_RGBA, "rgba", "RGBA 32 bits (8 bits / component)"},
	{GF_PIXEL_ARGB, "argb", "ARGB 32 bits (8 bits / component)"},
	{GF_PIXEL_BGRA, "bgra", "BGRA 32 bits (8 bits / component)"},
	{GF_PIXEL_ABGR, "abgr", "ABGR 32 bits (8 bits / component)"},
	{GF_PIXEL_RGB, "rgb", "RGB 24 bits (8 bits / component)"},
	{GF_PIXEL_BGR, "bgr", "BGR 24 bits (8 bits / component)"},
	{GF_PIXEL_XRGB, "xrgb", "xRGB 32 bits (8 bits / component)"},
	{GF_PIXEL_RGBX, "rgbx", "RGBx 32 bits (8 bits / component)"},
	{GF_PIXEL_XBGR, "xbgr", "xBGR 32 bits (8 bits / component)"},
	{GF_PIXEL_BGRX, "bgrx", "BGRx 32 bits (8 bits / component)"},
	{GF_PIXEL_RGBD, "rgbd", "RGB+depth 32 bits (8 bits / component)"},
	{GF_PIXEL_RGBDS, "rgbds", "RGB+depth+bit shape (8 bits / RGB component, 7 bit depth (low bits) + 1 bit shape)"},
	{GF_PIXEL_RGBS, "rgbs", "RGB 24 bits stereo (side-by-side) - to be removed\n"},
	{GF_PIXEL_RGBAS, "rgbas", "RGBA 32 bits stereo (side-by-side) - to be removed\n"},
	{0}
};

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

GF_EXPORT
u32 gf_pixel_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **description)
{
	u32 nb_pfmt = sizeof(GF_PixelFormats) / sizeof(GF_PixFmt);
	if (*idx >= nb_pfmt) return 0;
	if (! GF_PixelFormats[*idx].pixfmt) return 0;

	*name = GF_PixelFormats[*idx].name;
	*fileext = GF_PixelFormats[*idx].sname;
	if (! *fileext) *fileext = *name;
	*description = GF_PixelFormats[*idx].desc;
	nb_pfmt = GF_PixelFormats[*idx].pixfmt;
	(*idx)++;
	return nb_pfmt;
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
	case GF_PIXEL_GREYALPHA:
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
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
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
		if (height % 2) uv_height++;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		if (no_in_stride_uv && (stride%2) )
		 	stride_uv+=1;
		planes=3;
		size = stride * height + stride_uv * uv_height * 2;
		break;
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height / 2;
		if (height % 2) uv_height++;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		if (no_in_stride_uv && (stride%2) )
		 	stride_uv+=1;
		planes=4;
		size = 2*stride * height + stride_uv * uv_height * 2;
		break;
	case GF_PIXEL_YUV_10:
		stride = no_in_stride ? 2*width : *out_stride;
		uv_height = height / 2;
		if (height % 2) uv_height++;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		if (no_in_stride_uv && (stride%2) )
		 	stride_uv+=1;
		planes=3;
		size = stride * height + stride_uv * uv_height * 2;
		break;
	case GF_PIXEL_YUV422:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		if (no_in_stride_uv && (stride%2) )
		 	stride_uv+=1;
		planes=3;
		size = stride * height + stride_uv * height * 2;
		break;
	case GF_PIXEL_YUV422_10:
		stride = no_in_stride ? 2*width : *out_stride;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride/2 : *out_stride_uv;
		if (no_in_stride_uv && (stride%2) )
		 	stride_uv+=1;
		planes=3;
		size = stride * height + stride_uv * height * 2;
		break;
	case GF_PIXEL_YUV444:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=3;
		size = stride * height * 3;
		break;
	case GF_PIXEL_YUV444_10:
		stride = no_in_stride ? 2*width : *out_stride;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=3;
		size = stride * height * 3;
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
		uv_height = height/2;
		if (height % 2) uv_height++;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=2;
		size = 3 * stride * height / 2;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
		stride = no_in_stride ? 2*width : *out_stride;
		planes=1;
		size = height * stride;
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
	case GF_PIXEL_GREYALPHA:
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
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
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



