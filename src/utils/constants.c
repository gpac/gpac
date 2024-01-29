/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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
#include <gpac/bitstream.h>

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
	//alternate codecid name
	u32 alt_codecid;
	//if true, unframe format exists in gpac (we can reparse the bitstream)
	Bool unframe;
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
	{GF_CODECID_MPEG4_PART2, GF_CODECID_MPEG4_PART2, GF_STREAM_VISUAL, "MPEG-4 Visual part 2", "cmp|m4ve|m4v", "mp4v", "video/mp4v-es", .unframe=GF_TRUE},
	{GF_CODECID_AVC, GF_CODECID_AVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video", "264|avc|h264", "avc1", "video/avc", .unframe=GF_TRUE},
	{GF_CODECID_AVC_PS, GF_CODECID_AVC_PS, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Video Parameter Sets", "avcps", "avcp", "video/avc", .unframe=GF_TRUE},
	{GF_CODECID_SVC, GF_CODECID_SVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Scalable Video Coding", "svc|avc|264|h264", "svc1", "video/svc", .unframe=GF_TRUE},
	{GF_CODECID_MVC, GF_CODECID_MVC, GF_STREAM_VISUAL, "MPEG-4 AVC|H264 Multiview Video Coding", "mvc", "mvc1", "video/mvc", .unframe=GF_TRUE},
	{GF_CODECID_HEVC, GF_CODECID_HEVC, GF_STREAM_VISUAL, "HEVC Video", "hvc|hevc|h265", "hvc1", "video/hevc", .unframe=GF_TRUE},
	{GF_CODECID_LHVC, GF_CODECID_LHVC, GF_STREAM_VISUAL, "HEVC Video Layered Extensions", "lhvc|shvc|mhvc", "lhc1", "video/lhvc", .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_SIMPLE, GF_CODECID_MPEG2_SIMPLE, GF_STREAM_VISUAL, "MPEG-2 Visual Simple", "m2vs", "mp2v", "video/mp2v-es", .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_MAIN, GF_CODECID_MPEG2_MAIN, GF_STREAM_VISUAL, "MPEG-2 Visual Main", "m2v", "mp2v", "video/mp2v-es", GF_CODECID_MPEG2_SIMPLE, .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_SNR, GF_CODECID_MPEG2_SNR, GF_STREAM_VISUAL, "MPEG-2 Visual SNR", "m2v|m2vsnr", "mp2v", "video/mp2v-es", GF_CODECID_MPEG2_SIMPLE, .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_SPATIAL, GF_CODECID_MPEG2_SPATIAL, GF_STREAM_VISUAL, "MPEG-2 Visual Spatial", "m2v|m2vspat", "mp2v", "video/mp2v-es", GF_CODECID_MPEG2_SIMPLE, .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_HIGH, GF_CODECID_MPEG2_HIGH, GF_STREAM_VISUAL, "MPEG-2 Visual High", "m2v|m2vh", "mp2v", "video/mp2v-es", GF_CODECID_MPEG2_SIMPLE, .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_422, GF_CODECID_MPEG2_422, GF_STREAM_VISUAL, "MPEG-2 Visual 422", "m2v|m2v4", "mp2v", "video/mp2v-es", GF_CODECID_MPEG2_SIMPLE, .unframe=GF_TRUE},
	{GF_CODECID_MPEG1, GF_CODECID_MPEG1, GF_STREAM_VISUAL, "MPEG-1 Video", "m1v", "mp1v", "video/mp1v-es", .unframe=GF_TRUE},
	{GF_CODECID_JPEG, GF_CODECID_JPEG, GF_STREAM_VISUAL, "JPEG Image", "jpg|jpeg", "jpeg", "image/jpeg", .unframe=GF_TRUE},
	{GF_CODECID_PNG, GF_CODECID_PNG, GF_STREAM_VISUAL, "PNG Image", "png", "png ", "image/png", .unframe=GF_TRUE},
	{GF_CODECID_J2K, 0x6E, GF_STREAM_VISUAL, "JPEG2000 Image", "jp2|j2k", "mjp2", "image/jp2", .unframe=GF_TRUE},
	{GF_CODECID_AAC_MPEG4, GF_CODECID_AAC_MPEG4, GF_STREAM_AUDIO, "MPEG-4 AAC Audio", "aac", "mp4a", "audio/aac", .unframe=GF_TRUE},
	{GF_CODECID_AAC_MPEG2_MP, GF_CODECID_AAC_MPEG2_MP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Main", "aac|aac2m", "mp4a", "audio/aac", GF_CODECID_AAC_MPEG4, .unframe=GF_TRUE},
	{GF_CODECID_AAC_MPEG2_LCP, GF_CODECID_AAC_MPEG2_LCP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Low Complexity", "aac|aac2l", "mp4a", "audio/aac", GF_CODECID_AAC_MPEG4, .unframe=GF_TRUE},
	{GF_CODECID_AAC_MPEG2_SSRP, GF_CODECID_AAC_MPEG2_SSRP, GF_STREAM_AUDIO, "MPEG-2 AAC Audio Scalable Sampling Rate", "aac|aac2s", "mp4a", "audio/aac", GF_CODECID_AAC_MPEG4, .unframe=GF_TRUE},
	{GF_CODECID_MPEG_AUDIO, GF_CODECID_MPEG_AUDIO, GF_STREAM_AUDIO, "MPEG-1 Audio", "mp3|m1a", ".mp3", "audio/mp3", .unframe=GF_TRUE},
	{GF_CODECID_MPEG2_PART3, GF_CODECID_MPEG2_PART3, GF_STREAM_AUDIO, "MPEG-2 Audio", "mp2", ".mp3", "audio/mpeg", .unframe=GF_TRUE},
	{GF_CODECID_MPEG_AUDIO_L1, GF_CODECID_MPEG_AUDIO, GF_STREAM_AUDIO, "MPEG-1 Audio Layer 1", "mp1", ".mp3", "audio/mpeg", .unframe=GF_TRUE},
	{GF_CODECID_S263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "s263", "video/h263", .alt_codecid=GF_CODECID_H263, .unframe=GF_TRUE},
	{GF_CODECID_H263, 0, GF_STREAM_VISUAL, "H263 Video", "h263", "h263", "video/h263", .alt_codecid=GF_CODECID_S263, .unframe=GF_TRUE},
	{GF_CODECID_HEVC_TILES, 0, GF_STREAM_VISUAL, "HEVC tiles Video", "hvt1", "hvt1", "video/x-hevc-tiles", .alt_codecid=GF_CODECID_HEVC, .unframe=GF_TRUE},

	{GF_CODECID_EVRC, 0xA0, GF_STREAM_AUDIO, "EVRC Voice", "evc|evrc", "sevc", "audio/evrc"},
	{GF_CODECID_SMV, 0xA1, GF_STREAM_AUDIO, "SMV Voice", "smv", "ssmv", "audio/smv"},
	{GF_CODECID_QCELP, 0xE1, GF_STREAM_AUDIO, "QCELP Voice", "qcp|qcelp", "sqcp", "audio/qcelp"},
	{GF_CODECID_AMR, 0, GF_STREAM_AUDIO, "AMR Audio", "amr", "samr", "audio/amr"},
	{GF_CODECID_AMR_WB, 0, GF_STREAM_AUDIO, "AMR WideBand Audio", "amr|amrwb", "sawb", "audio/amr"},
	{GF_CODECID_EVRC_PV, 0, GF_STREAM_AUDIO, "EVRC (PacketVideo MUX) Audio", "qcp|evrcpv", "sevc", "audio/evrc"},
	{GF_CODECID_SMPTE_VC1, 0xA3, GF_STREAM_VISUAL, "SMPTE VC-1 Video", "vc1", "vc1 ", "video/vc1"},
	{GF_CODECID_DIRAC, 0xA4, GF_STREAM_VISUAL, "Dirac Video", "dirac", NULL, "video/dirac"},
	{GF_CODECID_AC3, 0xA5, GF_STREAM_AUDIO, "AC3 Audio", "ac3", "ac-3", "audio/ac3", .unframe=GF_TRUE},
	{GF_CODECID_EAC3, 0xA6, GF_STREAM_AUDIO, "Enhanced AC3 Audio", "eac3", "ec-3", "audio/eac3", .unframe=GF_TRUE},
	{GF_CODECID_TRUEHD, 0, GF_STREAM_AUDIO, "Dolby TrueHD", "mlp", "mlpa", "audio/truehd", .unframe=GF_TRUE},
	{GF_CODECID_DRA, 0xA7, GF_STREAM_AUDIO, "DRA Audio", "dra", NULL, "audio/dra"},
	{GF_CODECID_G719, 0xA8, GF_STREAM_AUDIO, "G719 Audio", "g719", NULL, "audio/g719"},
	{GF_CODECID_DTS_CA, 0xA9, GF_STREAM_AUDIO, "DTS Coherent Acoustics and Digital Surround Audio", "dstc", NULL, "audio/dts"},
	{GF_CODECID_DTS_HD_HR_MASTER, 0xAA, GF_STREAM_AUDIO, "DTS-HD High Resolution Audio and DTS-Master Audio", "dtsh", NULL, "audio/dts"},
	{GF_CODECID_DTS_HD_LOSSLESS, 0xAB, GF_STREAM_AUDIO, "DTS-HD Substream containing only XLLAudio", "dstl", NULL, "audio/dts"},
	{GF_CODECID_DTS_EXPRESS_LBR, 0xAC, GF_STREAM_AUDIO, "DTS Express low bit rate Audio", "dtse", NULL, "audio/dts"},
	{GF_CODECID_DTS_X, 0xB2, GF_STREAM_AUDIO, "DTS-X UHD Audio Profile 2", "dtsx", NULL, "audio/dts"},
	{GF_CODECID_DTS_Y, 0xB3, GF_STREAM_AUDIO, "DTS-X UHD Audio Profile 3", "dtsy", NULL, "audio/dts"},
	{GF_CODECID_OPUS, 0xAD, GF_STREAM_AUDIO, "Opus Audio", "opus", "Opus", "audio/opus"},
	{GF_CODECID_DVB_EIT, 0, GF_STREAM_PRIVATE_SCENE, "DVB Event Information", "eti", NULL, "application/x-dvb-eit"},
	{GF_CODECID_SVG, 0, GF_STREAM_PRIVATE_SCENE, "SVG over RTP", "svgr", NULL, "application/x-svg-rtp"},
	{GF_CODECID_SVG_GZ, 0, GF_STREAM_PRIVATE_SCENE, "SVG+gz over RTP", "svgzr", NULL, "application/x-svgz-rtp"},
	{GF_CODECID_DIMS, 0, GF_STREAM_PRIVATE_SCENE, "3GPP DIMS Scene", "dims", NULL, "application/3gpp-dims"},
	{GF_CODECID_WEBVTT, 0, GF_STREAM_TEXT, "WebVTT Text", "vtt", "wvtt", "text/vtt", .unframe=GF_TRUE},
	{GF_CODECID_SIMPLE_TEXT, 0, GF_STREAM_TEXT, "Simple Text Stream", "txt", "stxt", "text/subtitle"},
	{GF_CODECID_META_TEXT, 0, GF_STREAM_METADATA, "Metadata Text Stream", "mtxt", "mett", "application/text"},
	{GF_CODECID_META_XML, 0, GF_STREAM_METADATA, "Metadata XML Stream", "mxml", "metx", "application/text+xml"},
	{GF_CODECID_SUBS_TEXT, 0, GF_STREAM_TEXT, "Subtitle text Stream", "subs", "sbtt", "text/text"},
	{GF_CODECID_SUBS_XML, 0, GF_STREAM_TEXT, "Subtitle XML Stream", "subx", "stpp", "text/text+xml", .unframe=GF_TRUE},
	{GF_CODECID_TX3G, 0, GF_STREAM_TEXT, "Subtitle/text 3GPP/Apple Stream", "tx3g", "tx3g", "quicktime/text", .unframe=GF_TRUE},
	{GF_CODECID_SUBS_SSA, 0, GF_STREAM_TEXT, "SSA /ASS Subtitles", "ssa", NULL, "text/x-ssa"},
	{GF_CODECID_THEORA, 0xDF, GF_STREAM_VISUAL, "Theora Video", "theo|theora", NULL, "video/theora"},
	{GF_CODECID_VORBIS, 0xDD, GF_STREAM_AUDIO, "Vorbis Audio", "vorb|vorbis", NULL, "audio/vorbis"},
	{GF_CODECID_OPUS, 0xDE, GF_STREAM_AUDIO, "Opus Audio", "opus", NULL, "audio/opus"},
	{GF_CODECID_FLAC, 0, GF_STREAM_AUDIO, "Flac Audio", "flac", "fLaC", "audio/flac", .unframe=GF_TRUE},
	{GF_CODECID_SPEEX, 0, GF_STREAM_AUDIO, "Speex Audio", "spx|speex", NULL, "audio/speex"},
	{GF_CODECID_SUBPIC, 0xE0, GF_STREAM_TEXT, "VobSub Subtitle", "vobsub", NULL, "text/x-vobsub"},
	{GF_CODECID_SUBPIC, 0xE0, GF_STREAM_ND_SUBPIC, "VobSub Subtitle", "vobsub", NULL, "text/x-vobsub"},
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
	{GF_CODECID_RAW, 0, GF_STREAM_UNKNOWN, "Raw media", "raw", NULL, "*/*"},
	{GF_CODECID_RAW_UNCV, 0, GF_STREAM_UNKNOWN, "Raw Video", "uncv", NULL, "*/*"},

	{GF_CODECID_AV1, 0, GF_STREAM_VISUAL, "AOM AV1 Video", "av1|ivf|obu|av1b", NULL, "video/av1", .unframe=GF_TRUE},
	{GF_CODECID_VP8, 0, GF_STREAM_VISUAL, "VP8 Video", "vp8|ivf", NULL, "video/vp8"},
	{GF_CODECID_VP9, 0, GF_STREAM_VISUAL, "VP9 Video", "vp9|ivf", NULL, "video/vp9", .unframe=GF_TRUE},
	{GF_CODECID_VP10, 0, GF_STREAM_VISUAL, "VP10 Video", "vp10|ivf", NULL, "video/vp10"},

	{GF_CODECID_MPHA, 0, GF_STREAM_AUDIO, "MPEG-H Audio", "mhas", "mha1", "audio/x-mpegh", .unframe=GF_TRUE},
	{GF_CODECID_MHAS, 0, GF_STREAM_AUDIO, "MPEG-H AudioMux", "mhas", "mhm1", "audio/x-mhas", .unframe=GF_TRUE},

	{GF_CODECID_APCH, 0, GF_STREAM_VISUAL, "ProRes Video 422 HQ", "prores|apch", "apch", "video/prores", .unframe=GF_TRUE},
	{GF_CODECID_APCO, 0, GF_STREAM_VISUAL, "ProRes Video 422 Proxy", "prores|apco", "apco", "video/prores", GF_CODECID_APCH, .unframe=GF_TRUE},
	{GF_CODECID_APCN, 0, GF_STREAM_VISUAL, "ProRes Video 422 STD", "prores|apcn", "apcn", "video/prores", GF_CODECID_APCH, .unframe=GF_TRUE},
	{GF_CODECID_APCS, 0, GF_STREAM_VISUAL, "ProRes Video 422 LT", "prores|apcs", "apcs", "video/prores", GF_CODECID_APCH, .unframe=GF_TRUE},
	{GF_CODECID_AP4X, 0, GF_STREAM_VISUAL, "ProRes Video 4444 XQ", "prores|ap4x", "ap4x", "video/prores", GF_CODECID_APCH, .unframe=GF_TRUE},
	{GF_CODECID_AP4H, 0, GF_STREAM_VISUAL, "ProRes Video 4444", "prores|ap4h", "ap4h", "video/prores", GF_CODECID_APCH, .unframe=GF_TRUE},
	{GF_CODECID_FFMPEG, 0, GF_STREAM_UNKNOWN, "FFMPEG unmapped codec", "ffmpeg", NULL, NULL},

	{GF_CODECID_TMCD, 0, GF_STREAM_METADATA, "QT TimeCode", "tmcd", NULL, NULL},
	{GF_CODECID_VVC, 0, GF_STREAM_VISUAL, "VVC Video", "vvc|266|h266", "vvc1", "video/vvc", .unframe=GF_TRUE},
	{GF_CODECID_VVC_SUBPIC, 0, GF_STREAM_VISUAL, "VVC Subpicture Video", "vvs1", "vvs1", "video/x-vvc-subpic", .alt_codecid=GF_CODECID_VVC, .unframe=GF_TRUE},
	{GF_CODECID_USAC, GF_CODECID_AAC_MPEG4, GF_STREAM_AUDIO, "xHEAAC / USAC Audio", "usac|xheaac", "mp4a", "audio/x-xheaac", .unframe=GF_TRUE},
	{GF_CODECID_FFV1, 0, GF_STREAM_VISUAL, "FFMPEG Video Codec 1", "ffv1", NULL, "video/x-ffv1"},

	{GF_CODECID_DVB_SUBS, 0, GF_STREAM_TEXT, "DVB Subtitles", "dvbs", NULL, NULL},
	{GF_CODECID_DVB_TELETEXT, 0, GF_STREAM_TEXT, "DVB-TeleText", "dvbs", NULL, NULL},
	{GF_CODECID_MSPEG4_V3, 0, GF_STREAM_VISUAL, "MS-MPEG4 V3", "div3", NULL, NULL, GF_CODECID_MSPEG4_V3},

	{GF_CODECID_ALAC, 0, GF_STREAM_AUDIO, "Apple Lossless Audio", "caf", NULL, NULL},

};


GF_EXPORT
GF_CodecID gf_codecid_parse(const char *cname)
{
	u32 ilen = (u32) strlen(cname);
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		if (!strcmp(CodecRegistry[i].name, cname))
			return CodecRegistry[i].codecid;

		const char *n = CodecRegistry[i].sname;
		while (n) {
			char *sep = strchr(n, '|');
			u32 len;
			if (sep)
				len = (u32) (sep - n);
			else
				len = (u32) strlen(n);

			//allow case insensitive names
			if ((len==ilen) && !strnicmp(n, cname, len))
				return CodecRegistry[i].codecid;
			if (!sep) break;
			n = sep+1;
		}
	}
	return GF_CODECID_NONE;
}


#include <gpac/internal/isomedia_dev.h>

GF_EXPORT
GF_CodecID gf_codec_id_from_isobmf(u32 isobmftype)
{
	switch (isobmftype) {
	case GF_ISOM_SUBTYPE_DVHE:
	case GF_ISOM_SUBTYPE_DVH1:
		return GF_CODECID_HEVC;
	case GF_ISOM_SUBTYPE_DVA1:
	case GF_ISOM_SUBTYPE_DVAV:
		return GF_CODECID_AVC;
	case GF_ISOM_SUBTYPE_AV01:
	case GF_ISOM_SUBTYPE_DAV1:
		return GF_CODECID_AV1;
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
	case GF_ISOM_SUBTYPE_VVC1:
	case GF_ISOM_SUBTYPE_VVI1:
		return GF_CODECID_VVC;
	case GF_ISOM_SUBTYPE_VP08:
		return GF_CODECID_VP8;
	case GF_ISOM_SUBTYPE_VP09:
		return GF_CODECID_VP9;
	case GF_ISOM_SUBTYPE_VP10:
		return GF_CODECID_VP10;

	case GF_QT_SUBTYPE_APCH:
		return GF_CODECID_APCH;
	case GF_QT_SUBTYPE_APCO:
		return GF_CODECID_APCO;
	case GF_QT_SUBTYPE_APCN:
		return GF_CODECID_APCN;
	case GF_QT_SUBTYPE_APCS:
		return GF_CODECID_APCS;
	case GF_QT_SUBTYPE_AP4X:
		return GF_CODECID_AP4X;
	case GF_QT_SUBTYPE_AP4H:
		return GF_CODECID_AP4H;
	case GF_QT_SUBTYPE_TWOS:
	case GF_QT_SUBTYPE_SOWT:
	case GF_QT_SUBTYPE_FL32:
	case GF_QT_SUBTYPE_FL64:
	case GF_QT_SUBTYPE_IN24:
	case GF_QT_SUBTYPE_IN32:
	case GF_ISOM_SUBTYPE_IPCM:
	case GF_ISOM_SUBTYPE_FPCM:
		return GF_CODECID_RAW;
	case GF_ISOM_SUBTYPE_MLPA:
		return GF_CODECID_TRUEHD;
	case GF_ISOM_SUBTYPE_FFV1:
		return GF_CODECID_FFV1;
	case GF_ISOM_SUBTYPE_DTSC:
		return GF_CODECID_DTS_CA;
	case GF_ISOM_SUBTYPE_DTSL:
		return GF_CODECID_DTS_HD_LOSSLESS;
	case GF_ISOM_SUBTYPE_DTSH:
		return GF_CODECID_DTS_HD_HR_MASTER;
	case GF_ISOM_SUBTYPE_DTSE:
		return GF_CODECID_DTS_EXPRESS_LBR;
	case GF_ISOM_SUBTYPE_DTSX:
		return GF_CODECID_DTS_X;
	case GF_ISOM_SUBTYPE_DTSY:
		return GF_CODECID_DTS_Y;
	case GF_QT_SUBTYPE_ALAC:
		return GF_CODECID_ALAC;
	case GF_ISOM_SUBTYPE_VC1:
		return GF_CODECID_SMPTE_VC1;
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
const char *gf_codecid_name(GF_CodecID codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return "Codec Not Supported";
	return r->name;
}

GF_EXPORT
const char *gf_codecid_file_ext(GF_CodecID codecid)
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
const char *gf_codecid_mime(GF_CodecID codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (r && r->mime_type) return r->mime_type;
	return "application/octet-string";
}

GF_EXPORT
GF_CodecID gf_codecid_enum(u32 idx, const char **short_name, const char **long_name)
{
	u32 count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	if (idx >= count) return GF_CODECID_NONE;
	if (short_name) *short_name = CodecRegistry[idx].sname;
	if (long_name) *long_name = CodecRegistry[idx].name;
	return CodecRegistry[idx].codecid;
}


GF_EXPORT
u32 gf_codecid_type(GF_CodecID codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return GF_STREAM_UNKNOWN;
	return r->stream_type;
}

GF_EXPORT
GF_CodecID gf_codecid_alt(GF_CodecID codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return GF_CODECID_NONE;
	return r->alt_codecid;
}

GF_EXPORT
GF_CodecID gf_codecid_from_oti(u32 stream_type, u32 oti)
{
	CodecIDReg *r;
	if (!oti) {
		if ((stream_type==GF_STREAM_OD) || (stream_type==GF_STREAM_SCENE))
			oti = 1;
	}
	r = gf_codecid_reg_find_oti(stream_type, oti);
	if (!r) return GF_CODECID_NONE;
	return r->codecid;
}

GF_EXPORT
u8 gf_codecid_oti(GF_CodecID codecid)
{
	CodecIDReg *r = gf_codecid_reg_find(codecid);
	if (!r) return 0;
	return r->mpeg4_oti;
}

GF_EXPORT
u32 gf_codecid_4cc_type(GF_CodecID codecid)
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
Bool gf_codecid_has_unframer(GF_CodecID codecid)
{
	u32 i, count = sizeof(CodecRegistry) / sizeof(CodecIDReg);
	for (i=0; i<count; i++) {
		if (CodecRegistry[i].codecid==codecid) {
			return CodecRegistry[i].unframe;
		}
	}
	return GF_FALSE;
}

typedef struct
{
	u32 st;
	const char *name;
	const char *desc;
	const char *sname;
	const char *alt_name;
} GF_StreamTypeDesc;

static const GF_StreamTypeDesc GF_StreamTypes[] =
{
	{GF_STREAM_VISUAL, "Visual", "Video or Image stream", "video", "Video"},
	{GF_STREAM_AUDIO, "Audio", "Audio stream", "audio"},
	{GF_STREAM_SCENE, "SceneDescription", "Scene stream", "scene"},
	{GF_STREAM_TEXT, "Text", "Text or subtitle stream", "text"},
	{GF_STREAM_METADATA, "Metadata", "Metadata stream", "meta"},
	{GF_STREAM_FILE, "File", "Raw file stream", "file"},
	{GF_STREAM_ENCRYPTED, "Encrypted", "Encrypted media stream", "crypt"},
	{GF_STREAM_OD, "ObjectDescriptor", "MPEG-4 ObjectDescriptor stream", "od"},
	{GF_STREAM_OCR, "ClockReference", "MPEG-4 Clock Reference stream", "ocr"},
	{GF_STREAM_MPEG7, "MPEG7", "MPEG-7 description stream", "mpeg7"},
	{GF_STREAM_IPMP, "IPMP", "MPEG-4 IPMP/DRM stream", "ipmp"},
	{GF_STREAM_OCI, "OCI", "MPEG-4 ObjectContentInformation stream", "oci"},
	{GF_STREAM_MPEGJ, "MPEGJ", "MPEG-4 JAVA stream", "mpegj"},
	{GF_STREAM_INTERACT, "Interaction", "MPEG-4 Interaction Sensor stream", "interact"},
	{GF_STREAM_FONT, "Font", "MPEG-4 Font stream", "font"}
};

GF_EXPORT
const char *gf_stream_type_name(u32 streamType)
{
	u32 i, nb_st = sizeof(GF_StreamTypes) / sizeof(GF_StreamTypeDesc);
	for (i=0; i<nb_st; i++) {
		if (GF_StreamTypes[i].st == streamType)
			return GF_StreamTypes[i].name;
	}
	return "Unknown";
}

GF_EXPORT
const char *gf_stream_type_short_name(u32 streamType)
{
	u32 i, nb_st = sizeof(GF_StreamTypes) / sizeof(GF_StreamTypeDesc);
	for (i=0; i<nb_st; i++) {
		if (GF_StreamTypes[i].st == streamType)
			return GF_StreamTypes[i].sname;
	}
	return "unkn";
}

GF_EXPORT
u32 gf_stream_type_by_name(const char *val)
{
	u32 i, nb_st = sizeof(GF_StreamTypes) / sizeof(GF_StreamTypeDesc);
	for (i=0; i<nb_st; i++) {
		if (!stricmp(GF_StreamTypes[i].name, val))
			return GF_StreamTypes[i].st;
		if (GF_StreamTypes[i].alt_name && !stricmp(GF_StreamTypes[i].alt_name, val))
			return GF_StreamTypes[i].st;
	}
	if (strnicmp(val, "unkn", 4) && strnicmp(val, "undef", 5)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unknow stream type %s\n", val));
	}
	return GF_STREAM_UNKNOWN;
}

#if 0
static char szAllStreamTypes[500] = {0};

/*! Gets the list of names of all stream types defined
\return names of all stream types defined
 */
const char *gf_stream_type_all_names()
{
	if (!szAllStreamTypes[0]) {
		u32 i, nb_st = sizeof(GF_StreamTypes) / sizeof(GF_StreamTypeDesc);
		u32 tot_len=0;
		strcpy(szAllStreamTypes, "");
		for (i=0; i<nb_st; i++) {
			u32 len = (u32) strlen(GF_StreamTypes[i].name);
			if (len+tot_len+2>=500) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Not enough memory to hold all stream types!!\n"));
				break;
			}
			if (i) {
				strcat((char *)szAllStreamTypes, ",");
				tot_len += 1;
			}
			strcat((char *)szAllStreamTypes, GF_StreamTypes[i].name);
			tot_len += len;
		}
	}
	return szAllStreamTypes;
}
#endif


GF_EXPORT
u32 gf_stream_types_enum(u32 *idx, const char **name, const char **desc)
{
	u32 stype;
	u32 nb_sfmt = sizeof(GF_StreamTypes) / sizeof(GF_StreamTypeDesc);
	if (*idx >= nb_sfmt) return GF_STREAM_UNKNOWN;
	if (!GF_StreamTypes[*idx].st) return GF_STREAM_UNKNOWN;

	*name = GF_StreamTypes[*idx].name;
	*desc = GF_StreamTypes[*idx].desc;
	stype = GF_StreamTypes[*idx].st;
	(*idx)++;
	return stype;
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
	GF_AudioFormat sfmt;
	const char *name; //as used in gpac
	const char *desc;
	const char *sname; //short name, as used in gpac
} GF_AudioFmt;

static const GF_AudioFmt GF_AudioFormats[] =
{
	{GF_AUDIO_FMT_U8, "u8", "8 bit PCM", "pc8"},
	{GF_AUDIO_FMT_S16, "s16", "16 bit PCM Little Endian", "pcm"},
	{GF_AUDIO_FMT_S16_BE, "s16b", "16 bit PCM Big Endian", "pcmb"},
	{GF_AUDIO_FMT_S24, "s24", "24 bit PCM"},
	{GF_AUDIO_FMT_S24_BE, "s24b", "24 bit Big-Endian PCM"},
	{GF_AUDIO_FMT_S32, "s32", "32 bit PCM Little Endian"},
	{GF_AUDIO_FMT_S32_BE, "s32b", "32 bit PCM Big Endian"},
	{GF_AUDIO_FMT_FLT, "flt", "32-bit floating point PCM"},
	{GF_AUDIO_FMT_FLT_BE, "fltb", "32-bit floating point PCM Big Endian"},
	{GF_AUDIO_FMT_DBL, "dbl", "64-bit floating point PCM"},
	{GF_AUDIO_FMT_DBL_BE, "dblb", "64-bit floating point PCM Big Endian"},
	{GF_AUDIO_FMT_U8P, "u8p", "8 bit PCM planar", "pc8p"},
	{GF_AUDIO_FMT_S16P, "s16p", "16 bit PCM Little Endian planar", "pcmp"},
	{GF_AUDIO_FMT_S24P, "s24p", "24 bit PCM planar"},
	{GF_AUDIO_FMT_S32P, "s32p", "32 bit PCM Little Endian planar"},
	{GF_AUDIO_FMT_FLTP, "fltp", "32-bit floating point PCM planar"},
	{GF_AUDIO_FMT_DBLP, "dblp", "64-bit floating point PCM planar"},
	{0}
};


GF_EXPORT
GF_AudioFormat gf_audio_fmt_parse(const char *af_name)
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Unsupported audio format %s\n", af_name));
	return 0;
}

GF_EXPORT
const char *gf_audio_fmt_name(GF_AudioFormat sfmt)
{
	u32 i=0;
	while (!i || GF_AudioFormats[i].sfmt) {
		if (GF_AudioFormats[i].sfmt==sfmt) return GF_AudioFormats[i].name;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported audio format %d\n", sfmt ));
	return "unknown";
}
GF_EXPORT
const char *gf_audio_fmt_sname(GF_AudioFormat sfmt)
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
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported audio format %d\n", sfmt ));
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Not enough memory to hold all audio formats!!\n"));
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Not enough memory to hold all audio formats!!\n"));
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
GF_AudioFormat gf_audio_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **desc)
{
	GF_AudioFormat afmt;
	u32 nb_afmt = sizeof(GF_AudioFormats) / sizeof(GF_AudioFmt);
	if (*idx >= nb_afmt) return 0;
	if (!GF_AudioFormats[*idx].sfmt) return 0;

	*name = GF_AudioFormats[*idx].name;
	*desc = GF_AudioFormats[*idx].desc;

	*fileext = GF_AudioFormats[*idx].sname;
	if (! *fileext) *fileext = *name;
	afmt = GF_AudioFormats[*idx].sfmt;
	(*idx)++;
	return afmt;
}

GF_EXPORT
u32 gf_audio_fmt_bit_depth(GF_AudioFormat audio_fmt)
{
	switch (audio_fmt) {
	case GF_AUDIO_FMT_U8P:
	case GF_AUDIO_FMT_U8:
		return 8;

	case GF_AUDIO_FMT_S16P:
	case GF_AUDIO_FMT_S16_BE:
	case GF_AUDIO_FMT_S16:
		return 16;

	case GF_AUDIO_FMT_S32P:
	case GF_AUDIO_FMT_S32_BE:
	case GF_AUDIO_FMT_S32:
		return 32;

	case GF_AUDIO_FMT_FLTP:
	case GF_AUDIO_FMT_FLT:
	case GF_AUDIO_FMT_FLT_BE:
		return 32;

	case GF_AUDIO_FMT_DBLP:
	case GF_AUDIO_FMT_DBL:
	case GF_AUDIO_FMT_DBL_BE:
		return 64;

	case GF_AUDIO_FMT_S24P:
	case GF_AUDIO_FMT_S24_BE:
	case GF_AUDIO_FMT_S24:
		return 24;

	default:
		break;
	}
	return 0;
}

GF_EXPORT
Bool gf_audio_fmt_is_planar(GF_AudioFormat audio_fmt)
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

static struct pcmfmt_to_qt
{
	GF_AudioFormat afmt;
	u32 qt4cc;
} AudiosToQT[] = {
	{GF_AUDIO_FMT_S16, GF_QT_SUBTYPE_SOWT},
	{GF_AUDIO_FMT_FLT_BE, GF_QT_SUBTYPE_FL32},
	{GF_AUDIO_FMT_DBL_BE, GF_QT_SUBTYPE_FL64},
	{GF_AUDIO_FMT_S24_BE, GF_QT_SUBTYPE_IN24},
	{GF_AUDIO_FMT_S32_BE, GF_QT_SUBTYPE_IN32},
	{GF_AUDIO_FMT_S16_BE, GF_QT_SUBTYPE_TWOS},
};

GF_EXPORT
GF_AudioFormat gf_audio_fmt_from_isobmf(u32 msubtype)
{
	u32 i, count = GF_ARRAY_LENGTH(AudiosToQT);
	for (i=0; i<count; i++) {
		if (msubtype == AudiosToQT[i].qt4cc)
			return AudiosToQT[i].afmt;
	}
	return 0;
}

GF_EXPORT
u32 gf_audio_fmt_to_isobmf(GF_AudioFormat afmt)
{
	u32 i, count = GF_ARRAY_LENGTH(AudiosToQT);
	for (i=0; i<count; i++) {
		if (afmt == AudiosToQT[i].afmt)
			return AudiosToQT[i].qt4cc;
	}
	return 0;
}

GF_EXPORT
u32 gf_audio_fmt_get_cicp_layout(u32 nb_chan, u32 nb_surr, u32 nb_lfe)
{
	if (nb_chan <= nb_surr+nb_lfe) return 0;
	else nb_chan -= nb_surr+nb_lfe;
	if (!nb_chan && !nb_surr && !nb_lfe) return 0;
	else if ((nb_chan==1) && !nb_surr && !nb_lfe) return 1;
	else if ((nb_chan==2) && !nb_surr && !nb_lfe) return 2;
	else if ((nb_chan==3) && !nb_surr && !nb_lfe) return 3;
	else if ((nb_chan==3) && (nb_surr==1) && !nb_lfe) return 4;
	else if ((nb_chan==3) && (nb_surr==2) && !nb_lfe) return 5;

	else if ((nb_chan==3) && (nb_surr==2) && (nb_lfe==1)) return 6;
	else if ((nb_chan==5) && (nb_surr==0) && (nb_lfe==1)) return 6;
	else if ((nb_chan==6) && (nb_surr==0) && (nb_lfe==0)) return 6; //mis-signalled 5.1

	else if ((nb_chan==5) && (nb_surr==2) && (nb_lfe==1)) return 7;
	else if ((nb_chan==2) && (nb_surr==1) && !nb_lfe) return 9;
	else if ((nb_chan==2) && (nb_surr==2) && !nb_lfe) return 10;
	else if ((nb_chan==3) && (nb_surr==3) && (nb_lfe==1)) return 11;
	else if ((nb_chan==3) && (nb_surr==4) && (nb_lfe==1)) return 12;
	else if ((nb_chan==11) && (nb_surr==11) && (nb_lfe==2)) return 13;
	//we miss left / right front center vs left / right front vertical to signal this one
//	else if ((nb_chan==5) && (nb_surr==2) && (nb_lfe==1)) return 14;
	else if ((nb_chan==5) && (nb_surr==5) && (nb_lfe==2)) return 15;

	else if ((nb_chan==5) && (nb_surr==4) && (nb_lfe==1)) return 16;
	else if ((nb_chan==10) && (nb_surr==0) && (nb_lfe==0)) return 16; //mis-signalled 5.1.4

	else if ((nb_surr==5) && (nb_lfe==1) && (nb_chan==6)) return 17;
	else if ((nb_surr==7) && (nb_lfe==1) && (nb_chan==6)) return 18;
	else if ((nb_chan==5) && (nb_surr==6) && (nb_lfe==1)) return 19;
	else if ((nb_chan==7) && (nb_surr==6) && (nb_lfe==1)) return 20;

	GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("Unknown CICP mapping for channel config %d/%d.%d\n", nb_chan, nb_surr, nb_lfe));
	return 0;
}

typedef struct
{
	u32 cicp;
	const char *name;
	u64 channel_mask;
} GF_CICPAudioLayout;

static const GF_CICPAudioLayout GF_CICPLayouts[] =
{
	{1, "mono"/*"1/0.0"*/, GF_AUDIO_CH_FRONT_CENTER },
	{2, "stereo"/*"2/0.0"*/, GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT },
	{3, "3/0.0", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER },
	{4, "3/1.0", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_CENTER },
	{5, "3/2.0", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT },
	{6, "3/2.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE },
	{7, "5/2.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE },
	{8, "1+1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT },
	{9, "2/1.0", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_REAR_CENTER },
	{10, "2/2.0", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT },
	{11, "3/3.1", GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT | GF_AUDIO_CH_REAR_CENTER | GF_AUDIO_CH_LFE },
	{12, "3/4.1", GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE },
	{13, "11/11.2", GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER_LEFT | GF_AUDIO_CH_FRONT_CENTER_RIGHT | GF_AUDIO_CH_SIDE_SURROUND_LEFT | GF_AUDIO_CH_SIDE_SURROUND_RIGHT | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_REAR_CENTER | GF_AUDIO_CH_LFE | GF_AUDIO_CH_LFE2 | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_FRONT_TOP_CENTER | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT | GF_AUDIO_CH_REAR_CENTER_TOP | GF_AUDIO_CH_SIDE_SURROUND_TOP_LEFT | GF_AUDIO_CH_SIDE_SURROUND_TOP_RIGHT | GF_AUDIO_CH_CENTER_SURROUND_TOP | GF_AUDIO_CH_FRONT_BOTTOM_CENTER | GF_AUDIO_CH_FRONT_BOTTOM_LEFT | GF_AUDIO_CH_FRONT_BOTTOM_RIGHT },
	{14, "5/2.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT },
	{15, "5/5.2", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_SIDE_SURROUND_LEFT | GF_AUDIO_CH_SIDE_SURROUND_RIGHT | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_CENTER_SURROUND_TOP | GF_AUDIO_CH_LFE | GF_AUDIO_CH_LFE2 },
	{16, "5/4.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT },
	{17, "6/5.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_FRONT_TOP_CENTER | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT | GF_AUDIO_CH_CENTER_SURROUND_TOP },
	{18, "6/7.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_SURROUND_LEFT | GF_AUDIO_CH_SURROUND_RIGHT | GF_AUDIO_CH_BACK_SURROUND_LEFT | GF_AUDIO_CH_BACK_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_FRONT_TOP_CENTER | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT | GF_AUDIO_CH_CENTER_SURROUND_TOP },
	{19, "5/6.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_SIDE_SURROUND_LEFT | GF_AUDIO_CH_SIDE_SURROUND_RIGHT | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT },
	{20, "7/6.1", GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_SCREEN_EDGE_LEFT | GF_AUDIO_CH_SCREEN_EDGE_RIGHT | GF_AUDIO_CH_SIDE_SURROUND_LEFT | GF_AUDIO_CH_SIDE_SURROUND_RIGHT | GF_AUDIO_CH_REAR_SURROUND_LEFT | GF_AUDIO_CH_REAR_SURROUND_RIGHT | GF_AUDIO_CH_LFE | GF_AUDIO_CH_FRONT_TOP_LEFT | GF_AUDIO_CH_FRONT_TOP_RIGHT | GF_AUDIO_CH_SURROUND_TOP_LEFT | GF_AUDIO_CH_SURROUND_TOP_RIGHT }
};

GF_EXPORT
u64 gf_audio_fmt_get_layout_from_cicp(u32 cicp_layout)
{
	u32 i, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	for (i = 0; i < nb_cicp; i++) {
		if (GF_CICPLayouts[i].cicp == cicp_layout) return GF_CICPLayouts[i].channel_mask;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported cicp audio layout value %d\n", cicp_layout));
	return 0;
}

GF_EXPORT
const char *gf_audio_fmt_get_layout_name_from_cicp(u32 cicp_layout)
{
	u32 i, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	for (i = 0; i < nb_cicp; i++) {
		if (GF_CICPLayouts[i].cicp == cicp_layout) return GF_CICPLayouts[i].name;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported cicp audio layout value %d\n", cicp_layout));
	return "unknown";
}

GF_EXPORT
const char *gf_audio_fmt_get_layout_name(u64 ch_layout)
{
	u32 i, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	for (i = 0; i < nb_cicp; i++) {
		if (GF_CICPLayouts[i].channel_mask == ch_layout) return GF_CICPLayouts[i].name;
	}
	if (!(ch_layout & GF_AUDIO_CH_REAR_SURROUND_LEFT) && !(ch_layout & GF_AUDIO_CH_REAR_SURROUND_RIGHT)
		&& (ch_layout & GF_AUDIO_CH_SURROUND_LEFT) && (ch_layout & GF_AUDIO_CH_SURROUND_RIGHT)
	) {
		ch_layout &= ~(GF_AUDIO_CH_SURROUND_LEFT|GF_AUDIO_CH_SURROUND_RIGHT);
		ch_layout |= (GF_AUDIO_CH_REAR_SURROUND_LEFT|GF_AUDIO_CH_REAR_SURROUND_RIGHT);
		return gf_audio_fmt_get_layout_name(ch_layout);
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported audio layout value "LLU"\n", ch_layout));
	return "unknown";
}

GF_EXPORT
u64 gf_audio_fmt_get_layout_from_name(const char *name)
{
	u32 i, iname, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	if (!name) return 0;
	iname = atoi(name);
	for (i = 0; i < nb_cicp; i++) {
		if (!strcmp(GF_CICPLayouts[i].name, name))
			return GF_CICPLayouts[i].channel_mask;
		if (GF_CICPLayouts[i].cicp ==  iname)
			return GF_CICPLayouts[i].channel_mask;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported audio layout name %s\n", name));
	return 0;
}


GF_EXPORT
u32 gf_audio_fmt_get_cicp_from_layout(u64 chan_layout)
{
	u32 i, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	for (i = 0; i < nb_cicp; i++) {
		if (GF_CICPLayouts[i].channel_mask == chan_layout) return GF_CICPLayouts[i].cicp;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported cicp audio layout for channel layout "LLU"\n", chan_layout));
	return 255;
}

//unused
#if 0
/*! get channel CICP code  from name
\param name channel layout name
\return channel CICP code
*/
u32 gf_audio_fmt_get_cicp_from_name(const char *name)
{
	u32 i, iname, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	if (!name) return 0;
	iname = atoi(name);
	for (i = 0; i < nb_cicp; i++) {
		if (!strcmp(GF_CICPLayouts[i].name, name))
			return GF_CICPLayouts[i].cicp;
		if (GF_CICPLayouts[i].cicp == iname)
			return GF_CICPLayouts[i].cicp;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported audio layout name %s\n", name));
	return 0;
}

/*! get channel CICP name from
\param cicp_code channel cicp code
\return channel CICP name
*/
const char *gf_audio_fmt_get_cicp_name(u32 cicp_code)
{
	u32 i, nb_cicp = sizeof(GF_CICPLayouts) / sizeof(GF_CICPAudioLayout);
	for (i = 0; i < nb_cicp; i++) {
		if (GF_CICPLayouts[i].cicp == cicp_code) return GF_CICPLayouts[i].name;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unsupported cicp audio layout for channel layout "LLU"\n", cicp_code));
	return NULL;
}
#endif


GF_EXPORT
u32 gf_audio_fmt_get_num_channels_from_layout(u64 chan_layout)
{
	u32 i, nb_chan = 0;
	for (i=0; i<64; i++) {
		nb_chan += (chan_layout&1) ? 1 : 0;
		chan_layout>>=1;
	}
	return nb_chan;
}

static char szCICPLayoutAllNames[1024];
const char *gf_audio_fmt_cicp_all_names()
{
	if (szCICPLayoutAllNames[0] == 0) {
		u32 i, count = GF_ARRAY_LENGTH(GF_CICPLayouts);
		for (i=0; i<count; i++) {
			if (i) strcat(szCICPLayoutAllNames, ",");
			strcat(szCICPLayoutAllNames, GF_CICPLayouts[i].name);
		}
	}
	return szCICPLayoutAllNames;
}


GF_EXPORT
u32 gf_audio_fmt_cicp_enum(u32 idx, const char **short_name, u64 *ch_mask)
{
	u32 count = GF_ARRAY_LENGTH(GF_CICPLayouts);
	if (idx >= count) return 0;
	if (short_name) *short_name = GF_CICPLayouts[idx].name;
	if (ch_mask) *ch_mask = GF_CICPLayouts[idx].channel_mask;
	return GF_CICPLayouts[idx].cicp;
}


GF_EXPORT
u16 gf_audio_fmt_get_dolby_chanmap(u32 cicp)
{
	u16 res = 0;
	u64 layout = gf_audio_fmt_get_layout_from_cicp(cicp);

	if (layout & GF_AUDIO_CH_FRONT_LEFT) res |= (1<<15); // 0
	if (layout & GF_AUDIO_CH_FRONT_CENTER) res |= (1<<14); //1
	if (layout & GF_AUDIO_CH_FRONT_RIGHT) res |= (1<<13); //2
	if (layout & GF_AUDIO_CH_REAR_SURROUND_LEFT) res |= (1<<12); //3
	if (layout & GF_AUDIO_CH_REAR_SURROUND_RIGHT) res |= (1<<11); //4
	//Lc/Rc
	if (layout & GF_AUDIO_CH_FRONT_CENTER_LEFT) res |= (1<<11); //5
	//Lrs/Rrs
	if (layout & GF_AUDIO_CH_SURROUND_LEFT) res |= (1<<9); //6
	//Cs
	if (layout & GF_AUDIO_CH_REAR_CENTER) res |= (1<<8); //7
	//Ts
	if (layout & GF_AUDIO_CH_REAR_CENTER_TOP) res |= (1<<7); //8
	//Lsd/Rsd
	if (layout & GF_AUDIO_CH_SIDE_SURROUND_LEFT) res |= (1<<6); //9
	//Lw/Rw
	if (layout & GF_AUDIO_CH_FRONT_CENTER_LEFT) res |= (1<<5); //10
	//Vhl/Vhr
	if (layout & GF_AUDIO_CH_FRONT_TOP_LEFT) res |= (1<<4); //11
	//Vhc
	if (layout & GF_AUDIO_CH_FRONT_TOP_CENTER) res |= (1<<3); //12
	//Lts/Rts
	if (layout & GF_AUDIO_CH_SURROUND_TOP_LEFT) res |= (1<<2); //13
	//LFE2
	if (layout & GF_AUDIO_CH_LFE2) res |= (1<<1); //14
	//LFE
	if (layout & GF_AUDIO_CH_LFE) res |= (1); //15
	return res;

}


typedef struct
{
	GF_PixelFormat pixfmt;
	const char *name; //as used in gpac
	const char *desc; //as used in gpac
	const char *sname; //short name, as used in gpac
} GF_PixFmt;

//DO NOT CHANGE ORDER, YUV formats first !
static const GF_PixFmt GF_PixelFormats[] =
{
	{GF_PIXEL_YUV, "yuv420", "Planar YUV 420 8 bit", "yuv"},
	{GF_PIXEL_YVU, "yvu420", "Planar YVU 420 8 bit", "yvu"},
	{GF_PIXEL_YUV_10, "yuv420_10", "Planar YUV 420 10 bit", "yuvl"},
	{GF_PIXEL_YUV422, "yuv422", "Planar YUV 422 8 bit", "yuv2"},
	{GF_PIXEL_YUV422_10, "yuv422_10", "Planar YUV 422 10 bit", "yp2l"},
	{GF_PIXEL_YUV444, "yuv444", "Planar YUV 444 8 bit", "yuv4"},
	{GF_PIXEL_YUV444_10, "yuv444_10", "Planar YUV 444 10 bit", "yp4l"},
	{GF_PIXEL_UYVY, "uyvy", "Packed UYVY 422 8 bit"},
	{GF_PIXEL_VYUY, "vyuy", "Packed VYUV 422 8 bit"},
	{GF_PIXEL_YUYV, "yuyv", "Packed YUYV 422 8 bit"},
	{GF_PIXEL_YVYU, "yvyu", "Packed YVYU 422 8 bit"},
	{GF_PIXEL_UYVY_10, "uyvl", "Packed UYVY 422 10->16 bit"},
	{GF_PIXEL_VYUY_10, "vyul", "Packed VYUV 422 10->16 bit"},
	{GF_PIXEL_YUYV_10, "yuyl", "Packed YUYV 422 10->16 bit"},
	{GF_PIXEL_YVYU_10, "yvyl", "Packed YVYU 422 10->16 bit"},
	{GF_PIXEL_NV12, "nv12", "Semi-planar YUV 420 8 bit, Y plane and UV packed plane"},
	{GF_PIXEL_NV21, "nv21", "Semi-planar YVU 420 8 bit, Y plane and VU packed plane"},
	{GF_PIXEL_NV12_10, "nv1l", "Semi-planar YUV 420 10 bit, Y plane and UV plane"},
	{GF_PIXEL_NV21_10, "nv2l", "Semi-planar YVU 420 8 bit, Y plane and VU plane"},
	{GF_PIXEL_YUVA, "yuva", "Planar YUV+alpha 420 8 bit"},
	{GF_PIXEL_YUVD, "yuvd", "Planar YUV+depth  420 8 bit"},
	{GF_PIXEL_YUVA444, "yuv444a", "Planar YUV+alpha 444 8 bit", "yp4a"},
	{GF_PIXEL_YUV444_PACK, "yuv444p", "Packed YUV 444 8 bit", "yv4p"},
	{GF_PIXEL_VYU444_PACK, "v308", "Packed VYU 444 8 bit"},
	{GF_PIXEL_YUVA444_PACK, "yuv444ap", "Packed YUV+alpha 444 8 bit", "y4ap"},
	{GF_PIXEL_UYVA444_PACK, "v408", "Packed UYV+alpha 444 8 bit"},
	{GF_PIXEL_YUV444_10_PACK, "v410", "Packed UYV 444 10 bit LE"},
	{GF_PIXEL_V210, "v210", "Packed UYVY 422 10 bit LE"},

	//first non-yuv format
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
	{GF_PIXEL_GL_EXTERNAL, "extgl", "External OpenGL texture of unknown format, to be used with samplerExternalOES\n"},
	{GF_PIXEL_UNCV, "uncv", "Generic uncompressed format ISO/IEC 23001-17"},
	{0}
};

GF_EXPORT
GF_PixelFormat gf_pixel_fmt_parse(const char *pf_name)
{
	u32 i=0;
	if (!pf_name || !strcmp(pf_name, "none") || !strcmp(pf_name, "0")) return 0;
	while (GF_PixelFormats[i].pixfmt) {
		if (!strcmp(GF_PixelFormats[i].name, pf_name))
			return GF_PixelFormats[i].pixfmt;
		if (GF_PixelFormats[i].sname && !strcmp(GF_PixelFormats[i].sname, pf_name))
			return GF_PixelFormats[i].pixfmt;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %s\n", pf_name));
	return 0;
}

GF_EXPORT
Bool gf_pixel_fmt_probe(GF_PixelFormat pf_4cc, const char *pf_name)
{
	u32 i=0;
	if (pf_name) {
		if (!strcmp(pf_name, "none") || !strcmp(pf_name, "0")) return GF_TRUE;
	}
	while (GF_PixelFormats[i].pixfmt) {
		if (pf_4cc) {
			if (GF_PixelFormats[i].pixfmt == pf_4cc)
				return GF_TRUE;
		}
		if (pf_name) {
			if (!strcmp(GF_PixelFormats[i].name, pf_name))
				return GF_TRUE;
			if (GF_PixelFormats[i].sname && !strcmp(GF_PixelFormats[i].sname, pf_name))
				return GF_TRUE;
		}
		i++;
	}
	return GF_FALSE;
}

GF_EXPORT
const char *gf_pixel_fmt_name(GF_PixelFormat pfmt)
{
	u32 i=0;
	while (GF_PixelFormats[i].pixfmt) {
		if (GF_PixelFormats[i].pixfmt==pfmt) return GF_PixelFormats[i].name;
		i++;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %d (%s)\n", pfmt, gf_4cc_to_str(pfmt) ));
	return "unknown";
}
GF_EXPORT
const char *gf_pixel_fmt_sname(GF_PixelFormat pfmt)
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
	GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %d (%s)\n", pfmt, gf_4cc_to_str(pfmt) ));
	return "unknown";

}

GF_EXPORT
GF_PixelFormat gf_pixel_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **description)
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


GF_EXPORT
Bool gf_pixel_fmt_is_yuv(GF_PixelFormat pfmt)
{
	u32 i=0;
	while (GF_PixelFormats[i].pixfmt) {
		if (GF_PixelFormats[i].pixfmt==pfmt) return GF_TRUE;
		if (GF_PixelFormats[i].pixfmt==GF_PIXEL_GREYSCALE) return GF_FALSE;
		i++;
	}
	return GF_FALSE;
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
			u32 len;

			//we don't expose this one
			if (GF_PixelFormats[i].pixfmt==GF_PIXEL_GL_EXTERNAL) {
				i++;
				continue;
			}

			len = (u32) strlen(GF_PixelFormats[i].name);
			if (len+tot_len+2>=5000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Not enough memory to hold all pixel formats!!\n"));
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
			u32 len;
			const char *n;
			//we don't expose this one
			if (GF_PixelFormats[i].pixfmt==GF_PIXEL_GL_EXTERNAL) {
				i++;
				continue;
			}
			n = GF_PixelFormats[i].sname ? GF_PixelFormats[i].sname : GF_PixelFormats[i].name;
			len = (u32) strlen(n);
			if (len+tot_len+1>=5000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Not enough memory to hold all pixel formats!!\n"));
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
		stride = no_in_stride ? 3*width : *out_stride;
		size = stride * height;
		planes=1;
		break;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
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
	case GF_PIXEL_YUVA444:
		stride = no_in_stride ? width : *out_stride;
		uv_height = height;
		stride_uv = no_in_stride_uv ? stride : *out_stride_uv;
		planes=4;
		size = stride * height * 4;
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
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
		stride = no_in_stride ? 4*width : *out_stride;
		planes=1;
		size = height * stride;
		break;
	case GF_PIXEL_YUV444_PACK:
	case GF_PIXEL_VYU444_PACK:
		stride = no_in_stride ? 3 * width : *out_stride;
		planes=1;
		size = height * stride;
		break;
	case GF_PIXEL_YUVA444_PACK:
	case GF_PIXEL_UYVA444_PACK:
		stride = no_in_stride ? 4 * width : *out_stride;
		planes=1;
		size = height * stride;
		break;
	case GF_PIXEL_YUV444_10_PACK:
		stride = no_in_stride ? 4 * width : *out_stride;
		planes = 1;
		size = height * stride;
		break;

	case GF_PIXEL_GL_EXTERNAL:
		planes = 1;
		size = 0;
		stride = 0;
		stride_uv = 0;
		uv_height = 0;
		break;
	case GF_PIXEL_V210:
		if (no_in_stride) {
			stride = width;
			while (stride % 48) stride++;
			stride = stride * 16 / 6; //4 x 32 bits to represent 6 pixels
		} else {
			stride = *out_stride;
		}
		planes=1;
		size = height * stride;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %s, cannot get size info\n", gf_pixel_fmt_name(pixfmt) ));
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
Bool gf_pixel_fmt_is_transparent(GF_PixelFormat pixfmt)
{
	switch (pixfmt) {
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
	case GF_PIXEL_YUVA444_PACK:
	case GF_PIXEL_UYVA444_PACK:
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

GF_EXPORT
u32 gf_pixel_is_wide_depth(GF_PixelFormat pixfmt)
{
	switch (pixfmt) {
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUV444_10:
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
	case GF_PIXEL_YUV444_10_PACK:
	case GF_PIXEL_V210:
		return 10;
	default:
		return 8;
	}
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
	case GF_PIXEL_RGB_DEPTH:
		return 4;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
		return 3;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
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
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
		return 2;
	case GF_PIXEL_YUV444_PACK:
	case GF_PIXEL_VYU444_PACK:
	case GF_PIXEL_YUVA444_PACK:
	case GF_PIXEL_UYVA444_PACK:
	case GF_PIXEL_YUV444_10_PACK:
	case GF_PIXEL_V210:
		return 1;

	case GF_PIXEL_GL_EXTERNAL:
		return 1;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %s, cannot get bytes per pixel info\n", gf_pixel_fmt_name(pixfmt) ));
		break;
	}
	return 0;
}

GF_EXPORT
u32 gf_pixel_get_nb_comp(GF_PixelFormat pixfmt)
{
	switch (pixfmt) {
	case GF_PIXEL_GREYSCALE:
		return 1;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		return 2;
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
		return 3;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_RGBA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ABGR:
		return 4;
	case GF_PIXEL_RGBD:
		return 4;
	case GF_PIXEL_RGBDS:
		return 5;
	case GF_PIXEL_RGB_DEPTH:
		return 4;
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
		return 3;
	case GF_PIXEL_YUV:
	case GF_PIXEL_YVU:
		return 3;
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVA444:
		return 4;
	case GF_PIXEL_YUVD:
		return 4;
	case GF_PIXEL_YUV_10:
		return 3;
	case GF_PIXEL_YUV422:
		return 3;
	case GF_PIXEL_YUV422_10:
		return 3;
	case GF_PIXEL_YUV444:
		return 3;
	case GF_PIXEL_YUV444_10:
		return 3;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		return 3;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		return 3;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_VYUY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
		return 3;
	case GF_PIXEL_UYVY_10:
	case GF_PIXEL_VYUY_10:
	case GF_PIXEL_YUYV_10:
	case GF_PIXEL_YVYU_10:
		return 3;
	case GF_PIXEL_YUV444_PACK:
	case GF_PIXEL_VYU444_PACK:
		return 3;
	case GF_PIXEL_YUVA444_PACK:
	case GF_PIXEL_UYVA444_PACK:
		return 4;
	case GF_PIXEL_YUV444_10_PACK:
		return 3;
	case GF_PIXEL_V210:
		return 3;
	case GF_PIXEL_GL_EXTERNAL:
		return 1;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unsupported pixel format %s, cannot get number of components per pixel info\n", gf_pixel_fmt_name(pixfmt) ));
		break;
	}
	return 0;
}

static struct pixfmt_to_qt
{
	GF_PixelFormat pfmt;
	u32 qt4cc;
} PixelsToQT[] = {
	{GF_PIXEL_RGB, GF_QT_SUBTYPE_RAW},
	{GF_PIXEL_YUYV, GF_QT_SUBTYPE_YUYV},
	{GF_PIXEL_UYVY, GF_QT_SUBTYPE_UYVY},
	{GF_PIXEL_VYU444_PACK, GF_QT_SUBTYPE_YUV444},
	{GF_PIXEL_UYVA444_PACK, GF_QT_SUBTYPE_YUVA444},
	{GF_PIXEL_UYVY_10, GF_QT_SUBTYPE_YUV422_16},
	{GF_PIXEL_YVYU, GF_QT_SUBTYPE_YVYU},
	{GF_PIXEL_YUV444_10_PACK, GF_QT_SUBTYPE_YUV444_10},
	{GF_PIXEL_YUV, GF_QT_SUBTYPE_YUV420},
	{GF_PIXEL_YUV, GF_QT_SUBTYPE_I420},
	{GF_PIXEL_YUV, GF_QT_SUBTYPE_IYUV},
	{GF_PIXEL_YUV, GF_QT_SUBTYPE_YV12},
	{GF_PIXEL_RGBA, GF_QT_SUBTYPE_RGBA},
	{GF_PIXEL_ABGR, GF_QT_SUBTYPE_ABGR},
	{GF_PIXEL_V210, GF_QT_SUBTYPE_YUV422_10}
};

GF_EXPORT
GF_PixelFormat gf_pixel_fmt_from_qt_type(u32 qt_code)
{
	u32 i, count = GF_ARRAY_LENGTH(PixelsToQT);
	for (i=0; i<count; i++) {
		if (PixelsToQT[i].qt4cc==qt_code) return PixelsToQT[i].pfmt;
	}
	return 0;
}

GF_EXPORT
u32 gf_pixel_fmt_to_qt_type(GF_PixelFormat pix_fmt)
{
	u32 i, count = GF_ARRAY_LENGTH(PixelsToQT);
	for (i=0; i<count; i++) {
		if (PixelsToQT[i].pfmt==pix_fmt) return PixelsToQT[i].qt4cc;
	}
	//uncomment to see list of unmapped codecs when generating doc
	//GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Unknown mapping to QT/ISOBMFF for pixel format %s\n", gf_pixel_fmt_name(pix_fmt)));
	return 0;
}


static struct _itags {
	const char *name;
	const char *alt_name;
	u32 itag;
	u32 id3tag;
	u32 type;
	Bool match_substr;
} itunes_tags[] = {

	{"title", "name", GF_ISOM_ITUNE_NAME, GF_ID3V2_FRAME_TIT2, GF_ITAG_STR, 0},
	{"artist", NULL, GF_ISOM_ITUNE_ARTIST, GF_ID3V2_FRAME_TPE1, GF_ITAG_STR, 0},
	{"album_artist", "albumArtist", GF_ISOM_ITUNE_ALBUM_ARTIST, GF_ID3V2_FRAME_TPE2, GF_ITAG_STR, 1},
	{"album", NULL, GF_ISOM_ITUNE_ALBUM, GF_ID3V2_FRAME_TALB, GF_ITAG_STR, 0},
	{"group", "grouping", GF_ISOM_ITUNE_GROUP, GF_ID3V2_FRAME_TIT1, GF_ITAG_STR, 0},
	{"composer", NULL, GF_ISOM_ITUNE_COMPOSER, GF_ID3V2_FRAME_TCOM, GF_ITAG_STR, 0},
	{"writer", NULL, GF_ISOM_ITUNE_WRITER, GF_ID3V2_FRAME_TEXT, GF_ITAG_STR, 0},
	{"conductor", NULL, GF_ISOM_ITUNE_CONDUCTOR, GF_ID3V2_FRAME_TPE3, GF_ITAG_STR, 0},
	{"comment", "comments", GF_ISOM_ITUNE_COMMENT, GF_ID3V2_FRAME_COMM, GF_ITAG_STR, 1},
	//mapped dynamically to GF_ISOM_ITUNE_GENRE or GF_ISOM_ITUNE_GENRE_USER
	{"genre", NULL, GF_ISOM_ITUNE_GENRE, GF_ID3V2_FRAME_TCON, GF_ITAG_ID3_GENRE, 0},
	{"created", "releaseDate", GF_ISOM_ITUNE_CREATED, GF_ID3V2_FRAME_TYER, GF_ITAG_STR, 1},
	{"track", NULL, GF_ISOM_ITUNE_TRACK, 0, GF_ITAG_STR, 0},
	{"tracknum", NULL, GF_ISOM_ITUNE_TRACKNUMBER, GF_ID3V2_FRAME_TRCK, GF_ITAG_FRAC8, 0},
	{"disk", NULL, GF_ISOM_ITUNE_DISK, GF_ID3V2_FRAME_TPOS, GF_ITAG_FRAC6, 0},
	{"tempo", NULL, GF_ISOM_ITUNE_TEMPO, GF_ID3V2_FRAME_TBPM, GF_ITAG_INT16, 0},
	{"compilation", NULL, GF_ISOM_ITUNE_COMPILATION, GF_ID3V2_FRAME_TCMP, GF_ITAG_BOOL, 0},
	{"show", "tvShow", GF_ISOM_ITUNE_TV_SHOW, 0, GF_ITAG_STR, 0},
	{"episode_id", "tvEpisodeID", GF_ISOM_ITUNE_TV_EPISODE, 0, GF_ITAG_STR, 0},
	{"season", "tvSeason", GF_ISOM_ITUNE_TV_SEASON, 0, GF_ITAG_INT32, 0},
	{"episode", "tvEPisode", GF_ISOM_ITUNE_TV_EPISODE_NUM, 0, GF_ITAG_INT32, 0},
	{"network", "tvNetwork", GF_ISOM_ITUNE_TV_NETWORK, 0, GF_ITAG_STR, 0},
	{"sdesc", "description", GF_ISOM_ITUNE_DESCRIPTION, 0, GF_ITAG_STR, 0},
	{"ldesc", "longDescription", GF_ISOM_ITUNE_LONG_DESCRIPTION, GF_ID3V2_FRAME_TDES, GF_ITAG_STR, 0},
	{"lyrics", NULL, GF_ISOM_ITUNE_LYRICS, GF_ID3V2_FRAME_USLT, GF_ITAG_STR, 0},
	{"sort_name", "sortName", GF_ISOM_ITUNE_SORT_NAME, GF_ID3V2_FRAME_TSOT, GF_ITAG_STR, 0},
	{"sort_artist", "sortArtist", GF_ISOM_ITUNE_SORT_ARTIST, GF_ID3V2_FRAME_TSOP, GF_ITAG_STR, 0},
	{"sort_album_artist", "sortAlbumArtist", GF_ISOM_ITUNE_SORT_ALB_ARTIST, GF_ID3V2_FRAME_TSO2, GF_ITAG_STR, 0},
	{"sort_album", "sortAlbum", GF_ISOM_ITUNE_SORT_ALBUM, GF_ID3V2_FRAME_TSOA, GF_ITAG_STR, 0},
	{"sort_composer", "sortComposer", GF_ISOM_ITUNE_SORT_COMPOSER, GF_ID3V2_FRAME_TSOC, GF_ITAG_STR, 0},
	{"sort_show", "sortShow", GF_ISOM_ITUNE_SORT_SHOW, 0, GF_ITAG_STR, 0},
	{"cover", "artwork", GF_ISOM_ITUNE_COVER_ART, 0, GF_ITAG_FILE, 0},
	{"copyright", NULL, GF_ISOM_ITUNE_COPYRIGHT, GF_ID3V2_FRAME_TCOP, GF_ITAG_STR, 0},
	{"tool", "encodingTool", GF_ISOM_ITUNE_TOOL, 0, GF_ITAG_STR, 0},
	{"encoder", "encodedBy", GF_ISOM_ITUNE_ENCODER, GF_ID3V2_FRAME_TENC, GF_ITAG_STR, 0},
	{"pdate", "purchaseDate", GF_ISOM_ITUNE_PURCHASE_DATE, 0, GF_ITAG_STR, 0},
	{"podcast", NULL, GF_ISOM_ITUNE_PODCAST, 0, GF_ITAG_BOOL, 0},
	{"url", "podcastURL", GF_ISOM_ITUNE_PODCAST_URL, 0, GF_ITAG_STR, 0},
	{"keywords", NULL, GF_ISOM_ITUNE_KEYWORDS, GF_ID3V2_FRAME_TKWD, GF_ITAG_STR, 0},
	{"category", NULL, GF_ISOM_ITUNE_CATEGORY, GF_ID3V2_FRAME_TCAT, GF_ITAG_STR, 0},
	{"hdvideo", NULL, GF_ISOM_ITUNE_HD_VIDEO, 0, GF_ITAG_INT8, 0},
	{"media", "mediaType", GF_ISOM_ITUNE_MEDIA_TYPE, 0, GF_ITAG_INT8, 0},
	{"rating", "contentRating", GF_ISOM_ITUNE_RATING, 0, GF_ITAG_INT8, 0},
	{"gapless", NULL, GF_ISOM_ITUNE_GAPLESS, 0, GF_ITAG_BOOL, 0},
	{"art_director", NULL, GF_ISOM_ITUNE_ART_DIRECTOR, 0, GF_ITAG_STR, 0},
	{"arranger", NULL, GF_ISOM_ITUNE_ARRANGER, 0, GF_ITAG_STR, 0},
	{"lyricist", NULL, GF_ISOM_ITUNE_LYRICIST, 0, GF_ITAG_STR, 0},
	{"acknowledgement", NULL, GF_ISOM_ITUNE_COPY_ACK, 0, GF_ITAG_STR, 0},
	{"song_description", NULL, GF_ISOM_ITUNE_SONG_DESC, 0, GF_ITAG_STR, 0},
	{"director", NULL, GF_ISOM_ITUNE_DIRECTOR, 0, GF_ITAG_STR, 0},
	{"equalizer", NULL, GF_ISOM_ITUNE_EQ_PRESET, 0, GF_ITAG_STR, 0},
	{"liner", NULL, GF_ISOM_ITUNE_LINER_NOTES, 0, GF_ITAG_STR, 0},
	{"record_company", NULL, GF_ISOM_ITUNE_REC_COMPANY, 0, GF_ITAG_STR, 0},
	{"original_artist", NULL, GF_ISOM_ITUNE_ORIG_ARTIST, 0, GF_ITAG_STR, 0},
	{"phono_rights", NULL, GF_ISOM_ITUNE_PHONO_RIGHTS, 0, GF_ITAG_STR, 0},
	{"producer", NULL, GF_ISOM_ITUNE_PRODUCER, 0, GF_ITAG_STR, 0},
	{"performer", NULL, GF_ISOM_ITUNE_PERFORMER, 0, GF_ITAG_STR, 0},
	{"publisher", NULL, GF_ISOM_ITUNE_PUBLISHER, 0, GF_ITAG_STR, 0},
	{"sound_engineer", NULL, GF_ISOM_ITUNE_SOUND_ENG, 0, GF_ITAG_STR, 0},
	{"soloist", NULL, GF_ISOM_ITUNE_SOLOIST, 0, GF_ITAG_STR, 0},
	{"credits", NULL, GF_ISOM_ITUNE_CREDITS, 0, GF_ITAG_STR, 0},
	{"thanks", NULL, GF_ISOM_ITUNE_THANKS, 0, GF_ITAG_STR, 0},
	{"online_info", NULL, GF_ISOM_ITUNE_ONLINE, 0, GF_ITAG_STR, 0},
	{"exec_producer", NULL, GF_ISOM_ITUNE_EXEC_PRODUCER, 0, GF_ITAG_STR, 0},
	{"genre", NULL, GF_ISOM_ITUNE_GENRE_USER, GF_ID3V2_FRAME_TCON, GF_ITAG_ID3_GENRE, 0},
	{"location", NULL, GF_ISOM_ITUNE_LOCATION, 0, GF_ITAG_STR, 0},
};

GF_EXPORT
s32 gf_itags_find_by_id3tag(u32 id3tag)
{
	u32 i, count = GF_ARRAY_LENGTH(itunes_tags);
	if (id3tag==GF_ID3V2_FRAME_TYER) id3tag = GF_ID3V2_FRAME_TDRC;
	for (i=0; i<count; i++) {
		if (itunes_tags[i].id3tag == id3tag) return i;
	}
	return -1;
}

GF_EXPORT
s32 gf_itags_find_by_itag(u32 itag)
{
	u32 i, count = GF_ARRAY_LENGTH(itunes_tags);
	for (i=0; i<count; i++) {
		if (itunes_tags[i].itag == itag) return i;
	}
	return -1;
}

GF_EXPORT
s32 gf_itags_find_by_name(const char *tag_name)
{
	u32 i, count = GF_ARRAY_LENGTH(itunes_tags);
	for (i=0; i<count; i++) {
		if (!stricmp(tag_name, itunes_tags[i].name) || (itunes_tags[i].alt_name && !stricmp(tag_name, itunes_tags[i].alt_name))) {
			return i;
		} else if (itunes_tags[i].match_substr && !strnicmp(tag_name, itunes_tags[i].name, strlen(itunes_tags[i].name) )) {
			return i;
		}
	}
	return -1;
}

GF_EXPORT
s32 gf_itags_get_type(u32 tag_idx)
{
	u32 count = GF_ARRAY_LENGTH(itunes_tags);
	if (tag_idx>=count) return -1;
	return itunes_tags[tag_idx].type;
}

GF_EXPORT
const char *gf_itags_get_name(u32 tag_idx)
{
	u32 count = GF_ARRAY_LENGTH(itunes_tags);
	if (tag_idx>=count) return NULL;
	return itunes_tags[tag_idx].name;
}

GF_EXPORT
const char *gf_itags_get_alt_name(u32 tag_idx)
{
	u32 count = GF_ARRAY_LENGTH(itunes_tags);
	if (tag_idx>=count) return NULL;
	return itunes_tags[tag_idx].alt_name;
}

GF_EXPORT
u32 gf_itags_get_itag(u32 tag_idx)
{
	u32 count = GF_ARRAY_LENGTH(itunes_tags);
	if (tag_idx>=count) return 0;
	return itunes_tags[tag_idx].itag;
}

GF_EXPORT
u32 gf_itags_get_id3tag(u32 tag_idx)
{
	u32 count = GF_ARRAY_LENGTH(itunes_tags);
	if (tag_idx>=count) return 0;
	return itunes_tags[tag_idx].id3tag;
}

GF_EXPORT
const char *gf_itags_enum_tags(u32 *idx, u32 *itag, u32 *id3tag, u32 *type)
{
	u32 i, count = GF_ARRAY_LENGTH(itunes_tags);
	if (!idx || (count<= *idx)) return NULL;
	i = *idx;
	(*idx) ++;
	if (itag) *itag = itunes_tags[i].itag;
	if (id3tag) *id3tag = itunes_tags[i].id3tag;
	if (type) *type = itunes_tags[i].type;
	return itunes_tags[i].name;
}


static const char* ID3v1Genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco",
	"Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B",
	"Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
	"Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid",
	"House", "Game", "Sound Clip", "Gospel", "Noise",
	"AlternRock", "Bass", "Soul", "Punk", "Space",
	"Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance",
	"Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
	"Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes",
	"Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
	"Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
	"Folk", "Folk/Rock", "National Folk", "Swing",
};

GF_EXPORT
const char *gf_id3_get_genre(u32 tag)
{
	if ((tag>0) && (tag <= (sizeof(ID3v1Genres)/sizeof(const char *)) )) {
		return ID3v1Genres[tag-1];
	}
	return "Unknown";
}
GF_EXPORT
u32 gf_id3_get_genre_tag(const char *name)
{
	u32 i, count = sizeof(ID3v1Genres)/sizeof(const char *);
	if (!name) return 0;
	for (i=0; i<count; i++) {
		if (!stricmp(ID3v1Genres[i], name)) return i+1;
	}
	return 0;
}

struct cicp_prim
{
	u32 code;
	const char *name;
} CICPColorPrimaries[] = {
	{GF_CICP_PRIM_RESERVED_0, "reserved0"},
	{GF_CICP_PRIM_BT709, "BT709"},
	{GF_CICP_PRIM_UNSPECIFIED, "undef"},
	{GF_CICP_PRIM_RESERVED_3, "reserved3"},
	{GF_CICP_PRIM_BT470M, "BT470M"},
	{GF_CICP_PRIM_BT470G, "BT470G"},
	{GF_CICP_PRIM_SMPTE170, "SMPTE170"},
	{GF_CICP_PRIM_SMPTE240, "SMPTE240"},
	{GF_CICP_PRIM_FILM, "FILM"},
	{GF_CICP_PRIM_BT2020, "BT2020"},
	{GF_CICP_PRIM_SMPTE428, "SMPTE428"},
	{GF_CICP_PRIM_SMPTE431, "SMPTE431"},
	{GF_CICP_PRIM_SMPTE432, "SMPTE432"},
	{GF_CICP_PRIM_EBU3213, "EBU3213"},
};

static void cicp_parse_int(const char *val, u32 *ival)
{
	if (sscanf(val, "%u", ival)!=1) {
		*ival = (u32) -1;
	} else {
		char szCoef[100];
		sprintf(szCoef, "%u", *ival);
		if (stricmp(szCoef, val))
			*ival = -1;
	}
}

GF_EXPORT
u32 gf_cicp_parse_color_primaries(const char *val)
{
	u32 i, ival, count = GF_ARRAY_LENGTH(CICPColorPrimaries);
	cicp_parse_int(val, &ival);
	for (i=0; i<count; i++) {
		if (!stricmp(val, CICPColorPrimaries[i].name) || (ival==CICPColorPrimaries[i].code)) {
			return CICPColorPrimaries[i].code;
		}
	}
	if (strcmp(val, "-1")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknow CICP color primaries type %s\n", val));
	}
	return (u32) -1;
}

GF_EXPORT
const char *gf_cicp_color_primaries_name(u32 cicp_mx)
{
	u32 i, count = GF_ARRAY_LENGTH(CICPColorPrimaries);
	for (i=0; i<count; i++) {
		if (CICPColorPrimaries[i].code==cicp_mx) {
			return CICPColorPrimaries[i].name;
		}
	}
	return "unknown";
}

static char szCICPPrimAllNames[1024];

GF_EXPORT
const char *gf_cicp_color_primaries_all_names()
{
	if (szCICPPrimAllNames[0] == 0) {
		u32 i, count = GF_ARRAY_LENGTH(CICPColorPrimaries);
		for (i=0; i<count; i++) {
			if (i) strcat(szCICPPrimAllNames, ",");
			strcat(szCICPPrimAllNames, CICPColorPrimaries[i].name);
		}
	}
	return szCICPPrimAllNames;
}


struct cicp_trans
{
	u32 code;
	const char *name;
} CICPColorTransfer[] = {
	{GF_CICP_TRANSFER_RESERVED_0, "reserved0"},
	{GF_CICP_TRANSFER_BT709, "BT709"},
	{GF_CICP_TRANSFER_UNSPECIFIED, "undef"},
	{GF_CICP_TRANSFER_RESERVED_3, "reserved3"},
	{GF_CICP_TRANSFER_BT470M, "BT470M"},
	{GF_CICP_TRANSFER_BT470BG, "BT470BG"},
	{GF_CICP_TRANSFER_SMPTE170, "SMPTE170"},
	{GF_CICP_TRANSFER_SMPTE240, "SMPTE249"},
	{GF_CICP_TRANSFER_LINEAR, "Linear"},
	{GF_CICP_TRANSFER_LOG100, "Log100"},
	{GF_CICP_TRANSFER_LOG316, "Log316"},
	{GF_CICP_TRANSFER_IEC61966, "IEC61966"},
	{GF_CICP_TRANSFER_BT1361, "BT1361"},
	{GF_CICP_TRANSFER_SRGB, "sRGB"},
	{GF_CICP_TRANSFER_BT2020_10, "BT2020_10"},
	{GF_CICP_TRANSFER_BT2020_12, "BT2020_12"},
	{GF_CICP_TRANSFER_SMPTE2084, "SMPTE2084"},
	{GF_CICP_TRANSFER_SMPTE428, "SMPTE428"},
	{GF_CICP_TRANSFER_STDB67, "STDB67"}
};

GF_EXPORT
u32 gf_cicp_parse_color_transfer(const char *val)
{
	u32 i, ival, count = GF_ARRAY_LENGTH(CICPColorTransfer);
	cicp_parse_int(val, &ival);
	for (i=0; i<count; i++) {
		if (!stricmp(val, CICPColorTransfer[i].name) || (CICPColorTransfer[i].code==ival)) {
			return CICPColorTransfer[i].code;
		}
	}
	if (strcmp(val, "-1")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknow CICP color transfer type %s\n", val));
	}
	return (u32) -1;
}

GF_EXPORT
const char *gf_cicp_color_transfer_name(u32 cicp_mx)
{
	u32 i, count = GF_ARRAY_LENGTH(CICPColorTransfer);
	for (i=0; i<count; i++) {
		if (CICPColorTransfer[i].code==cicp_mx) {
			return CICPColorTransfer[i].name;
		}
	}
	return "unknown";
}

static char szCICPTFCAllNames[1024];

GF_EXPORT
const char *gf_cicp_color_transfer_all_names()
{
	if (szCICPTFCAllNames[0] == 0) {
		u32 i, count = GF_ARRAY_LENGTH(CICPColorTransfer);
		for (i=0; i<count; i++) {
			if (i) strcat(szCICPTFCAllNames, ",");
			strcat(szCICPTFCAllNames, CICPColorTransfer[i].name);
		}
	}
	return szCICPTFCAllNames;
}

struct cicp_mx
{
	u32 code;
	const char *name;
} CICPColorMatrixCoefficients[] = {
	{GF_CICP_MX_IDENTITY, "GBR"},
	{GF_CICP_MX_BT709, "BT709"},
	{GF_CICP_MX_UNSPECIFIED, "undef"},
	{GF_CICP_MX_FCC47, "FCC"},
	{GF_CICP_MX_BT601_625, "BT601"},
	{GF_CICP_MX_SMPTE170, "SMPTE170"},
	{GF_CICP_MX_SMPTE240, "SMPTE240"},
	{GF_CICP_MX_YCgCo, "YCgCo"},
	{GF_CICP_MX_BT2020, "BT2020"},
	{GF_CICP_MX_BT2020_CL, "BT2020cl"},
	{GF_CICP_MX_YDzDx, "YDzDx"},
};

GF_EXPORT
u32 gf_cicp_parse_color_matrix(const char *val)
{
	u32 i, ival, count = GF_ARRAY_LENGTH(CICPColorMatrixCoefficients);
	cicp_parse_int(val, &ival);
	for (i=0; i<count; i++) {
		if (!stricmp(val, CICPColorMatrixCoefficients[i].name) || (ival==CICPColorMatrixCoefficients[i].code)) {
			return CICPColorMatrixCoefficients[i].code;
		}
	}
	if (strcmp(val, "-1")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknow CICP color matrix type %s\n", val));
	}
	return (u32) -1;
}

GF_EXPORT
const char *gf_cicp_color_matrix_name(u32 cicp_mx)
{
	u32 i, count = GF_ARRAY_LENGTH(CICPColorMatrixCoefficients);
	for (i=0; i<count; i++) {
		if (CICPColorMatrixCoefficients[i].code==cicp_mx) {
			return CICPColorMatrixCoefficients[i].name;
		}
	}
	return "unknown";
}

static char szCICPMXAllNames[1024];
GF_EXPORT
const char *gf_cicp_color_matrix_all_names()
{
	if (szCICPMXAllNames[0] == 0) {
		u32 i, count = GF_ARRAY_LENGTH(CICPColorMatrixCoefficients);
		for (i=0; i<count; i++) {
			if (i) strcat(szCICPMXAllNames, ",");
			strcat(szCICPMXAllNames, CICPColorMatrixCoefficients[i].name);
		}
	}
	return szCICPMXAllNames;
}


GF_EXPORT
u64 gf_timestamp_rescale(u64 value, u64 timescale, u64 new_timescale)
{
	if (!timescale || !new_timescale || !value)
		return 0;
	//no timestamp
	if (value==0xFFFFFFFFFFFFFFFFUL)
		return value;

	if (new_timescale == timescale)
		return value;

	if (! (new_timescale % timescale)) {
		u32 div = (u32) (new_timescale / timescale);
		return value * div;
	}
	if (! (timescale % new_timescale)) {
		u32 div = (u32) (timescale / new_timescale);
		return value / div;
	}

	if (value <= GF_INT_MAX) {
		return (value * new_timescale) / timescale;
	}

	u64 int_part = value / timescale;
	u64 frac_part = (value % timescale * new_timescale) / timescale;
	if (int_part >= GF_INT_MAX) {
		Double res = (Double) value;
		res *= new_timescale;
		res /= timescale;
		return (u64) res;
	}
	return int_part * new_timescale + frac_part;
}

GF_EXPORT
s64 gf_timestamp_rescale_signed(s64 value, u64 timescale, u64 new_timescale)
{
	if (!timescale || !new_timescale)
		return 0;

	if (! (new_timescale % timescale)) {
		s32 div = (s32) (new_timescale / timescale);
		return value * div;
	}
	if (! (timescale % new_timescale)) {
		s32 div = (s32) (timescale / new_timescale);
		return value / div;
	}

	if (value <= GF_INT_MAX) {
		return (value * (s32) new_timescale) / (s32) timescale;
	}

	s64 int_part = value / timescale;
	s64 frac_part = ((value % timescale) * new_timescale) / timescale;
	if ((int_part >= GF_INT_MAX) || (int_part <= GF_INT_MIN)) {
		Double res = (Double) value;
		res *= new_timescale;
		res /= timescale;
		return (s64) res;
	}
	return int_part * (s32) new_timescale + frac_part;
}

#define TIMESTAMP_COMPARE(_op) \
	if (timescale1==timescale2) { \
		return (value1 _op value2); \
	} \
	\
	if ((value1 <= GF_INT_MAX) && (value2 <= GF_INT_MAX)) { \
		return (value1 * timescale2 _op value2 * timescale1); \
	} \
	\
	if ((value1==0xFFFFFFFFFFFFFFFFUL) || (value2==0xFFFFFFFFFFFFFFFFUL)) \
		return GF_FALSE; \
	\
	if (!timescale1 || !timescale2) return GF_FALSE; \
	\
	u64 v1_rescale = gf_timestamp_rescale(value1, timescale1, timescale2); \
	return (v1_rescale _op value2); \


GF_EXPORT
Bool gf_timestamp_less(u64 value1, u64 timescale1, u64 value2, u64 timescale2)
{
	TIMESTAMP_COMPARE(<)
}

GF_EXPORT
Bool gf_timestamp_less_or_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2)
{
	TIMESTAMP_COMPARE(<=)
}

GF_EXPORT
Bool gf_timestamp_greater(u64 value1, u64 timescale1, u64 value2, u64 timescale2)
{
	TIMESTAMP_COMPARE(>)
}

GF_EXPORT
Bool gf_timestamp_greater_or_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2)
{
	TIMESTAMP_COMPARE(>=)
}

GF_EXPORT
Bool gf_timestamp_equal(u64 value1, u64 timescale1, u64 value2, u64 timescale2)
{
	TIMESTAMP_COMPARE(==)
}

GF_EXPORT
Bool gf_pixel_fmt_get_uncc(GF_PixelFormat pixfmt, u32 profile_mode, u8 **dsi, u32 *dsi_size)
{
	u32 nb_comps;
	u32 comps_ID[10];
	u32 bits[10], i;
	memset(bits, 0, sizeof(u32)*10);
	u32 sampling=0;
	u32 ileave=1; //pixel interleave by default
	Bool is_10_bps=GF_FALSE;
	u32 block_size=0;
	u32 block_le=0;
	u32 block_pad_lsb=0;
	u32 block_reversed=0;
	u32 profile=0;
	Bool restricted_allowed=GF_FALSE;

	switch (pixfmt) {
	case GF_PIXEL_GREYSCALE:
		nb_comps=1;
		comps_ID[0] = 0;
		break;
	case GF_PIXEL_ALPHAGREY:
		nb_comps=2;
		comps_ID[0] = 7;
		comps_ID[1] = 0;
		break;
	case GF_PIXEL_GREYALPHA:
		nb_comps=2;
		comps_ID[0] = 0;
		comps_ID[1] = 7;
		break;
	case GF_PIXEL_RGB_444:
		nb_comps=3;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		bits[0] = bits[1] = bits[2] = 4;
		break;
	case GF_PIXEL_RGB_555:
		nb_comps=3;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		bits[0] = bits[1] = bits[2] = 5;
		break;
	case GF_PIXEL_RGB_565:
		nb_comps=3;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		bits[0] = bits[2] = 5;
		bits[1] = 6;
		break;
	case GF_PIXEL_RGBX:
		nb_comps=4;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		comps_ID[3] = 12;
		break;
	case GF_PIXEL_BGRX:
		nb_comps=4;
		comps_ID[0] = 6;
		comps_ID[1] = 5;
		comps_ID[2] = 4;
		comps_ID[3] = 12;
		break;
	case GF_PIXEL_XRGB:
		nb_comps=4;
		comps_ID[0] = 12;
		comps_ID[1] = 4;
		comps_ID[2] = 5;
		comps_ID[3] = 6;
		break;
	case GF_PIXEL_XBGR:
		nb_comps=4;
		comps_ID[0] = 12;
		comps_ID[1] = 6;
		comps_ID[2] = 5;
		comps_ID[3] = 4;
		break;
	case GF_PIXEL_ARGB:
		nb_comps=4;
		comps_ID[0] = 7;
		comps_ID[1] = 4;
		comps_ID[2] = 5;
		comps_ID[3] = 6;
		break;
	case GF_PIXEL_RGBA:
		nb_comps=4;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		comps_ID[3] = 7;
		profile = GF_4CC('r','g','b','a');
		restricted_allowed=GF_TRUE;
		break;
	case GF_PIXEL_BGRA:
		nb_comps=4;
		comps_ID[0] = 6;
		comps_ID[1] = 5;
		comps_ID[2] = 4;
		comps_ID[3] = 7;
		break;
	case GF_PIXEL_ABGR:
		nb_comps=4;
		comps_ID[0] = 7;
		comps_ID[1] = 6;
		comps_ID[2] = 5;
		comps_ID[3] = 4;
		profile = GF_4CC('a','b','g','r');
		restricted_allowed=GF_TRUE;
		break;
	case GF_PIXEL_RGBD:
		nb_comps=4;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		comps_ID[3] = 8;
		break;
	case GF_PIXEL_RGBDS:
		nb_comps=5;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		comps_ID[3] = 8;
		comps_ID[4] = 7;
		bits[3] = 7;
		bits[4] = 1;
		break;
	case GF_PIXEL_RGB:
		nb_comps=3;
		comps_ID[0] = 4;
		comps_ID[1] = 5;
		comps_ID[2] = 6;
		profile = GF_4CC('r','g','b','3');
		restricted_allowed=GF_TRUE;
		break;
	case GF_PIXEL_BGR:
		nb_comps=3;
		comps_ID[0] = 6;
		comps_ID[1] = 5;
		comps_ID[2] = 4;
		break;
	case GF_PIXEL_YUV_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_YUV:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		sampling=2;
		ileave=0;
		if (!is_10_bps)
			profile = GF_4CC('i','4','2','0');
		break;
	case GF_PIXEL_YVU:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 3;
		comps_ID[2] = 2;
		sampling=2;
		ileave=0;
		break;
	case GF_PIXEL_YUVA:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		comps_ID[3] = 7;
		sampling=2;
		ileave=0;
		break;
	case GF_PIXEL_YUVA444:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		comps_ID[3] = 7;
		ileave=0;
		break;
	case GF_PIXEL_YUVD:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		comps_ID[3] = 8;
		sampling=2;
		ileave=0;
		break;
	case GF_PIXEL_YUV422_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_YUV422:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		sampling=1;
		ileave=0;
		break;
	case GF_PIXEL_YUV444_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_YUV444:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		ileave=0;
		if (!is_10_bps)
			profile = GF_4CC('v','3','0','8');
		break;
	case GF_PIXEL_NV12_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_NV12:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		ileave=2;
		sampling=2;
		if (!is_10_bps)
			profile = GF_4CC('n','v','1','2');
		break;
	case GF_PIXEL_NV21_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_NV21:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 3;
		comps_ID[2] = 2;
		ileave=2;
		sampling=2;
		if (!is_10_bps)
			profile = GF_4CC('n','v','2','1');
		break;
	case GF_PIXEL_UYVY_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_UYVY:
		nb_comps=4;
		comps_ID[0] = 2;
		comps_ID[1] = 1;
		comps_ID[2] = 3;
		comps_ID[3] = 1;
		ileave=5;
		sampling=1;
		profile = GF_4CC('2','v','u','y');
		break;
	case GF_PIXEL_VYUY_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_VYUY:
		nb_comps=4;
		comps_ID[0] = 3;
		comps_ID[1] = 1;
		comps_ID[2] = 2;
		comps_ID[3] = 1;
		ileave=5;
		sampling=1;
		profile = GF_4CC('v','y','u','y');
		break;
	case GF_PIXEL_YUYV_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_YUYV:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 1;
		comps_ID[3] = 3;
		ileave=5;
		sampling=1;
		profile = GF_4CC('y','u','v','2');
		break;
	case GF_PIXEL_YVYU_10:
		is_10_bps=GF_TRUE;
	case GF_PIXEL_YVYU:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 3;
		comps_ID[2] = 1;
		comps_ID[3] = 2;
		ileave=5;
		sampling=1;
		profile = GF_4CC('y','v','y','u');
		break;

	case GF_PIXEL_YUV444_PACK:
		nb_comps=3;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		break;
	case GF_PIXEL_VYU444_PACK:
		nb_comps=3;
		comps_ID[0] = 3;
		comps_ID[1] = 1;
		comps_ID[2] = 2;
		profile = GF_4CC('v','3','0','8');
		break;
	case GF_PIXEL_YUVA444_PACK:
		nb_comps=4;
		comps_ID[0] = 1;
		comps_ID[1] = 2;
		comps_ID[2] = 3;
		comps_ID[3] = 7;
		break;
	case GF_PIXEL_UYVA444_PACK:
		nb_comps=4;
		comps_ID[0] = 2;
		comps_ID[1] = 1;
		comps_ID[2] = 3;
		comps_ID[3] = 7;
		profile = GF_4CC('v','4','0','8');
		break;
	case GF_PIXEL_YUV444_10_PACK:
		nb_comps=3;
		comps_ID[0] = 2;
		comps_ID[1] = 1;
		comps_ID[2] = 3;
		block_size=4;
		block_le=1;
		block_pad_lsb=1;
		block_reversed=1;
		bits[0] = bits[1] = bits[2] = 10;
		profile = GF_4CC('v','4','1','0');
		break;
	case GF_PIXEL_V210:
		nb_comps=4;
		comps_ID[0] = 2;
		comps_ID[1] = 1;
		comps_ID[2] = 3;
		comps_ID[3] = 1;
		block_size=4;
		block_le=1;
		block_pad_lsb=0;
		block_reversed=1;
		bits[0] = bits[1] = bits[2] = 10;
		sampling=1;
		profile = GF_4CC('v','2','1','0');
		break;
	//no mapping possible in uncv
	case GF_PIXEL_RGB_DEPTH:
	case GF_PIXEL_GL_EXTERNAL:
	case GF_PIXEL_UNCV:
		return GF_FALSE;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Pixel format %s not mapped to uncC, please contact GPAC devs\n", gf_pixel_fmt_name(pixfmt) ));
		return GF_FALSE;
	}
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_FALSE;

	if (!profile_mode) profile = 0;
	if (!profile || (profile_mode<2))
		restricted_allowed = GF_FALSE;

	if (!restricted_allowed) {
		//cmpd
		gf_bs_write_u32(bs, 12+nb_comps*2);
		gf_bs_write_u32(bs, GF_4CC('c','m','p','d'));
		gf_bs_write_u32(bs, nb_comps);
		for (i=0; i<nb_comps; i++)
			gf_bs_write_u16(bs, comps_ID[i]);
	}

	//uncC
	u32 end, pos = (u32) gf_bs_get_position(bs);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, GF_4CC('u','n','c','C'));
	gf_bs_write_u32(bs, restricted_allowed ? 1 : 0); //version and flags
	gf_bs_write_u32(bs, profile); //profile
	if (restricted_allowed) goto done;

	gf_bs_write_u32(bs, nb_comps);
	for (i=0; i<nb_comps; i++) {
		gf_bs_write_u16(bs, i);
		u32 nbbits = bits[i];
		if (!nbbits) nbbits = is_10_bps ? 10 : 8;
		gf_bs_write_u8(bs, nbbits-1);
		gf_bs_write_u8(bs, 0);
		gf_bs_write_u8(bs, is_10_bps ? 2 : 0);
	}
	gf_bs_write_u8(bs, sampling);
	gf_bs_write_u8(bs, ileave);
	gf_bs_write_u8(bs, block_size);
	gf_bs_write_int(bs, is_10_bps ? 1 : 0, 1); //out 10 bits formats are LE
	gf_bs_write_int(bs, block_pad_lsb, 1);
	gf_bs_write_int(bs, block_le, 1);
	gf_bs_write_int(bs, block_reversed, 1);
	gf_bs_write_int(bs, 1, 1); //pad unknown
	gf_bs_write_int(bs, 0, 3);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);
	gf_bs_write_u32(bs, 0);

done:
	end =(u32) gf_bs_get_position(bs);
	gf_bs_seek(bs, pos);
	gf_bs_write_u32(bs, end-pos);
	gf_bs_seek(bs, end);
	gf_bs_get_content(bs, dsi, dsi_size);
	gf_bs_del(bs);
	return GF_TRUE;
}
