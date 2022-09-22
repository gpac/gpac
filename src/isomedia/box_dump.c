/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/utf.h>
#include <gpac/network.h>
#include <gpac/color.h>
#include <gpac/avparse.h>
#include <gpac/base_coding.h>

#ifndef GPAC_DISABLE_ISOM_DUMP


static void dump_data(FILE *trace, char *data, u32 dataLength)
{
	u32 i;
	gf_fprintf(trace, "data:application/octet-string,");
	for (i=0; i<dataLength; i++) {
		gf_fprintf(trace, "%02X", (unsigned char) data[i]);
	}
}

static void dump_data_hex(FILE *trace, char *data, u32 dataLength)
{
	u32 i;
	gf_fprintf(trace, "0x");
	for (i=0; i<dataLength; i++) {
		gf_fprintf(trace, "%02X", (unsigned char) data[i]);
	}
}

static void dump_data_attribute(FILE *trace, char *name, u8 *data, u32 data_size)
{
	u32 i;
	if (!data || !data_size) {
		gf_fprintf(trace, "%s=\"\"", name);
		return;
	}
	gf_fprintf(trace, "%s=\"0x", name);
	for (i=0; i<data_size; i++) gf_fprintf(trace, "%02X", (unsigned char) data[i]);
	gf_fprintf(trace, "\" ");
}

static void dump_data_string(FILE *trace, char *data, u32 dataLength)
{
	u32 i;
	if (!data) return;
	for (i=0; i<dataLength; i++) {
		switch ((unsigned char) data[i]) {
		case '\'':
			gf_fprintf(trace, "&apos;");
			break;
		case '\"':
			gf_fprintf(trace, "&quot;");
			break;
		case '&':
			gf_fprintf(trace, "&amp;");
			break;
		case '>':
			gf_fprintf(trace, "&gt;");
			break;
		case '<':
			gf_fprintf(trace, "&lt;");
			break;
		case 0:
			break;
		default:
			gf_fprintf(trace, "%c", (u8) data[i]);
			break;
		}
	}
}


GF_Err gf_isom_box_array_dump(GF_List *list, FILE * trace)
{
	u32 i;
	GF_Box *a;
	if (!list) return GF_OK;
	i=0;
	while ((a = (GF_Box *)gf_list_enum(list, &i))) {
		gf_isom_box_dump(a, trace);
	}
	return GF_OK;
}

static Bool dump_skip_samples = GF_FALSE;
GF_EXPORT
GF_Err gf_isom_dump(GF_ISOFile *mov, FILE * trace, Bool skip_init, Bool skip_samples)
{
	u32 i;
	const char *fname;
	GF_Box *box;
	if (!mov || !trace) return GF_BAD_PARAM;

	gf_fprintf(trace, "<!--MP4Box dump trace-->\n");

	fname = strrchr(mov->fileName, '/');
	if (!fname) fname = strrchr(mov->fileName, '\\');
	if (!fname) fname = mov->fileName;
	else fname+=1;
	gf_fprintf(trace, "<IsoMediaFile xmlns=\"urn:mpeg:isobmff:schema:file:2016\" Name=\"%s\">\n", fname);

	dump_skip_samples = skip_samples;
	i=0;
	if (skip_init)
		i = mov->nb_box_init_seg;

	while ((box = (GF_Box *)gf_list_enum(mov->TopBoxes, &i))) {
		if (box->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
			gf_fprintf(trace, "<!--WARNING: Unknown Top-level Box Found -->\n");
		} else if (box->type==GF_ISOM_BOX_TYPE_UUID) {
		} else if (!gf_isom_box_is_file_level(box)) {
			gf_fprintf(trace, "<!--ERROR: Invalid Top-level Box Found (\"%s\")-->\n", gf_4cc_to_str(box->type));
		}
		gf_isom_box_dump(box, trace);
	}
	gf_fprintf(trace, "</IsoMediaFile>\n");
	return GF_OK;
}

GF_Err reftype_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrackReferenceTypeBox *p = (GF_TrackReferenceTypeBox *)a;
	if (!p->reference_type) return GF_OK;
	p->type = p->reference_type;

	gf_isom_box_dump_start(a, "TrackReferenceTypeBox", trace);
	gf_fprintf(trace, ">\n");
	for (i=0; i<p->trackIDCount; i++) {
		gf_fprintf(trace, "<TrackReferenceEntry TrackID=\"%d\"/>\n", p->trackIDs[i]);
	}
	if (!p->size)
		gf_fprintf(trace, "<TrackReferenceEntry TrackID=\"\"/>\n");

	gf_isom_box_dump_done("TrackReferenceTypeBox", a, trace);
	p->type = GF_ISOM_BOX_TYPE_REFT;
	return GF_OK;
}

GF_Err ireftype_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_ItemReferenceTypeBox *p = (GF_ItemReferenceTypeBox *)a;
	if (!p->reference_type) return GF_OK;

	p->type = p->reference_type;
	gf_isom_box_dump_start(a, "ItemReferenceBox", trace);
	gf_fprintf(trace, "from_item_id=\"%d\">\n", p->from_item_id);
	for (i = 0; i < p->reference_count; i++) {
		gf_fprintf(trace, "<ItemReferenceBoxEntry ItemID=\"%d\"/>\n", p->to_item_IDs[i]);
	}
	if (!p->size)
		gf_fprintf(trace, "<ItemReferenceBoxEntry ItemID=\"\"/>\n");

	gf_isom_box_dump_done("ItemReferenceBox", a, trace);

	p->type = GF_ISOM_BOX_TYPE_REFI;
	return GF_OK;
}

GF_Err free_box_dump(GF_Box *a, FILE * trace)
{
	GF_FreeSpaceBox *p = (GF_FreeSpaceBox *)a;
	gf_isom_box_dump_start(a, (a->type==GF_ISOM_BOX_TYPE_FREE) ? "FreeSpaceBox" : "SkipBox", trace);
	gf_fprintf(trace, "dataSize=\"%d\">\n", p->dataSize);
	gf_isom_box_dump_done( (a->type==GF_ISOM_BOX_TYPE_FREE) ? "FreeSpaceBox" : "SkipBox", a, trace);
	return GF_OK;
}

GF_Err wide_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "WideBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("WideBox", a, trace);
	return GF_OK;
}

GF_Err mdat_box_dump(GF_Box *a, FILE * trace)
{
	GF_MediaDataBox *p;
	const char *name = (a->type==GF_ISOM_BOX_TYPE_IDAT ? "ItemDataBox" : "MediaDataBox");
	p = (GF_MediaDataBox *)a;
	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, "dataSize=\""LLD"\">\n", p->dataSize);
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err moov_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieBox *p = (GF_MovieBox *) a;
	gf_isom_box_dump_start(a, "MovieBox", trace);
	if (p->internal_flags & GF_ISOM_BOX_COMPRESSED)
		gf_fprintf(trace, "compressedSize=\""LLU"\"", p->size - p->compressed_diff);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MovieBox", a, trace);
	return GF_OK;
}

GF_Err mvhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieHeaderBox *p;

	p = (GF_MovieHeaderBox *) a;

	gf_isom_box_dump_start(a, "MovieHeaderBox", trace);
	gf_fprintf(trace, "CreationTime=\""LLD"\" ", p->creationTime);
	gf_fprintf(trace, "ModificationTime=\""LLD"\" ", p->modificationTime);
	gf_fprintf(trace, "TimeScale=\"%d\" ", p->timeScale);
	gf_fprintf(trace, "Duration=\""LLD"\" ", p->duration);
	gf_fprintf(trace, "NextTrackID=\"%d\">\n", p->nextTrackID);

	gf_isom_box_dump_done("MovieHeaderBox", a, trace);
	return GF_OK;
}

GF_Err mdhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_MediaHeaderBox *p;

	p = (GF_MediaHeaderBox *)a;
	gf_isom_box_dump_start(a, "MediaHeaderBox", trace);
	gf_fprintf(trace, "CreationTime=\""LLD"\" ", p->creationTime);
	gf_fprintf(trace, "ModificationTime=\""LLD"\" ", p->modificationTime);
	gf_fprintf(trace, "TimeScale=\"%d\" ", p->timeScale);
	gf_fprintf(trace, "Duration=\""LLD"\" ", p->duration);
	gf_fprintf(trace, "LanguageCode=\"%c%c%c\">\n", p->packedLanguage[0], p->packedLanguage[1], p->packedLanguage[2]);
	gf_isom_box_dump_done("MediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err vmhd_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "VideoMediaHeaderBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("VideoMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err gmin_box_dump(GF_Box *a, FILE * trace)
{
	GF_GenericMediaHeaderInfoBox *p = (GF_GenericMediaHeaderInfoBox *)a;
	gf_isom_box_dump_start(a, "GenericMediaHeaderInformationBox", trace);
	gf_fprintf(trace, " graphicsMode=\"%d\" opcolorRed=\"%d\" opcolorGreen=\"%d\" opcolorBlue=\"%d\" balance=\"%d\">\n",
		p->graphics_mode, p->op_color_red, p->op_color_green, p->op_color_blue, p->balance);
	gf_isom_box_dump_done("GenericMediaHeaderInformationBox", a, trace);
	return GF_OK;
}

GF_Err clef_box_dump(GF_Box *a, FILE * trace)
{
	Float w, h;
	const char *name = "TrackCleanApertureDimensionsBox";
	GF_ApertureBox *p = (GF_ApertureBox *)a;
	if (p->type==GF_QT_BOX_TYPE_PROF)
		name = "TrackProductionApertureDimensionsBox";
	else if (p->type==GF_QT_BOX_TYPE_ENOF)
		name = "TrackEncodedPixelsDimensionsBox";

	gf_isom_box_dump_start(a, name, trace);
	w = (Float) (p->width&0xFFFF);
	w /= 0xFFFF;
	w += (p->width>>16);

	h = (Float) (p->height&0xFFFF);
	h /= 0xFFFF;
	h += (p->height>>16);

	gf_fprintf(trace, " width=\"%g\" height=\"%g\">\n", w, h);
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err smhd_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "SoundMediaHeaderBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("SoundMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err hmhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_HintMediaHeaderBox *p;

	p = (GF_HintMediaHeaderBox *)a;

	gf_isom_box_dump_start(a, "HintMediaHeaderBox", trace);
	gf_fprintf(trace, "MaximumPDUSize=\"%d\" ", p->maxPDUSize);
	gf_fprintf(trace, "AveragePDUSize=\"%d\" ", p->avgPDUSize);
	gf_fprintf(trace, "MaxBitRate=\"%d\" ", p->maxBitrate);
	gf_fprintf(trace, "AverageBitRate=\"%d\">\n", p->avgBitrate);

	gf_isom_box_dump_done("HintMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err nmhd_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "MPEGMediaHeaderBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MPEGMediaHeaderBox", a, trace);
	return GF_OK;
}

GF_Err stbl_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "SampleTableBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("SampleTableBox", a, trace);
	return GF_OK;
}

GF_Err dinf_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "DataInformationBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("DataInformationBox", a, trace);
	return GF_OK;
}

GF_Err url_box_dump(GF_Box *a, FILE * trace)
{
	GF_DataEntryURLBox *p;

	p = (GF_DataEntryURLBox *)a;
	gf_isom_box_dump_start(a, "URLDataEntryBox", trace);
	if (p->location) {
		gf_fprintf(trace, " URL=\"%s\">\n", p->location);
	} else {
		gf_fprintf(trace, ">\n");
		if (p->size) {
			if (! (p->flags & 1) ) {
				gf_fprintf(trace, "<!--ERROR: No location indicated-->\n");
			} else {
				gf_fprintf(trace, "<!--Data is contained in the movie file-->\n");
			}
		}
	}
	gf_isom_box_dump_done("URLDataEntryBox", a, trace);
	return GF_OK;
}

GF_Err urn_box_dump(GF_Box *a, FILE * trace)
{
	GF_DataEntryURNBox *p;

	p = (GF_DataEntryURNBox *)a;
	gf_isom_box_dump_start(a, "URNDataEntryBox", trace);
	if (p->nameURN) gf_fprintf(trace, " URN=\"%s\"", p->nameURN);
	if (p->location) gf_fprintf(trace, " URL=\"%s\"", p->location);
	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done("URNDataEntryBox", a, trace);
	return GF_OK;
}

GF_Err alis_box_dump(GF_Box *a, FILE * trace)
{
//	GF_DataEntryAliasBox *p = (GF_DataEntryAliasBox *)a;
	gf_isom_box_dump_start(a, "AliasDataEntryBox", trace);
	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done("AliasDataEntryBox", a, trace);
	return GF_OK;
}

GF_Err cprt_box_dump(GF_Box *a, FILE * trace)
{
	GF_CopyrightBox *p;

	p = (GF_CopyrightBox *)a;
	gf_isom_box_dump_start(a, "CopyrightBox", trace);
	gf_fprintf(trace, "LanguageCode=\"%s\" CopyrightNotice=\"%s\">\n", p->packedLanguageCode, p->notice);
	gf_isom_box_dump_done("CopyrightBox", a, trace);
	return GF_OK;
}

GF_Err kind_box_dump(GF_Box *a, FILE * trace)
{
	GF_KindBox *p;

	p = (GF_KindBox *)a;
	gf_isom_box_dump_start(a, "KindBox", trace);
	gf_fprintf(trace, "schemeURI=\"%s\" value=\"%s\">\n", p->schemeURI, (p->value ? p->value : ""));
	gf_isom_box_dump_done("KindBox", a, trace);
	return GF_OK;
}


static char *format_duration(u64 dur, u32 timescale, char *szDur)
{
	u32 h, m, s, ms;
	if (!timescale) return NULL;
	dur = (u32) (( ((Double) (s64) dur)/timescale)*1000);
	h = (u32) (dur / 3600000);
	dur -= h*3600000;
	m = (u32) (dur / 60000);
	dur -= m*60000;
	s = (u32) (dur/1000);
	dur -= s*1000;
	ms = (u32) (dur);

	if (h<=24) {
		sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	} else {
		u32 d = (u32) (dur / 3600000 / 24);
		h = (u32) (dur/3600000)-24*d;
		if (d<=365) {
			sprintf(szDur, "%d Days, %02d:%02d:%02d.%03d", d, h, m, s, ms);
		} else {
			u32 y=0;
			while (d>365) {
				y++;
				d-=365;
				if (y%4) d--;
			}
			sprintf(szDur, "%d Years %d Days, %02d:%02d:%02d.%03d", y, d, h, m, s, ms);
		}
	}
	return szDur;
}

static void dump_escape_string(FILE * trace, char *name)
{
	u32 i, len = name ? (u32) strlen(name) : 0;
	for (i=0; i<len; i++) {
		if (name[i]=='"') gf_fprintf(trace, "&quot;");
		else gf_fputc(name[i], trace);
	}
}

GF_Err chpl_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	GF_ChapterListBox *p = (GF_ChapterListBox *)a;
	gf_isom_box_dump_start(a, "ChapterListBox", trace);
	gf_fprintf(trace, ">\n");

	if (p->size) {
		count = gf_list_count(p->list);
		for (i=0; i<count; i++) {
			char szDur[20];
			GF_ChapterEntry *ce = (GF_ChapterEntry *)gf_list_get(p->list, i);
			gf_fprintf(trace, "<Chapter name=\"");
			dump_escape_string(trace, ce->name);
			gf_fprintf(trace, "\" startTime=\"%s\" />\n", format_duration(ce->start_time, 1000*10000, szDur));
		}
	} else {
		gf_fprintf(trace, "<Chapter name=\"\" startTime=\"\"/>\n");
	}
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		format_duration(0, 0, NULL);
		dump_escape_string(NULL, NULL);
	}
#endif
	gf_isom_box_dump_done("ChapterListBox", a, trace);
	return GF_OK;
}

GF_Err pdin_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_ProgressiveDownloadBox *p = (GF_ProgressiveDownloadBox *)a;
	gf_isom_box_dump_start(a, "ProgressiveDownloadBox", trace);
	gf_fprintf(trace, ">\n");

	if (p->size) {
		for (i=0; i<p->count; i++) {
			gf_fprintf(trace, "<DownloadInfo rate=\"%d\" estimatedTime=\"%d\" />\n", p->rates[i], p->times[i]);
		}
	} else {
		gf_fprintf(trace, "<DownloadInfo rate=\"\" estimatedTime=\"\" />\n");
	}
	gf_isom_box_dump_done("ProgressiveDownloadBox", a, trace);
	return GF_OK;
}

GF_Err hdlr_box_dump(GF_Box *a, FILE * trace)
{
	GF_HandlerBox *p = (GF_HandlerBox *)a;
	gf_isom_box_dump_start(a, "HandlerBox", trace);
	if (p->nameUTF8 && (u32) p->nameUTF8[0] == strlen(p->nameUTF8)-1) {
		gf_fprintf(trace, "hdlrType=\"%s\" Name=\"%s\" ", gf_4cc_to_str(p->handlerType), p->nameUTF8+1);
	} else {
		gf_fprintf(trace, "hdlrType=\"%s\" Name=\"%s\" ", gf_4cc_to_str(p->handlerType), p->nameUTF8);
	}
	gf_fprintf(trace, "reserved1=\"%d\" reserved2=\"", p->reserved1);
	dump_data(trace, (char *) p->reserved2, 12);
	gf_fprintf(trace, "\"");

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("HandlerBox", a, trace);
	return GF_OK;
}

GF_Err iods_box_dump(GF_Box *a, FILE * trace)
{
	GF_ObjectDescriptorBox *p;

	p = (GF_ObjectDescriptorBox *)a;
	gf_isom_box_dump_start(a, "ObjectDescriptorBox", trace);
	gf_fprintf(trace, ">\n");

	if (p->descriptor) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc(p->descriptor, trace, 1, GF_TRUE);
#else
		gf_fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	} else if (p->size) {
		gf_fprintf(trace, "<!--WARNING: Object Descriptor not present-->\n");
	}
	gf_isom_box_dump_done("ObjectDescriptorBox", a, trace);
	return GF_OK;
}

GF_Err trak_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackBox *p;

	p = (GF_TrackBox *)a;
	gf_isom_box_dump_start(a, "TrackBox", trace);
	gf_fprintf(trace, ">\n");
	if (p->size && !p->Header) {
		gf_fprintf(trace, "<!--INVALID FILE: Missing Track Header-->\n");
	}
	gf_isom_box_dump_done("TrackBox", a, trace);
	return GF_OK;
}

GF_Err mp4s_box_dump(GF_Box *a, FILE * trace)
{
	GF_MPEGSampleEntryBox *p;

	p = (GF_MPEGSampleEntryBox *)a;
	gf_isom_box_dump_start(a, "MPEGSystemsSampleDescriptionBox", trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	if (!p->esd && p->size) {
		gf_fprintf(trace, "<!--INVALID MP4 FILE: ESDBox not present in MPEG Sample Description -->\n");
	}
	gf_isom_box_dump_done("MPEGSystemsSampleDescriptionBox", a, trace);
	return GF_OK;
}


GF_Err video_sample_entry_box_dump(GF_Box *a, FILE * trace)
{
	Bool full_dump=GF_FALSE;
	GF_MPEGVisualSampleEntryBox *p = (GF_MPEGVisualSampleEntryBox *)a;
	const char *name;

	switch (p->type) {
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		name = "AVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_MVC_H264:
		name = "MVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_SVC_H264:
		name = "SVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC2:
	case GF_ISOM_SUBTYPE_HEV2:
		name = "HEVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_VVC1:
	case GF_ISOM_SUBTYPE_VVI1:
		name = "VVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_LHV1:
	case GF_ISOM_SUBTYPE_LHE1:
		name = "LHEVCSampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_AV01:
		name = "AV1SampleEntryBox";
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		name = "H263SampleDescriptionBox";
		break;
	case GF_ISOM_SUBTYPE_MJP2:
		name = "MJ2KSampleDescriptionBox";
		break;
	case GF_QT_SUBTYPE_APCH:
	case GF_QT_SUBTYPE_APCO:
	case GF_QT_SUBTYPE_APCN:
	case GF_QT_SUBTYPE_APCS:
	case GF_QT_SUBTYPE_AP4X:
	case GF_QT_SUBTYPE_AP4H:
		name = "ProResSampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_RAW:
		name = "RGBSampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUYV:
		name = "YUV422SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_UYVY:
		name = "YUV422SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUV420:
		name = "YUV420SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUV422_10:
		name = "YUV422_10_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUV444:
		name = "YUV444SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUV444_10:
		name = "YUV444_10_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_RAW_VID:
		name = "RGB_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUVA444:
		name = "YUVA_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YUV422_16:
		name = "YUV420_16_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_I420:
		name = "I420_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_IYUV:
		name = "IUYV_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YV12:
		name = "YV12_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_YVYU:
		name = "YVYU_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_RGBA:
		name = "RGBA_SampleEntryBox";
		full_dump=GF_TRUE;
		break;
	case GF_QT_SUBTYPE_ABGR:
		name = "ABGR_SampleEntryBox";
		full_dump=GF_TRUE;
		break;

	default:
		//DO NOT TOUCH FOR NOW, this breaks all hashes
		name = "MPEGVisualSampleDescriptionBox";
		break;
	}


	gf_isom_box_dump_start(a, name, trace);

	gf_fprintf(trace, " DataReferenceIndex=\"%d\" Width=\"%d\" Height=\"%d\"", p->dataReferenceIndex, p->Width, p->Height);

	if (full_dump) {
		Float dpih, dpiv;
		gf_fprintf(trace, " Version=\"%d\" Revision=\"%d\" Vendor=\"%s\" TemporalQuality=\"%d\" SpatialQuality=\"%d\" FramesPerSample=\"%d\" ColorTableIndex=\"%d\"",
			p->version, p->revision, gf_4cc_to_str(p->vendor), p->temporal_quality, p->spatial_quality, p->frames_per_sample, p->color_table_index);

		dpih = (Float) (p->horiz_res&0xFFFF);
		dpih /= 0xFFFF;
		dpih += (p->vert_res>>16);
		dpiv = (Float) (p->vert_res&0xFFFF);
		dpiv /= 0xFFFF;
		dpiv += (p->vert_res>>16);

		gf_fprintf(trace, " XDPI=\"%g\" YDPI=\"%g\" BitDepth=\"%d\"", dpih, dpiv, p->bit_depth);
	} else {
		//dump reserved info
		gf_fprintf(trace, " XDPI=\"%d\" YDPI=\"%d\" BitDepth=\"%d\"", p->horiz_res, p->vert_res, p->bit_depth);
	}
	if (strlen((const char*)p->compressor_name) ) {
		if (isalnum(p->compressor_name[0])) {
			gf_fprintf(trace, " CompressorName=\"%s\"\n", p->compressor_name);
		} else {
			gf_fprintf(trace, " CompressorName=\"%s\"\n", p->compressor_name+1);
		}
	}

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}


void base_audio_entry_dump(GF_AudioSampleEntryBox *p, FILE * trace)
{
	gf_fprintf(trace, " DataReferenceIndex=\"%d\"", p->dataReferenceIndex);
	if (p->version)
		gf_fprintf(trace, " Version=\"%d\"", p->version);

	if (p->samplerate_lo) {
		if (p->type==GF_ISOM_SUBTYPE_MLPA) {
			u32 sr = p->samplerate_hi;
			sr <<= 16;
			sr |= p->samplerate_lo;
			gf_fprintf(trace, " SampleRate=\"%d\"", sr);
		} else {
			gf_fprintf(trace, " SampleRate=\"%d.%d\"", p->samplerate_hi, p->samplerate_lo);
		}
	} else {
		gf_fprintf(trace, " SampleRate=\"%d\"", p->samplerate_hi);
	}
	gf_fprintf(trace, " Channels=\"%d\" BitsPerSample=\"%d\"", p->channel_count, p->bitspersample);
	if (p->qtff_mode) {
		gf_fprintf(trace, " isQTFF=\"%d\"", p->qtff_mode);
		gf_fprintf(trace, " qtRevisionLevel=\"%d\"", p->revision);
		gf_fprintf(trace, " qtVendor=\"%d\"", p->vendor);
		gf_fprintf(trace, " qtCompressionId=\"%d\"", p->compression_id);
		gf_fprintf(trace, " qtPacketSize=\"%d\"", p->packet_size);
		if (p->version == 1) {
			gf_fprintf(trace, " qtSamplesPerPacket=\"%d\"", p->extensions[0]<<24 | p->extensions[1]<<16 | p->extensions[2]<<8 | p->extensions[3]);
			gf_fprintf(trace, " qtBytesPerPacket=\"%d\"", p->extensions[4]<<24 | p->extensions[5]<<16 | p->extensions[6]<<8 | p->extensions[7]);
			gf_fprintf(trace, " qtBytesPerFrame=\"%d\"", p->extensions[8]<<24 | p->extensions[9]<<16 | p->extensions[10]<<8 | p->extensions[11]);
			gf_fprintf(trace, " qtBytesPerSample=\"%d\"", p->extensions[12]<<24 | p->extensions[13]<<16 | p->extensions[14]<<8 | p->extensions[15]);
		}
	}
}

GF_Err audio_sample_entry_box_dump(GF_Box *a, FILE * trace)
{
	char *szName;
	const char *error=NULL;
	GF_MPEGAudioSampleEntryBox *p = (GF_MPEGAudioSampleEntryBox *)a;

	switch (p->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
		szName = "AMRSampleDescriptionBox";
		if (!p->cfg_3gpp)
		 	error = "<!-- INVALID 3GPP FILE: Config not present in Sample Description-->";
		break;
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		szName = "AMR_WB_SampleDescriptionBox";
		if (!p->cfg_3gpp)
		 	error = "<!-- INVALID 3GPP FILE: Config not present in Sample Description-->";
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
		szName = "EVRCSampleDescriptionBox";
		if (!p->cfg_3gpp)
		 	error = "<!-- INVALID 3GPP FILE: Config not present in Sample Description-->";
		break;
	case GF_ISOM_SUBTYPE_3GP_QCELP:
		szName = "QCELPSampleDescriptionBox";
		if (!p->cfg_3gpp)
		 	error = "<!-- INVALID 3GPP Entry: Config not present in Audio Sample Description-->";
		break;
	case GF_ISOM_SUBTYPE_3GP_SMV:
		szName = "SMVSampleDescriptionBox";
		if (!p->cfg_3gpp)
		 	error = "<!-- INVALID 3GPP Entry: Config not present in Audio Sample Description-->";
		break;
	case GF_ISOM_BOX_TYPE_MP4A:
		szName = "MPEGAudioSampleDescriptionBox";
		if (!p->esd)
		 	error = "<!--INVALID MP4 Entry: ESDBox not present in Audio Sample Description -->";
		break;
	case GF_ISOM_BOX_TYPE_AC3:
		szName = "AC3SampleEntryBox";
		if (!p->cfg_ac3)
		 	error = "<!--INVALID AC3 Entry: AC3Config not present in Audio Sample Description -->";
		break;
	case GF_ISOM_BOX_TYPE_EC3:
		szName = "EC3SampleEntryBox";
		if (!p->cfg_ac3)
		 	error = "<!--INVALID EC3 Entry: AC3Config not present in Audio Sample Description -->";
		break;
	case GF_ISOM_BOX_TYPE_MHA1:
	case GF_ISOM_BOX_TYPE_MHA2:
		if (!p->cfg_mha)
		 	error = "<!--INVALID MPEG-H 3D Audio Entry: MHA config not present in Audio Sample Description -->";
	case GF_ISOM_BOX_TYPE_MHM1:
	case GF_ISOM_BOX_TYPE_MHM2:
		szName = "MHASampleEntry";
		break;
	case GF_ISOM_BOX_TYPE_MLPA:
		if (!p->cfg_mlp)
		 	error = "<!--INVALID TrueHD Audio Entry: DMLP config not present in Audio Sample Description -->";
		szName = "TrueHDSampleEntry";
		break;
	default:
		szName = "AudioSampleDescriptionBox";
		break;
	}

	gf_isom_box_dump_start(a, szName, trace);
	base_audio_entry_dump((GF_AudioSampleEntryBox *)p, trace);
	gf_fprintf(trace, ">\n");

	if (error) {
		gf_fprintf(trace, "%s\n", error);
	}
	gf_isom_box_dump_done(szName, a, trace);
	return GF_OK;
}

GF_Err gen_sample_entry_box_dump(GF_Box *a, FILE * trace)
{
	char *szName;
	GF_SampleEntryBox *p = (GF_SampleEntryBox *)a;

	switch (p->type) {
	case GF_QT_SUBTYPE_C608:
		szName = "ClosedCaption";
		break;
	default:
		szName = "GenericSampleDescriptionBox";
		break;
	}

	gf_isom_box_dump_start(a, szName, trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(szName, a, trace);
	return GF_OK;
}

static void gnr_dump_exts(u8 *data, u32 data_size, FILE *trace)
{
	GF_List *list = NULL;
	GF_Err e = GF_OK;
	if (!data) {
		gf_fprintf(trace, ">\n");
		return;
	}
	
	GF_BitStream *bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	gf_bs_set_cookie(bs, GF_ISOM_BS_COOKIE_NO_LOGS);
	while (gf_bs_available(bs)) {
		GF_Box *abox=NULL;
		e = gf_isom_box_parse(&abox, bs);
		if (!abox) break;
		if (!list) list = gf_list_new();
		gf_list_add(list, abox);
	}
	gf_bs_del(bs);

	if (!e && gf_list_count(list)) {
		gf_fprintf(trace, ">\n");
		while (gf_list_count(list)) {
			GF_Box *a = gf_list_pop_front(list);
			gf_isom_box_dump(a, trace);
			gf_isom_box_del(a);
		}
	} else {
		dump_data_attribute(trace, "data", data, data_size);
		gf_fprintf(trace, ">\n");
	}
	if (list)
		gf_isom_box_array_del(list);
}

GF_Err gnrm_box_dump(GF_Box *a, FILE * trace)
{
	GF_GenericSampleEntryBox *p = (GF_GenericSampleEntryBox *)a;
	if (p->EntryType)
		a->type = p->EntryType;

	gf_isom_box_dump_start(a, "SampleDescriptionEntryBox", trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\" ExtensionDataSize=\"%d\"", p->dataReferenceIndex, p->data_size);
	a->type = GF_ISOM_BOX_TYPE_GNRM;
	gnr_dump_exts(p->data, p->data_size, trace);

	gf_isom_box_dump_done("SampleDescriptionEntryBox", a, trace);
	return GF_OK;
}

GF_Err gnrv_box_dump(GF_Box *a, FILE * trace)
{
	GF_GenericVisualSampleEntryBox *p = (GF_GenericVisualSampleEntryBox *)a;
	if (p->EntryType)
		a->type = p->EntryType;

	gf_isom_box_dump_start(a, "VisualSampleDescriptionBox", trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\" Version=\"%d\" Revision=\"%d\" Vendor=\"%d\" TemporalQuality=\"%d\" SpacialQuality=\"%d\" Width=\"%d\" Height=\"%d\" HorizontalResolution=\"%d\" VerticalResolution=\"%d\" CompressorName=\"%s\" BitDepth=\"%d\"",
	        p->dataReferenceIndex, p->version, p->revision, p->vendor, p->temporal_quality, p->spatial_quality, p->Width, p->Height, p->horiz_res, p->vert_res, isalnum(p->compressor_name[0]) ? p->compressor_name : p->compressor_name+1, p->bit_depth);

	a->type = GF_ISOM_BOX_TYPE_GNRV;
	gnr_dump_exts(p->data, p->data_size, trace);

	gf_isom_box_dump_done("VisualSampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err gnra_box_dump(GF_Box *a, FILE * trace)
{
	GF_GenericAudioSampleEntryBox *p = (GF_GenericAudioSampleEntryBox *)a;
	if (p->EntryType)
		a->type = p->EntryType;

	gf_isom_box_dump_start(a, "AudioSampleDescriptionBox", trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\" Version=\"%d\" Revision=\"%d\" Vendor=\"%d\" ChannelCount=\"%d\" BitsPerSample=\"%d\" Samplerate=\"%d\"",
	        p->dataReferenceIndex, p->version, p->revision, p->vendor, p->channel_count, p->bitspersample, p->samplerate_hi);

	a->type = GF_ISOM_BOX_TYPE_GNRA;
	gnr_dump_exts(p->data, p->data_size, trace);

	gf_isom_box_dump_done("AudioSampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err edts_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "EditBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("EditBox", a, trace);
	return GF_OK;
}

GF_Err udta_box_dump(GF_Box *a, FILE * trace)
{
	GF_UserDataBox *p;
	GF_UserDataMap *map;
	u32 i;

	p = (GF_UserDataBox *)a;
	gf_isom_box_dump_start(a, "UserDataBox", trace);
	gf_fprintf(trace, ">\n");

	i=0;
	while ((map = (GF_UserDataMap *)gf_list_enum(p->recordList, &i))) {
		gf_isom_box_array_dump(map->boxes, trace);
	}
	gf_isom_box_dump_done("UserDataBox", a, trace);
	return GF_OK;
}

GF_Err dref_box_dump(GF_Box *a, FILE * trace)
{
//	GF_DataReferenceBox *p = (GF_DataReferenceBox *)a;
	gf_isom_box_dump_start(a, "DataReferenceBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("DataReferenceBox", a, trace);
	return GF_OK;
}

GF_Err stsd_box_dump(GF_Box *a, FILE * trace)
{
	GF_SampleDescriptionBox *p = (GF_SampleDescriptionBox *)a;
	gf_isom_box_dump_start(a, "SampleDescriptionBox", trace);
	if (p->version)
		gf_fprintf(trace, " version=\"%d\"", p->version);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("SampleDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err stts_box_dump(GF_Box *a, FILE * trace)
{
	GF_TimeToSampleBox *p;
	u32 i, nb_samples;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_TimeToSampleBox *)a;
	gf_isom_box_dump_start(a, "TimeToSampleBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	nb_samples = 0;
	for (i=0; i<p->nb_entries; i++) {
		gf_fprintf(trace, "<TimeToSampleEntry SampleDelta=\"%d\" SampleCount=\"%d\"/>\n", p->entries[i].sampleDelta, p->entries[i].sampleCount);
		nb_samples += p->entries[i].sampleCount;
	}
	if (p->size)
		gf_fprintf(trace, "<!-- counted %d samples in STTS entries -->\n", nb_samples);
	else
		gf_fprintf(trace, "<TimeToSampleEntry SampleDelta=\"\" SampleCount=\"\"/>\n");

	gf_isom_box_dump_done("TimeToSampleBox", a, trace);
	return GF_OK;
}

GF_Err ctts_box_dump(GF_Box *a, FILE * trace)
{
	GF_CompositionOffsetBox *p;
	u32 i, nb_samples;
	p = (GF_CompositionOffsetBox *)a;

	if (dump_skip_samples)
		return GF_OK;

	gf_isom_box_dump_start(a, "CompositionOffsetBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	nb_samples = 0;
	for (i=0; i<p->nb_entries; i++) {
		gf_fprintf(trace, "<CompositionOffsetEntry CompositionOffset=\"%d\" SampleCount=\"%d\"/>\n", p->entries[i].decodingOffset, p->entries[i].sampleCount);
		nb_samples += p->entries[i].sampleCount;
	}
	if (p->size)
		gf_fprintf(trace, "<!-- counted %d samples in CTTS entries -->\n", nb_samples);
	else
		gf_fprintf(trace, "<CompositionOffsetEntry CompositionOffset=\"\" SampleCount=\"\"/>\n");

	gf_isom_box_dump_done("CompositionOffsetBox", a, trace);
	return GF_OK;
}

GF_Err cslg_box_dump(GF_Box *a, FILE * trace)
{
	GF_CompositionToDecodeBox *p;

	p = (GF_CompositionToDecodeBox *)a;
	gf_isom_box_dump_start(a, "CompositionToDecodeBox", trace);
	gf_fprintf(trace, "compositionToDTSShift=\"%d\" leastDecodeToDisplayDelta=\"%d\" greatestDecodeToDisplayDelta=\"%d\" compositionStartTime=\"%d\" compositionEndTime=\"%d\">\n", p->compositionToDTSShift, p->leastDecodeToDisplayDelta, p->greatestDecodeToDisplayDelta, p->compositionStartTime, p->compositionEndTime);
	gf_isom_box_dump_done("CompositionToDecodeBox", a, trace);
	return GF_OK;
}

GF_Err ccst_box_dump(GF_Box *a, FILE * trace)
{
	GF_CodingConstraintsBox *p = (GF_CodingConstraintsBox *)a;
	gf_isom_box_dump_start(a, "CodingConstraintsBox", trace);
	gf_fprintf(trace, "all_ref_pics_intra=\"%d\" intra_pred_used=\"%d\" max_ref_per_pic=\"%d\" reserved=\"%d\">\n", p->all_ref_pics_intra, p->intra_pred_used, p->max_ref_per_pic, p->reserved);
	gf_isom_box_dump_done("CodingConstraintsBox", a, trace);
	return GF_OK;
}

GF_Err stsh_box_dump(GF_Box *a, FILE * trace)
{
	GF_ShadowSyncBox *p;
	u32 i;
	GF_StshEntry *t;

	p = (GF_ShadowSyncBox *)a;
	gf_isom_box_dump_start(a, "SyncShadowBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", gf_list_count(p->entries));
	i=0;
	while ((t = (GF_StshEntry *)gf_list_enum(p->entries, &i))) {
		gf_fprintf(trace, "<SyncShadowEntry ShadowedSample=\"%d\" SyncSample=\"%d\"/>\n", t->shadowedSampleNumber, t->syncSampleNumber);
	}
	if (!p->size) {
		gf_fprintf(trace, "<SyncShadowEntry ShadowedSample=\"\" SyncSample=\"\"/>\n");
	}
	gf_isom_box_dump_done("SyncShadowBox", a, trace);
	return GF_OK;
}

GF_Err elst_box_dump(GF_Box *a, FILE * trace)
{
	GF_EditListBox *p;
	u32 i;
	GF_EdtsEntry *t;

	p = (GF_EditListBox *)a;
	gf_isom_box_dump_start(a, "EditListBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", gf_list_count(p->entryList));

	i=0;
	while ((t = (GF_EdtsEntry *)gf_list_enum(p->entryList, &i))) {
		u32 rate_int = t->mediaRate>>16;
		u32 rate_frac = t->mediaRate&0xFFFF;
		if (rate_frac)
			gf_fprintf(trace, "<EditListEntry Duration=\""LLD"\" MediaTime=\""LLD"\" MediaRate=\"%u.%u\"/>\n", t->segmentDuration, t->mediaTime, rate_int, rate_frac*100/0xFFFF);
		else
			gf_fprintf(trace, "<EditListEntry Duration=\""LLD"\" MediaTime=\""LLD"\" MediaRate=\"%u\"/>\n", t->segmentDuration, t->mediaTime, rate_int);
	}
	if (!p->size) {
		gf_fprintf(trace, "<EditListEntry Duration=\"\" MediaTime=\"\" MediaRate=\"\"/>\n");
	}
	gf_isom_box_dump_done("EditListBox", a, trace);
	return GF_OK;
}

GF_Err stsc_box_dump(GF_Box *a, FILE * trace)
{
	GF_SampleToChunkBox *p;
	u32 i, nb_samples;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_SampleToChunkBox *)a;
	gf_isom_box_dump_start(a, "SampleToChunkBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	nb_samples = 0;
	for (i=0; i<p->nb_entries; i++) {
		gf_fprintf(trace, "<SampleToChunkEntry FirstChunk=\"%d\" SamplesPerChunk=\"%d\" SampleDescriptionIndex=\"%d\"/>\n", p->entries[i].firstChunk, p->entries[i].samplesPerChunk, p->entries[i].sampleDescriptionIndex);
		if (i+1<p->nb_entries) {
			nb_samples += (p->entries[i+1].firstChunk - p->entries[i].firstChunk) * p->entries[i].samplesPerChunk;
		} else {
			nb_samples += p->entries[i].samplesPerChunk;
		}
	}
	if (p->size)
		gf_fprintf(trace, "<!-- counted %d samples in STSC entries (could be less than sample count) -->\n", nb_samples);
	else
		gf_fprintf(trace, "<SampleToChunkEntry FirstChunk=\"\" SamplesPerChunk=\"\" SampleDescriptionIndex=\"\"/>\n");

	gf_isom_box_dump_done("SampleToChunkBox", a, trace);
	return GF_OK;
}

GF_Err stsz_box_dump(GF_Box *a, FILE * trace)
{
	GF_SampleSizeBox *p;
	u32 i;
	p = (GF_SampleSizeBox *)a;
	if (dump_skip_samples)
		return GF_OK;

	if (a->type == GF_ISOM_BOX_TYPE_STSZ) {
		gf_isom_box_dump_start(a, "SampleSizeBox", trace);
	}
	else {
		gf_isom_box_dump_start(a, "CompactSampleSizeBox", trace);
	}

	gf_fprintf(trace, "SampleCount=\"%d\"",  p->sampleCount);
	if (a->type == GF_ISOM_BOX_TYPE_STSZ) {
		if (p->sampleSize) {
			gf_fprintf(trace, " ConstantSampleSize=\"%d\"", p->sampleSize);
		}
	} else {
		gf_fprintf(trace, " SampleSizeBits=\"%d\"", p->sampleSize);
	}
	gf_fprintf(trace, ">\n");

	if ((a->type != GF_ISOM_BOX_TYPE_STSZ) || !p->sampleSize) {
		if (!p->sizes && p->size) {
			gf_fprintf(trace, "<!--WARNING: No Sample Size indications-->\n");
		} else if (p->sizes) {
			for (i=0; i<p->sampleCount; i++) {
				gf_fprintf(trace, "<SampleSizeEntry Size=\"%d\"/>\n", p->sizes[i]);
			}
		}
	}
	if (!p->size) {
		gf_fprintf(trace, "<SampleSizeEntry Size=\"\"/>\n");
	}
	gf_isom_box_dump_done((a->type == GF_ISOM_BOX_TYPE_STSZ) ? "SampleSizeBox" : "CompactSampleSizeBox", a, trace);
	return GF_OK;
}

GF_Err stco_box_dump(GF_Box *a, FILE * trace)
{
	GF_ChunkOffsetBox *p;
	u32 i;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_ChunkOffsetBox *)a;
	gf_isom_box_dump_start(a, "ChunkOffsetBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	if (!p->offsets && p->size) {
		gf_fprintf(trace, "<!--Warning: No Chunk Offsets indications-->\n");
	} else if (p->offsets) {
		for (i=0; i<p->nb_entries; i++) {
			gf_fprintf(trace, "<ChunkEntry offset=\"%u\"/>\n", p->offsets[i]);
		}
	}
	if (!p->size) {
		gf_fprintf(trace, "<ChunkEntry offset=\"\"/>\n");
	}
	gf_isom_box_dump_done("ChunkOffsetBox", a, trace);
	return GF_OK;
}

GF_Err stss_box_dump(GF_Box *a, FILE * trace)
{
	GF_SyncSampleBox *p;
	u32 i;
	const char *name, *entname;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_SyncSampleBox *)a;
	if (a->type==GF_ISOM_BOX_TYPE_STSS) {
		name = "SyncSampleBox";
		entname = "SyncSampleEntry";
	} else {
		name = "PartialSyncSampleBox";
		entname = "PartialSyncSampleEntry";
	}
	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	if (!p->sampleNumbers && p->size) {
		if (a->type==GF_ISOM_BOX_TYPE_STSS)
			gf_fprintf(trace, "<!--Warning: No Key Frames indications-->\n");
	} else if (p->sampleNumbers) {
		for (i=0; i<p->nb_entries; i++) {
			gf_fprintf(trace, "<%s sampleNumber=\"%u\"/>\n", entname, p->sampleNumbers[i]);
		}
	}
	if (!p->size) {
			gf_fprintf(trace, "<%s sampleNumber=\"\"/>\n", entname);
	}
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err stdp_box_dump(GF_Box *a, FILE * trace)
{
	GF_DegradationPriorityBox *p;
	u32 i;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_DegradationPriorityBox *)a;
	gf_isom_box_dump_start(a, "DegradationPriorityBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	if (!p->priorities && p->size) {
		gf_fprintf(trace, "<!--Warning: No Degradation Priority indications-->\n");
	} else if (p->priorities) {
		for (i=0; i<p->nb_entries; i++) {
			gf_fprintf(trace, "<DegradationPriorityEntry DegradationPriority=\"%d\"/>\n", p->priorities[i]);
		}
	}
	if (!p->size) {
		gf_fprintf(trace, "<DegradationPriorityEntry DegradationPriority=\"\"/>\n");
	}
	gf_isom_box_dump_done("DegradationPriorityBox", a, trace);
	return GF_OK;
}

GF_Err sdtp_box_dump(GF_Box *a, FILE * trace)
{
	GF_SampleDependencyTypeBox *p;
	u32 i;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_SampleDependencyTypeBox*)a;
	gf_isom_box_dump_start(a, "SampleDependencyTypeBox", trace);
	gf_fprintf(trace, "SampleCount=\"%d\">\n", p->sampleCount);

	if (!p->sample_info) {
		gf_fprintf(trace, "<!--Warning: No sample dependencies indications-->\n");
	} else {
		for (i=0; i<p->sampleCount; i++) {
			const char *type;
			u8 flag = p->sample_info[i];
			gf_fprintf(trace, "<SampleDependencyEntry ");
			switch ( (flag >> 6) & 3) {
			case 1: type="openGOP"; break;
			case 2: type="no"; break;
			case 3: type="SAP2"; break;
			default:
			case 0: type="unknown"; break;
			}
			gf_fprintf(trace, "isLeading=\"%s\" ", type);

			switch ( (flag >> 4) & 3) {
			case 1: type="yes"; break;
			case 2: type="no"; break;
			case 3: type="RESERVED"; break;
			default:
			case 0: type="unknown"; break;
			}
			gf_fprintf(trace, "dependsOnOther=\"%s\" ", type);

			switch ( (flag >> 2) & 3) {
			case 1: type="yes"; break;
			case 2: type="no"; break;
			case 3: type="RESERVED"; break;
			default:
			case 0: type="unknown"; break;
			}
			gf_fprintf(trace, "dependedOn=\"%s\" ", type);

			switch ( flag & 3) {
			case 1: type="yes"; break;
			case 2: type="no"; break;
			case 3: type="RESERVED"; break;
			default:
			case 0: type="unknown"; break;
			}
			gf_fprintf(trace, "hasRedundancy=\"%s\"/>\n", type);
		}
	}
	if (!p->size) {
		gf_fprintf(trace, "<SampleDependencyEntry dependsOnOther=\"unknown|yes|no|RESERVED\" dependedOn=\"unknown|yes|no|RESERVED\" hasRedundancy=\"unknown|yes|no|RESERVED\"/>\n");
	}
	gf_isom_box_dump_done("SampleDependencyTypeBox", a, trace);
	return GF_OK;
}

GF_Err co64_box_dump(GF_Box *a, FILE * trace)
{
	GF_ChunkLargeOffsetBox *p;
	u32 i;

	if (dump_skip_samples)
		return GF_OK;

	p = (GF_ChunkLargeOffsetBox *)a;
	gf_isom_box_dump_start(a, "ChunkLargeOffsetBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->nb_entries);

	if (!p->offsets && p->size) {
		gf_fprintf(trace, "<!-- Warning: No Chunk Offsets indications/>\n");
	} else if (p->offsets) {
		for (i=0; i<p->nb_entries; i++)
			gf_fprintf(trace, "<ChunkOffsetEntry offset=\""LLU"\"/>\n", p->offsets[i]);
	}
	if (!p->size) {
		gf_fprintf(trace, "<ChunkOffsetEntry offset=\"\"/>\n");
	}
	gf_isom_box_dump_done("ChunkLargeOffsetBox", a, trace);
	return GF_OK;
}

GF_Err esds_box_dump(GF_Box *a, FILE * trace)
{
	GF_ESDBox *p;

	p = (GF_ESDBox *)a;
	gf_isom_box_dump_start(a, "MPEG4ESDescriptorBox", trace);
	gf_fprintf(trace, ">\n");

	if (p->desc) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc((GF_Descriptor *) p->desc, trace, 1, GF_TRUE);
#else
		gf_fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	} else if (p->size) {
		gf_fprintf(trace, "<!--INVALID MP4 FILE: ESD not present in MPEG Sample Description or corrupted-->\n");
	}
	gf_isom_box_dump_done("MPEG4ESDescriptorBox", a, trace);
	return GF_OK;
}

GF_Err minf_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "MediaInformationBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MediaInformationBox", a, trace);
	return GF_OK;
}

GF_Err tkhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackHeaderBox *p;
	p = (GF_TrackHeaderBox *)a;
	gf_isom_box_dump_start(a, "TrackHeaderBox", trace);

	gf_fprintf(trace, "CreationTime=\""LLD"\" ModificationTime=\""LLD"\" TrackID=\"%u\" Duration=\""LLD"\"",
			p->creationTime, p->modificationTime, p->trackID, p->duration);

	if (p->alternate_group) gf_fprintf(trace, " AlternateGroupID=\"%d\"", p->alternate_group);
	if (p->volume) {
		gf_fprintf(trace, " Volume=\"%.2f\"", (Float)p->volume / 256);
	} else if (p->width || p->height) {
		gf_fprintf(trace, " Width=\"%.2f\" Height=\"%.2f\"", (Float)p->width / 65536, (Float)p->height / 65536);
		if (p->layer) gf_fprintf(trace, " Layer=\"%d\"", p->layer);
	}
	gf_fprintf(trace, ">\n");
	if (p->width || p->height) {
		gf_fprintf(trace, "<Matrix m11=\"0x%.8x\" m12=\"0x%.8x\" m13=\"0x%.8x\" ", p->matrix[0], p->matrix[1], p->matrix[2]);
		gf_fprintf(trace, "m21=\"0x%.8x\" m22=\"0x%.8x\" m23=\"0x%.8x\" ", p->matrix[3], p->matrix[4], p->matrix[5]);
		gf_fprintf(trace, "m31=\"0x%.8x\" m32=\"0x%.8x\" m33=\"0x%.8x\"/>\n", p->matrix[6], p->matrix[7], p->matrix[8]);
	}

	gf_isom_box_dump_done("TrackHeaderBox", a, trace);
	return GF_OK;
}

GF_Err tref_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "TrackReferenceBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("TrackReferenceBox", a, trace);
	return GF_OK;
}

GF_Err mdia_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "MediaBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MediaBox", a, trace);
	return GF_OK;
}

GF_Err mfra_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "MovieFragmentRandomAccessBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MovieFragmentRandomAccessBox", a, trace);
	return GF_OK;
}

GF_Err tfra_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrackFragmentRandomAccessBox *p = (GF_TrackFragmentRandomAccessBox *)a;
	gf_isom_box_dump_start(a, "TrackFragmentRandomAccessBox", trace);
	gf_fprintf(trace, "TrackId=\"%u\" number_of_entries=\"%u\">\n", p->track_id, p->nb_entries);
	for (i=0; i<p->nb_entries; i++) {
		gf_fprintf(trace, "<RandomAccessEntry time=\""LLU"\" moof_offset=\""LLU"\" traf=\"%u\" trun=\"%u\" sample=\"%u\"/>\n",
			p->entries[i].time, p->entries[i].moof_offset,
			p->entries[i].traf_number, p->entries[i].trun_number, p->entries[i].sample_number);
	}
	if (!p->size) {
		gf_fprintf(trace, "<RandomAccessEntry time=\"\" moof_offset=\"\" traf=\"\" trun=\"\" sample=\"\"/>\n");
	}
	gf_isom_box_dump_done("TrackFragmentRandomAccessBox", a, trace);
	return GF_OK;
}

GF_Err mfro_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieFragmentRandomAccessOffsetBox *p = (GF_MovieFragmentRandomAccessOffsetBox *)a;

	gf_isom_box_dump_start(a, "MovieFragmentRandomAccessOffsetBox", trace);

	gf_fprintf(trace, "container_size=\"%d\" >\n", p->container_size);
	gf_isom_box_dump_done("MovieFragmentRandomAccessOffsetBox", a, trace);
	return GF_OK;
}


GF_Err elng_box_dump(GF_Box *a, FILE * trace)
{
	GF_ExtendedLanguageBox *p = (GF_ExtendedLanguageBox *)a;
	gf_isom_box_dump_start(a, "ExtendedLanguageBox", trace);
	gf_fprintf(trace, "LanguageCode=\"%s\">\n", p->extended_language);
	gf_isom_box_dump_done("ExtendedLanguageBox", a, trace);
	return GF_OK;
}

GF_Err unkn_box_dump(GF_Box *a, FILE * trace)
{
	Bool str_dump = GF_FALSE;
	const char *name = "UnknownBox";
	GF_UnknownBox *u = (GF_UnknownBox *)a;
	if (!a->type && (a->size==8)) {
		name = "TerminatorBox";
	} else if (u->original_4cc==GF_4CC('n','a','m','e') && (u->dataSize>4) && !u->data[0] && !u->data[1] && !u->data[2] && !u->data[3]) {
		name = "iTunesName";
		str_dump = GF_TRUE;
	} else if (u->original_4cc==GF_4CC('m','e','a','n') && (u->dataSize>4) && !u->data[0] && !u->data[1] && !u->data[2] && !u->data[3]) {
		name = "iTunesMean";
		str_dump = GF_TRUE;
	}

	gf_isom_box_dump_start(a, name, trace);

	if (str_dump) {
		u32 i;
		gf_fprintf(trace, " value=\"");
		for (i=4; i<u->dataSize; i++)
			gf_fprintf(trace, "%c", (char) u->data[i]);
		gf_fprintf(trace, "\"");
	} else if (u->dataSize && u->dataSize<100) {
		dump_data_attribute(trace, "data", u->data, u->dataSize);
	}

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err uuid_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "UUIDBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("UUIDBox", a, trace);
	return GF_OK;
}

GF_Err void_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "VoidBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("VoidBox", a, trace);
	return GF_OK;
}

GF_Err ftyp_box_dump(GF_Box *a, FILE * trace)
{
	GF_FileTypeBox *p;
	u32 i;

	p = (GF_FileTypeBox *)a;
	gf_isom_box_dump_start(a, (a->type == GF_ISOM_BOX_TYPE_FTYP ? "FileTypeBox" : "SegmentTypeBox"), trace);
	gf_fprintf(trace, "MajorBrand=\"%s\" MinorVersion=\"%d\">\n", gf_4cc_to_str(p->majorBrand), p->minorVersion);

	for (i=0; i<p->altCount; i++) {
		gf_fprintf(trace, "<BrandEntry AlternateBrand=\"%s\"/>\n", gf_4cc_to_str(p->altBrand[i]));
	}
	if (!p->type) {
		gf_fprintf(trace, "<BrandEntry AlternateBrand=\"4CC\"/>\n");
	}
	gf_isom_box_dump_done((a->type == GF_ISOM_BOX_TYPE_FTYP ? "FileTypeBox" : "SegmentTypeBox"), a, trace);
	return GF_OK;
}

GF_Err padb_box_dump(GF_Box *a, FILE * trace)
{
	GF_PaddingBitsBox *p;
	u32 i;

	p = (GF_PaddingBitsBox *)a;
	gf_isom_box_dump_start(a, "PaddingBitsBox", trace);
	gf_fprintf(trace, "EntryCount=\"%d\">\n", p->SampleCount);
	for (i=0; i<p->SampleCount; i+=1) {
		gf_fprintf(trace, "<PaddingBitsEntry PaddingBits=\"%d\"/>\n", p->padbits[i]);
	}
	if (!p->size) {
		gf_fprintf(trace, "<PaddingBitsEntry PaddingBits=\"\"/>\n");
	}
	gf_isom_box_dump_done("PaddingBitsBox", a, trace);
	return GF_OK;
}

GF_Err gppc_box_dump(GF_Box *a, FILE * trace)
{
	GF_3GPPConfigBox *p = (GF_3GPPConfigBox *)a;
	switch (p->cfg.type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		gf_isom_box_dump_start(a, "AMRConfigurationBox", trace);
		gf_fprintf(trace, "Vendor=\"%s\" Version=\"%d\"", gf_4cc_to_str(p->cfg.vendor), p->cfg.decoder_version);
		gf_fprintf(trace, " FramesPerSample=\"%d\" SupportedModes=\"%x\" ModeRotating=\"%d\"", p->cfg.frames_per_sample, p->cfg.AMR_mode_set, p->cfg.AMR_mode_change_period);
		gf_fprintf(trace, ">\n");
		gf_isom_box_dump_done("AMRConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
		gf_isom_box_dump_start(a, "EVRCConfigurationBox", trace);
		gf_fprintf(trace, "Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", gf_4cc_to_str(p->cfg.vendor), p->cfg.decoder_version, p->cfg.frames_per_sample);
		gf_isom_box_dump_done("EVRCConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_QCELP:
		gf_isom_box_dump_start(a, "QCELPConfigurationBox", trace);
		gf_fprintf(trace, "Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", gf_4cc_to_str(p->cfg.vendor), p->cfg.decoder_version, p->cfg.frames_per_sample);
		gf_isom_box_dump_done("QCELPConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_SMV:
		gf_isom_box_dump_start(a, "SMVConfigurationBox", trace);
		gf_fprintf(trace, "Vendor=\"%s\" Version=\"%d\" FramesPerSample=\"%d\" >\n", gf_4cc_to_str(p->cfg.vendor), p->cfg.decoder_version, p->cfg.frames_per_sample);
		gf_isom_box_dump_done("SMVConfigurationBox", a, trace);
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		gf_isom_box_dump_start(a, "H263ConfigurationBox", trace);
		gf_fprintf(trace, "Vendor=\"%s\" Version=\"%d\"", gf_4cc_to_str(p->cfg.vendor), p->cfg.decoder_version);
		gf_fprintf(trace, " Profile=\"%d\" Level=\"%d\"", p->cfg.H263_profile, p->cfg.H263_level);
		gf_fprintf(trace, ">\n");
		gf_isom_box_dump_done("H263ConfigurationBox", a, trace);
		break;
	default:
		break;
	}
	return GF_OK;
}


GF_Err avcc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	GF_AVCConfigurationBox *p = (GF_AVCConfigurationBox *) a;
	const char *name;
	switch (p->type) {
	case GF_ISOM_BOX_TYPE_MVCC:
		name = "MVC";
		break;
	case GF_ISOM_BOX_TYPE_SVCC:
		name = "SVC";
		break;
	case GF_ISOM_BOX_TYPE_AVCE:
		name = "DV-AVC";
		break;
	default:
		name = "AVC";
		break;
	}

	char boxname[256];
	sprintf(boxname, "%sConfigurationBox", name);
	gf_isom_box_dump_start(a, boxname, trace);
	gf_fprintf(trace, ">\n");

	gf_fprintf(trace, "<%sDecoderConfigurationRecord", name);

	if (! p->config) {
		if (p->size) {
			gf_fprintf(trace, ">\n");
			gf_fprintf(trace, "<!-- INVALID AVC ENTRY : no AVC/SVC config record -->\n");
		} else {

			gf_fprintf(trace, " configurationVersion=\"\" AVCProfileIndication=\"\" profile_compatibility=\"\" AVCLevelIndication=\"\" nal_unit_size=\"\" complete_representation=\"\"");

			gf_fprintf(trace, " chroma_format=\"\" luma_bit_depth=\"\" chroma_bit_depth=\"\"");
			gf_fprintf(trace, ">\n");

			gf_fprintf(trace, "<SequenceParameterSet size=\"\" content=\"\"/>\n");
			gf_fprintf(trace, "<PictureParameterSet size=\"\" content=\"\"/>\n");
			gf_fprintf(trace, "<SequenceParameterSetExtensions size=\"\" content=\"\"/>\n");
		}
		gf_fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);
		gf_isom_box_dump_done(boxname, a, trace);
		return GF_OK;
	}

	gf_fprintf(trace, " configurationVersion=\"%d\" AVCProfileIndication=\"%d\" profile_compatibility=\"%d\" AVCLevelIndication=\"%d\" nal_unit_size=\"%d\"", p->config->configurationVersion, p->config->AVCProfileIndication, p->config->profile_compatibility, p->config->AVCLevelIndication, p->config->nal_unit_size);


	if ((p->type==GF_ISOM_BOX_TYPE_SVCC) || (p->type==GF_ISOM_BOX_TYPE_MVCC) )
		gf_fprintf(trace, " complete_representation=\"%d\"", p->config->complete_representation);

	if (p->type==GF_ISOM_BOX_TYPE_AVCC) {
		if (gf_avcc_use_extensions(p->config->AVCProfileIndication)) {
			gf_fprintf(trace, " chroma_format=\"%s\" luma_bit_depth=\"%d\" chroma_bit_depth=\"%d\"", gf_avc_hevc_get_chroma_format_name(p->config->chroma_format), p->config->luma_bit_depth, p->config->chroma_bit_depth);
		}
	}

	gf_fprintf(trace, ">\n");

	count = gf_list_count(p->config->sequenceParameterSets);
	for (i=0; i<count; i++) {
		GF_NALUFFParam *c = (GF_NALUFFParam *)gf_list_get(p->config->sequenceParameterSets, i);
		gf_fprintf(trace, "<SequenceParameterSet size=\"%d\" content=\"", c->size);
		dump_data(trace, c->data, c->size);
		gf_fprintf(trace, "\"/>\n");
	}
	count = gf_list_count(p->config->pictureParameterSets);
	for (i=0; i<count; i++) {
		GF_NALUFFParam *c = (GF_NALUFFParam *)gf_list_get(p->config->pictureParameterSets, i);
		gf_fprintf(trace, "<PictureParameterSet size=\"%d\" content=\"", c->size);
		dump_data(trace, c->data, c->size);
		gf_fprintf(trace, "\"/>\n");
	}

	if (p->config->sequenceParameterSetExtensions) {
		count = gf_list_count(p->config->sequenceParameterSetExtensions);
		for (i=0; i<count; i++) {
			GF_NALUFFParam *c = (GF_NALUFFParam *)gf_list_get(p->config->sequenceParameterSetExtensions, i);
			gf_fprintf(trace, "<SequenceParameterSetExtensions size=\"%d\" content=\"", c->size);
			dump_data(trace, c->data, c->size);
			gf_fprintf(trace, "\"/>\n");
		}
	}

	gf_fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);

	gf_isom_box_dump_done(boxname, a, trace);
	return GF_OK;
}

GF_Err hvcc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	const char *name;
	switch (a->type) {
	case GF_ISOM_BOX_TYPE_HVCC:
		name = "HEVC";
		break;
	case GF_ISOM_BOX_TYPE_HVCE:
		name = "DV-HEVC";
		break;
	default:
		name = "L-HEVC";
		break;
	}
	char boxname[256];
	GF_HEVCConfigurationBox *p = (GF_HEVCConfigurationBox *) a;

	sprintf(boxname, "%sConfigurationBox", name);
	gf_isom_box_dump_start(a, boxname, trace);
	gf_fprintf(trace, ">\n");

	if (! p->config) {
		if (p->size) {
			gf_fprintf(trace, "<!-- INVALID HEVC ENTRY: no HEVC/SHVC config record -->\n");
		} else {
			gf_fprintf(trace, "<%sDecoderConfigurationRecord nal_unit_size=\"\" configurationVersion=\"\" ", name);
			if (a->type==GF_ISOM_BOX_TYPE_HVCC) {
				gf_fprintf(trace, "profile_space=\"\" tier_flag=\"\" profile_idc=\"\" general_profile_compatibility_flags=\"\" progressive_source_flag=\"\" interlaced_source_flag=\"\" non_packed_constraint_flag=\"\" frame_only_constraint_flag=\"\" constraint_indicator_flags=\"\" level_idc=\"\" ");
			}
			gf_fprintf(trace, "min_spatial_segmentation_idc=\"\" parallelismType=\"\" ");

			if (a->type==GF_ISOM_BOX_TYPE_HVCC)
				gf_fprintf(trace, "chroma_format=\"\" luma_bit_depth=\"\" chroma_bit_depth=\"\" avgFrameRate=\"\" constantFrameRate=\"\" numTemporalLayers=\"\" temporalIdNested=\"\"");

			gf_fprintf(trace, ">\n");
			gf_fprintf(trace, "<ParameterSetArray nalu_type=\"\" complete_set=\"\">\n");
			gf_fprintf(trace, "<ParameterSet size=\"\" content=\"\"/>\n");
			gf_fprintf(trace, "</ParameterSetArray>\n");
			gf_fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);
		}
		gf_fprintf(trace, "</%sConfigurationBox>\n", name);
		return GF_OK;
	}

	gf_fprintf(trace, "<%sDecoderConfigurationRecord nal_unit_size=\"%d\" ", name, p->config->nal_unit_size);
	gf_fprintf(trace, "configurationVersion=\"%u\" ", p->config->configurationVersion);
	if (a->type==GF_ISOM_BOX_TYPE_HVCC) {
		gf_fprintf(trace, "profile_space=\"%u\" ", p->config->profile_space);
		gf_fprintf(trace, "tier_flag=\"%u\" ", p->config->tier_flag);
		gf_fprintf(trace, "profile_idc=\"%u\" ", p->config->profile_idc);
		gf_fprintf(trace, "general_profile_compatibility_flags=\"%X\" ", p->config->general_profile_compatibility_flags);
		gf_fprintf(trace, "progressive_source_flag=\"%u\" ", p->config->progressive_source_flag);
		gf_fprintf(trace, "interlaced_source_flag=\"%u\" ", p->config->interlaced_source_flag);
		gf_fprintf(trace, "non_packed_constraint_flag=\"%u\" ", p->config->non_packed_constraint_flag);
		gf_fprintf(trace, "frame_only_constraint_flag=\"%u\" ", p->config->frame_only_constraint_flag);
		gf_fprintf(trace, "constraint_indicator_flags=\""LLX"\" ", p->config->constraint_indicator_flags);
		gf_fprintf(trace, "level_idc=\"%d\" ", p->config->level_idc);
	}
	gf_fprintf(trace, "min_spatial_segmentation_idc=\"%u\" ", p->config->min_spatial_segmentation_idc);
	gf_fprintf(trace, "parallelismType=\"%u\" ", p->config->parallelismType);

	if (a->type==GF_ISOM_BOX_TYPE_HVCC)
		gf_fprintf(trace, "chroma_format=\"%s\" luma_bit_depth=\"%u\" chroma_bit_depth=\"%u\" avgFrameRate=\"%u\" constantFrameRate=\"%u\" numTemporalLayers=\"%u\" temporalIdNested=\"%u\"",
	        gf_avc_hevc_get_chroma_format_name(p->config->chromaFormat), p->config->luma_bit_depth, p->config->chroma_bit_depth, p->config->avgFrameRate, p->config->constantFrameRate, p->config->numTemporalLayers, p->config->temporalIdNested);

	gf_fprintf(trace, ">\n");

	count = gf_list_count(p->config->param_array);
	for (i=0; i<count; i++) {
		u32 nalucount, j;
		GF_NALUFFParamArray *ar = (GF_NALUFFParamArray*)gf_list_get(p->config->param_array, i);
		gf_fprintf(trace, "<ParameterSetArray nalu_type=\"%d\" complete_set=\"%d\">\n", ar->type, ar->array_completeness);
		nalucount = gf_list_count(ar->nalus);
		for (j=0; j<nalucount; j++) {
			GF_NALUFFParam *c = (GF_NALUFFParam *)gf_list_get(ar->nalus, j);
			gf_fprintf(trace, "<ParameterSet size=\"%d\" content=\"", c->size);
			dump_data(trace, c->data, c->size);
			gf_fprintf(trace, "\"/>\n");
		}
		gf_fprintf(trace, "</ParameterSetArray>\n");
	}

	gf_fprintf(trace, "</%sDecoderConfigurationRecord>\n", name);

	gf_isom_box_dump_done(boxname, a, trace);
	return GF_OK;
}

GF_Err vvcc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, count;
	char boxname[256];
	GF_VVCConfigurationBox *p = (GF_VVCConfigurationBox *) a;

	sprintf(boxname, "VVCConfigurationBox");
	gf_isom_box_dump_start(a, boxname, trace);
	gf_fprintf(trace, ">\n");

	if (! p->config) {
		if (p->size) {
			gf_fprintf(trace, "<!-- INVALID VVC ENTRY: no VVC config record -->\n");
		} else {
			gf_fprintf(trace, "<VVCDecoderConfigurationRecord nal_unit_size=\"\" configurationVersion=\"\" ");
			gf_fprintf(trace, "general_profile_idc=\"\" general_tier_flag=\"\" general_sub_profile_idc=\"\" general_constraint_info=\"\" general_level_idc=\"\" ");
			gf_fprintf(trace, "chroma_format=\"\" luma_bit_depth=\"\" chroma_bit_depth=\"\" avgFrameRate=\"\" constantFrameRate=\"\" numTemporalLayers=\"\" maxWidth=\"\" maxHeight=\"\"");

			gf_fprintf(trace, ">\n");
			gf_fprintf(trace, "<ParameterSetArray nalu_type=\"\" complete_set=\"\">\n");
			gf_fprintf(trace, "<ParameterSet size=\"\" content=\"\"/>\n");
			gf_fprintf(trace, "</ParameterSetArray>\n");
			gf_fprintf(trace, "</VVCDecoderConfigurationRecord>\n");
		}
		gf_fprintf(trace, "</VVCConfigurationBox>\n");
		return GF_OK;
	}

	gf_fprintf(trace, "<VVCDecoderConfigurationRecord nal_unit_size=\"%d\" ", p->config->nal_unit_size);
	if (p->config->ptl_present) {

		gf_fprintf(trace, "chroma_format=\"%s\" chroma_bit_depth=\"%u\" avgFrameRate=\"%u\" constantFrameRate=\"%u\" numTemporalLayers=\"%u\" maxWidth=\"%u\" maxHeight=\"%u\" ",
			gf_avc_hevc_get_chroma_format_name(p->config->chroma_format),
			p->config->bit_depth, p->config->avgFrameRate, p->config->constantFrameRate, p->config->numTemporalLayers, p->config->maxPictureWidth, p->config->maxPictureHeight);

		gf_fprintf(trace, "general_profile_idc=\"%u\" ", p->config->general_profile_idc);
		gf_fprintf(trace, "general_tier_flag=\"%u\" ", p->config->general_tier_flag);
		gf_fprintf(trace, "general_sub_profile_idc=\"%u\" ", p->config->general_sub_profile_idc);
		if (p->config->general_constraint_info) {
			gf_fprintf(trace, "general_constraint_info=\"");
			dump_data_hex(trace, p->config->general_constraint_info, p->config->num_constraint_info);
			gf_fprintf(trace, "\" ");
		}
		gf_fprintf(trace, "general_level_idc=\"%u\" ", p->config->general_level_idc);
	}
	gf_fprintf(trace, ">\n");

	count = gf_list_count(p->config->param_array);
	for (i=0; i<count; i++) {
		u32 nalucount, j;
		GF_NALUFFParamArray *ar = (GF_NALUFFParamArray*)gf_list_get(p->config->param_array, i);
		gf_fprintf(trace, "<ParameterSetArray nalu_type=\"%d\" complete_set=\"%d\">\n", ar->type, ar->array_completeness);
		nalucount = gf_list_count(ar->nalus);
		for (j=0; j<nalucount; j++) {
			GF_NALUFFParam *c = (GF_NALUFFParam *)gf_list_get(ar->nalus, j);
			gf_fprintf(trace, "<ParameterSet size=\"%d\" content=\"", c->size);
			dump_data(trace, c->data, c->size);
			gf_fprintf(trace, "\"/>\n");
		}
		gf_fprintf(trace, "</ParameterSetArray>\n");
	}

	gf_fprintf(trace, "</VVCDecoderConfigurationRecord>\n");

	gf_isom_box_dump_done(boxname, a, trace);
	return GF_OK;
}


GF_Err vvnc_box_dump(GF_Box *a, FILE * trace)
{
	GF_VVCNaluConfigurationBox *p = (GF_VVCNaluConfigurationBox *) a;

	gf_isom_box_dump_start(a, "VVCNaluConfigurationBox", trace);
	gf_fprintf(trace, " nal_unit_size=\"%d\">\n", p->nal_unit_size);
	gf_isom_box_dump_done("VVCNaluConfigurationBox", a, trace);
	return GF_OK;
}


GF_Err av1c_box_dump(GF_Box *a, FILE *trace) {
	GF_AV1ConfigurationBox *ptr = (GF_AV1ConfigurationBox*)a;
	gf_fprintf(trace, "<AV1ConfigurationBox>\n");
	if (ptr->config) {
		u32 i, obu_count = gf_list_count(ptr->config->obu_array);

		gf_fprintf(trace, "<AV1Config version=\"%u\" profile=\"%u\" level_idx0=\"%u\" tier=\"%u\" ", (u32)ptr->config->version, (u32)ptr->config->seq_profile, (u32)ptr->config->seq_level_idx_0, (u32)ptr->config->seq_tier_0);
		gf_fprintf(trace, "high_bitdepth=\"%u\" twelve_bit=\"%u\" monochrome=\"%u\" ", (u32)ptr->config->high_bitdepth, (u32)ptr->config->twelve_bit, (u32)ptr->config->monochrome);
		gf_fprintf(trace, "chroma_subsampling_x=\"%u\" chroma_subsampling_y=\"%u\" chroma_sample_position=\"%u\" ", (u32)ptr->config->chroma_subsampling_x, (u32)ptr->config->chroma_subsampling_y, (u32)ptr->config->chroma_sample_position);
		gf_fprintf(trace, "initial_presentation_delay=\"%u\" OBUs_count=\"%u\">\n", ptr->config->initial_presentation_delay_minus_one+1, obu_count);

		for (i=0; i<obu_count; i++) {
			GF_AV1_OBUArrayEntry *obu_a = gf_list_get(ptr->config->obu_array, i);
			gf_fprintf(trace, "<OBU type=\"%d\" name=\"%s\" size=\"%d\" content=\"", obu_a->obu_type, gf_av1_get_obu_name(obu_a->obu_type), (u32) obu_a->obu_length);
			dump_data(trace, (char *)obu_a->obu, (u32) obu_a->obu_length);
			gf_fprintf(trace, "\"/>\n");
		}
		gf_fprintf(trace, "</AV1Config>\n");
	}
	gf_fprintf(trace, "</AV1ConfigurationBox>\n");
	return GF_OK;
}


GF_Err vpcc_box_dump(GF_Box *a, FILE *trace) {
	GF_VPConfigurationBox *ptr = (GF_VPConfigurationBox*)a;
	gf_fprintf(trace, "<VPConfigurationBox>\n");
	if (ptr->config) {
		gf_fprintf(trace, "<VPConfig");

		gf_fprintf(trace, " profile=\"%u\"", ptr->config->profile);
		gf_fprintf(trace, " level=\"%u\"", ptr->config->level);
		gf_fprintf(trace, " bit_depth=\"%u\"", ptr->config->bit_depth);
		gf_fprintf(trace, " chroma_subsampling=\"%u\"", ptr->config->chroma_subsampling);
		gf_fprintf(trace, " video_fullRange_flag=\"%u\"", ptr->config->video_fullRange_flag);
		gf_fprintf(trace, " colour_primaries=\"%u\"", ptr->config->colour_primaries);
		gf_fprintf(trace, " transfer_characteristics=\"%u\"", ptr->config->transfer_characteristics);
		gf_fprintf(trace, " matrix_coefficients=\"%u\"", ptr->config->matrix_coefficients);
		gf_fprintf(trace, " codec_initdata_size=\"%u\"", ptr->config->codec_initdata_size);

		gf_fprintf(trace, ">\n</VPConfig>\n");
	}
	gf_fprintf(trace, "</VPConfigurationBox>\n");
	return GF_OK;
}

GF_Err SmDm_box_dump(GF_Box *a, FILE *trace) {
	GF_SMPTE2086MasteringDisplayMetadataBox * ptr = (GF_SMPTE2086MasteringDisplayMetadataBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "SMPTE2086MasteringDisplayMetadataBox", trace);
	gf_fprintf(trace, "primaryRChromaticity_x=\"%u\" ", ptr->primaryRChromaticity_x);
	gf_fprintf(trace, "primaryRChromaticity_y=\"%u\" ", ptr->primaryRChromaticity_y);
	gf_fprintf(trace, "primaryGChromaticity_x=\"%u\" ", ptr->primaryGChromaticity_x);
	gf_fprintf(trace, "primaryGChromaticity_y=\"%u\" ", ptr->primaryGChromaticity_y);
	gf_fprintf(trace, "primaryBChromaticity_x=\"%u\" ", ptr->primaryBChromaticity_x);
	gf_fprintf(trace, "primaryBChromaticity_y=\"%u\" ", ptr->primaryBChromaticity_y);
	gf_fprintf(trace, "whitePointChromaticity_x=\"%u\" ", ptr->whitePointChromaticity_x);
	gf_fprintf(trace, "whitePointChromaticity_y=\"%u\" ", ptr->whitePointChromaticity_y);
	gf_fprintf(trace, "luminanceMax=\"%u\" ", ptr->luminanceMax);
	gf_fprintf(trace, "luminanceMin=\"%u\">\n", ptr->luminanceMin);
	gf_isom_box_dump_done("SMPTE2086MasteringDisplayMetadataBox", a, trace);
	return GF_OK;
}

GF_Err CoLL_box_dump(GF_Box *a, FILE *trace) {
	GF_VPContentLightLevelBox * ptr = (GF_VPContentLightLevelBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "VPContentLightLevelBox", trace);
	gf_fprintf(trace, "maxCLL=\"%u\" maxFALL=\"%u\">\n", ptr->maxCLL, ptr->maxFALL);
	gf_isom_box_dump_done("VPContentLightLevelBox", a, trace);
	return GF_OK;
}

GF_Err m4ds_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_Descriptor *desc;
	GF_MPEG4ExtensionDescriptorsBox *p = (GF_MPEG4ExtensionDescriptorsBox *) a;
	gf_isom_box_dump_start(a, "MPEG4ExtensionDescriptorsBox", trace);
	gf_fprintf(trace, ">\n");

	i=0;
	while ((desc = (GF_Descriptor *)gf_list_enum(p->descriptors, &i))) {
#ifndef GPAC_DISABLE_OD_DUMP
		gf_odf_dump_desc(desc, trace, 1, GF_TRUE);
#else
		gf_fprintf(trace, "<!-- Object Descriptor Dumping disabled in this build of GPAC -->\n");
#endif
	}
	gf_isom_box_dump_done("MPEG4ExtensionDescriptorsBox", a, trace);
	return GF_OK;
}

GF_Err btrt_box_dump(GF_Box *a, FILE * trace)
{
	GF_BitRateBox *p = (GF_BitRateBox*)a;
	gf_isom_box_dump_start(a, "BitRateBox", trace);
	gf_fprintf(trace, "BufferSizeDB=\"%d\" avgBitRate=\"%d\" maxBitRate=\"%d\">\n", p->bufferSizeDB, p->avgBitrate, p->maxBitrate);
	gf_isom_box_dump_done("BitRateBox", a, trace);
	return GF_OK;
}

GF_Err ftab_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_FontTableBox *p = (GF_FontTableBox *)a;
	gf_isom_box_dump_start(a, "FontTableBox", trace);
	gf_fprintf(trace, ">\n");
	for (i=0; i<p->entry_count; i++) {
		gf_fprintf(trace, "<FontRecord ID=\"%d\" name=\"%s\"/>\n", p->fonts[i].fontID, p->fonts[i].fontName ? p->fonts[i].fontName : "NULL");
	}
	if (!p->size) {
		gf_fprintf(trace, "<FontRecord ID=\"\" name=\"\"/>\n");
	}
	gf_isom_box_dump_done("FontTableBox", a, trace);
	return GF_OK;
}

static void tx3g_dump_rgba8(FILE * trace, char *name, u32 col)
{
	gf_fprintf(trace, "%s=\"%x %x %x %x\"", name, (col>>16)&0xFF, (col>>8)&0xFF, (col)&0xFF, (col>>24)&0xFF);
}
static void tx3g_dump_rgb16(FILE * trace, char *name, char col[6])
{
	gf_fprintf(trace, "%s=\"%x %x %x\"", name, *((u16*)col), *((u16*)(col+2)), *((u16*)(col+4)));
}
static void tx3g_dump_box(FILE * trace, GF_BoxRecord *rec)
{
	gf_fprintf(trace, "<BoxRecord top=\"%d\" left=\"%d\" bottom=\"%d\" right=\"%d\"/>\n", rec->top, rec->left, rec->bottom, rec->right);
}
static void tx3g_dump_style(FILE * trace, GF_StyleRecord *rec)
{
	gf_fprintf(trace, "<StyleRecord startChar=\"%d\" endChar=\"%d\" fontID=\"%d\" styles=\"", rec->startCharOffset, rec->endCharOffset, rec->fontID);
	if (!rec->style_flags) {
		gf_fprintf(trace, "Normal");
	} else {
		if (rec->style_flags & 1) gf_fprintf(trace, "Bold ");
		if (rec->style_flags & 2) gf_fprintf(trace, "Italic ");
		if (rec->style_flags & 4) gf_fprintf(trace, "Underlined ");
	}
	gf_fprintf(trace, "\" fontSize=\"%d\" ", rec->font_size);
	tx3g_dump_rgba8(trace, "textColor", rec->text_color);
	gf_fprintf(trace, "/>\n");
}

GF_Err tx3g_box_dump(GF_Box *a, FILE * trace)
{
	GF_Tx3gSampleEntryBox *p = (GF_Tx3gSampleEntryBox *)a;
	gf_isom_box_dump_start(a, "Tx3gSampleEntryBox", trace);
	gf_fprintf(trace, "dataReferenceIndex=\"%d\" displayFlags=\"%x\" horizontal-justification=\"%d\" vertical-justification=\"%d\" ",
	        p->dataReferenceIndex, p->displayFlags, p->horizontal_justification, p->vertical_justification);

	tx3g_dump_rgba8(trace, "backgroundColor", p->back_color);
	gf_fprintf(trace, ">\n");
	gf_fprintf(trace, "<DefaultBox>\n");
	tx3g_dump_box(trace, &p->default_box);
	gf_isom_box_dump_done("DefaultBox", a, trace);

	gf_fprintf(trace, "<DefaultStyle>\n");
	tx3g_dump_style(trace, &p->default_style);
	gf_fprintf(trace, "</DefaultStyle>\n");
	gf_isom_box_dump_done("Tx3gSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err text_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextSampleEntryBox *p = (GF_TextSampleEntryBox *)a;
	gf_isom_box_dump_start(a, "TextSampleEntryBox", trace);
	gf_fprintf(trace, "dataReferenceIndex=\"%d\" displayFlags=\"%x\" textJustification=\"%d\"  ",
	        p->dataReferenceIndex, p->displayFlags, p->textJustification);
	if (p->textName)
		gf_fprintf(trace, "textName=\"%s\" ", p->textName);
	tx3g_dump_rgb16(trace, "background-color", p->background_color);
	tx3g_dump_rgb16(trace, " foreground-color", p->foreground_color);
	gf_fprintf(trace, ">\n");

	gf_fprintf(trace, "<DefaultBox>\n");
	tx3g_dump_box(trace, &p->default_box);
	gf_isom_box_dump_done("DefaultBox", a, trace);
	gf_isom_box_dump_done("TextSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err styl_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TextStyleBox*p = (GF_TextStyleBox*)a;
	gf_isom_box_dump_start(a, "TextStyleBox", trace);
	gf_fprintf(trace, ">\n");
	for (i=0; i<p->entry_count; i++) tx3g_dump_style(trace, &p->styles[i]);
	if (!p->size) {
		gf_fprintf(trace, "<StyleRecord startChar=\"\" endChar=\"\" fontID=\"\" styles=\"Normal|Bold|Italic|Underlined\" fontSize=\"\" textColor=\"\" />\n");
	}
	gf_isom_box_dump_done("TextStyleBox", a, trace);
	return GF_OK;
}
GF_Err hlit_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextHighlightBox*p = (GF_TextHighlightBox*)a;
	gf_isom_box_dump_start(a, "TextHighlightBox", trace);
	gf_fprintf(trace, "startcharoffset=\"%d\" endcharoffset=\"%d\">\n", p->startcharoffset, p->endcharoffset);
	gf_isom_box_dump_done("TextHighlightBox", a, trace);
	return GF_OK;
}
GF_Err hclr_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextHighlightColorBox*p = (GF_TextHighlightColorBox*)a;
	gf_isom_box_dump_start(a, "TextHighlightColorBox", trace);
	tx3g_dump_rgba8(trace, "highlight_color", p->hil_color);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("TextHighlightColorBox", a, trace);
	return GF_OK;
}

GF_Err krok_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TextKaraokeBox*p = (GF_TextKaraokeBox*)a;
	gf_isom_box_dump_start(a, "TextKaraokeBox", trace);
	gf_fprintf(trace, "highlight_starttime=\"%d\">\n", p->highlight_starttime);
	for (i=0; i<p->nb_entries; i++) {
		gf_fprintf(trace, "<KaraokeRecord highlight_endtime=\"%d\" start_charoffset=\"%d\" end_charoffset=\"%d\"/>\n", p->records[i].highlight_endtime, p->records[i].start_charoffset, p->records[i].end_charoffset);
	}
	if (!p->size) {
		gf_fprintf(trace, "<KaraokeRecord highlight_endtime=\"\" start_charoffset=\"\" end_charoffset=\"\"/>\n");
	}
	gf_isom_box_dump_done("TextKaraokeBox", a, trace);
	return GF_OK;
}
GF_Err dlay_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextScrollDelayBox*p = (GF_TextScrollDelayBox*)a;
	gf_isom_box_dump_start(a, "TextScrollDelayBox", trace);
	gf_fprintf(trace, "scroll_delay=\"%d\">\n", p->scroll_delay);
	gf_isom_box_dump_done("TextScrollDelayBox", a, trace);
	return GF_OK;
}
GF_Err href_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextHyperTextBox*p = (GF_TextHyperTextBox*)a;
	gf_isom_box_dump_start(a, "TextHyperTextBox", trace);
	gf_fprintf(trace, "startcharoffset=\"%d\" endcharoffset=\"%d\" URL=\"%s\" altString=\"%s\">\n", p->startcharoffset, p->endcharoffset, p->URL ? p->URL : "NULL", p->URL_hint ? p->URL_hint : "NULL");
	gf_isom_box_dump_done("TextHyperTextBox", a, trace);
	return GF_OK;
}
GF_Err tbox_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextBoxBox*p = (GF_TextBoxBox*)a;
	gf_isom_box_dump_start(a, "TextBoxBox", trace);
	gf_fprintf(trace, ">\n");
	tx3g_dump_box(trace, &p->box);
	gf_isom_box_dump_done("TextBoxBox", a, trace);
	return GF_OK;
}
GF_Err blnk_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextBlinkBox*p = (GF_TextBlinkBox*)a;
	gf_isom_box_dump_start(a, "TextBlinkBox", trace);
	gf_fprintf(trace, "start_charoffset=\"%d\" end_charoffset=\"%d\">\n", p->startcharoffset, p->endcharoffset);
	gf_isom_box_dump_done("TextBlinkBox", a, trace);
	return GF_OK;
}
GF_Err twrp_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextWrapBox*p = (GF_TextWrapBox*)a;
	gf_isom_box_dump_start(a, "TextWrapBox", trace);
	gf_fprintf(trace, "wrap_flag=\"%s\">\n", p->wrap_flag ? ( (p->wrap_flag>1) ? "Reserved" : "Automatic" ) : "No Wrap");
	gf_isom_box_dump_done("TextWrapBox", a, trace);
	return GF_OK;
}


GF_Err meta_box_dump(GF_Box *a, FILE * trace)
{
	GF_MetaBox *ptr = (GF_MetaBox *)a;
	gf_isom_box_dump_start_ex(a, "MetaBox", trace, ptr->is_qt ? GF_FALSE : GF_TRUE);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MetaBox", a, trace);
	return GF_OK;
}


GF_Err xml_box_dump(GF_Box *a, FILE * trace)
{
	GF_XMLBox *p = (GF_XMLBox *)a;
	gf_isom_box_dump_start(a, "XMLBox", trace);
	gf_fprintf(trace, ">\n");
	gf_fprintf(trace, "<![CDATA[\n");
	if (p->xml)
		gf_fwrite(p->xml, strlen(p->xml), trace);
	gf_fprintf(trace, "]]>\n");
	gf_isom_box_dump_done("XMLBox", a, trace);
	return GF_OK;
}


GF_Err bxml_box_dump(GF_Box *a, FILE * trace)
{
	GF_BinaryXMLBox *p = (GF_BinaryXMLBox *)a;
	gf_isom_box_dump_start(a, "BinaryXMLBox", trace);
	gf_fprintf(trace, "binarySize=\"%d\">\n", p->data_length);
	gf_isom_box_dump_done("BinaryXMLBox", a, trace);
	return GF_OK;
}


GF_Err pitm_box_dump(GF_Box *a, FILE * trace)
{
	GF_PrimaryItemBox *p = (GF_PrimaryItemBox *)a;
	gf_isom_box_dump_start(a, "PrimaryItemBox", trace);
	gf_fprintf(trace, "item_ID=\"%d\">\n", p->item_ID);
	gf_isom_box_dump_done("PrimaryItemBox", a, trace);
	return GF_OK;
}

GF_Err ipro_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ItemProtectionBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ItemProtectionBox", a, trace);
	return GF_OK;
}

GF_Err infe_box_dump(GF_Box *a, FILE * trace)
{
	GF_ItemInfoEntryBox *p = (GF_ItemInfoEntryBox *)a;
	gf_isom_box_dump_start(a, "ItemInfoEntryBox", trace);
	gf_fprintf(trace, "item_ID=\"%d\" item_protection_index=\"%d\" item_name=\"%s\" content_type=\"%s\" content_encoding=\"%s\" item_type=\"%s\">\n", p->item_ID, p->item_protection_index, p->item_name, p->content_type, p->content_encoding, gf_4cc_to_str(p->item_type));
	gf_isom_box_dump_done("ItemInfoEntryBox", a, trace);
	return GF_OK;
}

GF_Err iinf_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ItemInfoBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ItemInfoBox", a, trace);
	return GF_OK;
}

GF_Err iloc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, j, count, count2;
	GF_ItemLocationBox *p = (GF_ItemLocationBox*)a;
	gf_isom_box_dump_start(a, "ItemLocationBox", trace);
	gf_fprintf(trace, "offset_size=\"%d\" length_size=\"%d\" base_offset_size=\"%d\" index_size=\"%d\">\n", p->offset_size, p->length_size, p->base_offset_size, p->index_size);
	count = gf_list_count(p->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *ie = (GF_ItemLocationEntry *)gf_list_get(p->location_entries, i);
		count2 = gf_list_count(ie->extent_entries);
		gf_fprintf(trace, "<ItemLocationEntry item_ID=\"%d\" data_reference_index=\"%d\" base_offset=\""LLD"\" construction_method=\"%d\">\n", ie->item_ID, ie->data_reference_index, ie->base_offset, ie->construction_method);
		for (j=0; j<count2; j++) {
			GF_ItemExtentEntry *iee = (GF_ItemExtentEntry *)gf_list_get(ie->extent_entries, j);
			gf_fprintf(trace, "<ItemExtentEntry extent_offset=\""LLD"\" extent_length=\""LLD"\" extent_index=\""LLD"\" />\n", iee->extent_offset, iee->extent_length, iee->extent_index);
		}
		gf_fprintf(trace, "</ItemLocationEntry>\n");
	}
	if (!p->size) {
		gf_fprintf(trace, "<ItemLocationEntry item_ID=\"\" data_reference_index=\"\" base_offset=\"\" construction_method=\"\">\n");
		gf_fprintf(trace, "<ItemExtentEntry extent_offset=\"\" extent_length=\"\" extent_index=\"\" />\n");
		gf_fprintf(trace, "</ItemLocationEntry>\n");
	}
	gf_isom_box_dump_done("ItemLocationBox", a, trace);
	return GF_OK;
}

GF_Err iref_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ItemReferenceBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ItemReferenceBox", a, trace);
	return GF_OK;
}

GF_Err hinf_box_dump(GF_Box *a, FILE * trace)
{
//	GF_HintInfoBox *p  = (GF_HintInfoBox *)a;
	gf_isom_box_dump_start(a, "HintInfoBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("HintInfoBox", a, trace);
	return GF_OK;
}

GF_Err trpy_box_dump(GF_Box *a, FILE * trace)
{
	GF_TRPYBox *p = (GF_TRPYBox *)a;
	gf_isom_box_dump_start(a, "LargeTotalRTPBytesBox", trace);
	gf_fprintf(trace, "RTPBytesSent=\""LLD"\">\n", p->nbBytes);
	gf_isom_box_dump_done("LargeTotalRTPBytesBox", a, trace);
	return GF_OK;
}

GF_Err totl_box_dump(GF_Box *a, FILE * trace)
{
	GF_TOTLBox *p;

	p = (GF_TOTLBox *)a;
	gf_isom_box_dump_start(a, "TotalRTPBytesBox", trace);
	gf_fprintf(trace, "RTPBytesSent=\"%d\">\n", p->nbBytes);
	gf_isom_box_dump_done("TotalRTPBytesBox", a, trace);
	return GF_OK;
}

GF_Err nump_box_dump(GF_Box *a, FILE * trace)
{
	GF_NUMPBox *p;

	p = (GF_NUMPBox *)a;
	gf_isom_box_dump_start(a, "LargeTotalPacketBox", trace);
	gf_fprintf(trace, "PacketsSent=\""LLD"\">\n", p->nbPackets);
	gf_isom_box_dump_done("LargeTotalPacketBox", a, trace);
	return GF_OK;
}

GF_Err npck_box_dump(GF_Box *a, FILE * trace)
{
	GF_NPCKBox *p;
	p = (GF_NPCKBox *)a;
	gf_isom_box_dump_start(a, "TotalPacketBox", trace);
	gf_fprintf(trace, "packetsSent=\"%d\">\n", p->nbPackets);
	gf_isom_box_dump_done("TotalPacketBox", a, trace);
	return GF_OK;
}

GF_Err tpyl_box_dump(GF_Box *a, FILE * trace)
{
	GF_NTYLBox *p;
	p = (GF_NTYLBox *)a;
	gf_isom_box_dump_start(a, "LargeTotalMediaBytesBox", trace);
	gf_fprintf(trace, "BytesSent=\""LLD"\">\n", p->nbBytes);
	gf_isom_box_dump_done("LargeTotalMediaBytesBox", a, trace);
	return GF_OK;
}

GF_Err tpay_box_dump(GF_Box *a, FILE * trace)
{
	GF_TPAYBox *p;
	p = (GF_TPAYBox *)a;
	gf_isom_box_dump_start(a, "TotalMediaBytesBox", trace);
	gf_fprintf(trace, "BytesSent=\"%d\">\n", p->nbBytes);
	gf_isom_box_dump_done("TotalMediaBytesBox", a, trace);
	return GF_OK;
}

GF_Err maxr_box_dump(GF_Box *a, FILE * trace)
{
	GF_MAXRBox *p;
	p = (GF_MAXRBox *)a;
	gf_isom_box_dump_start(a, "MaxDataRateBox", trace);
	gf_fprintf(trace, "MaxDataRate=\"%d\" Granularity=\"%d\">\n", p->maxDataRate, p->granularity);
	gf_isom_box_dump_done("MaxDataRateBox", a, trace);
	return GF_OK;
}

GF_Err dmed_box_dump(GF_Box *a, FILE * trace)
{
	GF_DMEDBox *p;

	p = (GF_DMEDBox *)a;
	gf_isom_box_dump_start(a, "BytesFromMediaTrackBox", trace);
	gf_fprintf(trace, "BytesSent=\""LLD"\">\n", p->nbBytes);
	gf_isom_box_dump_done("BytesFromMediaTrackBox", a, trace);
	return GF_OK;
}

GF_Err dimm_box_dump(GF_Box *a, FILE * trace)
{
	GF_DIMMBox *p;
	p = (GF_DIMMBox *)a;
	gf_isom_box_dump_start(a, "ImmediateDataBytesBox", trace);
	gf_fprintf(trace, "BytesSent=\""LLD"\">\n", p->nbBytes);
	gf_isom_box_dump_done("ImmediateDataBytesBox", a, trace);
	return GF_OK;
}

GF_Err drep_box_dump(GF_Box *a, FILE * trace)
{
	GF_DREPBox *p;
	p = (GF_DREPBox *)a;
	gf_isom_box_dump_start(a, "RepeatedDataBytesBox", trace);
	gf_fprintf(trace, "RepeatedBytes=\""LLD"\">\n", p->nbBytes);
	gf_isom_box_dump_done("RepeatedDataBytesBox", a, trace);
	return GF_OK;
}

GF_Err tssy_box_dump(GF_Box *a, FILE * trace)
{
	GF_TimeStampSynchronyBox *p = (GF_TimeStampSynchronyBox *)a;
	gf_isom_box_dump_start(a, "TimeStampSynchronyBox", trace);
	gf_fprintf(trace, "timestamp_sync=\"%d\">\n", p->timestamp_sync);
	gf_isom_box_dump_done("TimeStampSynchronyBox", a, trace);
	return GF_OK;
}

GF_Err rssr_box_dump(GF_Box *a, FILE * trace)
{
	GF_ReceivedSsrcBox *p = (GF_ReceivedSsrcBox *)a;
	gf_isom_box_dump_start(a, "ReceivedSsrcBox", trace);
	gf_fprintf(trace, "SSRC=\"%d\">\n", p->ssrc);
	gf_isom_box_dump_done("ReceivedSsrcBox", a, trace);
	return GF_OK;
}

GF_Err tmin_box_dump(GF_Box *a, FILE * trace)
{
	GF_TMINBox *p;
	p = (GF_TMINBox *)a;
	gf_isom_box_dump_start(a, "MinTransmissionTimeBox", trace);
	gf_fprintf(trace, "MinimumTransmitTime=\"%d\">\n", p->minTime);
	gf_isom_box_dump_done("MinTransmissionTimeBox", a, trace);
	return GF_OK;
}

GF_Err tmax_box_dump(GF_Box *a, FILE * trace)
{
	GF_TMAXBox *p;
	p = (GF_TMAXBox *)a;
	gf_isom_box_dump_start(a, "MaxTransmissionTimeBox", trace);
	gf_fprintf(trace, "MaximumTransmitTime=\"%d\">\n", p->maxTime);
	gf_isom_box_dump_done("MaxTransmissionTimeBox", a, trace);
	return GF_OK;
}

GF_Err pmax_box_dump(GF_Box *a, FILE * trace)
{
	GF_PMAXBox *p;
	p = (GF_PMAXBox *)a;
	gf_isom_box_dump_start(a, "MaxPacketSizeBox", trace);
	gf_fprintf(trace, "MaximumSize=\"%d\">\n", p->maxSize);
	gf_isom_box_dump_done("MaxPacketSizeBox", a, trace);
	return GF_OK;
}

GF_Err dmax_box_dump(GF_Box *a, FILE * trace)
{
	GF_DMAXBox *p;
	p = (GF_DMAXBox *)a;
	gf_isom_box_dump_start(a, "MaxPacketDurationBox", trace);
	gf_fprintf(trace, "MaximumDuration=\"%d\">\n", p->maxDur);
	gf_isom_box_dump_done("MaxPacketDurationBox", a, trace);
	return GF_OK;
}

GF_Err payt_box_dump(GF_Box *a, FILE * trace)
{
	GF_PAYTBox *p;
	p = (GF_PAYTBox *)a;
	gf_isom_box_dump_start(a, "PayloadTypeBox", trace);
	gf_fprintf(trace, "PayloadID=\"%d\" PayloadString=\"%s\">\n", p->payloadCode, p->payloadString);
	gf_isom_box_dump_done("PayloadTypeBox", a, trace);
	return GF_OK;
}

GF_Err name_box_dump(GF_Box *a, FILE * trace)
{
	GF_NameBox *p;
	p = (GF_NameBox *)a;
	gf_isom_box_dump_start(a, "NameBox", trace);
	gf_fprintf(trace, "Name=\"%s\">\n", p->string);
	gf_isom_box_dump_done("NameBox", a, trace);
	return GF_OK;
}

GF_Err rely_box_dump(GF_Box *a, FILE * trace)
{
	GF_RelyHintBox *p;
	p = (GF_RelyHintBox *)a;
	gf_isom_box_dump_start(a, "RelyTransmissionBox", trace);
	gf_fprintf(trace, "preferred=\"%d\" required=\"%d\">\n", p->preferred, p->required);
	gf_isom_box_dump_done("RelyTransmissionBox", a, trace);
	return GF_OK;
}

GF_Err snro_box_dump(GF_Box *a, FILE * trace)
{
	GF_SeqOffHintEntryBox *p;
	p = (GF_SeqOffHintEntryBox *)a;
	gf_isom_box_dump_start(a, "PacketSequenceOffsetBox", trace);
	gf_fprintf(trace, "SeqNumOffset=\"%d\">\n", p->SeqOffset);
	gf_isom_box_dump_done("PacketSequenceOffsetBox", a, trace);
	return GF_OK;
}

GF_Err tims_box_dump(GF_Box *a, FILE * trace)
{
	GF_TSHintEntryBox *p;
	p = (GF_TSHintEntryBox *)a;
	gf_isom_box_dump_start(a, "RTPTimeScaleBox", trace);
	gf_fprintf(trace, "TimeScale=\"%d\">\n", p->timeScale);
	gf_isom_box_dump_done("RTPTimeScaleBox", a, trace);
	return GF_OK;
}

GF_Err tsro_box_dump(GF_Box *a, FILE * trace)
{
	GF_TimeOffHintEntryBox *p;
	p = (GF_TimeOffHintEntryBox *)a;
	gf_isom_box_dump_start(a, "TimeStampOffsetBox", trace);
	gf_fprintf(trace, "TimeStampOffset=\"%d\">\n", p->TimeOffset);
	gf_isom_box_dump_done("TimeStampOffsetBox", a, trace);
	return GF_OK;
}


GF_Err ghnt_box_dump(GF_Box *a, FILE * trace)
{
	char *name;
	GF_HintSampleEntryBox *p;
	p = (GF_HintSampleEntryBox *)a;

	if (a->type == GF_ISOM_BOX_TYPE_RTP_STSD) {
		name = "RTPHintSampleEntryBox";
	} else if (a->type == GF_ISOM_BOX_TYPE_SRTP_STSD) {
		name = "SRTPHintSampleEntryBox";
	} else if (a->type == GF_ISOM_BOX_TYPE_FDP_STSD) {
		name = "FDPHintSampleEntryBox";
	} else if (a->type == GF_ISOM_BOX_TYPE_RRTP_STSD) {
		name = "RTPReceptionHintSampleEntryBox";
	} else if (a->type == GF_ISOM_BOX_TYPE_RTCP_STSD) {
		name = "RTCPReceptionHintSampleEntryBox";
	} else {
		name = "GenericHintSampleEntryBox";
	}
	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\" HintTrackVersion=\"%d\" LastCompatibleVersion=\"%d\"", p->dataReferenceIndex, p->HintTrackVersion, p->LastCompatibleVersion);
	if ((a->type == GF_ISOM_BOX_TYPE_RTP_STSD) || (a->type == GF_ISOM_BOX_TYPE_SRTP_STSD) || (a->type == GF_ISOM_BOX_TYPE_RRTP_STSD) || (a->type == GF_ISOM_BOX_TYPE_RTCP_STSD)) {
		gf_fprintf(trace, " MaxPacketSize=\"%d\"", p->MaxPacketSize);
	} else if (a->type == GF_ISOM_BOX_TYPE_FDP_STSD) {
		gf_fprintf(trace, " partition_entry_ID=\"%d\" FEC_overhead=\"%d\"", p->partition_entry_ID, p->FEC_overhead);
	}
	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err hnti_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "HintTrackInfoBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("HintTrackInfoBox", a, trace);
	return GF_OK;
}

GF_Err sdp_box_dump(GF_Box *a, FILE * trace)
{
	GF_SDPBox *p = (GF_SDPBox *)a;
	gf_isom_box_dump_start(a, "SDPBox", trace);
	gf_fprintf(trace, ">\n");
	if (p->sdpText)
		gf_fprintf(trace, "<!-- sdp text: %s -->\n", p->sdpText);
	gf_isom_box_dump_done("SDPBox", a, trace);
	return GF_OK;
}

GF_Err rtp_hnti_box_dump(GF_Box *a, FILE * trace)
{
	GF_RTPBox *p = (GF_RTPBox *)a;
	gf_isom_box_dump_start(a, "RTPMovieHintInformationBox", trace);
	gf_fprintf(trace, "descriptionformat=\"%s\">\n", gf_4cc_to_str(p->subType));
	if (p->sdpText)
		gf_fprintf(trace, "<!-- sdp text: %s -->\n", p->sdpText);
	gf_isom_box_dump_done("RTPMovieHintInformationBox", a, trace);
	return GF_OK;
}

GF_Err rtpo_box_dump(GF_Box *a, FILE * trace)
{
	GF_RTPOBox *p;
	p = (GF_RTPOBox *)a;
	gf_isom_box_dump_start(a, "RTPTimeOffsetBox", trace);
	gf_fprintf(trace, "PacketTimeOffset=\"%d\">\n", p->timeOffset);
	gf_isom_box_dump_done("RTPTimeOffsetBox", a, trace);
	return GF_OK;
}

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

GF_Err mvex_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "MovieExtendsBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MovieExtendsBox", a, trace);
	return GF_OK;
}

GF_Err mehd_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieExtendsHeaderBox *p = (GF_MovieExtendsHeaderBox*)a;
	gf_isom_box_dump_start(a, "MovieExtendsHeaderBox", trace);
	gf_fprintf(trace, "fragmentDuration=\""LLD"\" >\n", p->fragment_duration);
	gf_isom_box_dump_done("MovieExtendsHeaderBox", a, trace);
	return GF_OK;
}

void sample_flags_dump(const char *name, u32 sample_flags, FILE * trace)
{
	gf_fprintf(trace, "<%s", name);
	gf_fprintf(trace, " IsLeading=\"%d\"", GF_ISOM_GET_FRAG_LEAD(sample_flags) );
	gf_fprintf(trace, " SampleDependsOn=\"%d\"", GF_ISOM_GET_FRAG_DEPENDS(sample_flags) );
	gf_fprintf(trace, " SampleIsDependedOn=\"%d\"", GF_ISOM_GET_FRAG_DEPENDED(sample_flags) );
	gf_fprintf(trace, " SampleHasRedundancy=\"%d\"", GF_ISOM_GET_FRAG_REDUNDANT(sample_flags) );
	gf_fprintf(trace, " SamplePadding=\"%d\"", GF_ISOM_GET_FRAG_PAD(sample_flags) );
	gf_fprintf(trace, " SampleSync=\"%d\"", GF_ISOM_GET_FRAG_SYNC(sample_flags));
	gf_fprintf(trace, " SampleDegradationPriority=\"%d\"", GF_ISOM_GET_FRAG_DEG(sample_flags));
	gf_fprintf(trace, "/>\n");
}

GF_Err trex_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackExtendsBox *p;
	p = (GF_TrackExtendsBox *)a;
	gf_isom_box_dump_start(a, "TrackExtendsBox", trace);
	gf_fprintf(trace, "TrackID=\"%d\"", p->trackID);
	gf_fprintf(trace, " SampleDescriptionIndex=\"%d\" SampleDuration=\"%d\" SampleSize=\"%d\"", p->def_sample_desc_index, p->def_sample_duration, p->def_sample_size);
	gf_fprintf(trace, ">\n");
	sample_flags_dump("DefaultSampleFlags", p->def_sample_flags, trace);
	gf_isom_box_dump_done("TrackExtendsBox", a, trace);
	return GF_OK;
}

GF_Err trep_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackExtensionPropertiesBox *p = (GF_TrackExtensionPropertiesBox*)a;
	gf_isom_box_dump_start(a, "TrackExtensionPropertiesBox", trace);
	gf_fprintf(trace, "TrackID=\"%d\">\n", p->trackID);
	gf_isom_box_dump_done("TrackExtensionPropertiesBox", a, trace);
	return GF_OK;
}

GF_Err moof_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieFragmentBox *p;
	p = (GF_MovieFragmentBox *)a;
	gf_isom_box_dump_start(a, "MovieFragmentBox", trace);
	gf_fprintf(trace, "TrackFragments=\"%d\"", gf_list_count(p->TrackList));
	if (p->internal_flags & GF_ISOM_BOX_COMPRESSED)
		gf_fprintf(trace, " compressedSize=\""LLU"\"", p->size - p->compressed_diff);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("MovieFragmentBox", a, trace);
	return GF_OK;
}

GF_Err mfhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_MovieFragmentHeaderBox *p;
	p = (GF_MovieFragmentHeaderBox *)a;
	gf_isom_box_dump_start(a, "MovieFragmentHeaderBox", trace);
	gf_fprintf(trace, "FragmentSequenceNumber=\"%d\">\n", p->sequence_number);
	gf_isom_box_dump_done("MovieFragmentHeaderBox", a, trace);
	return GF_OK;
}

GF_Err traf_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "TrackFragmentBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("TrackFragmentBox", a, trace);
	return GF_OK;
}

static void frag_dump_sample_flags(FILE * trace, u32 flags, u32 field_idx)
{
	if (!field_idx) return;
	if (field_idx==1) {
		gf_fprintf(trace, " IsLeading=\"%d\" DependsOn=\"%d\"", GF_ISOM_GET_FRAG_LEAD(flags), GF_ISOM_GET_FRAG_DEPENDS(flags));
	} else if (field_idx==2) {
		gf_fprintf(trace, " IsLeading=\"%d\" DependsOn=\"%d\" IsDependedOn=\"%d\" HasRedundancy=\"%d\" SamplePadding=\"%d\" Sync=\"%d\"",
	        GF_ISOM_GET_FRAG_LEAD(flags), GF_ISOM_GET_FRAG_DEPENDS(flags), GF_ISOM_GET_FRAG_DEPENDED(flags), GF_ISOM_GET_FRAG_REDUNDANT(flags), GF_ISOM_GET_FRAG_PAD(flags), GF_ISOM_GET_FRAG_SYNC(flags));
	} else {
		gf_fprintf(trace, " SamplePadding=\"%d\" Sync=\"%d\" DegradationPriority=\"%d\" IsLeading=\"%d\" DependsOn=\"%d\" IsDependedOn=\"%d\" HasRedundancy=\"%d\"",
	        GF_ISOM_GET_FRAG_PAD(flags), GF_ISOM_GET_FRAG_SYNC(flags), GF_ISOM_GET_FRAG_DEG(flags),
	        GF_ISOM_GET_FRAG_LEAD(flags), GF_ISOM_GET_FRAG_DEPENDS(flags), GF_ISOM_GET_FRAG_DEPENDED(flags), GF_ISOM_GET_FRAG_REDUNDANT(flags));
	}
}

GF_Err tfhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackFragmentHeaderBox *p;
	p = (GF_TrackFragmentHeaderBox *)a;
	gf_isom_box_dump_start(a, "TrackFragmentHeaderBox", trace);
	gf_fprintf(trace, "TrackID=\"%u\"", p->trackID);

	if (p->flags & GF_ISOM_TRAF_BASE_OFFSET) {
		gf_fprintf(trace, " BaseDataOffset=\""LLU"\"", p->base_data_offset);
	} else {
		gf_fprintf(trace, " BaseDataOffset=\"%s\"", (p->flags & GF_ISOM_MOOF_BASE_OFFSET) ? "moof" : "moof-or-previous-traf");
	}

	if (p->flags & GF_ISOM_TRAF_SAMPLE_DESC)
		gf_fprintf(trace, " SampleDescriptionIndex=\"%u\"", p->sample_desc_index);
	if (p->flags & GF_ISOM_TRAF_SAMPLE_DUR)
		gf_fprintf(trace, " SampleDuration=\"%u\"", p->def_sample_duration);
	if (p->flags & GF_ISOM_TRAF_SAMPLE_SIZE)
		gf_fprintf(trace, " SampleSize=\"%u\"", p->def_sample_size);

	if (p->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) {
		frag_dump_sample_flags(trace, p->def_sample_flags, 3);
	}

	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done("TrackFragmentHeaderBox", a, trace);
	return GF_OK;
}

GF_Err tfxd_box_dump(GF_Box *a, FILE * trace)
{
	GF_MSSTimeExtBox *ptr = (GF_MSSTimeExtBox*)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "MSSTimeExtensionBox", trace);
	gf_fprintf(trace, "AbsoluteTime=\""LLU"\" FragmentDuration=\""LLU"\">\n", ptr->absolute_time_in_track_timescale, ptr->fragment_duration_in_track_timescale);
	gf_fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"%d\"/>\n", ptr->version, ptr->flags);
	gf_isom_box_dump_done("MSSTimeExtensionBox", a, trace);
	return GF_OK;
}

GF_Err tfrf_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_MSSTimeRefBox *ptr = (GF_MSSTimeRefBox*)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "MSSTimeReferenceBox", trace);
	gf_fprintf(trace, "FragmentsCount=\"%d\">\n", ptr->frags_count);
	gf_fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"%d\"/>\n", ptr->version, ptr->flags);
	for (i=0; i<ptr->frags_count; i++) {
		gf_fprintf(trace, "<Fragment AbsoluteTime=\""LLU"\" FragmentDuration=\""LLU"\">\n", ptr->frags[i].absolute_time_in_track_timescale, ptr->frags[i].fragment_duration_in_track_timescale);
	}
	gf_isom_box_dump_done("MSSTimeReferenceBox", a, trace);
	return GF_OK;
}
GF_Err trun_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, flags;
	Bool full_dump = GF_FALSE;
	GF_TrackFragmentRunBox *p;

	p = (GF_TrackFragmentRunBox *)a;
	flags = p->flags;
#ifdef GF_ENABLE_CTRN
	if (p->use_ctrn) {
		p->flags = p->ctrn_flags;
		p->type = GF_ISOM_BOX_TYPE_CTRN;
	}
	gf_isom_box_dump_start(a, p->use_ctrn ? "CompactTrackRunBox" : "TrackRunBox", trace);
#else
	gf_isom_box_dump_start(a, "TrackRunBox", trace);
#endif
	p->flags = flags;
	p->type = GF_ISOM_BOX_TYPE_TRUN;
	gf_fprintf(trace, "SampleCount=\"%d\"", p->sample_count);

	if (p->flags & GF_ISOM_TRUN_DATA_OFFSET)
		gf_fprintf(trace, " DataOffset=\"%d\"", p->data_offset);

#ifdef GF_ENABLE_CTRN
	if (p->use_ctrn) {
		if (p->ctrn_flags & GF_ISOM_CTRN_DATAOFFSET_16)
			gf_fprintf(trace, " dataOffset16Bits=\"yes\"");

		if (p->ctso_multiplier)
			gf_fprintf(trace, " ctsoMultiplier=\"%d\"", p->ctso_multiplier);

		if (p->ctrn_flags & GF_ISOM_CTRN_INHERIT_DUR)
			gf_fprintf(trace, " inheritDuration=\"yes\"");
		if (p->ctrn_flags & GF_ISOM_CTRN_INHERIT_SIZE)
			gf_fprintf(trace, " inheritSize=\"yes\"");
		if (p->ctrn_flags & GF_ISOM_CTRN_INHERIT_FLAGS)
			gf_fprintf(trace, " inheritFlags=\"yes\"");
		if (p->ctrn_flags & GF_ISOM_CTRN_INHERIT_CTSO)
			gf_fprintf(trace, " inheritCTSOffset=\"yes\"");

		gf_fprintf(trace, " firstSampleDurationBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_first_dur));
		gf_fprintf(trace, " firstSampleSizeBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_first_size));
		gf_fprintf(trace, " firstSampleFlagsBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_first_sample_flags));
		gf_fprintf(trace, " firstSampleCTSOBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_first_ctts));
		gf_fprintf(trace, " sampleDurationBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_dur));
		gf_fprintf(trace, " sampleSizeBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_size));
		gf_fprintf(trace, " sampleFlagsBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_sample_flags));
		gf_fprintf(trace, " sampleCTSOBits=\"%d\"", gf_isom_ctrn_field_size_bits(p->ctrn_ctts));
		if (p->ctrn_flags & 0x00FFFF00)
			full_dump = GF_TRUE;

		gf_fprintf(trace, ">\n");
	} else
#endif
	{
		gf_fprintf(trace, ">\n");

		if (p->flags & GF_ISOM_TRUN_FIRST_FLAG) {
			sample_flags_dump("FirstSampleFlags", p->first_sample_flags, trace);
		}
		if (p->flags & (GF_ISOM_TRUN_DURATION|GF_ISOM_TRUN_SIZE|GF_ISOM_TRUN_CTS_OFFSET|GF_ISOM_TRUN_FLAGS)) {
			full_dump = GF_TRUE;
		}
	}

	if (full_dump) {
		for (i=0; i<p->nb_samples; i++) {
			GF_TrunEntry *ent = &p->samples[i];
			
			gf_fprintf(trace, "<TrackRunEntry");

#ifdef GF_ENABLE_CTRN
			if (p->use_ctrn) {
				if ((i==1) && (p->ctrn_flags&GF_ISOM_CTRN_FIRST_SAMPLE) ) {
					if (p->ctrn_first_dur)
						gf_fprintf(trace, " Duration=\"%u\"", ent->Duration);
					if (p->ctrn_first_size)
						gf_fprintf(trace, " Size=\"%u\"", ent->size);
					if (p->ctrn_first_ctts) {
						if (p->version == 0)
							gf_fprintf(trace, " CTSOffset=\"%u\"", (u32) ent->CTS_Offset);
						else
							gf_fprintf(trace, " CTSOffset=\"%d\"", ent->CTS_Offset);
					}
					if (p->ctrn_first_sample_flags)
						frag_dump_sample_flags(trace, ent->flags, p->ctrn_first_sample_flags);
				} else {
					if (p->ctrn_dur)
						gf_fprintf(trace, " Duration=\"%u\"", ent->Duration);
					if (p->ctrn_size)
						gf_fprintf(trace, " Size=\"%u\"", ent->size);
					if (p->ctrn_ctts) {
						if (p->version == 0)
							gf_fprintf(trace, " CTSOffset=\"%u\"", (u32) ent->CTS_Offset);
						else
							gf_fprintf(trace, " CTSOffset=\"%d\"", ent->CTS_Offset);
					}
					if (p->ctrn_sample_flags)
						frag_dump_sample_flags(trace, ent->flags, p->ctrn_sample_flags);
				}
			} else
#endif
			{

				if (p->flags & GF_ISOM_TRUN_DURATION)
					gf_fprintf(trace, " Duration=\"%u\"", ent->Duration);
				if (p->flags & GF_ISOM_TRUN_SIZE)
					gf_fprintf(trace, " Size=\"%u\"", ent->size);
				if (p->flags & GF_ISOM_TRUN_CTS_OFFSET)
				{
					if (p->version == 0)
						gf_fprintf(trace, " CTSOffset=\"%u\"", (u32) ent->CTS_Offset);
					else
						gf_fprintf(trace, " CTSOffset=\"%d\"", ent->CTS_Offset);
				}

				if (p->flags & GF_ISOM_TRUN_FLAGS) {
					frag_dump_sample_flags(trace, ent->flags, 3);
				}
			}
			gf_fprintf(trace, "/>\n");
		}
	} else if (p->size) {
		gf_fprintf(trace, "<!-- all default values used -->\n");
	} else {
		gf_fprintf(trace, "<TrackRunEntry Duration=\"\" Size=\"\" CTSOffset=\"\"");
		frag_dump_sample_flags(trace, 0, 3);
		gf_fprintf(trace, "/>\n");
	}

#ifdef GF_ENABLE_CTRN
	gf_isom_box_dump_done(p->use_ctrn ? "CompactTrackRunBox" : "TrackRunBox", a, trace);
#else
	gf_isom_box_dump_done("TrackRunBox", a, trace);
#endif
	return GF_OK;
}

#endif

#ifndef GPAC_DISABLE_ISOM_HINTING

GF_Err DTE_Dump(GF_List *dte, FILE * trace)
{
	u32 i, count;

	count = gf_list_count(dte);
	for (i=0; i<count; i++) {
		GF_GenericDTE *p;
		GF_ImmediateDTE *i_p;
		GF_SampleDTE *s_p;
		GF_StreamDescDTE *sd_p;
		p = (GF_GenericDTE *)gf_list_get(dte, i);
		switch (p->source) {
		case 0:
			gf_fprintf(trace, "<EmptyDataEntry/>\n");
			break;
		case 1:
			i_p = (GF_ImmediateDTE *) p;
			gf_fprintf(trace, "<ImmediateDataEntry DataSize=\"%d\"/>\n", i_p->dataLength);
			break;
		case 2:
			s_p = (GF_SampleDTE *) p;
			gf_fprintf(trace, "<SampleDataEntry DataSize=\"%d\" SampleOffset=\"%d\" SampleNumber=\"%d\" TrackReference=\"%d\"/>\n",
			        s_p->dataLength, s_p->byteOffset, s_p->sampleNumber, s_p->trackRefIndex);
			break;
		case 3:
			sd_p = (GF_StreamDescDTE *) p;
			gf_fprintf(trace, "<SampleDescriptionEntry DataSize=\"%d\" DescriptionOffset=\"%d\" StreamDescriptionindex=\"%d\" TrackReference=\"%d\"/>\n",
			        sd_p->dataLength, sd_p->byteOffset, sd_p->streamDescIndex, sd_p->trackRefIndex);
			break;
		default:
			gf_fprintf(trace, "<UnknownTableEntry/>\n");
			break;
		}
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_dump_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE * trace)
{
	GF_ISOSample *tmp;
	GF_HintSampleEntryBox *entry;
	u32 descIndex, count, count2, i;
	GF_Err e=GF_OK;
	GF_BitStream *bs;
	GF_HintSample *s;
	GF_TrackBox *trak;
	GF_RTPPacket *pck;
	char *szName;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	tmp = gf_isom_get_sample(the_file, trackNumber, SampleNum, &descIndex);
	if (!tmp) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, descIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) {
		gf_isom_sample_del(&tmp);
		return e;
	}

	//check we can read the sample
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		szName = "RTP";
		break;
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		szName = "RCTP";
		break;
	case GF_ISOM_BOX_TYPE_FDP_STSD:
		szName = "FDP";
		break;
	default:
		gf_isom_sample_del(&tmp);
		return GF_NOT_SUPPORTED;
	}

	bs = gf_bs_new(tmp->data, tmp->dataLength, GF_BITSTREAM_READ);
	s = gf_isom_hint_sample_new(entry->type);
	s->trackID = trak->Header->trackID;
	s->sampleNumber = SampleNum;

	gf_isom_hint_sample_read(s, bs, tmp->dataLength);
	gf_bs_del(bs);

	count = gf_list_count(s->packetTable);

	gf_fprintf(trace, "<%sHintSample SampleNumber=\"%d\" DecodingTime=\""LLD"\" RandomAccessPoint=\"%d\" PacketCount=\"%u\" reserved=\"%u\">\n", szName, SampleNum, tmp->DTS, tmp->IsRAP, s->packetCount, s->reserved);

	if (s->hint_subtype==GF_ISOM_BOX_TYPE_FDP_STSD) {
		e = gf_isom_box_dump((GF_Box*) s, trace);
		goto err_exit;
	}

	if (s->packetCount != count) {
		gf_fprintf(trace, "<!-- WARNING: Broken %s hint sample, %d entries indicated but only %d parsed -->\n", szName, s->packetCount, count);
	}


	for (i=0; i<count; i++) {
		pck = (GF_RTPPacket *)gf_list_get(s->packetTable, i);

		if (pck->hint_subtype==GF_ISOM_BOX_TYPE_RTCP_STSD) {
			GF_RTCPPacket *rtcp_pck = (GF_RTCPPacket *) pck;
			gf_fprintf(trace, "<RTCPHintPacket PacketNumber=\"%d\" V=\"%d\" P=\"%d\" Count=\"%d\" PayloadType=\"%d\" ",
		        i+1,  rtcp_pck->Version, rtcp_pck->Padding, rtcp_pck->Count, rtcp_pck->PayloadType);

			if (rtcp_pck->data) dump_data_attribute(trace, "payload", (char*)rtcp_pck->data, rtcp_pck->length);
			gf_fprintf(trace, ">\n");
			gf_fprintf(trace, "</RTCPHintPacket>\n");

		} else {
			gf_fprintf(trace, "<RTPHintPacket PacketNumber=\"%d\" P=\"%d\" X=\"%d\" M=\"%d\" PayloadType=\"%d\"",
		        i+1,  pck->P_bit, pck->X_bit, pck->M_bit, pck->payloadType);

			gf_fprintf(trace, " SequenceNumber=\"%d\" RepeatedPacket=\"%d\" DropablePacket=\"%d\" RelativeTransmissionTime=\"%d\" FullPacketSize=\"%d\">\n",
		        pck->SequenceNumber, pck->R_bit, pck->B_bit, pck->relativeTransTime, gf_isom_hint_rtp_length(pck));


			//TLV is made of Boxes
			count2 = gf_list_count(pck->TLV);
			if (count2) {
				gf_fprintf(trace, "<PrivateExtensionTable EntryCount=\"%d\">\n", count2);
				gf_fprintf(trace, "</PrivateExtensionTable>\n");
			}
			//DTE is made of NON boxes
			count2 = gf_list_count(pck->DataTable);
			if (count2) {
				gf_fprintf(trace, "<PacketDataTable EntryCount=\"%d\">\n", count2);
				DTE_Dump(pck->DataTable, trace);
				gf_fprintf(trace, "</PacketDataTable>\n");
			}
			gf_fprintf(trace, "</RTPHintPacket>\n");
		}
	}

err_exit:
	gf_fprintf(trace, "</%sHintSample>\n", szName);
	gf_isom_sample_del(&tmp);
	gf_isom_hint_sample_del(s);
	return e;
}

#endif /*GPAC_DISABLE_ISOM_HINTING*/

static void tx3g_dump_box_nobox(FILE * trace, GF_BoxRecord *rec)
{
	gf_fprintf(trace, "<TextBox top=\"%d\" left=\"%d\" bottom=\"%d\" right=\"%d\"/>\n", rec->top, rec->left, rec->bottom, rec->right);
}

static void tx3g_print_char_offsets(FILE * trace, u32 start, u32 end, u32 *shift_offset, u32 so_count)
{
	u32 i;
	if (shift_offset) {
		for (i=0; i<so_count; i++) {
			if (start>shift_offset[i]) {
				start --;
				break;
			}
		}
		for (i=0; i<so_count; i++) {
			if (end>shift_offset[i]) {
				end --;
				break;
			}
		}
	}
	if (start || end) gf_fprintf(trace, "fromChar=\"%d\" toChar=\"%d\" ", start, end);
}

static void tx3g_dump_style_nobox(FILE * trace, GF_StyleRecord *rec, u32 *shift_offset, u32 so_count)
{
	gf_fprintf(trace, "<Style ");
	if (rec->startCharOffset || rec->endCharOffset)
		tx3g_print_char_offsets(trace, rec->startCharOffset, rec->endCharOffset, shift_offset, so_count);

	gf_fprintf(trace, "styles=\"");
	if (!rec->style_flags) {
		gf_fprintf(trace, "Normal");
	} else {
		if (rec->style_flags & 1) gf_fprintf(trace, "Bold ");
		if (rec->style_flags & 2) gf_fprintf(trace, "Italic ");
		if (rec->style_flags & 4) gf_fprintf(trace, "Underlined ");
	}
	gf_fprintf(trace, "\" fontID=\"%d\" fontSize=\"%d\" ", rec->fontID, rec->font_size);
	tx3g_dump_rgba8(trace, "color", rec->text_color);
	gf_fprintf(trace, "/>\n");
}

char *tx3g_format_time(u64 ts, u32 timescale, char *szDur, Bool is_srt)
{
	u32 h, m, s, ms;
	ts = (u32) (ts*1000 / timescale);
	h = (u32) (ts / 3600000);
	m = (u32) (ts/ 60000) - h*60;
	s = (u32) (ts/1000) - h*3600 - m*60;
	ms = (u32) (ts) - h*3600000 - m*60000 - s*1000;
	if (is_srt) {
		sprintf(szDur, "%02d:%02d:%02d,%03d", h, m, s, ms);
	} else {
		sprintf(szDur, "%02d:%02d:%02d.%03d", h, m, s, ms);
	}
	return szDur;
}

void dump_ttxt_header(FILE *dump, GF_Tx3gSampleEntryBox *txt_e, u32 def_width, u32 def_height)
{
	gf_fprintf(dump, "<TextSampleDescription horizontalJustification=\"");
	switch (txt_e->horizontal_justification) {
	case 1:
		gf_fprintf(dump, "center");
		break;
	case -1:
		gf_fprintf(dump, "right");
		break;
	default:
		gf_fprintf(dump, "left");
		break;
	}
	gf_fprintf(dump, "\" verticalJustification=\"");
	switch (txt_e->vertical_justification) {
	case 1:
		gf_fprintf(dump, "center");
		break;
	case -1:
		gf_fprintf(dump, "bottom");
		break;
	default:
		gf_fprintf(dump, "top");
		break;
	}
	gf_fprintf(dump, "\" ");
	tx3g_dump_rgba8(dump, "backColor", txt_e->back_color);
	gf_fprintf(dump, " verticalText=\"%s\"", (txt_e->displayFlags & GF_TXT_VERTICAL) ? "yes" : "no");
	gf_fprintf(dump, " fillTextRegion=\"%s\"", (txt_e->displayFlags & GF_TXT_FILL_REGION) ? "yes" : "no");
	gf_fprintf(dump, " continuousKaraoke=\"%s\"", (txt_e->displayFlags & GF_TXT_KARAOKE) ? "yes" : "no");
	Bool has_scroll = GF_FALSE;
	if (txt_e->displayFlags & GF_TXT_SCROLL_IN) {
		has_scroll = GF_TRUE;
		if (txt_e->displayFlags & GF_TXT_SCROLL_OUT) gf_fprintf(dump, " scroll=\"InOut\"");
		else gf_fprintf(dump, " scroll=\"In\"");
	} else if (txt_e->displayFlags & GF_TXT_SCROLL_OUT) {
		has_scroll = GF_TRUE;
		gf_fprintf(dump, " scroll=\"Out\"");
	} else {
		gf_fprintf(dump, " scroll=\"None\"");
	}
	if (has_scroll) {
		u32 mode = (txt_e->displayFlags & GF_TXT_SCROLL_DIRECTION)>>7;
		switch (mode) {
		case GF_TXT_SCROLL_CREDITS:
			gf_fprintf(dump, " scrollMode=\"Credits\"");
			break;
		case GF_TXT_SCROLL_MARQUEE:
			gf_fprintf(dump, " scrollMode=\"Marquee\"");
			break;
		case GF_TXT_SCROLL_DOWN:
			gf_fprintf(dump, " scrollMode=\"Down\"");
			break;
		case GF_TXT_SCROLL_RIGHT:
			gf_fprintf(dump, " scrollMode=\"Right\"");
			break;
		default:
			gf_fprintf(dump, " scrollMode=\"Unknown\"");
			break;
		}
	}
	gf_fprintf(dump, ">\n");
	gf_fprintf(dump, "<FontTable>\n");
	if (txt_e->font_table) {
		u32 j;
		for (j=0; j<txt_e->font_table->entry_count; j++) {
			gf_fprintf(dump, "<FontTableEntry fontName=\"%s\" fontID=\"%d\"/>\n", txt_e->font_table->fonts[j].fontName, txt_e->font_table->fonts[j].fontID);

		}
	}
	gf_fprintf(dump, "</FontTable>\n");
	if ((txt_e->default_box.bottom == txt_e->default_box.top) || (txt_e->default_box.right == txt_e->default_box.left)) {
		txt_e->default_box.top = txt_e->default_box.left = 0;
		txt_e->default_box.right = def_width / 65536;
		txt_e->default_box.bottom = def_height / 65536;
	}
	tx3g_dump_box_nobox(dump, &txt_e->default_box);
	tx3g_dump_style_nobox(dump, &txt_e->default_style, NULL, 0);
	gf_fprintf(dump, "</TextSampleDescription>\n");
}

void dump_ttxt_sample(FILE *dump, GF_TextSample *s_txt, u64 ts, u32 timescale, u32 di, Bool box_dump)
{
	GF_Box *a;
	u32 len, j;
	u32 shift_offset[20];
	u32 so_count;
	char szDur[100];
	gf_fprintf(dump, "<TextSample sampleTime=\"%s\" sampleDescriptionIndex=\"%d\"", tx3g_format_time(ts, timescale, szDur, GF_FALSE), di);

	if (!box_dump) {
		if (s_txt->highlight_color) {
			gf_fprintf(dump, " ");
			tx3g_dump_rgba8(dump, "highlightColor", s_txt->highlight_color->hil_color);
		}
		if (s_txt->scroll_delay) {
			Double delay = s_txt->scroll_delay->scroll_delay;
			delay /= timescale;
			gf_fprintf(dump, " scrollDelay=\"%g\"", delay);
		}
		if (s_txt->wrap) gf_fprintf(dump, " wrap=\"%s\"", (s_txt->wrap->wrap_flag==0x01) ? "Automatic" : "None");
	}

	so_count = 0;

	gf_fprintf(dump, " xml:space=\"preserve\">");
	if (s_txt->len) {
		unsigned short utf16Line[10000];
		/*UTF16*/
		if ((s_txt->len>2) && ((unsigned char) s_txt->text[0] == (unsigned char) 0xFE) && ((unsigned char) s_txt->text[1] == (unsigned char) 0xFF)) {
			/*copy 2 more chars because the lib always add 2 '0' at the end for UTF16 end of string*/
			memcpy((char *) utf16Line, s_txt->text+2, sizeof(char) * (s_txt->len));
			len = gf_utf8_wcslen((const u16*)utf16Line);
		} else {
			char *str;
			str = s_txt->text;
			len = gf_utf8_mbstowcs((u16*)utf16Line, 10000, (const char **) &str);
		}
		if (len != GF_UTF8_FAIL) {
			utf16Line[len] = 0;
			for (j=0; j<len; j++) {
				if ((utf16Line[j]=='\n') || (utf16Line[j]=='\r') || (utf16Line[j]==0x85) || (utf16Line[j]==0x2028) || (utf16Line[j]==0x2029) ) {
					gf_fprintf(dump, "\n");
					if ((utf16Line[j]=='\r') && (utf16Line[j+1]=='\n')) {
						shift_offset[so_count] = j;
						so_count++;
						j++;
					}
				}
				else {
					switch (utf16Line[j]) {
					case '\'':
						gf_fprintf(dump, "&apos;");
						break;
					case '\"':
						gf_fprintf(dump, "&quot;");
						break;
					case '&':
						gf_fprintf(dump, "&amp;");
						break;
					case '>':
						gf_fprintf(dump, "&gt;");
						break;
					case '<':
						gf_fprintf(dump, "&lt;");
						break;
					default:
						if (utf16Line[j] < 128) {
							gf_fprintf(dump, "%c", (u8) utf16Line[j]);
						} else {
							gf_fprintf(dump, "&#%d;", utf16Line[j]);
						}
						break;
					}
				}
			}
		}
	}

	if (box_dump) {

		if (s_txt->highlight_color)
			gf_isom_box_dump((GF_Box*) s_txt->highlight_color, dump);
		if (s_txt->scroll_delay)
			gf_isom_box_dump((GF_Box*) s_txt->scroll_delay, dump);
		if (s_txt->wrap)
			gf_isom_box_dump((GF_Box*) s_txt->wrap, dump);
		if (s_txt->box)
			gf_isom_box_dump((GF_Box*) s_txt->box, dump);
		if (s_txt->styles)
			gf_isom_box_dump((GF_Box*) s_txt->styles, dump);
	} else {

		if (s_txt->box) tx3g_dump_box_nobox(dump, &s_txt->box->box);
		if (s_txt->styles) {
			for (j=0; j<s_txt->styles->entry_count; j++) {
				tx3g_dump_style_nobox(dump, &s_txt->styles->styles[j], shift_offset, so_count);
			}
		}
	}
	j=0;
	while ((a = (GF_Box *)gf_list_enum(s_txt->others, &j))) {
		if (box_dump) {
			gf_isom_box_dump((GF_Box*) a, dump);
			continue;
		}

		switch (a->type) {
		case GF_ISOM_BOX_TYPE_HLIT:
			gf_fprintf(dump, "<Highlight ");
			tx3g_print_char_offsets(dump, ((GF_TextHighlightBox *)a)->startcharoffset, ((GF_TextHighlightBox *)a)->endcharoffset, shift_offset, so_count);
			gf_fprintf(dump, "/>\n");
			break;
		case GF_ISOM_BOX_TYPE_HREF:
		{
			GF_TextHyperTextBox *ht = (GF_TextHyperTextBox *)a;
			gf_fprintf(dump, "<HyperLink ");
			tx3g_print_char_offsets(dump, ht->startcharoffset, ht->endcharoffset, shift_offset, so_count);
			gf_fprintf(dump, "URL=\"%s\" URLToolTip=\"%s\"/>\n", ht->URL ? ht->URL : "", ht->URL_hint ? ht->URL_hint : "");
		}
		break;
		case GF_ISOM_BOX_TYPE_BLNK:
			gf_fprintf(dump, "<Blinking ");
			tx3g_print_char_offsets(dump, ((GF_TextBlinkBox *)a)->startcharoffset, ((GF_TextBlinkBox *)a)->endcharoffset, shift_offset, so_count);
			gf_fprintf(dump, "/>\n");
			break;
		case GF_ISOM_BOX_TYPE_KROK:
		{
			u32 k;
			Double t;
			GF_TextKaraokeBox *krok = (GF_TextKaraokeBox *)a;
			t = krok->highlight_starttime;
			t /= timescale;
			gf_fprintf(dump, "<Karaoke startTime=\"%g\">\n", t);
			for (k=0; k<krok->nb_entries; k++) {
				t = krok->records[k].highlight_endtime;
				t /= timescale;
				gf_fprintf(dump, "<KaraokeRange ");
				tx3g_print_char_offsets(dump, krok->records[k].start_charoffset, krok->records[k].end_charoffset, shift_offset, so_count);
				gf_fprintf(dump, "endTime=\"%g\"/>\n", t);
			}
			gf_fprintf(dump, "</Karaoke>\n");
		}
			break;
		}
	}

	gf_fprintf(dump, "</TextSample>\n");

}
static GF_Err gf_isom_dump_ttxt_track(GF_ISOFile *the_file, u32 track, FILE *dump, GF_TextDumpType dump_type)
{
	u32 i, count, di, nb_descs;
	u64 last_DTS;
	char szDur[100];
	GF_Tx3gSampleEntryBox *txt_e;
	Bool box_dump = (dump_type==GF_TEXTDUMPTYPE_TTXT_BOXES) ? GF_TRUE : GF_FALSE;
	Bool skip_empty = (dump_type==GF_TEXTDUMPTYPE_TTXT_CHAP) ? GF_TRUE : GF_FALSE;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	txt_e = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, 0);
	switch (txt_e->type) {
	case GF_ISOM_BOX_TYPE_TX3G:
	case GF_ISOM_BOX_TYPE_TEXT:
		break;
	case GF_ISOM_BOX_TYPE_STPP:
	case GF_ISOM_BOX_TYPE_SBTT:
	default:
		return GF_BAD_PARAM;
	}

	if (box_dump) {
		gf_fprintf(dump, "<TextTrack trackID=\"%d\" version=\"1.1\">\n", gf_isom_get_track_id(the_file, track) );
	} else {
		gf_fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
		gf_fprintf(dump, "<!-- GPAC 3GPP Text Stream -->\n");

		gf_fprintf(dump, "<TextStream version=\"1.1\">\n");
	}
	gf_fprintf(dump, "<TextStreamHeader width=\"%d\" height=\"%d\" layer=\"%d\" translation_x=\"%d\" translation_y=\"%d\">\n", trak->Header->width >> 16 , trak->Header->height >> 16, trak->Header->layer, trak->Header->matrix[6] >> 16, trak->Header->matrix[7] >> 16);

	nb_descs = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	for (i=0; i<nb_descs; i++) {
		txt_e = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (!txt_e) break;

		if (box_dump) {
			gf_isom_box_dump((GF_Box*) txt_e, dump);
		} else if  (txt_e->type==GF_ISOM_BOX_TYPE_TX3G) {
			dump_ttxt_header(dump, txt_e, trak->Header->width, trak->Header->height);
		} else {
			GF_TextSampleEntryBox *text = (GF_TextSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
			gf_fprintf(dump, "<TextSampleDescription horizontalJustification=\"");
			switch (text->textJustification) {
			case 1:
				gf_fprintf(dump, "center");
				break;
			case -1:
				gf_fprintf(dump, "right");
				break;
			default:
				gf_fprintf(dump, "left");
				break;
			}
			gf_fprintf(dump, "\"");

			tx3g_dump_rgb16(dump, " backColor", text->background_color);

			if ((text->default_box.bottom == text->default_box.top) || (text->default_box.right == text->default_box.left)) {
				text->default_box.top = text->default_box.left = 0;
				text->default_box.right = trak->Header->width / 65536;
				text->default_box.bottom = trak->Header->height / 65536;
			}

			if (text->displayFlags & GF_TXT_SCROLL_IN) {
				if (text->displayFlags & GF_TXT_SCROLL_OUT) gf_fprintf(dump, " scroll=\"InOut\"");
				else gf_fprintf(dump, " scroll=\"In\"");
			} else if (text->displayFlags & GF_TXT_SCROLL_OUT) {
				gf_fprintf(dump, " scroll=\"Out\"");
			} else {
				gf_fprintf(dump, " scroll=\"None\"");
			}
			gf_fprintf(dump, ">\n");

			tx3g_dump_box_nobox(dump, &text->default_box);
			gf_fprintf(dump, "</TextSampleDescription>\n");
		}
	}
	gf_fprintf(dump, "</TextStreamHeader>\n");

	last_DTS = 0;
	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_BitStream *bs;
		GF_TextSample *s_txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		s_txt = gf_isom_parse_text_sample(bs);
		gf_bs_del(bs);

		if (skip_empty && (s_txt->len<1)) {
			gf_isom_sample_del(&s);
			gf_isom_delete_text_sample(s_txt);
			gf_set_progress("TTXT Extract", i, count);
			continue;
		}

		dump_ttxt_sample(dump, s_txt, s->DTS, trak->Media->mediaHeader->timeScale, di, box_dump);
		if (!s_txt->len) {
			last_DTS = (u32) trak->Media->mediaHeader->duration;
		} else {
			last_DTS = s->DTS;
		}

		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(s_txt);
		gf_set_progress("TTXT Extract", i, count);
	}
	if (!skip_empty && (last_DTS < trak->Media->mediaHeader->duration)) {
		gf_fprintf(dump, "<TextSample sampleTime=\"%s\" text=\"\" />\n", tx3g_format_time(trak->Media->mediaHeader->duration, trak->Media->mediaHeader->timeScale, szDur, GF_FALSE));
	}

	if (box_dump) {
		gf_fprintf(dump, "</TextTrack>\n");
	} else {
		gf_fprintf(dump, "</TextStream>\n");
	}
	if (count) gf_set_progress("TTXT Extract", count, count);
	return GF_OK;
}

#include <gpac/webvtt.h>

GF_Err dump_ttxt_sample_srt(FILE *dump, GF_TextSample *txt, GF_Tx3gSampleEntryBox *txtd, Bool vtt_dump)
{
	u32 len, j, k;
	if (!txt || !txt->len) {
		gf_fprintf(dump, "\n");
	} else {
		u32 styles, char_num, new_styles, color, new_color;
		u16 utf16Line[10000];

		/*UTF16*/
		if ((txt->len>2) && ((unsigned char) txt->text[0] == (unsigned char) 0xFE) && ((unsigned char) txt->text[1] == (unsigned char) 0xFF)) {
			memcpy(utf16Line, txt->text+2, sizeof(char)*txt->len);
			( ((char *)utf16Line)[txt->len] ) = 0;
			len = txt->len;
		} else {
			u8 *str = (u8 *) (txt->text);
			len = gf_utf8_mbstowcs(utf16Line, 10000, (const char **) &str);
			if (len == GF_UTF8_FAIL) return GF_NON_COMPLIANT_BITSTREAM;
			utf16Line[len] = 0;
		}
		char_num = 0;
		styles = 0;
		new_styles = txtd->default_style.style_flags;
		color = new_color = txtd->default_style.text_color;

		for (j=0; j<len; j++) {
			Bool is_new_line;

			if (txt->styles) {
				new_styles = txtd->default_style.style_flags;
				new_color = txtd->default_style.text_color;
				for (k=0; k<txt->styles->entry_count; k++) {
					if (txt->styles->styles[k].startCharOffset>char_num) continue;
					if (txt->styles->styles[k].endCharOffset<char_num+1) continue;

					if (txt->styles->styles[k].style_flags & (GF_TXT_STYLE_ITALIC | GF_TXT_STYLE_BOLD | GF_TXT_STYLE_UNDERLINED | GF_TXT_STYLE_STRIKETHROUGH)) {
						new_styles = txt->styles->styles[k].style_flags;
						new_color = txt->styles->styles[k].text_color;
						break;
					}
				}
			}
			if (new_styles != styles) {
				if ((new_styles & GF_TXT_STYLE_BOLD) && !(styles & GF_TXT_STYLE_BOLD)) gf_fprintf(dump, "<b>");
				if ((new_styles & GF_TXT_STYLE_ITALIC) && !(styles & GF_TXT_STYLE_ITALIC)) gf_fprintf(dump, "<i>");
				if ((new_styles & GF_TXT_STYLE_UNDERLINED) && !(styles & GF_TXT_STYLE_UNDERLINED)) gf_fprintf(dump, "<u>");
				if ((new_styles & GF_TXT_STYLE_STRIKETHROUGH) && !(styles & GF_TXT_STYLE_STRIKETHROUGH)) gf_fprintf(dump, "<strike>");

				if ((styles & GF_TXT_STYLE_STRIKETHROUGH) && !(new_styles & GF_TXT_STYLE_STRIKETHROUGH)) gf_fprintf(dump, "</strike>");
				if ((styles & GF_TXT_STYLE_UNDERLINED) && !(new_styles & GF_TXT_STYLE_UNDERLINED)) gf_fprintf(dump, "</u>");
				if ((styles & GF_TXT_STYLE_ITALIC) && !(new_styles & GF_TXT_STYLE_ITALIC)) gf_fprintf(dump, "</i>");
				if ((styles & GF_TXT_STYLE_BOLD) && !(new_styles & GF_TXT_STYLE_BOLD)) gf_fprintf(dump, "</b>");

				styles = new_styles;
			}
			if (!vtt_dump && (new_color != color)) {
				if (new_color ==txtd->default_style.text_color) {
					gf_fprintf(dump, "</font>");
				} else {
					gf_fprintf(dump, "<font color=\"%s\">", gf_color_get_name(new_color) );
				}
				color = new_color;
			}

			/*not sure if styles must be reseted at line breaks in srt...*/
			is_new_line = GF_FALSE;
			if ((utf16Line[j]=='\n') || (utf16Line[j]=='\r') ) {
				if ((utf16Line[j]=='\r') && (utf16Line[j+1]=='\n')) j++;
				gf_fprintf(dump, "\n");
				is_new_line = GF_TRUE;
			}

			if (!is_new_line) {
				u32 sl;
				char szChar[30];
				s16 swT[2], *swz;
				swT[0] = utf16Line[j];
				swT[1] = 0;
				swz= (s16 *)swT;
				sl = gf_utf8_wcstombs(szChar, 30, (const unsigned short **) &swz);
				if (sl == GF_UTF8_FAIL) sl=0;
				szChar[sl]=0;
				gf_fprintf(dump, "%s", szChar);
			}
			char_num++;
		}
		new_styles = 0;
		if (new_styles != styles) {
			if (styles & GF_TXT_STYLE_STRIKETHROUGH) gf_fprintf(dump, "</strike>");
			if (styles & GF_TXT_STYLE_UNDERLINED) gf_fprintf(dump, "</u>");
			if (styles & GF_TXT_STYLE_ITALIC) gf_fprintf(dump, "</i>");
			if (styles & GF_TXT_STYLE_BOLD) gf_fprintf(dump, "</b>");

//				styles = 0;
		}

		if (color != txtd->default_style.text_color) {
			gf_fprintf(dump, "</font>");
//				color = txtd->default_style.text_color;
		}
		gf_fprintf(dump, "\n");
	}
	return GF_OK;
}

static void vttmx_timestamp_dump(GF_BitStream *bs, GF_WebVTTTimestamp *ts, Bool dump_hour, Bool write_srt)
{
	char szTS[200];
	szTS[0] = 0;
	if (dump_hour) {
		sprintf(szTS, "%02u:", ts->hour);
		gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
	}
	sprintf(szTS, "%02u:%02u%c%03u", ts->min, ts->sec, write_srt ? ',' : '.', ts->ms);
	gf_bs_write_data(bs, szTS, (u32) strlen(szTS) );
}

void webvtt_write_cue_bs(GF_BitStream *bs, GF_WebVTTCue *cue, Bool write_srt)
{
	Bool write_hour = GF_FALSE;
	if (!cue) return;
	if (!write_srt && cue->pre_text) {
		gf_bs_write_data(bs, cue->pre_text, (u32) strlen(cue->pre_text));
		gf_bs_write_data(bs, "\n\n", 2);
	}
	if (!write_srt && cue->id) {
		u32 len = (u32) strlen(cue->id) ;
		gf_bs_write_data(bs, cue->id, len);
		if (len && (cue->id[len-1]!='\n'))
			gf_bs_write_data(bs, "\n", 1);
	}

	if (gf_opts_get_bool("core", "webvtt-hours")) write_hour = GF_TRUE;
	else if (cue->start.hour || cue->end.hour) write_hour = GF_TRUE;
	else if (write_srt) write_hour = GF_TRUE;

	vttmx_timestamp_dump(bs, &cue->start, write_hour, write_srt);
	gf_bs_write_data(bs, " --> ", 5);
	vttmx_timestamp_dump(bs, &cue->end, write_hour, write_srt);

	if (!write_srt && cue->settings) {
		gf_bs_write_data(bs, " ", 1);
		gf_bs_write_data(bs, cue->settings, (u32) strlen(cue->settings));
	}
	gf_bs_write_data(bs, "\n", 1);
	if (cue->text)
		gf_bs_write_data(bs, cue->text, (u32) strlen(cue->text));

	if (!write_srt)
		gf_bs_write_data(bs, "\n\n", 2);
	else
		gf_bs_write_data(bs, "\n", 1);

	if (!write_srt && cue->post_text) {
		gf_bs_write_data(bs, cue->post_text, (u32) strlen(cue->post_text));
		gf_bs_write_data(bs, "\n\n", 2);
	}
}

static GF_Err gf_isom_dump_srt_track(GF_ISOFile *the_file, u32 track, FILE *dump)
{
	u32 i, j, count, di, ts, cur_frame;
	u64 start, end;
	GF_Tx3gSampleEntryBox *txtd;
	char szDur[100];
	Bool is_wvtt = GF_FALSE;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	u32 subtype = gf_isom_get_media_subtype(the_file, track, 1);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	ts = trak->Media->mediaHeader->timeScale;
	cur_frame = 0;
	end = 0;

	switch (subtype) {
	case GF_ISOM_SUBTYPE_TX3G:
	case GF_ISOM_SUBTYPE_TEXT:
	case GF_ISOM_SUBTYPE_STXT:
		break;
	case GF_ISOM_SUBTYPE_WVTT:
		is_wvtt = GF_TRUE;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	s64 ts_offset=0;
	gf_isom_get_edit_list_type(the_file, track, &ts_offset);

	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_BitStream *bs;
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		start = s->DTS+ts_offset;
		if (s->dataLength==2) {
			gf_isom_sample_del(&s);
			continue;
		}
		if (i+1<count) {
			GF_ISOSample *next = gf_isom_get_sample_info(the_file, track, i+2, NULL, NULL);
			if (next) {
				end = next->DTS+ts_offset;
				gf_isom_sample_del(&next);
			}
		} else {
			end = gf_isom_get_media_duration(the_file, track) ;
		}
		if (!is_wvtt) {
			cur_frame++;
			gf_fprintf(dump, "%d\n", cur_frame);
			tx3g_format_time(start, ts, szDur, GF_TRUE);
			gf_fprintf(dump, "%s --> ", szDur);
			tx3g_format_time(end, ts, szDur, GF_TRUE);
			gf_fprintf(dump, "%s\n", szDur);
		}


		if (is_wvtt) {
			u64 start_ts, end_ts;
			GF_List *cues;
			u32 nb_cues;
			u8 *data;
			u32 data_len;

			start_ts = s->DTS * 1000;
			start_ts /= trak->Media->mediaHeader->timeScale;
			end_ts = end * 1000;
			end_ts /= trak->Media->mediaHeader->timeScale;
			cues = gf_webvtt_parse_cues_from_data(s->data, s->dataLength, start_ts, end_ts);
			gf_isom_sample_del(&s);
			nb_cues = gf_list_count(cues);

			if (!nb_cues) {
				gf_list_del(cues);
				continue;
			}

			cur_frame++;
			gf_fprintf(dump, "%d\n", cur_frame);

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			for (j = 0; j < gf_list_count(cues); j++) {
				GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, j);
				webvtt_write_cue_bs(bs, cue, GF_TRUE);
				gf_webvtt_cue_del(cue);
			}
			gf_list_del(cues);
			gf_bs_write_u16(bs, 0);
			gf_bs_get_content(bs, &data, &data_len);
			gf_bs_del(bs);

			if (data) {
				gf_fprintf(dump, "%s\n", data);
				gf_free(data);
			} else {
				gf_fprintf(dump, "\n");
			}
			continue;
		} else if (subtype == GF_ISOM_SUBTYPE_STXT) {
			if (s->dataLength)
				gf_fprintf(dump, "%s\n", s->data);
			gf_isom_sample_del(&s);
			continue;
		}
		else if ((subtype!=GF_ISOM_SUBTYPE_TX3G) && (subtype!=GF_ISOM_SUBTYPE_TEXT)) {
			gf_fprintf(dump, "unknown\n");
			gf_isom_sample_del(&s);
			continue;
		}
		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_text_sample(bs);
		gf_bs_del(bs);

		txtd = (GF_Tx3gSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, di-1);

		GF_Err e = dump_ttxt_sample_srt(dump, txt, txtd, GF_FALSE);

		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
		gf_fprintf(dump, "\n");
		gf_set_progress("SRT Extract", i, count);
		if (e) return e;
	}
	if (count) gf_set_progress("SRT Extract", i, count);
	return GF_OK;
}

static GF_Err gf_isom_dump_svg_track(GF_ISOFile *the_file, u32 track, FILE *dump)
{
	char nhmlFileName[1024];
	FILE *nhmlFile;
	u32 i, count, di, ts, cur_frame;
	u64 start, end;
	GF_BitStream *bs;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	strcpy(nhmlFileName, the_file->fileName);
	strcat(nhmlFileName, ".nhml");
	nhmlFile = gf_fopen(nhmlFileName, "wt");
	gf_fprintf(nhmlFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	gf_fprintf(nhmlFile, "<NHNTStream streamType=\"3\" objectTypeIndication=\"10\" timeScale=\"%d\" baseMediaFile=\"file.svg\" inRootOD=\"yes\">\n", trak->Media->mediaHeader->timeScale);
	gf_fprintf(nhmlFile, "<NHNTSample isRAP=\"yes\" DTS=\"0\" xmlFrom=\"doc.start\" xmlTo=\"text_1.start\"/>\n");

	ts = trak->Media->mediaHeader->timeScale;
	cur_frame = 0;
	end = 0;

	gf_fprintf(dump, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	gf_fprintf(dump, "<svg version=\"1.2\" baseProfile=\"tiny\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"%d\" height=\"%d\" fill=\"black\">\n", trak->Header->width >> 16 , trak->Header->height >> 16);
	gf_fprintf(dump, "<g transform=\"translate(%d, %d)\" text-anchor=\"middle\">\n", (trak->Header->width >> 16)/2 , (trak->Header->height >> 16)/2);

	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		start = s->DTS;
		if (s->dataLength==2) {
			gf_isom_sample_del(&s);
			continue;
		}
		if (i+1<count) {
			GF_ISOSample *next = gf_isom_get_sample_info(the_file, track, i+2, NULL, NULL);
			if (next) {
				end = next->DTS;
				gf_isom_sample_del(&next);
			}
		}

		cur_frame++;
		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_text_sample(bs);
		gf_bs_del(bs);

		if (!txt->len) continue;

		gf_fprintf(dump, " <text id=\"text_%d\" display=\"none\">%s\n", cur_frame, txt->text);
		gf_fprintf(dump, "  <set attributeName=\"display\" to=\"inline\" begin=\"%g\" end=\"%g\"/>\n", ((s64)start*1.0)/ts, ((s64)end*1.0)/ts);
		gf_fprintf(dump, "  <discard begin=\"%g\"/>\n", ((s64)end*1.0)/ts);
		gf_fprintf(dump, " </text>\n");
		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
		gf_fprintf(dump, "\n");
		gf_set_progress("SRT Extract", i, count);

		if (i == count - 2) {
			gf_fprintf(nhmlFile, "<NHNTSample isRAP=\"no\" DTS=\"%f\" xmlFrom=\"text_%d.start\" xmlTo=\"doc.end\"/>\n", ((s64)start*1.0), cur_frame);
		} else {
			gf_fprintf(nhmlFile, "<NHNTSample isRAP=\"no\" DTS=\"%f\" xmlFrom=\"text_%d.start\" xmlTo=\"text_%d.start\"/>\n", ((s64)start*1.0), cur_frame, cur_frame+1);
		}

	}
	gf_fprintf(dump, "</g>\n");
	gf_fprintf(dump, "</svg>\n");

	gf_fprintf(nhmlFile, "</NHNTStream>\n");
	gf_fclose(nhmlFile);
	gf_file_delete(nhmlFileName);
	if (count) gf_set_progress("SRT Extract", i, count);
	return GF_OK;
}

static GF_Err gf_isom_dump_ogg_chap(GF_ISOFile *the_file, u32 track, FILE *dump, GF_TextDumpType dump_type)
{
	u32 i, count, di, ts, cur_frame;
	u64 start;
	GF_BitStream *bs;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, track);
	if (!trak) return GF_BAD_PARAM;
	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	ts = trak->Media->mediaHeader->timeScale;
	cur_frame = 0;

	count = gf_isom_get_sample_count(the_file, track);
	for (i=0; i<count; i++) {
		GF_TextSample *txt;
		GF_ISOSample *s = gf_isom_get_sample(the_file, track, i+1, &di);
		if (!s) continue;

		start = s->DTS;
		if (s->dataLength==2) {
			gf_isom_sample_del(&s);
			continue;
		}
		if (i+1<count) {
			GF_ISOSample *next = gf_isom_get_sample_info(the_file, track, i+2, NULL, NULL);
			if (next) {
				gf_isom_sample_del(&next);
			}
		}

		cur_frame++;
		bs = gf_bs_new(s->data, s->dataLength, GF_BITSTREAM_READ);
		txt = gf_isom_parse_text_sample(bs);
		gf_bs_del(bs);

		if (!txt->len) continue;

		if (dump_type==GF_TEXTDUMPTYPE_OGG_CHAP) {
			char szDur[20];
			fprintf(dump, "CHAPTER%02d=%s\n", i+1, format_duration(start, ts, szDur));
			fprintf(dump, "CHAPTER%02dNAME=%s\n", i+1, txt->text);
		} else {
			fprintf(dump, "AddChapterBySecond("LLD",%s)\n", start / ts, txt->text);
		}

		gf_isom_sample_del(&s);
		gf_isom_delete_text_sample(txt);
	}
	return GF_OK;
}
GF_EXPORT
GF_Err gf_isom_text_dump(GF_ISOFile *the_file, u32 track, FILE *dump, GF_TextDumpType dump_type)
{
	switch (dump_type) {
	case GF_TEXTDUMPTYPE_SVG:
		return gf_isom_dump_svg_track(the_file, track, dump);
	case GF_TEXTDUMPTYPE_SRT:
		return gf_isom_dump_srt_track(the_file, track, dump);
	case GF_TEXTDUMPTYPE_TTXT:
	case GF_TEXTDUMPTYPE_TTXT_BOXES:
	case GF_TEXTDUMPTYPE_TTXT_CHAP:
		return gf_isom_dump_ttxt_track(the_file, track, dump, dump_type);
	case GF_TEXTDUMPTYPE_OGG_CHAP:
	case GF_TEXTDUMPTYPE_ZOOM_CHAP:
		return gf_isom_dump_ogg_chap(the_file, track, dump, dump_type);
	default:
		return GF_BAD_PARAM;
	}
}


/* ISMA 1.0 Encryption and Authentication V 1.0  dump */
GF_Err sinf_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ProtectionSchemeInfoBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ProtectionSchemeInfoBox", a, trace);
	return GF_OK;
}

GF_Err frma_box_dump(GF_Box *a, FILE * trace)
{
	GF_OriginalFormatBox *p;
	p = (GF_OriginalFormatBox *)a;
	gf_isom_box_dump_start(a, "OriginalFormatBox", trace);
	gf_fprintf(trace, "data_format=\"%s\">\n", gf_4cc_to_str(p->data_format));
	gf_isom_box_dump_done("OriginalFormatBox", a, trace);
	return GF_OK;
}

GF_Err schm_box_dump(GF_Box *a, FILE * trace)
{
	GF_SchemeTypeBox *p;
	p = (GF_SchemeTypeBox *)a;
	gf_isom_box_dump_start(a, "SchemeTypeBox", trace);
	gf_fprintf(trace, "scheme_type=\"%s\" scheme_version=\"%d\" ", gf_4cc_to_str(p->scheme_type), p->scheme_version);
	if (p->URI) gf_fprintf(trace, "scheme_uri=\"%s\"", p->URI);
	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done("SchemeTypeBox", a, trace);
	return GF_OK;
}

GF_Err schi_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "SchemeInformationBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("SchemeInformationBox", a, trace);
	return GF_OK;
}

GF_Err iKMS_box_dump(GF_Box *a, FILE * trace)
{
	GF_ISMAKMSBox *p;
	p = (GF_ISMAKMSBox *)a;
	gf_isom_box_dump_start(a, "KMSBox", trace);
	gf_fprintf(trace, "kms_URI=\"%s\">\n", p->URI);
	gf_isom_box_dump_done("KMSBox", a, trace);
	return GF_OK;

}

GF_Err iSFM_box_dump(GF_Box *a, FILE * trace)
{
	GF_ISMASampleFormatBox *p;
	const char *name = (a->type==GF_ISOM_BOX_TYPE_ISFM) ? "ISMASampleFormat" : "OMADRMAUFormatBox";
	p = (GF_ISMASampleFormatBox *)a;
	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, "selective_encryption=\"%d\" key_indicator_length=\"%d\" IV_length=\"%d\">\n", p->selective_encryption, p->key_indicator_length, p->IV_length);
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err iSLT_box_dump(GF_Box *a, FILE * trace)
{
	GF_ISMACrypSaltBox *p = (GF_ISMACrypSaltBox *)a;
	gf_isom_box_dump_start(a, "ISMACrypSaltBox", trace);
	gf_fprintf(trace, "salt=\""LLU"\">\n", p->salt);
	gf_isom_box_dump_done("ISMACrypSaltBox", a, trace);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_dump_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, FILE * trace)
{
	u32 i, count;
	GF_SampleEntryBox *entry;
	GF_Err e;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;


	gf_fprintf(trace, "<ISMACrypSampleDescriptions>\n");
	count = gf_isom_get_sample_description_count(the_file, trackNumber);
	for (i=0; i<count; i++) {
		e = Media_GetSampleDesc(trak->Media, i+1, (GF_SampleEntryBox **) &entry, NULL);
		if (e) return e;

		switch (entry->type) {
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCS:
		case GF_ISOM_BOX_TYPE_ENCT:
		case GF_ISOM_BOX_TYPE_ENCF:
		case GF_ISOM_BOX_TYPE_ENCM:
			break;
		default:
			continue;
		}
		gf_isom_box_dump(entry, trace);
	}
	gf_fprintf(trace, "</ISMACrypSampleDescriptions>\n");
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_dump_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE * trace)
{
	GF_ISOSample *samp;
	GF_ISMASample  *isma_samp;
	u32 descIndex;

	samp = gf_isom_get_sample(the_file, trackNumber, SampleNum, &descIndex);
	if (!samp) return GF_BAD_PARAM;

	isma_samp = gf_isom_get_ismacryp_sample(the_file, trackNumber, samp, descIndex);
	if (!isma_samp) {
		gf_isom_sample_del(&samp);
		return GF_NOT_SUPPORTED;
	}

	gf_fprintf(trace, "<ISMACrypSample SampleNumber=\"%d\" DataSize=\"%d\" CompositionTime=\""LLD"\" ", SampleNum, isma_samp->dataLength, (samp->DTS+samp->CTS_Offset) );
	if (samp->CTS_Offset) gf_fprintf(trace, "DecodingTime=\""LLD"\" ", samp->DTS);
	if (gf_isom_has_sync_points(the_file, trackNumber)) gf_fprintf(trace, "RandomAccessPoint=\"%s\" ", samp->IsRAP ? "Yes" : "No");
	gf_fprintf(trace, "IsEncrypted=\"%s\" ", (isma_samp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? "Yes" : "No");
	if (isma_samp->flags & GF_ISOM_ISMA_IS_ENCRYPTED) {
		gf_fprintf(trace, "IV=\""LLD"\" ", isma_samp->IV);
		if (isma_samp->key_indicator) dump_data_attribute(trace, "KeyIndicator", (char*)isma_samp->key_indicator, isma_samp->KI_length);
	}
	gf_fprintf(trace, "/>\n");

	gf_isom_sample_del(&samp);
	gf_isom_ismacryp_delete_sample(isma_samp);
	return GF_OK;
}

/* end of ISMA 1.0 Encryption and Authentication V 1.0 */


/* Apple extensions */

GF_Err ilst_item_box_dump(GF_Box *a, FILE * trace)
{
	u32 val, itype=0;
	Bool no_dump = GF_FALSE;
	Bool unknown = GF_FALSE;
	GF_DataBox *dbox = NULL;
	const char *name = "UnknownBox";
	GF_ListItemBox *itune = (GF_ListItemBox *)a;


	if (itune->type==GF_ISOM_BOX_TYPE_iTunesSpecificInfo) {
		name = "iTunesSpecificBox";
		no_dump = GF_TRUE;
		dbox = itune->data;
	} else if (itune->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
		dbox = (GF_DataBox *) gf_isom_box_find_child(itune->child_boxes, GF_ISOM_BOX_TYPE_DATA);
		unknown = GF_TRUE;
	} else {
		s32 idx = gf_itags_find_by_itag(itune->type);
		if (idx>=0) {
			name = gf_itags_get_name((u32) idx);
			itype = gf_itags_get_type((u32) idx);
			dbox = itune->data;
		}
	}
	gf_isom_box_dump_start(a, name, trace);

	if (!no_dump && dbox) {
		GF_BitStream *bs;
		switch (itune->type) {
		case GF_ISOM_ITUNE_DISK:
		case GF_ISOM_ITUNE_TRACKNUMBER:
			bs = gf_bs_new(dbox->data, dbox->dataSize, GF_BITSTREAM_READ);
			gf_bs_read_int(bs, 16);
			val = gf_bs_read_int(bs, 16);
			if (itune->type==GF_ISOM_ITUNE_DISK) {
				gf_fprintf(trace, " DiskNumber=\"%d\" NbDisks=\"%d\" ", val, gf_bs_read_int(bs, 16) );
			} else {
				gf_fprintf(trace, " TrackNumber=\"%d\" NbTracks=\"%d\" ", val, gf_bs_read_int(bs, 16) );
			}
			gf_bs_del(bs);
			break;
		case GF_ISOM_ITUNE_TEMPO:
			bs = gf_bs_new(dbox->data, dbox->dataSize, GF_BITSTREAM_READ);
			gf_fprintf(trace, " BPM=\"%d\" ", gf_bs_read_int(bs, 16) );
			gf_bs_del(bs);
			break;
		case GF_ISOM_ITUNE_COMPILATION:
			gf_fprintf(trace, " IsCompilation=\"%s\" ", (dbox && dbox->data && dbox->data[0]) ? "yes" : "no");
			break;
		case GF_ISOM_ITUNE_GAPLESS:
			gf_fprintf(trace, " IsGapeless=\"%s\" ", (dbox && dbox->data && itune->data->data[0]) ? "yes" : "no");
			break;
		case GF_ISOM_ITUNE_GENRE:
			if (dbox && dbox->data && itune->data->dataSize>=2) {
				u32 genre = itune->data->data[0];
				genre<<=8;
				genre |= itune->data->data[1];
				gf_fprintf(trace, " value=\"%s\" ", gf_id3_get_genre((u32) genre));
			}
			break;
		default:
			if (dbox && dbox->data) {
				gf_fprintf(trace, " value=\"");
				switch (itype) {
				case GF_ITAG_STR:
					dump_data_string(trace, dbox->data, dbox->dataSize);
					break;
				case GF_ITAG_INT8:
					if (dbox->dataSize)
						gf_fprintf(trace, "%d", dbox->data[0]);
					break;
				case GF_ITAG_INT16:
					if (dbox->dataSize>1) {
						u16 v = dbox->data[0];
						v<<=8;
						v |= dbox->data[1];
						gf_fprintf(trace, "%d", v);
					}
					break;
				case GF_ITAG_INT32:
					if (dbox->dataSize>3) {
						u32 v = dbox->data[0]; v<<=8;
						v |= dbox->data[1]; v<<=8;
						v |= dbox->data[2]; v<<=8;
						v |= dbox->data[3];
						gf_fprintf(trace, "%d", v);
					}
					break;
				default:
					if (!unknown && gf_utf8_is_legal(dbox->data, dbox->dataSize) ) {
						dump_data_string(trace, dbox->data, dbox->dataSize);
					} else {
						dump_data(trace, dbox->data, dbox->dataSize);
					}
				break;
				}
				gf_fprintf(trace, "\" ");
			}
			break;
		}
	}
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_ADOBE

GF_Err abst_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeBootstrapInfoBox *p = (GF_AdobeBootstrapInfoBox*)a;
	gf_isom_box_dump_start(a, "AdobeBootstrapBox", trace);

	gf_fprintf(trace, "BootstrapinfoVersion=\"%u\" Profile=\"%u\" Live=\"%u\" Update=\"%u\" TimeScale=\"%u\" CurrentMediaTime=\""LLU"\" SmpteTimeCodeOffset=\""LLU"\" ",
	        p->bootstrapinfo_version, p->profile, p->live, p->update, p->time_scale, p->current_media_time, p->smpte_time_code_offset);
	if (p->movie_identifier)
		gf_fprintf(trace, "MovieIdentifier=\"%s\" ", p->movie_identifier);
	if (p->drm_data)
		gf_fprintf(trace, "DrmData=\"%s\" ", p->drm_data);
	if (p->meta_data)
		gf_fprintf(trace, "MetaData=\"%s\" ", p->meta_data);
	gf_fprintf(trace, ">\n");

	for (i=0; i<p->server_entry_count; i++) {
		char *str = (char*)gf_list_get(p->server_entry_table, i);
		gf_fprintf(trace, "<ServerEntry>%s</ServerEntry>\n", str);
	}

	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_entry_table, i);
		gf_fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->segment_run_table_count; i++)
		gf_isom_box_dump(gf_list_get(p->segment_run_table_entries, i), trace);

	for (i=0; i<p->fragment_run_table_count; i++)
		gf_isom_box_dump(gf_list_get(p->fragment_run_table_entries, i), trace);

	gf_isom_box_dump_done("AdobeBootstrapBox", a, trace);
	return GF_OK;
}

GF_Err afra_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeFragRandomAccessBox *p = (GF_AdobeFragRandomAccessBox*)a;
	gf_isom_box_dump_start(a, "AdobeFragmentRandomAccessBox", trace);

	gf_fprintf(trace, "LongIDs=\"%u\" LongOffsets=\"%u\" TimeScale=\"%u\">\n", p->long_ids, p->long_offsets, p->time_scale);

	for (i=0; i<p->entry_count; i++) {
		GF_AfraEntry *ae = (GF_AfraEntry *)gf_list_get(p->local_access_entries, i);
		gf_fprintf(trace, "<LocalAccessEntry Time=\""LLU"\" Offset=\""LLU"\"/>\n", ae->time, ae->offset);
	}

	for (i=0; i<p->global_entry_count; i++) {
		GF_GlobalAfraEntry *gae = (GF_GlobalAfraEntry *)gf_list_get(p->global_access_entries, i);
		gf_fprintf(trace, "<GlobalAccessEntry Time=\""LLU"\" Segment=\"%u\" Fragment=\"%u\" AfraOffset=\""LLU"\" OffsetFromAfra=\""LLU"\"/>\n",
		        gae->time, gae->segment, gae->fragment, gae->afra_offset, gae->offset_from_afra);
	}

	gf_isom_box_dump_done("AdobeFragmentRandomAccessBox", a, trace);
	return GF_OK;
}

GF_Err afrt_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeFragmentRunTableBox *p = (GF_AdobeFragmentRunTableBox*)a;
	gf_isom_box_dump_start(a, "AdobeFragmentRunTableBox", trace);

	gf_fprintf(trace, "TimeScale=\"%u\">\n", p->timescale);

	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_segment_url_modifiers, i);
		gf_fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->fragment_run_entry_count; i++) {
		GF_AdobeFragmentRunEntry *fre = (GF_AdobeFragmentRunEntry *)gf_list_get(p->fragment_run_entry_table, i);
		gf_fprintf(trace, "<FragmentRunEntry FirstFragment=\"%u\" FirstFragmentTimestamp=\""LLU"\" FirstFragmentDuration=\"%u\"", fre->first_fragment, fre->first_fragment_timestamp, fre->fragment_duration);
		if (!fre->fragment_duration)
			gf_fprintf(trace, " DiscontinuityIndicator=\"%u\"", fre->discontinuity_indicator);
		gf_fprintf(trace, "/>\n");
	}

	gf_isom_box_dump_done("AdobeFragmentRunTableBox", a, trace);
	return GF_OK;
}

GF_Err asrt_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_AdobeSegmentRunTableBox *p = (GF_AdobeSegmentRunTableBox*)a;
	gf_isom_box_dump_start(a, "AdobeSegmentRunTableBox", trace);

	gf_fprintf(trace, ">\n");

	for (i=0; i<p->quality_entry_count; i++) {
		char *str = (char*)gf_list_get(p->quality_segment_url_modifiers, i);
		gf_fprintf(trace, "<QualityEntry>%s</QualityEntry>\n", str);
	}

	for (i=0; i<p->segment_run_entry_count; i++) {
		GF_AdobeSegmentRunEntry *sre = (GF_AdobeSegmentRunEntry *)gf_list_get(p->segment_run_entry_table, i);
		gf_fprintf(trace, "<SegmentRunEntry FirstSegment=\"%u\" FragmentsPerSegment=\"%u\"/>\n", sre->first_segment, sre->fragment_per_segment);
	}

	gf_isom_box_dump_done("AdobeSegmentRunTableBox", a, trace);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_ADOBE*/

GF_Err ilst_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_Box *tag;
	GF_Err e;
	GF_ItemListBox *ptr;
	ptr = (GF_ItemListBox *)a;
	gf_isom_box_dump_start(a, "ItemListBox", trace);
	gf_fprintf(trace, ">\n");

	i=0;
	while ( (tag = (GF_Box*)gf_list_enum(ptr->child_boxes, &i))) {
		e = ilst_item_box_dump(tag, trace);
		if(e) return e;
	}
	gf_isom_box_dump_done("ItemListBox", NULL, trace);
	return GF_OK;
}

GF_Err databox_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "data", trace);

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("data", a, trace);
	return GF_OK;
}

GF_Err ohdr_box_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMCommonHeaderBox *ptr = (GF_OMADRMCommonHeaderBox *)a;
	gf_isom_box_dump_start(a, "OMADRMCommonHeaderBox", trace);

	gf_fprintf(trace, "EncryptionMethod=\"%d\" PaddingScheme=\"%d\" PlaintextLength=\""LLD"\" ",
	        ptr->EncryptionMethod, ptr->PaddingScheme, ptr->PlaintextLength);
	if (ptr->RightsIssuerURL) gf_fprintf(trace, "RightsIssuerURL=\"%s\" ", ptr->RightsIssuerURL);
	if (ptr->ContentID) gf_fprintf(trace, "ContentID=\"%s\" ", ptr->ContentID);
	if (ptr->TextualHeaders) {
		u32 i, offset;
		char *start = ptr->TextualHeaders;
		gf_fprintf(trace, "TextualHeaders=\"");
		i=offset=0;
		while (i<ptr->TextualHeadersLen) {
			if (start[i]==0) {
				gf_fprintf(trace, "%s ", start+offset);
				offset=i+1;
			}
			i++;
		}
		gf_fprintf(trace, "%s\"  ", start+offset);
	}

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("OMADRMCommonHeaderBox", a, trace);
	return GF_OK;
}
GF_Err grpi_box_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMGroupIDBox *ptr = (GF_OMADRMGroupIDBox *)a;
	gf_isom_box_dump_start(a, "OMADRMGroupIDBox", trace);

	gf_fprintf(trace, "GroupID=\"%s\" EncryptionMethod=\"%d\" GroupKey=\" ", ptr->GroupID, ptr->GKEncryptionMethod);
	if (ptr->GroupKey)
		dump_data(trace, ptr->GroupKey, ptr->GKLength);
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("OMADRMGroupIDBox", a, trace);
	return GF_OK;
}
GF_Err mdri_box_dump(GF_Box *a, FILE * trace)
{
	//GF_OMADRMMutableInformationBox *ptr = (GF_OMADRMMutableInformationBox*)a;
	gf_isom_box_dump_start(a, "OMADRMMutableInformationBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("OMADRMMutableInformationBox", a, trace);
	return GF_OK;
}
GF_Err odtt_box_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMTransactionTrackingBox *ptr = (GF_OMADRMTransactionTrackingBox *)a;
	gf_isom_box_dump_start(a, "OMADRMTransactionTrackingBox", trace);

	gf_fprintf(trace, "TransactionID=\"");
	dump_data(trace, ptr->TransactionID, 16);
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("OMADRMTransactionTrackingBox", a, trace);
	return GF_OK;
}
GF_Err odrb_box_dump(GF_Box *a, FILE * trace)
{
	GF_OMADRMRightsObjectBox*ptr = (GF_OMADRMRightsObjectBox*)a;
	gf_isom_box_dump_start(a, "OMADRMRightsObjectBox", trace);

	gf_fprintf(trace, "OMARightsObject=\"");
	dump_data(trace, ptr->oma_ro, ptr->oma_ro_size);
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("OMADRMRightsObjectBox", a, trace);
	return GF_OK;
}
GF_Err odkm_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "OMADRMKMSBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("OMADRMKMSBox", a, trace);
	return GF_OK;
}


GF_Err pasp_box_dump(GF_Box *a, FILE * trace)
{
	GF_PixelAspectRatioBox *ptr = (GF_PixelAspectRatioBox*)a;
	gf_isom_box_dump_start(a, "PixelAspectRatioBox", trace);
	gf_fprintf(trace, "hSpacing=\"%d\" vSpacing=\"%d\" >\n", ptr->hSpacing, ptr->vSpacing);
	gf_isom_box_dump_done("PixelAspectRatioBox", a, trace);
	return GF_OK;
}

GF_Err clap_box_dump(GF_Box *a, FILE * trace)
{
	GF_CleanApertureBox *ptr = (GF_CleanApertureBox*)a;
	gf_isom_box_dump_start(a, "CleanApertureBox", trace);
	gf_fprintf(trace, "cleanApertureWidthN=\"%d\" cleanApertureWidthD=\"%d\" ", ptr->cleanApertureWidthN, ptr->cleanApertureWidthD);
	gf_fprintf(trace, "cleanApertureHeightN=\"%d\" cleanApertureHeightD=\"%d\" ", ptr->cleanApertureHeightN, ptr->cleanApertureHeightD);
	gf_fprintf(trace, "horizOffN=\"%d\" horizOffD=\"%d\" ", ptr->horizOffN, ptr->horizOffD);
	gf_fprintf(trace, "vertOffN=\"%d\" vertOffD=\"%d\"", ptr->vertOffN, ptr->vertOffD);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("CleanApertureBox", a, trace);
	return GF_OK;
}


GF_Err tsel_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrackSelectionBox *ptr = (GF_TrackSelectionBox *)a;
	gf_isom_box_dump_start(a, "TrackSelectionBox", trace);

	gf_fprintf(trace, "switchGroup=\"%d\" >\n", ptr->switchGroup);
	for (i=0; i<ptr->attributeListCount; i++) {
		gf_fprintf(trace, "<TrackSelectionCriteria value=\"%s\"/>\n", gf_4cc_to_str(ptr->attributeList[i]) );
	}
	if (!ptr->size)
		gf_fprintf(trace, "<TrackSelectionCriteria value=\"\"/>\n");

	gf_isom_box_dump_done("TrackSelectionBox", a, trace);
	return GF_OK;
}

GF_Err metx_box_dump(GF_Box *a, FILE * trace)
{
	GF_MetaDataSampleEntryBox *ptr = (GF_MetaDataSampleEntryBox*)a;
	const char *name;
	switch (ptr->type) {
	case GF_ISOM_BOX_TYPE_METX:
		name = "XMLMetaDataSampleEntryBox";
		break;
	case GF_ISOM_BOX_TYPE_METT:
		name = "TextMetaDataSampleEntryBox";
		break;
	case GF_ISOM_BOX_TYPE_SBTT:
		name = "SubtitleSampleEntryBox";
		break;
	case GF_ISOM_BOX_TYPE_STXT:
		name = "SimpleTextSampleEntryBox";
		break;
	case GF_ISOM_BOX_TYPE_STPP:
		name = "XMLSubtitleSampleEntryBox";
		break;
	default:
		name = "UnknownTextSampleEntryBox";
		break;
	}
	gf_isom_box_dump_start(a, name, trace);

	if (ptr->type==GF_ISOM_BOX_TYPE_METX) {
		gf_fprintf(trace, "namespace=\"%s\" ", ptr->xml_namespace);
		if (ptr->xml_schema_loc) gf_fprintf(trace, "schema_location=\"%s\" ", ptr->xml_schema_loc);
		if (ptr->content_encoding) gf_fprintf(trace, "content_encoding=\"%s\" ", ptr->content_encoding);

	} else if (ptr->type==GF_ISOM_BOX_TYPE_STPP) {
		gf_fprintf(trace, "namespace=\"%s\" ", ptr->xml_namespace);
		if (ptr->xml_schema_loc) gf_fprintf(trace, "schema_location=\"%s\" ", ptr->xml_schema_loc);
		if (ptr->mime_type) gf_fprintf(trace, "auxiliary_mime_types=\"%s\" ", ptr->mime_type);
	}
	//mett, sbtt, stxt
	else {
		gf_fprintf(trace, "mime_type=\"%s\" ", ptr->mime_type);
		if (ptr->content_encoding) gf_fprintf(trace, "content_encoding=\"%s\" ", ptr->content_encoding);
	}
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err txtc_box_dump(GF_Box *a, FILE * trace)
{
	GF_TextConfigBox *ptr = (GF_TextConfigBox*)a;
	const char *name = (ptr->type==GF_ISOM_BOX_TYPE_TXTC) ?  "TextConfigBox" : "MIMEBox";

	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, ">\n");

	if (ptr->config) gf_fprintf(trace, "<![CDATA[%s]]>", ptr->config);

	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err dims_box_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSSampleEntryBox *p = (GF_DIMSSampleEntryBox*)a;
	gf_isom_box_dump_start(a, "DIMSSampleEntryBox", trace);
	gf_fprintf(trace, "dataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	gf_isom_box_dump_done("DIMSSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err diST_box_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSScriptTypesBox *p = (GF_DIMSScriptTypesBox*)a;
	gf_isom_box_dump_start(a, "DIMSScriptTypesBox", trace);
	gf_fprintf(trace, "types=\"%s\">\n", p->content_script_types);
	gf_isom_box_dump_done("DIMSScriptTypesBox", a, trace);
	return GF_OK;
}

GF_Err dimC_box_dump(GF_Box *a, FILE * trace)
{
	GF_DIMSSceneConfigBox *p = (GF_DIMSSceneConfigBox *)a;
	gf_isom_box_dump_start(a, "DIMSSceneConfigBox", trace);
	gf_fprintf(trace, "profile=\"%d\" level=\"%d\" pathComponents=\"%d\" useFullRequestHosts=\"%d\" streamType=\"%d\" containsRedundant=\"%d\" textEncoding=\"%s\" contentEncoding=\"%s\" >\n",
	        p->profile, p->level, p->pathComponents, p->fullRequestHost, p->streamType, p->containsRedundant, p->textEncoding, p->contentEncoding);
	gf_isom_box_dump_done("DIMSSceneConfigBox", a, trace);
	return GF_OK;
}

GF_Err dOps_box_dump(GF_Box *a, FILE * trace)
{
	GF_OpusSpecificBox *p = (GF_OpusSpecificBox *)a;

	gf_isom_box_dump_start(a, "OpusSpecificBox", trace);
	gf_fprintf(trace, "version=\"%d\" OutputChannelCount=\"%d\" PreSkip=\"%d\" InputSampleRate=\"%d\" OutputGain=\"%d\" ChannelMappingFamily=\"%d\"",
		p->version, p->opcfg.OutputChannelCount, p->opcfg.PreSkip, p->opcfg.InputSampleRate, p->opcfg.OutputGain, p->opcfg.ChannelMappingFamily);

	if (p->opcfg.ChannelMappingFamily) {
		u32 i;
		gf_fprintf(trace, " StreamCount=\"%d\" CoupledStreamCount=\"%d\" channelMapping=\"", p->opcfg.StreamCount, p->opcfg.CoupledCount);
		for (i=0; i<p->opcfg.OutputChannelCount; i++) {
			gf_fprintf(trace, "%s%d", i ? " " : "", p->opcfg.ChannelMapping[i]);
		}
		gf_fprintf(trace, "\"");
	}
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("OpusSpecificBox", a, trace);

	return GF_OK;
}

GF_Err dac3_box_dump(GF_Box *a, FILE * trace)
{
	GF_AC3ConfigBox *p = (GF_AC3ConfigBox *)a;

	if (p->cfg.is_ec3) {
		u32 i;
		a->type = GF_ISOM_BOX_TYPE_DEC3;
		gf_isom_box_dump_start(a, "EC3SpecificBox", trace);
		a->type = GF_ISOM_BOX_TYPE_DAC3;
		gf_fprintf(trace, "nb_streams=\"%d\" data_rate=\"%d\">\n", p->cfg.nb_streams, p->cfg.brcode*1000);
		for (i=0; i<p->cfg.nb_streams; i++) {
			gf_fprintf(trace, "<EC3StreamConfig fscod=\"%d\" bsid=\"%d\" bsmod=\"%d\" acmod=\"%d\" lfon=\"%d\" asvc=\"%d\" num_sub_dep=\"%d\" chan_loc=\"%d\"/>\n",
			        p->cfg.streams[i].fscod, p->cfg.streams[i].bsid, p->cfg.streams[i].bsmod, p->cfg.streams[i].acmod, p->cfg.streams[i].lfon, p->cfg.streams[i].asvc, p->cfg.streams[i].nb_dep_sub, p->cfg.streams[i].chan_loc);
		}
		if (p->cfg.atmos_ec3_ext || p->cfg.complexity_index_type) {
			gf_fprintf(trace, "<ExtendedConfig flag_ec3_extension_type_a=\"%d\" complexity_index_type_a=\"%d\"/>\n",
				p->cfg.atmos_ec3_ext, p->cfg.complexity_index_type);
		}
		gf_isom_box_dump_done("EC3SpecificBox", a, trace);
	} else {
		gf_isom_box_dump_start(a, "AC3SpecificBox", trace);
		gf_fprintf(trace, "fscod=\"%d\" bsid=\"%d\" bsmod=\"%d\" acmod=\"%d\" lfon=\"%d\" bit_rate_code=\"%d\">\n",
		        p->cfg.streams[0].fscod, p->cfg.streams[0].bsid, p->cfg.streams[0].bsmod, p->cfg.streams[0].acmod, p->cfg.streams[0].lfon, p->cfg.brcode);
		gf_isom_box_dump_done("AC3SpecificBox", a, trace);
	}
	return GF_OK;
}

GF_Err dmlp_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrueHDConfigBox *p = (GF_TrueHDConfigBox *)a;

	gf_isom_box_dump_start(a, "TrueHDConfigBox", trace);
	gf_fprintf(trace, "format_info=\"%u\" peak_data_rate=\"%u\">\n",
			p->format_info, p->peak_data_rate);
	gf_isom_box_dump_done("TrueHDConfigBox", a, trace);
	return GF_OK;
}

GF_Err dvcC_box_dump(GF_Box *a, FILE * trace)
{
	GF_DOVIConfigurationBox *p = (GF_DOVIConfigurationBox *)a;
	gf_isom_box_dump_start(a, "DOVIConfigurationBox", trace);
	gf_fprintf(trace, "dv_version_major=\"%u\" dv_version_minor=\"%u\" dv_profile=\"%u\" dv_level=\"%u\" rpu_present_flag=\"%u\" el_present_flag=\"%u\" bl_present_flag=\"%u\" compatibility_id=\"%u\">\n",
		p->DOVIConfig.dv_version_major, p->DOVIConfig.dv_version_minor, p->DOVIConfig.dv_profile, p->DOVIConfig.dv_level,
		p->DOVIConfig.rpu_present_flag, p->DOVIConfig.el_present_flag, p->DOVIConfig.bl_present_flag, p->DOVIConfig.dv_bl_signal_compatibility_id);
	gf_isom_box_dump_done("DOVIConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err dvvC_box_dump(GF_Box *a, FILE * trace)
{
	return dvcC_box_dump(a, trace);
}

GF_Err lsrc_box_dump(GF_Box *a, FILE * trace)
{
	GF_LASERConfigurationBox *p = (GF_LASERConfigurationBox *)a;
	gf_isom_box_dump_start(a, "LASeRConfigurationBox", trace);
	dump_data_attribute(trace, "LASeRHeader", p->hdr, p->hdr_size);
	gf_fprintf(trace, ">");
	gf_isom_box_dump_done("LASeRConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err lsr1_box_dump(GF_Box *a, FILE * trace)
{
	GF_LASeRSampleEntryBox *p = (GF_LASeRSampleEntryBox*)a;
	gf_isom_box_dump_start(a, "LASeRSampleEntryBox", trace);
	gf_fprintf(trace, "DataReferenceIndex=\"%d\">\n", p->dataReferenceIndex);
	gf_isom_box_dump_done("LASeRSampleEntryBox", a, trace);
	return GF_OK;
}


GF_Err sidx_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SegmentIndexBox *p = (GF_SegmentIndexBox *)a;
	gf_isom_box_dump_start(a, "SegmentIndexBox", trace);

	gf_fprintf(trace, "reference_ID=\"%d\" timescale=\"%d\" earliest_presentation_time=\""LLD"\" first_offset=\""LLD"\"", p->reference_ID, p->timescale, p->earliest_presentation_time, p->first_offset);
	if (p->internal_flags & GF_ISOM_BOX_COMPRESSED)
		gf_fprintf(trace, " compressedSize=\""LLU"\"", p->size - p->compressed_diff);
	gf_fprintf(trace, ">\n");

	for (i=0; i<p->nb_refs; i++) {
		gf_fprintf(trace, "<Reference type=\"%d\" size=\"%d\" duration=\"%d\" startsWithSAP=\"%d\" SAP_type=\"%d\" SAPDeltaTime=\"%d\"/>\n", p->refs[i].reference_type, p->refs[i].reference_size, p->refs[i].subsegment_duration, p->refs[i].starts_with_SAP, p->refs[i].SAP_type, p->refs[i].SAP_delta_time);
	}
	if (!p->size) {
		gf_fprintf(trace, "<Reference type=\"\" size=\"\" duration=\"\" startsWithSAP=\"\" SAP_type=\"\" SAPDeltaTime=\"\"/>\n");
	}
	gf_isom_box_dump_done("SegmentIndexBox", a, trace);
	return GF_OK;
}

GF_Err ssix_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, j;
	GF_SubsegmentIndexBox *p = (GF_SubsegmentIndexBox *)a;
	gf_isom_box_dump_start(a, "SubsegmentIndexBox", trace);

	gf_fprintf(trace, "subsegment_count=\"%d\"", p->subsegment_count);
	if (p->internal_flags & GF_ISOM_BOX_COMPRESSED)
		gf_fprintf(trace, " compressedSize=\""LLU"\"", p->size - p->compressed_diff);
	gf_fprintf(trace, ">\n");

	for (i = 0; i < p->subsegment_count; i++) {
		gf_fprintf(trace, "<Subsegment range_count=\"%d\">\n", p->subsegments[i].range_count);
		for (j = 0; j < p->subsegments[i].range_count; j++) {
			gf_fprintf(trace, "<Range level=\"%d\" range_size=\"%d\"/>\n", p->subsegments[i].ranges[j].level, p->subsegments[i].ranges[j].range_size);
		}
		gf_fprintf(trace, "</Subsegment>\n");
	}
	if (!p->size) {
		gf_fprintf(trace, "<Subsegment range_count=\"\">\n");
		gf_fprintf(trace, "<Range level=\"\" range_size=\"\"/>\n");
		gf_fprintf(trace, "</Subsegment>\n");
	}
	gf_isom_box_dump_done("SubsegmentIndexBox", a, trace);
	return GF_OK;
}


GF_Err leva_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_LevelAssignmentBox *p = (GF_LevelAssignmentBox *)a;
	gf_isom_box_dump_start(a, "LevelAssignmentBox", trace);

	gf_fprintf(trace, "level_count=\"%d\" >\n", p->level_count);
	for (i = 0; i < p->level_count; i++) {
		gf_fprintf(trace, "<Assignement track_id=\"%d\" padding_flag=\"%d\" assignement_type=\"%d\" grouping_type=\"%s\" grouping_type_parameter=\"%d\" sub_track_id=\"%d\" />\n", p->levels[i].track_id, p->levels[i].padding_flag, p->levels[i].type, gf_4cc_to_str(p->levels[i].grouping_type) , p->levels[i].grouping_type_parameter, p->levels[i].sub_track_id);
	}
	if (!p->size) {
		gf_fprintf(trace, "<Assignement track_id=\"\" padding_flag=\"\" assignement_type=\"\" grouping_type=\"\" grouping_type_parameter=\"\" sub_track_id=\"\" />\n");
	}
	gf_isom_box_dump_done("LevelAssignmentBox", a, trace);
	return GF_OK;
}

GF_Err strk_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "SubTrackBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("SubTrackBox", a, trace);
	return GF_OK;
}

GF_Err stri_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SubTrackInformationBox *p = (GF_SubTrackInformationBox *)a;
	gf_isom_box_dump_start(a, "SubTrackInformationBox", trace);

	gf_fprintf(trace, "switch_group=\"%d\" alternate_group=\"%d\" sub_track_id=\"%d\">\n", p->switch_group, p->alternate_group, p->sub_track_id);

	for (i = 0; i < p->attribute_count; i++) {
		gf_fprintf(trace, "<SubTrackInformationAttribute value=\"%s\"/>\n", gf_4cc_to_str(p->attribute_list[i]) );
	}
	if (!p->size)
		gf_fprintf(trace, "<SubTrackInformationAttribute value=\"\"/>\n");

	gf_isom_box_dump_done("SubTrackInformationBox", a, trace);
	return GF_OK;
}

GF_Err stsg_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SubTrackSampleGroupBox *p = (GF_SubTrackSampleGroupBox *)a;
	gf_isom_box_dump_start(a, "SubTrackSampleGroupBox", trace);

	if (p->grouping_type)
		gf_fprintf(trace, "grouping_type=\"%s\"", gf_4cc_to_str(p->grouping_type) );
	gf_fprintf(trace, ">\n");

	for (i = 0; i < p->nb_groups; i++) {
		gf_fprintf(trace, "<SubTrackSampleGroupBoxEntry group_description_index=\"%d\"/>\n", p->group_description_index[i]);
	}
	if (!p->size)
		gf_fprintf(trace, "<SubTrackSampleGroupBoxEntry group_description_index=\"\"/>\n");

	gf_isom_box_dump_done("SubTrackSampleGroupBox", a, trace);
	return GF_OK;
}

GF_Err pcrb_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_PcrInfoBox *p = (GF_PcrInfoBox *)a;
	gf_isom_box_dump_start(a, "MPEG2TSPCRInfoBox", trace);
	gf_fprintf(trace, "subsegment_count=\"%d\">\n", p->subsegment_count);

	for (i=0; i<p->subsegment_count; i++) {
		gf_fprintf(trace, "<PCRInfo PCR=\""LLU"\" />\n", p->pcr_values[i]);
	}
	if (!p->size) {
		gf_fprintf(trace, "<PCRInfo PCR=\"\" />\n");
	}
	gf_isom_box_dump_done("MPEG2TSPCRInfoBox", a, trace);
	return GF_OK;
}

GF_Err subs_box_dump(GF_Box *a, FILE * trace)
{
	u32 entry_count, i, j;
	u16 subsample_count;
	GF_SubSampleEntry *pSubSamp;
	GF_SubSampleInformationBox *ptr = (GF_SubSampleInformationBox *) a;

	if (!a) return GF_BAD_PARAM;

	entry_count = gf_list_count(ptr->Samples);
	gf_isom_box_dump_start(a, "SubSampleInformationBox", trace);

	gf_fprintf(trace, "EntryCount=\"%d\">\n", entry_count);

	for (i=0; i<entry_count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry*) gf_list_get(ptr->Samples, i);

		subsample_count = gf_list_count(pSamp->SubSamples);

		gf_fprintf(trace, "<SampleEntry SampleDelta=\"%d\" SubSampleCount=\"%d\">\n", pSamp->sample_delta, subsample_count);

		for (j=0; j<subsample_count; j++) {
			pSubSamp = (GF_SubSampleEntry*) gf_list_get(pSamp->SubSamples, j);
			gf_fprintf(trace, "<SubSample Size=\"%u\" Priority=\"%u\" Discardable=\"%d\" Reserved=\"%08X\"/>\n", pSubSamp->subsample_size, pSubSamp->subsample_priority, pSubSamp->discardable, pSubSamp->reserved);
		}
		gf_fprintf(trace, "</SampleEntry>\n");
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<SampleEntry SampleDelta=\"\" SubSampleCount=\"\">\n");
		gf_fprintf(trace, "<SubSample Size=\"\" Priority=\"\" Discardable=\"\" Reserved=\"\"/>\n");
		gf_fprintf(trace, "</SampleEntry>\n");
	}

	gf_isom_box_dump_done("SubSampleInformationBox", a, trace);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err tfdt_box_dump(GF_Box *a, FILE * trace)
{
	GF_TFBaseMediaDecodeTimeBox *ptr = (GF_TFBaseMediaDecodeTimeBox*) a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "TrackFragmentBaseMediaDecodeTimeBox", trace);

	gf_fprintf(trace, "baseMediaDecodeTime=\""LLD"\">\n", ptr->baseMediaDecodeTime);
	gf_isom_box_dump_done("TrackFragmentBaseMediaDecodeTimeBox", a, trace);
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

GF_Err rvcc_box_dump(GF_Box *a, FILE * trace)
{
	GF_RVCConfigurationBox *ptr = (GF_RVCConfigurationBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "RVCConfigurationBox", trace);
	gf_fprintf(trace, "predefined=\"%d\"", ptr->predefined_rvc_config);
	if (! ptr->predefined_rvc_config) gf_fprintf(trace, " rvc_meta_idx=\"%d\"", ptr->rvc_meta_idx);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("RVCConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err sbgp_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleGroupBox *ptr = (GF_SampleGroupBox*) a;
	if (!a) return GF_BAD_PARAM;

	if (dump_skip_samples)
		return GF_OK;

	gf_isom_box_dump_start(a, "SampleGroupBox", trace);

	if (ptr->grouping_type)
		gf_fprintf(trace, "grouping_type=\"%s\"", gf_4cc_to_str(ptr->grouping_type) );

	if (ptr->version==1) {
		if (isalnum(ptr->grouping_type_parameter&0xFF)) {
			gf_fprintf(trace, " grouping_type_parameter=\"%s\"", gf_4cc_to_str(ptr->grouping_type_parameter) );
		} else {
			gf_fprintf(trace, " grouping_type_parameter=\"%u\"", ptr->grouping_type_parameter);
		}
	}
	gf_fprintf(trace, ">\n");
	for (i=0; i<ptr->entry_count; i++) {
		GF_SampleGroupEntry *pe = &ptr->sample_entries[i];
		if (pe->group_description_index>0x10000) {
			gf_fprintf(trace, "<SampleGroupBoxEntry sample_count=\"%u\" group_description_index=\"%u\" group_description_in_traf=\"1\" />\n", pe->sample_count, pe->group_description_index-0x10000);
		} else {
			gf_fprintf(trace, "<SampleGroupBoxEntry sample_count=\"%u\" group_description_index=\"%u\"/>\n", ptr->sample_entries[i].sample_count, ptr->sample_entries[i].group_description_index );
		}
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<SampleGroupBoxEntry sample_count=\"\" group_description_index=\"\" group_description_in_traf=\"\"/>\n");
	}
	gf_isom_box_dump_done("SampleGroupBox", a, trace);
	return GF_OK;
}

static void oinf_entry_dump(GF_OperatingPointsInformation *ptr, FILE * trace)
{
	u32 i, count;

	if (!ptr) {
		gf_fprintf(trace, "<OperatingPointsInformation scalability_mask=\"Multiview|Spatial scalability|Auxiliary|unknown\" num_profile_tier_level=\"\" num_operating_points=\"\" dependency_layers=\"\">\n");

		gf_fprintf(trace, " <ProfileTierLevel general_profile_space=\"\" general_tier_flag=\"\" general_profile_idc=\"\" general_profile_compatibility_flags=\"\" general_constraint_indicator_flags=\"\" />\n");

		gf_fprintf(trace, "<OperatingPoint output_layer_set_idx=\"\" max_temporal_id=\"\" layer_count=\"\" minPicWidth=\"\" minPicHeight=\"\" maxPicWidth=\"\" maxPicHeight=\"\" maxChromaFormat=\"\" maxBitDepth=\"\" frame_rate_info_flag=\"\" bit_rate_info_flag=\"\" avgFrameRate=\"\" constantFrameRate=\"\" maxBitRate=\"\" avgBitRate=\"\"/>\n");

		gf_fprintf(trace, "<Layer dependent_layerID=\"\" num_layers_dependent_on=\"\" dependent_on_layerID=\"\" dimension_identifier=\"\"/>\n");
		gf_fprintf(trace, "</OperatingPointsInformation>\n");
		return;
	}


	gf_fprintf(trace, "<OperatingPointsInformation");
	gf_fprintf(trace, " scalability_mask=\"%u (", ptr->scalability_mask);
	switch (ptr->scalability_mask) {
	case 2:
		gf_fprintf(trace, "Multiview");
		break;
	case 4:
		gf_fprintf(trace, "Spatial scalability");
		break;
	case 8:
		gf_fprintf(trace, "Auxiliary");
		break;
	default:
		gf_fprintf(trace, "unknown");
	}
	gf_fprintf(trace, ")\" num_profile_tier_level=\"%u\"", gf_list_count(ptr->profile_tier_levels) );
	gf_fprintf(trace, " num_operating_points=\"%u\" dependency_layers=\"%u\"", gf_list_count(ptr->operating_points), gf_list_count(ptr->dependency_layers));
	gf_fprintf(trace, ">\n");


	count=gf_list_count(ptr->profile_tier_levels);
	for (i = 0; i < count; i++) {
		LHEVC_ProfileTierLevel *ptl = (LHEVC_ProfileTierLevel *)gf_list_get(ptr->profile_tier_levels, i);
		gf_fprintf(trace, " <ProfileTierLevel general_profile_space=\"%u\" general_tier_flag=\"%u\" general_profile_idc=\"%u\" general_profile_compatibility_flags=\"%X\" general_constraint_indicator_flags=\""LLX"\" />\n", ptl->general_profile_space, ptl->general_tier_flag, ptl->general_profile_idc, ptl->general_profile_compatibility_flags, ptl->general_constraint_indicator_flags);
	}


	count=gf_list_count(ptr->operating_points);
	for (i = 0; i < count; i++) {
		LHEVC_OperatingPoint *op = (LHEVC_OperatingPoint *)gf_list_get(ptr->operating_points, i);
		gf_fprintf(trace, "<OperatingPoint output_layer_set_idx=\"%u\"", op->output_layer_set_idx);
		gf_fprintf(trace, " max_temporal_id=\"%u\" layer_count=\"%u\"", op->max_temporal_id, op->layer_count);
		gf_fprintf(trace, " minPicWidth=\"%u\" minPicHeight=\"%u\"", op->minPicWidth, op->minPicHeight);
		gf_fprintf(trace, " maxPicWidth=\"%u\" maxPicHeight=\"%u\"", op->maxPicWidth, op->maxPicHeight);
		gf_fprintf(trace, " maxChromaFormat=\"%u\" maxBitDepth=\"%u\"", op->maxChromaFormat, op->maxBitDepth);
		gf_fprintf(trace, " frame_rate_info_flag=\"%u\" bit_rate_info_flag=\"%u\"", op->frame_rate_info_flag, op->bit_rate_info_flag);
		if (op->frame_rate_info_flag)
			gf_fprintf(trace, " avgFrameRate=\"%u\" constantFrameRate=\"%u\"", op->avgFrameRate, op->constantFrameRate);
		if (op->bit_rate_info_flag)
			gf_fprintf(trace, " maxBitRate=\"%u\" avgBitRate=\"%u\"", op->maxBitRate, op->avgBitRate);
		gf_fprintf(trace, "/>\n");
	}
	count=gf_list_count(ptr->dependency_layers);
	for (i = 0; i < count; i++) {
		u32 j;
		LHEVC_DependentLayer *dep = (LHEVC_DependentLayer *)gf_list_get(ptr->dependency_layers, i);
		gf_fprintf(trace, "<Layer dependent_layerID=\"%u\" num_layers_dependent_on=\"%u\"", dep->dependent_layerID, dep->num_layers_dependent_on);
		if (dep->num_layers_dependent_on) {
			gf_fprintf(trace, " dependent_on_layerID=\"");
			for (j = 0; j < dep->num_layers_dependent_on; j++)
				gf_fprintf(trace, "%d ", dep->dependent_on_layerID[j]);
			gf_fprintf(trace, "\"");
		}
		gf_fprintf(trace, " dimension_identifier=\"");
		for (j = 0; j < 16; j++)
			if (ptr->scalability_mask & (1 << j))
				gf_fprintf(trace, "%d ", dep->dimension_identifier[j]);
		gf_fprintf(trace, "\"/>\n");
	}
	gf_fprintf(trace, "</OperatingPointsInformation>\n");
	return;
}

static void linf_dump(GF_LHVCLayerInformation *ptr, FILE * trace)
{
	u32 i, count;
	if (!ptr) {
		gf_fprintf(trace, "<LayerInformation num_layers=\"\">\n");
		gf_fprintf(trace, "<LayerInfoItem layer_id=\"\" min_temporalId=\"\" max_temporalId=\"\" sub_layer_presence_flags=\"\"/>\n");
		gf_fprintf(trace, "</LayerInformation>\n");
		return;
	}

	count = gf_list_count(ptr->num_layers_in_track);
	gf_fprintf(trace, "<LayerInformation num_layers=\"%d\">\n", count );
	for (i = 0; i < count; i++) {
		LHVCLayerInfoItem *li = (LHVCLayerInfoItem *)gf_list_get(ptr->num_layers_in_track, i);
		gf_fprintf(trace, "<LayerInfoItem layer_id=\"%d\" min_temporalId=\"%d\" max_temporalId=\"%d\" sub_layer_presence_flags=\"%d\"/>\n", li->layer_id, li->min_TemporalId, li->max_TemporalId, li->sub_layer_presence_flags);
	}
	gf_fprintf(trace, "</LayerInformation>\n");
	return;
}

static void trif_dump(FILE * trace, char *data, u32 data_size)
{
	GF_BitStream *bs;
	u32 id, independent, filter_disabled;
	Bool full_picture, has_dep, tile_group;

	if (!data) {
		gf_fprintf(trace, "<TileRegionGroupEntry ID=\"\" tileGroup=\"\" independent=\"\" full_picture=\"\" filter_disabled=\"\" x=\"\" y=\"\" w=\"\" h=\"\">\n");
		gf_fprintf(trace, "<TileRegionDependency tileID=\"\"/>\n");
		gf_fprintf(trace, "</TileRegionGroupEntry>\n");
		return;
	}

	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	id = gf_bs_read_u16(bs);
	tile_group = gf_bs_read_int(bs, 1);
	gf_fprintf(trace, "<TileRegionGroupEntry ID=\"%d\" tileGroup=\"%d\" ", id, tile_group);
	if (tile_group) {
		independent = gf_bs_read_int(bs, 2);
		full_picture = (Bool)gf_bs_read_int(bs, 1);
		filter_disabled = gf_bs_read_int(bs, 1);
		has_dep = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 2);
		gf_fprintf(trace, "independent=\"%d\" full_picture=\"%d\" filter_disabled=\"%d\" ", independent, full_picture, filter_disabled);

		if (!full_picture) {
			gf_fprintf(trace, "x=\"%d\" y=\"%d\" ", gf_bs_read_u16(bs), gf_bs_read_u16(bs));
		}
		gf_fprintf(trace, "w=\"%d\" h=\"%d\" ", gf_bs_read_u16(bs), gf_bs_read_u16(bs));
		if (!has_dep) {
			gf_fprintf(trace, "/>\n");
		} else {
			u32 count = gf_bs_read_u16(bs);
			gf_fprintf(trace, ">\n");
			while (count) {
				count--;
				gf_fprintf(trace, "<TileRegionDependency tileID=\"%d\"/>\n", gf_bs_read_u16(bs) );
			}
			gf_fprintf(trace, "</TileRegionGroupEntry>\n");
		}
	}
	gf_bs_del(bs);
}

static void nalm_dump(FILE * trace, char *data, u32 data_size)
{
	GF_BitStream *bs;

	Bool rle, large_size;
	u32 entry_count;

	if (!data) {
		gf_fprintf(trace, "<NALUMap rle=\"\" large_size=\"\">\n");
		gf_fprintf(trace, "<NALUMapEntry NALU_startNumber=\"\" groupID=\"\"/>\n");
		gf_fprintf(trace, "</NALUMap>\n");
		return;
	}

	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	gf_bs_read_int(bs, 6);
	large_size = gf_bs_read_int(bs, 1);
	rle = gf_bs_read_int(bs, 1);
	entry_count = gf_bs_read_int(bs, large_size ? 16 : 8);
	gf_fprintf(trace, "<NALUMap rle=\"%d\" large_size=\"%d\">\n", rle, large_size);

	while (entry_count) {
		u32 ID;
		gf_fprintf(trace, "<NALUMapEntry ");
		if (rle) {
			u32 start_num = gf_bs_read_int(bs, large_size ? 16 : 8);
			gf_fprintf(trace, "NALU_startNumber=\"%d\" ", start_num);
		}
		ID = gf_bs_read_u16(bs);
		gf_fprintf(trace, "groupID=\"%d\"/>\n", ID);
		entry_count--;
	}
	gf_bs_del(bs);
	gf_fprintf(trace, "</NALUMap>\n");
	return;
}


GF_Err sgpd_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleGroupDescriptionBox *ptr = (GF_SampleGroupDescriptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "SampleGroupDescriptionBox", trace);

	if (ptr->grouping_type)
		gf_fprintf(trace, "grouping_type=\"%s\"", gf_4cc_to_str(ptr->grouping_type) );
	if (ptr->version==1) gf_fprintf(trace, " default_length=\"%d\"", ptr->default_length);
	if ((ptr->version>=2) && ptr->default_description_index) gf_fprintf(trace, " default_group_index=\"%d\"", ptr->default_description_index);
	gf_fprintf(trace, ">\n");
	for (i=0; i<gf_list_count(ptr->group_descriptions); i++) {
		void *entry = gf_list_get(ptr->group_descriptions, i);
		switch (ptr->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_ROLL:
			gf_fprintf(trace, "<RollRecoveryEntry roll_distance=\"%d\" />\n", ((GF_RollRecoveryEntry*)entry)->roll_distance );
			break;
		case GF_ISOM_SAMPLE_GROUP_PROL:
			gf_fprintf(trace, "<AudioPreRollEntry roll_distance=\"%d\" />\n", ((GF_RollRecoveryEntry*)entry)->roll_distance );
			break;
		case GF_ISOM_SAMPLE_GROUP_TELE:
			gf_fprintf(trace, "<TemporalLevelEntry level_independently_decodable=\"%d\"/>\n", ((GF_TemporalLevelEntry*)entry)->level_independently_decodable);
			break;
		case GF_ISOM_SAMPLE_GROUP_RAP:
			gf_fprintf(trace, "<VisualRandomAccessEntry num_leading_samples_known=\"%s\"", ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known ? "yes" : "no");
			if (((GF_VisualRandomAccessEntry*)entry)->num_leading_samples_known)
				gf_fprintf(trace, " num_leading_samples=\"%d\"", ((GF_VisualRandomAccessEntry*)entry)->num_leading_samples);
			gf_fprintf(trace, "/>\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_SYNC:
			gf_fprintf(trace, "<SyncSampleGroupEntry NAL_unit_type=\"%d\"/>\n", ((GF_SYNCEntry*)entry)->NALU_type);
			break;
		case GF_ISOM_SAMPLE_GROUP_SEIG:
		{
			GF_CENCSampleEncryptionGroupEntry *seig = (GF_CENCSampleEncryptionGroupEntry *)entry;
			Bool use_mkey = seig->key_info[0] ? GF_TRUE : GF_FALSE;

			gf_fprintf(trace, "<CENCSampleEncryptionGroupEntry IsEncrypted=\"%d\"", seig->IsProtected);
			if (use_mkey) {
				u32 k, nb_keys, kpos=3;
				nb_keys = seig->key_info[1];
				nb_keys <<= 8;
				nb_keys |= seig->key_info[2];

				gf_fprintf(trace, ">\n");
				for (k=0; k<nb_keys; k++) {
					if (kpos + 17 > seig->key_info_size)
						break;
					u8 iv_size = seig->key_info[kpos];
					gf_fprintf(trace, "<CENCKey IV_size=\"%d\" KID=\"", iv_size);
					dump_data_hex(trace, seig->key_info+kpos+1, 16);
					kpos += 17;
					gf_fprintf(trace, "\"");
					if ((seig->IsProtected == 1) && !iv_size) {
						if (kpos + 1 >= seig->key_info_size)
							break;
						u8 const_IV_size = seig->key_info[kpos];
						gf_fprintf(trace, " constant_IV_size=\"%d\"  constant_IV=\"", const_IV_size);
						if (kpos + 1 + const_IV_size >= seig->key_info_size)
							break;
						dump_data_hex(trace, (char *)seig->key_info + kpos + 1, const_IV_size);
						kpos += 1 + const_IV_size;
						gf_fprintf(trace, "\"");
					}
					gf_fprintf(trace, "/>\n");
				}
				gf_fprintf(trace, "</CENCSampleEncryptionGroupEntry>\n");
			} else {
				gf_fprintf(trace, " IV_size=\"%d\" KID=\"", seig->key_info[3]);
				dump_data_hex(trace, seig->key_info+4, 16);
				if ((seig->IsProtected == 1) && !seig->key_info[3]) {
					gf_fprintf(trace, "\" constant_IV_size=\"%d\"  constant_IV=\"", seig->key_info[20]);
					dump_data_hex(trace, (char *)seig->key_info + 21, seig->key_info[20]);
				}
				gf_fprintf(trace, "\"/>\n");
			}
		}
			break;
		case GF_ISOM_SAMPLE_GROUP_OINF:
			oinf_entry_dump(entry, trace);
			break;
		case GF_ISOM_SAMPLE_GROUP_LINF:
			linf_dump(entry, trace);
			break;
		case GF_ISOM_SAMPLE_GROUP_TRIF:
			trif_dump(trace, (char *) ((GF_DefaultSampleGroupDescriptionEntry*)entry)->data,  ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			break;

		case GF_ISOM_SAMPLE_GROUP_NALM:
			nalm_dump(trace, (char *) ((GF_DefaultSampleGroupDescriptionEntry*)entry)->data,  ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			break;
		case GF_ISOM_SAMPLE_GROUP_SAP:
			gf_fprintf(trace, "<SAPEntry dependent_flag=\"%d\" SAP_type=\"%d\" />\n", ((GF_SAPEntry*)entry)->dependent_flag, ((GF_SAPEntry*)entry)->SAP_type);
			break;
		case GF_ISOM_SAMPLE_GROUP_SPOR:
		{
			GF_SubpictureOrderEntry *spor = (GF_SubpictureOrderEntry *) entry;
			gf_fprintf(trace, "<SubPictureOrderEntry subpic_id_info_flag=\"%d\" refs=\"", spor->subpic_id_info_flag);
			for (i=0; i<spor->num_subpic_ref_idx; i++) {
				if (i) gf_fprintf(trace, " ");
				gf_fprintf(trace, "%d", spor->subp_track_ref_idx[i]);
			}
			gf_fprintf(trace, "\"");
			if (spor->subpic_id_info_flag) {
				gf_fprintf(trace, " subpic_id_len_minus1=\"%d\" subpic_id_bit_pos=\"%d\" start_code_emul_flag=\"%d\"", spor->spinfo.subpic_id_len_minus1, spor->spinfo.subpic_id_bit_pos, spor->spinfo.start_code_emul_flag);
				gf_fprintf(trace, " %s=\"%d\"", spor->spinfo.pps_sps_subpic_id_flag ? "pps_id" : "sps_id", spor->spinfo.xps_id);
			}
			gf_fprintf(trace, "/>\n");
		}
			break;
		case GF_ISOM_SAMPLE_GROUP_SULM:
		{
			GF_SubpictureLayoutMapEntry *sulm = (GF_SubpictureLayoutMapEntry *) entry;
			gf_fprintf(trace, "<SubPictureLayoutMapEntry groupID_info_4cc=\"%s\" groupIDs=\"", gf_4cc_to_str(sulm->groupID_info_4cc) );
			for (i=0; i<sulm->nb_entries; i++) {
				if (i) gf_fprintf(trace, " ");
				gf_fprintf(trace, "%d", sulm->groupIDs[i]);
			}
			gf_fprintf(trace, "\"/>\n");
		}
			break;
		default:
			gf_fprintf(trace, "<DefaultSampleGroupDescriptionEntry size=\"%d\" data=\"", ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			dump_data(trace, (char *) ((GF_DefaultSampleGroupDescriptionEntry*)entry)->data,  ((GF_DefaultSampleGroupDescriptionEntry*)entry)->length);
			gf_fprintf(trace, "\"/>\n");
		}
	}
	if (!ptr->size) {
		switch (ptr->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_ROLL:
			gf_fprintf(trace, "<RollRecoveryEntry roll_distance=\"\"/>\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_PROL:
			gf_fprintf(trace, "<AudioPreRollEntry roll_distance=\"\"/>\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_TELE:
			gf_fprintf(trace, "<TemporalLevelEntry level_independently_decodable=\"\"/>\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_RAP:
			gf_fprintf(trace, "<VisualRandomAccessEntry num_leading_samples_known=\"yes|no\" num_leading_samples=\"\" />\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_SYNC:
			gf_fprintf(trace, "<SyncSampleGroupEntry NAL_unit_type=\"\" />\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_SEIG:
			gf_fprintf(trace, "<CENCSampleEncryptionGroupEntry IsEncrypted=\"\" IV_size=\"\" KID=\"\" constant_IV_size=\"\"  constant_IV=\"\"/>\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_OINF:
			oinf_entry_dump(NULL, trace);
			break;
		case GF_ISOM_SAMPLE_GROUP_LINF:
			linf_dump(NULL, trace);
			break;
		case GF_ISOM_SAMPLE_GROUP_TRIF:
			trif_dump(trace, NULL, 0);
			break;
		case GF_ISOM_SAMPLE_GROUP_NALM:
			nalm_dump(trace, NULL, 0);
			break;
		case GF_ISOM_SAMPLE_GROUP_SAP:
			gf_fprintf(trace, "<SAPEntry dependent_flag=\"\" SAP_type=\"\" />\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_SPOR:
			gf_fprintf(trace, "<SubPictureOrderEntry subpic_id_info_flag=\"\" refs=\"\" subpic_id_len_minus1=\"\" subpic_id_bit_pos=\"\" start_code_emul_flag=\"\" pps_id=\"\" sps_id=\"\" />\n");
			break;
		case GF_ISOM_SAMPLE_GROUP_SULM:
			gf_fprintf(trace, "<SubPictureLayoutMapEntry groupID_info_4cc=\"\" groupIDs=\"\" />\n");
			break;

		default:
			gf_fprintf(trace, "<DefaultSampleGroupDescriptionEntry size=\"\" data=\"\"/>\n");
		}
	}

	gf_isom_box_dump_done("SampleGroupDescriptionBox", a, trace);
	return GF_OK;
}

GF_Err saiz_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleAuxiliaryInfoSizeBox *ptr = (GF_SampleAuxiliaryInfoSizeBox*) a;
	if (!a) return GF_BAD_PARAM;

	if (dump_skip_samples)
		return GF_OK;

	gf_isom_box_dump_start(a, "SampleAuxiliaryInfoSizeBox", trace);

	gf_fprintf(trace, "default_sample_info_size=\"%d\" sample_count=\"%d\"", ptr->default_sample_info_size, ptr->sample_count);
	if (ptr->flags & 1) {
		if (isalnum(ptr->aux_info_type>>24)) {
			gf_fprintf(trace, " aux_info_type=\"%s\" aux_info_type_parameter=\"%d\"", gf_4cc_to_str(ptr->aux_info_type), ptr->aux_info_type_parameter);
		} else {
			gf_fprintf(trace, " aux_info_type=\"%d\" aux_info_type_parameter=\"%d\"", ptr->aux_info_type, ptr->aux_info_type_parameter);
		}
	}
	gf_fprintf(trace, ">\n");
	if (ptr->default_sample_info_size==0) {
		for (i=0; i<ptr->sample_count; i++) {
			gf_fprintf(trace, "<SAISize size=\"%d\" />\n", ptr->sample_info_size[i]);
		}
	}
	if (!ptr->size) {
			gf_fprintf(trace, "<SAISize size=\"\" />\n");
	}
	gf_isom_box_dump_done("SampleAuxiliaryInfoSizeBox", a, trace);
	return GF_OK;
}

GF_Err saio_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_SampleAuxiliaryInfoOffsetBox *ptr = (GF_SampleAuxiliaryInfoOffsetBox*) a;
	if (!a) return GF_BAD_PARAM;

	if (dump_skip_samples)
		return GF_OK;

	gf_isom_box_dump_start(a, "SampleAuxiliaryInfoOffsetBox", trace);

	gf_fprintf(trace, "entry_count=\"%d\"", ptr->entry_count);
	if (ptr->flags & 1) {
		if (isalnum(ptr->aux_info_type>>24)) {
			gf_fprintf(trace, " aux_info_type=\"%s\" aux_info_type_parameter=\"%d\"", gf_4cc_to_str(ptr->aux_info_type), ptr->aux_info_type_parameter);
		} else {
			gf_fprintf(trace, " aux_info_type=\"%d\" aux_info_type_parameter=\"%d\"", ptr->aux_info_type, ptr->aux_info_type_parameter);
		}
	}

	gf_fprintf(trace, ">\n");

	if (ptr->version==0) {
		for (i=0; i<ptr->entry_count; i++) {
			gf_fprintf(trace, "<SAIChunkOffset offset=\"%d\"/>\n", (u32) ptr->offsets[i]);
		}
	} else {
		for (i=0; i<ptr->entry_count; i++) {
			gf_fprintf(trace, "<SAIChunkOffset offset=\""LLD"\"/>\n", ptr->offsets[i]);
		}
	}
	if (!ptr->size) {
			gf_fprintf(trace, "<SAIChunkOffset offset=\"\"/>\n");
	}
	gf_isom_box_dump_done("SampleAuxiliaryInfoOffsetBox", a, trace);
	return GF_OK;
}

GF_Err pssh_box_dump(GF_Box *a, FILE * trace)
{
	GF_ProtectionSystemHeaderBox *ptr = (GF_ProtectionSystemHeaderBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "ProtectionSystemHeaderBox", trace);

	gf_fprintf(trace, "SystemID=\"");
	dump_data_hex(trace, (char *) ptr->SystemID, 16);
	gf_fprintf(trace, "\">\n");

	if (ptr->KID_count) {
		u32 i;
		for (i=0; i<ptr->KID_count; i++) {
			gf_fprintf(trace, " <PSSHKey KID=\"");
			dump_data_hex(trace, (char *) ptr->KIDs[i], 16);
			gf_fprintf(trace, "\"/>\n");
		}
	}
	if (ptr->private_data_size) {
		gf_fprintf(trace, " <PSSHData size=\"%d\" value=\"", ptr->private_data_size);
		dump_data_hex(trace, (char *) ptr->private_data, ptr->private_data_size);
		gf_fprintf(trace, "\"/>\n");
	}
	if (!ptr->size) {
		gf_fprintf(trace, " <PSSHKey KID=\"\"/>\n");
		gf_fprintf(trace, " <PSSHData size=\"\" value=\"\"/>\n");
	}
	gf_isom_box_dump_done("ProtectionSystemHeaderBox", a, trace);
	return GF_OK;
}

GF_Err tenc_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackEncryptionBox *ptr = (GF_TrackEncryptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "TrackEncryptionBox", trace);

	gf_fprintf(trace, "isEncrypted=\"%d\"", ptr->isProtected);

	if (ptr->key_info[3])
		gf_fprintf(trace, " IV_size=\"%d\" KID=\"", ptr->key_info[3]);
	else {
		gf_fprintf(trace, " constant_IV_size=\"%d\" constant_IV=\"", ptr->key_info[20]);
		dump_data_hex(trace, (char *) ptr->key_info+21, ptr->key_info[20]);
		gf_fprintf(trace, "\"  KID=\"");
	}
	dump_data_hex(trace, (char *) ptr->key_info+4, 16);
	if (ptr->version)
		gf_fprintf(trace, "\" crypt_byte_block=\"%d\" skip_byte_block=\"%d", ptr->crypt_byte_block, ptr->skip_byte_block);
	gf_fprintf(trace, "\">\n");

	if (!ptr->size) {
		gf_fprintf(trace, " IV_size=\"\" KID=\"\" constant_IV_size=\"\" constant_IV=\"\" crypt_byte_block=\"\" skip_byte_block=\"\">\n");
		gf_fprintf(trace, "<TENCKey IV_size=\"\" KID=\"\" const_IV_size=\"\" constIV=\"\"/>\n");
	}
	gf_isom_box_dump_done("TrackEncryptionBox", a, trace);
	return GF_OK;
}

GF_Err piff_pssh_box_dump(GF_Box *a, FILE * trace)
{
	GF_PIFFProtectionSystemHeaderBox *ptr = (GF_PIFFProtectionSystemHeaderBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "PIFFProtectionSystemHeaderBox", trace);
	fprintf(trace, "Version=\"%d\" Flags=\"%d\" ", ptr->version, ptr->flags);

	gf_fprintf(trace, "SystemID=\"");
	dump_data_hex(trace, (char *) ptr->SystemID, 16);
	gf_fprintf(trace, "\" PrivateData=\"");
	dump_data_hex(trace, (char *) ptr->private_data, ptr->private_data_size);
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("PIFFProtectionSystemHeaderBox", a, trace);
	return GF_OK;
}

GF_Err piff_tenc_box_dump(GF_Box *a, FILE * trace)
{
	GF_PIFFTrackEncryptionBox *ptr = (GF_PIFFTrackEncryptionBox*) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "PIFFTrackEncryptionBox", trace);
	fprintf(trace, "Version=\"%d\" Flags=\"%d\" ", ptr->version, ptr->flags);

	gf_fprintf(trace, "AlgorithmID=\"%d\" IV_size=\"%d\" KID=\"", ptr->AlgorithmID, ptr->key_info[3]);
	dump_data_hex(trace,(char *) ptr->key_info+4, 16);
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("PIFFTrackEncryptionBox", a, trace);
	return GF_OK;
}

u8 key_info_get_iv_size(const u8 *key_info, u32 key_info_size, u32 idx, u8 *const_iv_size, const u8 **const_iv);

GF_Err senc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, sample_count;
	const char *name;
	GF_BitStream *bs = NULL;
	u32 piff_IV_size = 0;
	Bool use_multikey = GF_FALSE;
	GF_SampleEncryptionBox *ptr = (GF_SampleEncryptionBox *) a;
	if (!a) return GF_BAD_PARAM;

	if (dump_skip_samples)
		return GF_OK;

	if (ptr->internal_4cc == GF_ISOM_BOX_UUID_PSEC)
		name = "PIFFSampleEncryptionBox";
	else
		name = "SampleEncryptionBox";

	gf_isom_box_dump_start(a, name, trace);

	if (ptr->internal_4cc == GF_ISOM_BOX_UUID_PSEC) {
		gf_fprintf(trace, "Version=\"%d\" Flags=\"%d\" ", ptr->version, ptr->flags);
		if (ptr->flags & 1) {
			gf_fprintf(trace, " AlgorithmID=\"%d\" IV_size=\"%d\" KID=\"", ptr->AlgorithmID, ptr->IV_size);
			dump_data(trace, (char *) ptr->KID, 16);
			gf_fprintf(trace, "\"");
			piff_IV_size = ptr->IV_size;
		}
	}

	sample_count = gf_list_count(ptr->samp_aux_info);
	gf_fprintf(trace, "sampleCount=\"%d\">\n", sample_count);
	if (ptr->internal_4cc != GF_ISOM_BOX_UUID_PSEC) {
		//WARNING - PSEC (UUID) IS TYPECASTED TO SENC (FULL BOX) SO WE CANNOT USE USUAL FULL BOX FUNCTIONS
		gf_fprintf(trace, "<FullBoxInfo Version=\"%d\" Flags=\"0x%X\"/>\n", ptr->version, ptr->flags);

		if ((ptr->version==1) && !ptr->piff_type)
			use_multikey = GF_TRUE;
	}


	for (i=0; i<sample_count; i++) {
		u32 nb_keys=0;
		u32 iv_size=0;
		u32 subs_bits=16;
		GF_CENCSampleAuxInfo *sai = (GF_CENCSampleAuxInfo *)gf_list_get(ptr->samp_aux_info, i);
		if (!sai) break;
		if (sai->isNotProtected) continue;

		gf_fprintf(trace, "<SampleEncryptionEntry sampleNumber=\"%d\"", i+1);
		if (sai->key_info) {
			if (!use_multikey) {
				iv_size = sai->key_info[3];
			} else {
				nb_keys = sai->key_info[1];
				nb_keys <<= 8;
				nb_keys |= sai->key_info[2];
				subs_bits = 32;
			}
		}
		//piff
		else {
			iv_size = piff_IV_size ? piff_IV_size : sai->key_info_size;
		}

		if (!bs)
			bs = gf_bs_new(sai->cenc_data, sai->cenc_data_size, GF_BITSTREAM_READ);
		else
			gf_bs_reassign_buffer(bs, sai->cenc_data, sai->cenc_data_size);

		if (!use_multikey) {
			gf_fprintf(trace, " IV_size=\"%u\"", iv_size);
			if (iv_size) {
				gf_fprintf(trace, " IV=\"");
				if (iv_size <= sai->cenc_data_size) {
					dump_data_hex(trace, (char *) sai->cenc_data, iv_size);
				} else {
					gf_fprintf(trace, "CORRUPTED");
				}
				gf_fprintf(trace, "\"");
				gf_bs_skip_bytes(bs, iv_size);
			}
		} else {
			u32 k, nb_ivs = gf_bs_read_u16(bs);
			if (nb_ivs) {
				gf_fprintf(trace, " multiIV=\"[");
			}
			for (k=0; k<nb_ivs; k++) {
				u32 pos;
				u32 idx = gf_bs_read_u16(bs);
				u8 mk_iv_size = key_info_get_iv_size(sai->key_info, sai->key_info_size, idx, NULL, NULL);
				pos = (u32) gf_bs_get_position(bs);
				if (mk_iv_size + pos <= sai->cenc_data_size) {
					gf_fprintf(trace, "%sidx:%d,iv_size:%d,IV:", k ? "," : "", idx, mk_iv_size);
					dump_data_hex(trace, (char *) sai->cenc_data+pos, mk_iv_size);
				} else {
					gf_fprintf(trace, "%sidx:%d,iv_size:%d,IV:CORRUPTED", k ? "," : "", idx, mk_iv_size);
				}
				gf_bs_skip_bytes(bs, mk_iv_size);
			}
			if (nb_ivs) {
				gf_fprintf(trace, "]\"");
			}
		}
		if (use_multikey || ((ptr->flags & 0x2) && (sai->cenc_data_size>iv_size)) ) {
			u32 j, nb_subs;

			nb_subs = gf_bs_read_int(bs, subs_bits);
			gf_fprintf(trace, " SubsampleCount=\"%u\"", nb_subs);
			gf_fprintf(trace, ">\n");

			for (j=0; j<nb_subs; j++) {
				u32 clear, crypt;
				gf_fprintf(trace, "<SubSampleEncryptionEntry");
				if (nb_keys>1) {
					u32 kidx = gf_bs_read_u16(bs);
					gf_fprintf(trace, " MultiKeyIndex=\"%u\"", kidx);
				}
				clear = gf_bs_read_u16(bs);
				crypt = gf_bs_read_u32(bs);
				gf_fprintf(trace, " NumClearBytes=\"%u\" NumEncryptedBytes=\"%u\"/>\n", clear, crypt);
			}
		} else {
			gf_fprintf(trace, ">\n");
		}
		gf_fprintf(trace, "</SampleEncryptionEntry>\n");
	}
	if (bs)
		gf_bs_del(bs);
		
	if (!ptr->size) {
		gf_fprintf(trace, "<SampleEncryptionEntry sampleCount=\"\" IV=\"\" SubsampleCount=\"\">\n");
		gf_fprintf(trace, "<SubSampleEncryptionEntry NumClearBytes=\"\" NumEncryptedBytes=\"\"/>\n");
		gf_fprintf(trace, "</SampleEncryptionEntry>\n");
	}
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err piff_psec_box_dump(GF_Box *a, FILE * trace)
{
	return senc_box_dump(a, trace);
}

GF_Err prft_box_dump(GF_Box *a, FILE * trace)
{
	Double fracs;
	GF_ProducerReferenceTimeBox *ptr = (GF_ProducerReferenceTimeBox *) a;
	time_t secs;
	struct tm t;
	secs = (ptr->ntp >> 32) - GF_NTP_SEC_1900_TO_1970;
	if (secs < 0) {
		if (ptr->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("NTP time is not valid, using value 0\n"));
		}
		secs = 0;
	}
	t = *gf_gmtime(&secs);
	fracs = (Double) (ptr->ntp & 0xFFFFFFFFULL);
	fracs /= 0xFFFFFFFF;
	fracs *= 1000;
	gf_isom_box_dump_start(a, "ProducerReferenceTimeBox", trace);

	gf_fprintf(trace, "referenceTrackID=\"%d\" timestamp=\""LLU"\" NTP=\""LLU"\" UTC=\"%d-%02d-%02dT%02d:%02d:%02d.%03dZ\">\n", ptr->refTrackID, ptr->timestamp, ptr->ntp, 1900+t.tm_year, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, (u32) t.tm_sec, (u32) fracs);
	gf_isom_box_dump_done("ProducerReferenceTimeBox", a, trace);

	return GF_OK;
}

GF_Err adkm_box_dump(GF_Box *a, FILE * trace)
{
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeDRMKeyManagementSystemBox", trace);

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AdobeDRMKeyManagementSystemBox", a, trace);
	return GF_OK;
}

GF_Err ahdr_box_dump(GF_Box *a, FILE * trace)
{
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeDRMHeaderBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AdobeDRMHeaderBox", a, trace);
	return GF_OK;
}

GF_Err aprm_box_dump(GF_Box *a, FILE * trace)
{
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeStdEncryptionParamsBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AdobeStdEncryptionParamsBox", a, trace);
	return GF_OK;
}

GF_Err aeib_box_dump(GF_Box *a, FILE * trace)
{
	GF_AdobeEncryptionInfoBox *ptr = (GF_AdobeEncryptionInfoBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeEncryptionInfoBox", trace);
	gf_fprintf(trace, "EncryptionAlgorithm=\"%s\" KeyLength=\"%d\">\n", ptr->enc_algo, ptr->key_length);
	gf_isom_box_dump_done("AdobeEncryptionInfoBox", a, trace);
	return GF_OK;
}

GF_Err akey_box_dump(GF_Box *a, FILE * trace)
{
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeKeyInfoBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AdobeKeyInfoBox", a, trace);
	return GF_OK;
}

GF_Err flxs_box_dump(GF_Box *a, FILE * trace)
{
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeFlashAccessParamsBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AdobeFlashAccessParamsBox", a, trace);
	return GF_OK;
}

GF_Err adaf_box_dump(GF_Box *a, FILE * trace)
{
	GF_AdobeDRMAUFormatBox *ptr = (GF_AdobeDRMAUFormatBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AdobeDRMAUFormatBox ", trace);
	gf_fprintf(trace, "SelectiveEncryption=\"%d\" IV_length=\"%d\">\n", ptr->selective_enc ? 1 : 0, ptr->IV_length);
	gf_isom_box_dump_done("AdobeDRMAUFormatBox", a, trace);
	return GF_OK;
}

/* Image File Format dump */
GF_Err ispe_box_dump(GF_Box *a, FILE * trace)
{
	GF_ImageSpatialExtentsPropertyBox *ptr = (GF_ImageSpatialExtentsPropertyBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ImageSpatialExtentsPropertyBox", trace);
	gf_fprintf(trace, "image_width=\"%d\" image_height=\"%d\">\n", ptr->image_width, ptr->image_height);
	gf_isom_box_dump_done("ImageSpatialExtentsPropertyBox", a, trace);
	return GF_OK;
}

GF_Err a1lx_box_dump(GF_Box *a, FILE * trace)
{
    GF_AV1LayeredImageIndexingPropertyBox *ptr = (GF_AV1LayeredImageIndexingPropertyBox*)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AV1LayeredImageIndexingPropertyBox", trace);
	gf_fprintf(trace, "large_size=\"%d\" layer_size0=\"%d\" layer_size1=\"%d\" layer_size2=\"%d\">\n", ptr->large_size, ptr->layer_size[0], ptr->layer_size[1], ptr->layer_size[2]);
	gf_isom_box_dump_done("AV1LayeredImageIndexingPropertyBox", a, trace);
	return GF_OK;
}

GF_Err a1op_box_dump(GF_Box *a, FILE * trace)
{
    GF_AV1OperatingPointSelectorPropertyBox *ptr = (GF_AV1OperatingPointSelectorPropertyBox*)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "AV1OperatingPointSelectorPropertyBox", trace);
	gf_fprintf(trace, "op_index=\"%d\">\n", ptr->op_index);
	gf_isom_box_dump_done("AV1OperatingPointSelectorPropertyBox", a, trace);
	return GF_OK;
}

GF_Err colr_box_dump(GF_Box *a, FILE * trace)
{
	u8 *prof_data_64=NULL;
	u32 size_64;
	GF_ColourInformationBox *ptr = (GF_ColourInformationBox *)a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "ColourInformationBox", trace);

	if (ptr->is_jp2) {
		gf_fprintf(trace, "method=\"%d\" precedence=\"%d\" approx=\"%d\"", ptr->method, ptr->precedence, ptr->approx);
		if (ptr->opaque_size) {
			gf_fprintf(trace, " colour=\"");
			dump_data_hex(trace, ptr->opaque,ptr->opaque_size);
			gf_fprintf(trace, "\"");
		}
		gf_fprintf(trace, ">\n");
	} else {
		switch (ptr->colour_type) {
		case GF_ISOM_SUBTYPE_NCLC:
			gf_fprintf(trace, "colour_type=\"%s\" colour_primaries=\"%d\" transfer_characteristics=\"%d\" matrix_coefficients=\"%d\">\n", gf_4cc_to_str(ptr->colour_type), ptr->colour_primaries, ptr->transfer_characteristics, ptr->matrix_coefficients);
			break;
		case GF_ISOM_SUBTYPE_NCLX:
			gf_fprintf(trace, "colour_type=\"%s\" colour_primaries=\"%d\" transfer_characteristics=\"%d\" matrix_coefficients=\"%d\" full_range_flag=\"%d\">\n", gf_4cc_to_str(ptr->colour_type), ptr->colour_primaries, ptr->transfer_characteristics, ptr->matrix_coefficients, ptr->full_range_flag);
			break;
		case GF_ISOM_SUBTYPE_PROF:
		case GF_ISOM_SUBTYPE_RICC:
			gf_fprintf(trace, "colour_type=\"%s\">\n", gf_4cc_to_str(ptr->colour_type));
			if (ptr->opaque != NULL) {
				gf_fprintf(trace, "<profile><![CDATA[");
				size_64 = 2*ptr->opaque_size+3;
				prof_data_64 = gf_malloc(sizeof(char) * size_64);
				size_64 = gf_base64_encode((const char *) ptr->opaque, ptr->opaque_size, (char *)prof_data_64, size_64);
				prof_data_64[size_64] = 0;
				gf_fprintf(trace, "%s", prof_data_64);
				gf_fprintf(trace, "]]></profile>");
			}
			break;
		default:
			gf_fprintf(trace, "colour_type=\"%s\">\n", gf_4cc_to_str(ptr->colour_type));
			break;
		}
	}

	gf_isom_box_dump_done("ColourInformationBox", a, trace);
	return GF_OK;
}

GF_Err pixi_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_PixelInformationPropertyBox *ptr = (GF_PixelInformationPropertyBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "PixelInformationPropertyBox", trace);
	gf_fprintf(trace, ">\n");
	for (i = 0; i < ptr->num_channels; i++) {
		gf_fprintf(trace, "<BitPerChannel bits_per_channel=\"%d\"/>\n", ptr->bits_per_channel[i]);
	}
	if (!ptr->size)
		gf_fprintf(trace, "<BitPerChannel bits_per_channel=\"\"/>\n");

	gf_isom_box_dump_done("PixelInformationPropertyBox", a, trace);
	return GF_OK;
}

GF_Err rloc_box_dump(GF_Box *a, FILE * trace)
{
	GF_RelativeLocationPropertyBox *ptr = (GF_RelativeLocationPropertyBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "RelativeLocationPropertyBox", trace);
	gf_fprintf(trace, "horizontal_offset=\"%d\" vertical_offset=\"%d\">\n", ptr->horizontal_offset, ptr->vertical_offset);
	gf_isom_box_dump_done("RelativeLocationPropertyBox", a, trace);
	return GF_OK;
}

GF_Err irot_box_dump(GF_Box *a, FILE * trace)
{
	GF_ImageRotationBox *ptr = (GF_ImageRotationBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ImageRotationBox", trace);
	gf_fprintf(trace, "angle=\"%d\">\n", (ptr->angle*90));
	gf_isom_box_dump_done("ImageRotationBox", a, trace);
	return GF_OK;
}

GF_Err imir_box_dump(GF_Box *a, FILE * trace)
{
	GF_ImageMirrorBox *ptr = (GF_ImageMirrorBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ImageMirrorBox", trace);
	gf_fprintf(trace, "axis=\"%s\">\n", (ptr->axis ? "horizontal" : "vertical"));
	gf_isom_box_dump_done("ImageMirrorBox", a, trace);
	return GF_OK;
}

GF_Err clli_box_dump(GF_Box *a, FILE * trace)
{
	GF_ContentLightLevelBox *ptr = (GF_ContentLightLevelBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ContentLightLevelBox", trace);
	gf_fprintf(trace, "max_content_light_level=\"%u\" max_pic_average_light_level=\"%u\">\n", ptr->clli.max_content_light_level, ptr->clli.max_pic_average_light_level);
	gf_isom_box_dump_done("ContentLightLevelBox", a, trace);
	return GF_OK;
}

GF_Err mdcv_box_dump(GF_Box *a, FILE * trace)
{
	int c = 0;
	GF_MasteringDisplayColourVolumeBox *ptr = (GF_MasteringDisplayColourVolumeBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "MasteringDisplayColourVolumeBox", trace);
	for (c = 0; c < 3; c++) {
		gf_fprintf(trace, "display_primaries_%d_x=\"%u\" display_primaries_%d_y=\"%u\" ", c, ptr->mdcv.display_primaries[c].x, c, ptr->mdcv.display_primaries[c].y);
	}
	gf_fprintf(trace, "white_point_x=\"%u\" white_point_y=\"%u\" max_display_mastering_luminance=\"%u\" min_display_mastering_luminance=\"%u\">\n", ptr->mdcv.white_point_x, ptr->mdcv.white_point_y, ptr->mdcv.max_display_mastering_luminance, ptr->mdcv.min_display_mastering_luminance);
	gf_isom_box_dump_done("MasteringDisplayColourVolumeBox", a, trace);
	return GF_OK;
}

GF_Err ipco_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ItemPropertyContainerBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ItemPropertyContainerBox", a, trace);
	return GF_OK;
}

GF_Err iprp_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "ItemPropertiesBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("ItemPropertiesBox", a, trace);
	return GF_OK;
}

GF_Err ipma_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, j;
	GF_ItemPropertyAssociationBox *ptr = (GF_ItemPropertyAssociationBox *)a;
	u32 entry_count;
	if (!a) return GF_BAD_PARAM;
	entry_count = gf_list_count(ptr->entries);
	gf_isom_box_dump_start(a, "ItemPropertyAssociationBox", trace);
	gf_fprintf(trace, "entry_count=\"%d\">\n", entry_count);
	for (i = 0; i < entry_count; i++) {
		GF_ItemPropertyAssociationEntry *entry = (GF_ItemPropertyAssociationEntry *)gf_list_get(ptr->entries, i);
		gf_fprintf(trace, "<AssociationEntry item_ID=\"%d\" association_count=\"%d\">\n", entry->item_id, entry->nb_associations);
		for (j = 0; j < entry->nb_associations; j++) {
			gf_fprintf(trace, "<Property index=\"%d\" essential=\"%d\"/>\n", entry->associations[j].index, entry->associations[j].essential);
		}
		gf_fprintf(trace, "</AssociationEntry>\n");
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<AssociationEntry item_ID=\"\" association_count=\"\">\n");
		gf_fprintf(trace, "<Property index=\"\" essential=\"\"/>\n");
		gf_fprintf(trace, "</AssociationEntry>\n");
	}
	gf_isom_box_dump_done("ItemPropertyAssociationBox", a, trace);
	return GF_OK;
}

GF_Err auxc_box_dump(GF_Box *a, FILE * trace)
{
	GF_AuxiliaryTypePropertyBox *ptr = (GF_AuxiliaryTypePropertyBox *)a;

	gf_isom_box_dump_start(a, "AuxiliaryTypePropertyBox", trace);
	gf_fprintf(trace, "aux_type=\"%s\" ", ptr->aux_urn);
	dump_data_attribute(trace, "aux_subtype", ptr->data, ptr->data_size);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AuxiliaryTypePropertyBox", a, trace);
	return GF_OK;
}

GF_Err auxi_box_dump(GF_Box *a, FILE * trace)
{
	GF_AuxiliaryTypeInfoBox *ptr = (GF_AuxiliaryTypeInfoBox *)a;

	gf_isom_box_dump_start(a, "AuxiliaryTypeInfoBox", trace);
	gf_fprintf(trace, "aux_track_type=\"%s\" ", ptr->aux_track_type);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("AuxiliaryTypeInfoBox", a, trace);
	return GF_OK;
}

GF_Err oinf_box_dump(GF_Box *a, FILE * trace)
{
	GF_OINFPropertyBox *ptr = (GF_OINFPropertyBox *)a;
	gf_isom_box_dump_start(a, "OperatingPointsInformationPropertyBox", trace);
	gf_fprintf(trace, ">\n");

	oinf_entry_dump(ptr->oinf, trace);

	gf_isom_box_dump_done("OperatingPointsInformationPropertyBox", a, trace);
	return GF_OK;
}
GF_Err tols_box_dump(GF_Box *a, FILE * trace)
{
	GF_TargetOLSPropertyBox *ptr = (GF_TargetOLSPropertyBox *)a;
	gf_isom_box_dump_start(a, "TargetOLSPropertyBox", trace);
	gf_fprintf(trace, "target_ols_index=\"%d\">\n", ptr->target_ols_index);

	gf_isom_box_dump_done("TargetOLSPropertyBox", a, trace);
	return GF_OK;
}

GF_Err trgr_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "TrackGroupBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("TrackGroupBox", a, trace);
	return GF_OK;
}

GF_Err trgt_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackGroupTypeBox *ptr = (GF_TrackGroupTypeBox *) a;
	a->type = ptr->group_type;
	gf_isom_box_dump_start(a, "TrackGroupTypeBox", trace);
	a->type = GF_ISOM_BOX_TYPE_TRGT;
	gf_fprintf(trace, "track_group_id=\"%d\">\n", ptr->track_group_id);
	gf_isom_box_dump_done("TrackGroupTypeBox", a, trace);
	return GF_OK;
}

GF_Err grpl_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "GroupListBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("GroupListBox", a, trace);
	return GF_OK;
}

GF_Err grptype_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_EntityToGroupTypeBox *ptr = (GF_EntityToGroupTypeBox *) a;
	a->type = ptr->grouping_type;
	gf_isom_box_dump_start(a, "EntityToGroupTypeBox", trace);
	a->type = GF_ISOM_BOX_TYPE_GRPT;
	gf_fprintf(trace, "group_id=\"%d\">\n", ptr->group_id);

	for (i=0; i<ptr->entity_id_count ; i++)
		gf_fprintf(trace, "<EntityToGroupTypeBoxEntry EntityID=\"%d\"/>\n", ptr->entity_ids[i]);

	if (!ptr->size)
		gf_fprintf(trace, "<EntityToGroupTypeBoxEntry EntityID=\"\"/>\n");

	gf_isom_box_dump_done("EntityToGroupTypeBox", a, trace);
	return GF_OK;
}

GF_Err stvi_box_dump(GF_Box *a, FILE * trace)
{
	GF_StereoVideoBox *ptr = (GF_StereoVideoBox *) a;
	gf_isom_box_dump_start(a, "StereoVideoBox", trace);

	gf_fprintf(trace, "single_view_allowed=\"%d\" stereo_scheme=\"%d\" ", ptr->single_view_allowed, ptr->stereo_scheme);
	dump_data_attribute(trace, "stereo_indication_type", ptr->stereo_indication_type, ptr->sit_len);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("StereoVideoBox", a, trace);
	return GF_OK;
}

GF_Err def_parent_box_dump(GF_Box *a, FILE *trace)
{
	char *name = "GenericContainerBox";

	switch (a->type) {
	case GF_QT_BOX_TYPE_WAVE:
		name = "DecompressionParamBox";
		break;
	case GF_QT_BOX_TYPE_TMCD:
		name = "TimeCodeBox";
		break;
	case GF_ISOM_BOX_TYPE_GMHD:
		name = "GenericMediaHeaderBox";
		break;
	case GF_QT_BOX_TYPE_TAPT:
		name = "TrackApertureBox";
		break;
	case GF_ISOM_BOX_TYPE_STRD:
		name = "SubTrackDefinitionBox";
		break;
	case GF_ISOM_BOX_TYPE_SV3D:
		name = "SphericalVideoBox";
		break;
	case GF_ISOM_BOX_TYPE_PROJ:
		name = "ProjectionBox";
		break;
	case GF_ISOM_BOX_TYPE_OTYP:
		name = "OriginalFileTypeBox";
		break;
	}

	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}

GF_Err def_parent_full_box_dump(GF_Box *a, FILE *trace)
{
	char *name = "GenericFullBox";

	switch (a->type) {
	case GF_ISOM_BOX_TYPE_MVCI:
		name = "MultiviewInformationBox";
		break;
	}

	gf_isom_box_dump_start(a, name, trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done(name, a, trace);
	return GF_OK;
}
GF_Err fiin_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "FDItemInformationBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("FDItemInformationBox", a, trace);
	return GF_OK;
}

GF_Err fecr_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	char *box_name;
	FECReservoirBox *ptr = (FECReservoirBox *) a;
	if (a->type==GF_ISOM_BOX_TYPE_FIRE) {
		box_name = "FILEReservoirBox";
	} else {
		box_name = "FECReservoirBox";
	}
	gf_isom_box_dump_start(a, box_name, trace);

	gf_fprintf(trace, ">\n");

	for (i=0; i<ptr->nb_entries; i++) {
		gf_fprintf(trace, "<%sEntry itemID=\"%d\" symbol_count=\"%d\"/>\n", box_name, ptr->entries[i].item_id, ptr->entries[i].symbol_count);
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<%sEntry itemID=\"\" symbol_count=\"\"/>\n", box_name);
	}
	gf_isom_box_dump_done(box_name, a, trace);
	return GF_OK;
}

GF_Err gitn_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GroupIdToNameBox *ptr = (GroupIdToNameBox *) a;
	gf_isom_box_dump_start(a, "GroupIdToNameBox", trace);

	gf_fprintf(trace, ">\n");

	for (i=0; i<ptr->nb_entries; i++) {
		gf_fprintf(trace, "<GroupIdToNameBoxEntry groupID=\"%d\" name=\"%s\"/>\n", ptr->entries[i].group_id, ptr->entries[i].name);
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<GroupIdToNameBoxEntryEntry groupID=\"\" name=\"\"/>\n");
	}

	gf_isom_box_dump_done("GroupIdToNameBox", a, trace);
	return GF_OK;
}

GF_Err paen_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "FDPartitionEntryBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("FDPartitionEntryBox", a, trace);
	return GF_OK;
}

GF_Err fpar_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	FilePartitionBox *ptr = (FilePartitionBox *) a;
	gf_isom_box_dump_start(a, "FilePartitionBox", trace);

	gf_fprintf(trace, "itemID=\"%d\" FEC_encoding_ID=\"%d\" FEC_instance_ID=\"%d\" max_source_block_length=\"%d\" encoding_symbol_length=\"%d\" max_number_of_encoding_symbols=\"%d\" ", ptr->itemID, ptr->FEC_encoding_ID, ptr->FEC_instance_ID, ptr->max_source_block_length, ptr->encoding_symbol_length, ptr->max_number_of_encoding_symbols);

	if (ptr->scheme_specific_info)
		dump_data_attribute(trace, "scheme_specific_info", (char*)ptr->scheme_specific_info, (u32)strlen(ptr->scheme_specific_info) );

	gf_fprintf(trace, ">\n");

	for (i=0; i<ptr->nb_entries; i++) {
		gf_fprintf(trace, "<FilePartitionBoxEntry block_count=\"%d\" block_size=\"%d\"/>\n", ptr->entries[i].block_count, ptr->entries[i].block_size);
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<FilePartitionBoxEntry block_count=\"\" block_size=\"\"/>\n");
	}

	gf_isom_box_dump_done("FilePartitionBox", a, trace);
	return GF_OK;
}

GF_Err segr_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, k;
	FDSessionGroupBox *ptr = (FDSessionGroupBox *) a;
	gf_isom_box_dump_start(a, "FDSessionGroupBox", trace);
	gf_fprintf(trace, ">\n");

	for (i=0; i<ptr->num_session_groups; i++) {
		gf_fprintf(trace, "<FDSessionGroupBoxEntry groupIDs=\"");
		for (k=0; k<ptr->session_groups[i].nb_groups; k++) {
			gf_fprintf(trace, "%d ", ptr->session_groups[i].group_ids[k]);
		}
		gf_fprintf(trace, "\" channels=\"");
		for (k=0; k<ptr->session_groups[i].nb_channels; k++) {
			gf_fprintf(trace, "%d ", ptr->session_groups[i].channels[k]);
		}
		gf_fprintf(trace, "\"/>\n");
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<FDSessionGroupBoxEntry groupIDs=\"\" channels=\"\"/>\n");
	}

	gf_isom_box_dump_done("FDSessionGroupBox", a, trace);
	return GF_OK;
}

GF_Err srpp_box_dump(GF_Box *a, FILE * trace)
{
	GF_SRTPProcessBox *ptr = (GF_SRTPProcessBox *) a;
	gf_isom_box_dump_start(a, "SRTPProcessBox", trace);

	gf_fprintf(trace, "encryption_algorithm_rtp=\"%d\" encryption_algorithm_rtcp=\"%d\" integrity_algorithm_rtp=\"%d\" integrity_algorithm_rtcp=\"%d\">\n", ptr->encryption_algorithm_rtp, ptr->encryption_algorithm_rtcp, ptr->integrity_algorithm_rtp, ptr->integrity_algorithm_rtcp);
	gf_isom_box_dump_done("SRTPProcessBox", a, trace);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_HINTING

GF_Err fdpa_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_FDpacketBox *ptr = (GF_FDpacketBox *) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "FDpacketBox", trace);
	gf_fprintf(trace, "sender_current_time_present=\"%d\" expected_residual_time_present=\"%d\" session_close_bit=\"%d\" object_close_bit=\"%d\" transport_object_identifier=\"%d\">\n", ptr->info.sender_current_time_present, ptr->info.expected_residual_time_present, ptr->info.session_close_bit, ptr->info.object_close_bit, ptr->info.transport_object_identifier);

	for (i=0; i<ptr->header_ext_count; i++) {
		gf_fprintf(trace, "<FDHeaderExt type=\"%d\"", ptr->headers[i].header_extension_type);
		if (ptr->headers[i].header_extension_type > 127) {
			dump_data_attribute(trace, "content", (char *) ptr->headers[i].content, 3);
		} else if (ptr->headers[i].data_length) {
			dump_data_attribute(trace, "data", ptr->headers[i].data, ptr->headers[i].data_length);
		}
		gf_fprintf(trace, "/>\n");
	}
	if (!ptr->size) {
		gf_fprintf(trace, "<FDHeaderExt type=\"\" content=\"\" data=\"\"/>\n");
	}
	gf_isom_box_dump_done("FDpacketBox", a, trace);
	return GF_OK;
}

GF_Err extr_box_dump(GF_Box *a, FILE * trace)
{
	GF_ExtraDataBox *ptr = (GF_ExtraDataBox *) a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ExtraDataBox", trace);
	dump_data_attribute(trace, "data", ptr->data, ptr->data_length);
	gf_fprintf(trace, ">\n");
	if (ptr->feci)
		gf_isom_box_dump(ptr->feci, trace);
	gf_isom_box_dump_done("ExtraDataBox", a, trace);
	return GF_OK;
}

GF_Err fdsa_box_dump(GF_Box *a, FILE * trace)
{
	GF_Err e;
	GF_HintSample *ptr = (GF_HintSample *) a;
	if (!a) return GF_BAD_PARAM;

	gf_isom_box_dump_start(a, "FDSampleBox", trace);
	gf_fprintf(trace, ">\n");

	e = gf_isom_box_array_dump(ptr->packetTable, trace);
	if (e) return e;
	gf_isom_box_dump_done("FDSampleBox", a, trace);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_HINTING*/

GF_Err trik_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_TrickPlayBox *p = (GF_TrickPlayBox *) a;

	gf_isom_box_dump_start(a, "TrickPlayBox", trace);

	gf_fprintf(trace, ">\n");
	for (i=0; i<p->entry_count; i++) {
		gf_fprintf(trace, "<TrickPlayBoxEntry pic_type=\"%d\" dependency_level=\"%d\"/>\n", p->entries[i].pic_type, p->entries[i].dependency_level);
	}
	if (!p->size)
		gf_fprintf(trace, "<TrickPlayBoxEntry pic_type=\"\" dependency_level=\"\"/>\n");

	gf_isom_box_dump_done("TrickPlayBox", a, trace);
	return GF_OK;
}

GF_Err bloc_box_dump(GF_Box *a, FILE * trace)
{
	GF_BaseLocationBox *p = (GF_BaseLocationBox *) a;

	gf_isom_box_dump_start(a, "BaseLocationBox", trace);

	gf_fprintf(trace, "baseLocation=\"%s\" basePurlLocation=\"%s\">\n", p->baseLocation, p->basePurlLocation);
	gf_isom_box_dump_done("BaseLocationBox", a, trace);
	return GF_OK;
}

GF_Err ainf_box_dump(GF_Box *a, FILE * trace)
{
	GF_AssetInformationBox *p = (GF_AssetInformationBox *) a;

	gf_isom_box_dump_start(a, "AssetInformationBox", trace);

	gf_fprintf(trace, "profile_version=\"%d\" APID=\"%s\">\n", p->profile_version, p->APID);
	gf_isom_box_dump_done("AssetInformationBox", a, trace);
	return GF_OK;
}


GF_Err mhac_box_dump(GF_Box *a, FILE * trace)
{
	GF_MHAConfigBox *p = (GF_MHAConfigBox *) a;

	gf_isom_box_dump_start(a, "MHAConfigurationBox", trace);

	gf_fprintf(trace, "configurationVersion=\"%d\" mpegh3daProfileLevelIndication=\"0x%02X\" referenceChannelLayout=\"%d\" data=\"", p->configuration_version, p->mha_pl_indication, p->reference_channel_layout);
	dump_data(trace, p->mha_config, p->mha_config_size);
	gf_fprintf(trace, "\">\n");

	gf_isom_box_dump_done("MHAConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err mhap_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_MHACompatibleProfilesBox *p = (GF_MHACompatibleProfilesBox *) a;

	gf_isom_box_dump_start(a, "MHACompatibleProfilesBox", trace);
	gf_fprintf(trace, "compatible_profiles=\"");
	for (i=0; i<p->num_profiles; i++) {
		if (i)
			gf_fprintf(trace, " ", p->compat_profiles[i]);
		gf_fprintf(trace, "%d", p->compat_profiles[i]);
	}
	gf_fprintf(trace, "\">\n");
	gf_isom_box_dump_done("MHACompatibleProfilesBox", a, trace);
	return GF_OK;
}
GF_Err tmcd_box_dump(GF_Box *a, FILE * trace)
{
	GF_TimeCodeSampleEntryBox *p = (GF_TimeCodeSampleEntryBox *) a;

	gf_isom_box_dump_start(a, "TimeCodeSampleEntryBox", trace);

	gf_fprintf(trace, "DataReferenceIndex=\"%d\" Flags=\"%08X\" TimeScale=\"%d\" FrameDuration=\"%d\" FramesPerTick=\"%d\">\n", p->dataReferenceIndex, p->flags, p->timescale, p->frame_duration, p->frames_per_counter_tick);

	gf_isom_box_dump_done("TimeCodeSampleEntryBox", a, trace);
	return GF_OK;
}

GF_Err tcmi_box_dump(GF_Box *a, FILE * trace)
{
	GF_TimeCodeMediaInformationBox *p = (GF_TimeCodeMediaInformationBox *) a;

	gf_isom_box_dump_start(a, "TimeCodeMediaInformationBox", trace);
	gf_fprintf(trace, "textFont=\"%d\" textFace=\"%d\" textSize=\"%d\" textColorRed=\"%d\" textColorGreen=\"%d\" textColorBlue=\"%d\" backColorRed=\"%d\" backColorGreen=\"%d\" backColorBlue=\"%d\"",
			p->text_font, p->text_face, p->text_size, p->text_color_red, p->text_color_green, p->text_color_blue, p->back_color_red, p->back_color_green, p->back_color_blue);
	if (p->font)
		gf_fprintf(trace, " font=\"%s\"", p->font);

	gf_fprintf(trace, ">\n");

	gf_isom_box_dump_done("TimeCodeMediaInformationBox", a, trace);
	return GF_OK;
}

GF_Err fiel_box_dump(GF_Box *a, FILE * trace)
{
	GF_FieldInfoBox *p = (GF_FieldInfoBox *) a;

	gf_isom_box_dump_start(a, "FieldInfoBox", trace);
	gf_fprintf(trace, "count=\"%d\" order=\"%d\">\n", p->field_count, p->field_order);
	gf_isom_box_dump_done("FieldInfoBox", a, trace);
	return GF_OK;
}

GF_Err gama_box_dump(GF_Box *a, FILE * trace)
{
	GF_GamaInfoBox *p = (GF_GamaInfoBox *) a;

	gf_isom_box_dump_start(a, "GamaInfoBox", trace);
	gf_fprintf(trace, "gama=\"%d\">\n", p->gama);
	gf_isom_box_dump_done("GamaInfoBox", a, trace);
	return GF_OK;
}

GF_Err chrm_box_dump(GF_Box *a, FILE * trace)
{
	GF_ChromaInfoBox *p = (GF_ChromaInfoBox *) a;
	if (a->type==GF_QT_BOX_TYPE_ENDA) {
		gf_isom_box_dump_start(a, "AudioEndianBox", trace);
		gf_fprintf(trace, "littleEndian=\"%d\">\n", p->chroma);
		gf_isom_box_dump_done("AudioEndianBox", a, trace);
	} else {
		gf_isom_box_dump_start(a, "ChromaInfoBox", trace);
		gf_fprintf(trace, "chroma=\"%d\">\n", p->chroma);
		gf_isom_box_dump_done("ChromaInfoBox", a, trace);
	}
	return GF_OK;
}

GF_Err chan_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_ChannelLayoutInfoBox *p = (GF_ChannelLayoutInfoBox *) a;

	gf_isom_box_dump_start(a, "ChannelLayoutInfoBox", trace);
	gf_fprintf(trace, "layout=\"%d\" bitmap=\"%d\">\n", p->layout_tag, p->bitmap);
	for (i=0; i<p->num_audio_description; i++) {
		GF_AudioChannelDescription *adesc = &p->audio_descs[i];
		gf_fprintf(trace, "<AudioChannelDescription label=\"%d\" flags=\"%08X\" coordinates=\"%f %f %f\"/>\n", adesc->label, adesc->flags, adesc->coordinates[0], adesc->coordinates[1], adesc->coordinates[2]);
	}
	gf_isom_box_dump_done("ChannelLayoutInfoBox", a, trace);
	return GF_OK;
}


GF_Err jp2h_box_dump(GF_Box *a, FILE * trace)
{
	gf_isom_box_dump_start(a, "JP2HeaderBox", trace);
	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("JP2HeaderBox", a, trace);
	return GF_OK;
}

GF_Err ihdr_box_dump(GF_Box *a, FILE * trace)
{
	GF_J2KImageHeaderBox  *p = (GF_J2KImageHeaderBox *) a;
	gf_isom_box_dump_start(a, "ImageHeaderBox", trace);
	gf_fprintf(trace, "width=\"%d\" height=\"%d\" nb_comp=\"%d\" BPC=\"%d\" Compression=\"%d\" UnkC=\"%d\" IPR=\"%d\">\n", p->width, p->height, p->nb_comp, p->bpc, p->Comp, p->UnkC, p->IPR);
	gf_isom_box_dump_done("ImageHeaderBox", a, trace);
	return GF_OK;
}

GF_Err dfla_box_dump(GF_Box *a, FILE * trace)
{
	GF_FLACConfigBox *ptr = (GF_FLACConfigBox *)a;
	gf_isom_box_dump_start(a, "FLACSpecificBox", trace);
	gf_fprintf(trace, " dataSize=\"%d\">\n", ptr->dataSize);
	gf_isom_box_dump_done("FLACSpecificBox", a, trace);
	return GF_OK;
}


GF_Err mvcg_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_MultiviewGroupBox *ptr = (GF_MultiviewGroupBox *)a;
	gf_isom_box_dump_start(a, "MultiviewGroupBox", trace);
	gf_fprintf(trace, " multiview_group_id=\"%d\">\n", ptr->multiview_group_id);
	for (i=0; i<ptr->num_entries; i++) {
		gf_fprintf(trace, "<MVCIEntry type=\"%d\"", ptr->entries[i].entry_type);
		switch (ptr->entries[i].entry_type) {
		case 0:
			gf_fprintf(trace, " trackID=\"%d\"", ptr->entries[i].trackID);
			break;
		case 1:
			gf_fprintf(trace, " trackID=\"%d\" tierID=\"%d\"", ptr->entries[i].trackID, ptr->entries[i].tierID);
			break;
		case 2:
			gf_fprintf(trace, " output_view_id=\"%d\"", ptr->entries[i].output_view_id);
			break;
		case 3:
			gf_fprintf(trace, " start_view_id=\"%d\" view_count=\"%d\"", ptr->entries[i].start_view_id, ptr->entries[i].view_count);
			break;
		}
		gf_fprintf(trace, "/>\n");
	}
	gf_isom_box_dump_done("MultiviewGroupBox", a, trace);
	return GF_OK;
}

GF_Err vwid_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, j;
	GF_ViewIdentifierBox *ptr = (GF_ViewIdentifierBox *) a;
	gf_isom_box_dump_start(a, "ViewIdentifierBox", trace);
	gf_fprintf(trace, " min_temporal_id=\"%d\" max_temporal_id=\"%d\">\n", ptr->min_temporal_id, ptr->max_temporal_id);
	for (i=0; i<ptr->num_views; i++) {
		gf_fprintf(trace, "<ViewInfo viewid=\"%d\" viewOrderindex=\"%d\" texInStream=\"%d\" texInTrack=\"%d\" depthInStream=\"%d\" depthInTrack=\"%d\" baseViewId=\"%d\">\n",
			ptr->views[i].view_id,
			ptr->views[i].view_order_index,
			ptr->views[i].texture_in_stream,
			ptr->views[i].texture_in_track,
			ptr->views[i].depth_in_stream,
			ptr->views[i].depth_in_track,
			ptr->views[i].base_view_type
		);
		for (j=0; j<ptr->views[i].num_ref_views; j++) {
			gf_fprintf(trace, "<RefViewInfo dependentComponentIDC=\"%d\" referenceViewID=\"%d\"/>\n", ptr->views[i].view_refs[j].dep_comp_idc, ptr->views[i].view_refs[j].ref_view_id);
		}
		gf_fprintf(trace, "</ViewInfo>\n");
	}
	gf_isom_box_dump_done("ViewIdentifierBox", a, trace);
	return GF_OK;
}

GF_Err pcmC_box_dump(GF_Box *a, FILE * trace)
{
	GF_PCMConfigBox *p = (GF_PCMConfigBox *) a;

	gf_isom_box_dump_start(a, "PCMConfigurationBox", trace);
	gf_fprintf(trace, " format_flags=\"%d\" PCM_sample_size=\"%d\">\n", p->format_flags, p->PCM_sample_size);
	gf_isom_box_dump_done("PCMConfigurationBox", a, trace);
	return GF_OK;
}

GF_Err chnl_box_dump(GF_Box *a, FILE * trace)
{
	GF_ChannelLayoutBox *p = (GF_ChannelLayoutBox *) a;
	gf_isom_box_dump_start(a, "ChannelLayoutBox", trace);
	gf_fprintf(trace, " stream_structure=\"%d\"", p->layout.stream_structure);
	if (p->layout.stream_structure & 2)
		gf_fprintf(trace, " object_count=\"%d\"", p->layout.object_count);

	if (p->layout.stream_structure & 1) {
		gf_fprintf(trace, " definedLayout=\"%d\"", p->layout.definedLayout);
		if (p->layout.definedLayout!=0) {
			gf_fprintf(trace, " omittedChannelsMap=\""LLU"\"", p->layout.omittedChannelsMap);
		}
	}

	gf_fprintf(trace, ">\n");
	if ((p->layout.stream_structure & 1) && (p->layout.definedLayout==0)) {
		u32 i;
		for (i=0; i<p->layout.channels_count; i++) {
			gf_fprintf(trace, "<Speaker position=\"%d\"", p->layout.layouts[i].position);
			if (p->layout.layouts[i].position==126) {
				gf_fprintf(trace, " azimuth=\"%d\" elevation=\"%d\"", p->layout.layouts[i].azimuth, p->layout.layouts[i].elevation);
			}
			gf_fprintf(trace, "/>\n");
		}
	}

	gf_isom_box_dump_done("ChannelLayoutBox", a, trace);
	return GF_OK;
}

GF_Err load_box_dump(GF_Box *a, FILE * trace)
{
	GF_TrackLoadBox *p = (GF_TrackLoadBox *) a;

	gf_isom_box_dump_start(a, "TrackLoadBox", trace);
	fprintf(trace, "preload_start_time=\"%d\" preload_duration=\"%d\" preload_flags=\"%d\" default_hints=\"%d\">\n", p->preload_start_time, p->preload_duration, p->preload_flags, p->default_hints);
	gf_isom_box_dump_done("TrackLoadBox", a, trace);
	return GF_OK;
}

GF_Err emsg_box_dump(GF_Box *a, FILE * trace)
{
	GF_EventMessageBox *p = (GF_EventMessageBox *) a;

	gf_isom_box_dump_start(a, "EventMessageBox", trace);
	fprintf(trace, "timescale=\"%u\" presentation_time%s=\""LLU"\" event_duration=\"%u\" event_id=\"%u\"",
		p->timescale, (p->version==0) ? "_delta" : "", p->presentation_time_delta, p->event_duration, p->event_id);

	if (p->scheme_id_uri)
		fprintf(trace, " scheme_id_uri=\"%s\"", p->scheme_id_uri);
	if (p->value)
		fprintf(trace, " value=\"%s\"", p->value);

	if (p->message_data)
		dump_data_attribute(trace, " message_data", p->message_data, p->message_data_size);

	gf_fprintf(trace, ">\n");
	gf_isom_box_dump_done("EventMessageBox", a, trace);
	return GF_OK;
}

GF_Err csgp_box_dump(GF_Box *a, FILE * trace)
{
	u32 i;
	GF_CompactSampleGroupBox *ptr = (GF_CompactSampleGroupBox*)a;
	Bool use_msb_traf = ptr->flags & (1<<7);
	Bool use_grpt_param = ptr->flags & (1<<6);

	gf_isom_box_dump_start(a, "CompactSampleGroupBox", trace);
	fprintf(trace, "version=\"%u\" index_msb_indicates_fragment_local_description=\"%d\" grouping_type_parameter_present=\"%d\" pattern_size_code=\"%d\" count_size_code=\"%d\" index_size_code=\"%d\" grouping_type=\"%s\" pattern_count=\"%d\"",
		ptr->version,
		use_msb_traf,
		use_grpt_param,
		((ptr->flags>>4) & 0x3),
		((ptr->flags>>2) & 0x3),
		(ptr->flags & 0x3),
		gf_4cc_to_str(ptr->grouping_type),
		ptr->pattern_count
	);

	if (use_grpt_param)
		fprintf(trace, " grouping_type_paramter=\"%u\"", ptr->grouping_type_parameter);
	fprintf(trace, ">\n");

	for (i=0; i<ptr->pattern_count; i++) {
		u32 j;
		fprintf(trace, "<Pattern length=\"%u\" sample_count=\"%u\" sample_group_indices=\"", ptr->patterns[i].length, ptr->patterns[i].sample_count);
		for (j=0; j<ptr->patterns[i].length; j++) {
			u32 idx = ptr->patterns[i].sample_group_description_indices[j];
			if (j) fprintf(trace, " ");
			if (use_msb_traf && (idx>0x10000))
				fprintf(trace, "%d(traf)", idx-0x10000);
			else
				fprintf(trace, "%d", idx);
		}
		fprintf(trace, "\"/>\n");
	}

	gf_isom_box_dump_done("CompactSampleGroupBox", a, trace);
	return GF_OK;
}


GF_Err ienc_box_dump(GF_Box *a, FILE * trace)
{
	u32 i, nb_keys, kpos;
	GF_ItemEncryptionPropertyBox *ptr = (GF_ItemEncryptionPropertyBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ItemEncryptionPropertyBox", trace);
	if (ptr->version)
		gf_fprintf(trace, " skip_byte_block=\"%d\" crypt_byte_block=\"%d\"", ptr->skip_byte_block, ptr->crypt_byte_block);
	gf_fprintf(trace, ">\n");
	nb_keys = ptr->key_info ? ptr->key_info[2] : 0;
	kpos = 3;
	for (i = 0; i < nb_keys; i++) {
		u8 iv_size = ptr->key_info[kpos];
		gf_fprintf(trace, "<KeyInfo KID=\"");
		dump_data_hex(trace, ptr->key_info + kpos + 1, 16);
		gf_fprintf(trace, "\"");
		kpos+=17;
		if (iv_size) {
			gf_fprintf(trace, " IV_size=\"%d\"/>\n", iv_size);
		} else {
			iv_size = ptr->key_info[kpos];
			gf_fprintf(trace, " constant_IV_size=\"%d\" constant_IV=\"", iv_size);
			dump_data_hex(trace, ptr->key_info + kpos + 1, iv_size);
			gf_fprintf(trace, "\"/>\n");
			kpos += 1 + iv_size;
		}
	}
	if (!ptr->size)
		gf_fprintf(trace, "<KeyInfo KID=\"\" IV_size=\"\" constant_IV_size=\"\" constant_IV=\"\" />\n");

	gf_isom_box_dump_done("ItemEncryptionPropertyBox", a, trace);
	return GF_OK;
}

GF_Err iaux_box_dump(GF_Box *a, FILE * trace)
{
	GF_AuxiliaryInfoPropertyBox *ptr = (GF_AuxiliaryInfoPropertyBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ItemAuxiliaryInformationBox", trace);
	gf_fprintf(trace, " aux_info_type=\"%d\" aux_info_parameter=\"%d\">\n", ptr->aux_info_type, ptr->aux_info_parameter);
	gf_isom_box_dump_done("ItemAuxiliaryInformationBox", a, trace);
	return GF_OK;
}

#include <gpac/utf.h>

GF_Err xtra_box_dump(GF_Box *a, FILE * trace)
{
	GF_XtraBox *ptr = (GF_XtraBox *)a;
	u32 i, count = gf_list_count(ptr->tags);

	gf_isom_box_dump_start(a, "XtraBox", trace);
	gf_fprintf(trace, ">\n");
	for (i=0; i<count; i++) {
		GF_XtraTag *tag = gf_list_get(ptr->tags, i);

		gf_fprintf(trace, "<WMATag name=\"%s\" version=\"%d\" type=\"%d\"", tag->name, tag->flags, tag->prop_type);
		if (!tag->prop_type) {
			u16 *src_str = (u16 *) tag->prop_value;
			u32 len = UTF8_MAX_BYTES_PER_CHAR * gf_utf8_wcslen(src_str);
			char *utf8str = (char *)gf_malloc(len + 1);
			u32 res_len = gf_utf8_wcstombs(utf8str, len, (const unsigned short **) &src_str);
			if (res_len != GF_UTF8_FAIL) {
				utf8str[res_len] = 0;

				gf_fprintf(trace, " value=\"%s\">\n", utf8str);
			}
			gf_free(utf8str);
		} else {
			gf_fprintf(trace, " value=\"");
			dump_data_hex(trace, tag->prop_value, tag->prop_size);
			gf_fprintf(trace, "\">\n");
		}
	}
	gf_isom_box_dump_done("XtraBox", a, trace);
	return GF_OK;
}


GF_Err st3d_box_dump(GF_Box *a, FILE * trace)
{
	GF_Stereo3DBox  *ptr = (GF_Stereo3DBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "Stereo3DBox", trace);
	gf_fprintf(trace, " stereo_type=\"%d\">\n", ptr->stereo_type);
	gf_isom_box_dump_done("Stereo3DBox", a, trace);
	return GF_OK;
}

GF_Err svhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_SphericalVideoInfoBox *ptr = (GF_SphericalVideoInfoBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "SphericalVideoInfoBox", trace);
	gf_fprintf(trace, " info=\"%s\">\n", ptr->string);
	gf_isom_box_dump_done("SphericalVideoInfoBox", a, trace);
	return GF_OK;
}

GF_Err prhd_box_dump(GF_Box *a, FILE * trace)
{
	GF_ProjectionHeaderBox *ptr = (GF_ProjectionHeaderBox *)a;
	if (!a) return GF_BAD_PARAM;
	gf_isom_box_dump_start(a, "ProjectionHeaderBox", trace);
	gf_fprintf(trace, " yaw=\"%d\" pitch=\"%d\" roll=\"%d\">\n", ptr->yaw, ptr->pitch, ptr->roll);
	gf_isom_box_dump_done("ProjectionHeaderBox", a, trace);
	return GF_OK;
}

GF_Err proj_type_box_dump(GF_Box *a, FILE * trace)
{
	GF_ProjectionTypeBox *ptr = (GF_ProjectionTypeBox *)a;
	if (!a) return GF_BAD_PARAM;
	if (ptr->type == GF_ISOM_BOX_TYPE_CBMP) {
		gf_isom_box_dump_start(a, "CubemapProjectionBox", trace);
		gf_fprintf(trace, " layout=\"%d\" padding=\"%d\">\n", ptr->layout, ptr->padding);
		gf_isom_box_dump_done("CubemapProjectionBox", a, trace);
	}
	else if (ptr->type == GF_ISOM_BOX_TYPE_EQUI) {
		gf_isom_box_dump_start(a, "EquirectangularProjectionBox", trace);
		gf_fprintf(trace, " top=\"%d\" bottom=\"%d\" left=\"%d\" right=\"%d\">\n", ptr->bounds_top, ptr->bounds_bottom, ptr->bounds_left, ptr->bounds_right);
		gf_isom_box_dump_done("EquirectangularProjectionBox", a, trace);
	}
	else if (ptr->type == GF_ISOM_BOX_TYPE_MSHP) {
		gf_isom_box_dump_start(a, "MeshProjectionBox", trace);
		gf_fprintf(trace, " crc=\"%08X\" encoding=\"%s\" left=\"%d\" right=\"%d\">\n", ptr->crc, gf_4cc_to_str(ptr->encoding_4cc) );
		gf_isom_box_dump_done("MeshProjectionBox", a, trace);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_DUMP*/
