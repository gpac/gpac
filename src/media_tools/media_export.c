/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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


#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_READ_ONLY

#include <gpac/internal/avilib.h>
#include <gpac/internal/ogg.h>

static GF_Err gf_export_message(GF_MediaExporter *dumper, GF_Err e, char *format, ...)
{
	va_list args;
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return e;

	va_start(args, format);
	if (dumper->export_message) {
		char szMsg[1024];
		vsprintf(szMsg, format, args);
		dumper->export_message(dumper, e, szMsg);
	} else {
		vfprintf(stdout,format,args);
		fprintf(stdout, "\n");
		if (e) fprintf(stderr, "Error: %s\n", gf_error_to_string(e));
	}
	va_end(args);
	return e;
}

void dump_progress(GF_MediaExporter *dumper, u32 cur_samp, u32 count)
{
	if (dumper->export_progress) {
		dumper->export_progress(dumper, cur_samp, count);
	} else {
		gf_cbk_on_progress("Exporting", cur_samp, count);
	}
}

/*that's very very crude, we only support vorbis & theora in MP4 - this will need cleanup as soon as possible*/
static GF_Err gf_dump_to_ogg(GF_MediaExporter *dumper, char *szName, u32 track)
{
	FILE *out;
	ogg_stream_state os;
	ogg_packet op;
	ogg_page og;
	u32 count, i, di, theora_kgs, nb_i, nb_p;
	GF_BitStream *bs;
	GF_ISOSample *samp;
	GF_ESD *esd = gf_isom_get_esd(dumper->file, track, 1);


	gf_rand_init(1);
	ogg_stream_init(&os, gf_rand());

	op.granulepos = 0;
	op.packetno = 0;
	op.b_o_s = 1;
	op.e_o_s = 0;

	out = gf_f64_open(szName, "wb");
	if (!out) return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);

	theora_kgs = 0;
	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		op.bytes = gf_bs_read_u16(bs);
		op.packet = malloc(sizeof(char) * op.bytes);
		gf_bs_read_data(bs, op.packet, op.bytes);
		ogg_stream_packetin(&os, &op);

		if (op.b_o_s) {
			ogg_stream_pageout(&os, &og);
			fwrite(og.header, 1, og.header_len, out);
			fwrite(og.body, 1, og.body_len, out);
			op.b_o_s = 0;

			if (esd->decoderConfig->objectTypeIndication==0xDF) {
				u32 kff;
				GF_BitStream *vbs = gf_bs_new(op.packet, op.bytes, GF_BITSTREAM_READ);
				gf_bs_skip_bytes(vbs, 40);
				gf_bs_read_int(vbs, 6); /* quality */
				kff = 1 << gf_bs_read_int(vbs, 5);
				gf_bs_del(vbs);

				theora_kgs = 0;
				kff--;
				while (kff) {
					theora_kgs ++;
					kff >>= 1;
				}
			}
		}
		free(op.packet);
		op.packetno ++;
	}
	gf_bs_del(bs);
	gf_odf_desc_del((GF_Descriptor *)esd);

	while (ogg_stream_pageout(&os, &og)>0) {
		fwrite(og.header, 1, og.header_len, out);
		fwrite(og.body, 1, og.body_len, out);
	}
	
	op.granulepos = -1;

	count = gf_isom_get_sample_count(dumper->file, track);

	nb_i = nb_p = 0;
	samp = gf_isom_get_sample(dumper->file, track, 1, &di);
	for (i=0; i<count; i++) {
		GF_ISOSample *next_samp = gf_isom_get_sample(dumper->file, track, i+2, &di);
		if (!samp) break;
		op.bytes = samp->dataLength;
		op.packet = samp->data;
		op.packetno ++;

		if (theora_kgs) {
			if (samp->IsRAP) {
				if (i) nb_i+=nb_p+1;
				nb_p = 0;
			} else {
				nb_p++;
			}
			op.granulepos = nb_i;
			op.granulepos <<= theora_kgs;
			op.granulepos |= nb_p;
		} else {
			if (next_samp) op.granulepos = next_samp->DTS;
		}
		if (!next_samp) op.e_o_s = 1;

		ogg_stream_packetin(&os, &op);

		gf_isom_sample_del(&samp);
		samp = next_samp;
		next_samp = NULL;
		dump_progress(dumper, i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;

		while (ogg_stream_pageout(&os, &og)>0) {
			fwrite(og.header, 1, og.header_len, out);
			fwrite(og.body, 1, og.body_len, out);
		}
	}
	if (samp) gf_isom_sample_del(&samp);

	while (ogg_stream_flush(&os, &og)>0) {
		fwrite(og.header, 1, og.header_len, out);
		fwrite(og.body, 1, og.body_len, out);
	}
    ogg_stream_clear(&os);
	fclose(out);
	return GF_OK;
}

GF_Err gf_export_hint(GF_MediaExporter *dumper)
{
	GF_Err e;
	char szName[1000], szType[5];
	char *pck;
	FILE *out;
	u32 track, i, size, m_stype, sn, count;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	m_stype = gf_isom_get_media_subtype(dumper->file, track, 1);

	e = gf_isom_reset_hint_reader(dumper->file, track, dumper->sample_num ? dumper->sample_num : 1, 0, 0, 0);
	if (e) return gf_export_message(dumper, e, "Error initializing hint reader");

	gf_export_message(dumper, GF_OK, "Extracting hint track samples - type %s", szType);

	count = gf_isom_get_sample_count(dumper->file, track);
	if (dumper->sample_num) count = 0;

	i = 1;
	while (1) {
		e = gf_isom_next_hint_packet(dumper->file, track, &pck, &size, NULL, NULL, NULL, &sn);
		if (e==GF_EOS) break;
		if (dumper->sample_num && (dumper->sample_num != sn)) {
			free(pck);
			break;
		}
		if (e) return gf_export_message(dumper, e, "Error fetching hint packet %d", i);
		sprintf(szName, "%s_pck_%04d.%s", dumper->out_name, i, gf_4cc_to_str(m_stype));
		out = fopen(szName, "wb");
		fwrite(pck, size, 1, out);
		fclose(out);
		free(pck);
		i++;
		if (count) dump_progress(dumper, sn, count);
	}
	if (count) dump_progress(dumper, count, count);

	return GF_OK;
}

GF_Err gf_media_export_samples(GF_MediaExporter *dumper)
{
	GF_DecoderConfig *dcfg;
	GF_GenericSampleDescription *udesc;
	char szName[1000], szEXT[10], szNum[1000];
	FILE *out;
	GF_BitStream *bs;
	u32 track, i, di, count, m_type, m_stype;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	m_type = gf_isom_get_media_type(dumper->file, track);
	m_stype = gf_isom_get_media_subtype(dumper->file, track, 1);

	if (dumper->sample_num) sprintf(szNum, " %d", dumper->sample_num);
	else strcpy(szNum, "s");

	udesc = NULL;
	dcfg = NULL;
	if ((m_stype==GF_ISOM_SUBTYPE_MPEG4) || (m_stype==GF_ISOM_SUBTYPE_MPEG4_CRYP)) 
		dcfg = gf_isom_get_decoder_config(dumper->file, track, 1);

	strcpy(szName, dumper->out_name ? dumper->out_name : "");
	if (dcfg) {
		switch (dcfg->streamType) {
		case GF_STREAM_VISUAL:
			switch (dcfg->objectTypeIndication) {
			case 0x20:
				strcpy(szEXT, ".cmp");
				gf_export_message(dumper, GF_OK, "Dumping MPEG-4 Visual sample%s", szNum);
				break;
			case 0x21:
				strcpy(szEXT, ".h264");
				gf_export_message(dumper, GF_OK, "Dumping MPEG-4 AVC-H264 Visual sample%s", szNum);
				break;
			case 0x6C:
				strcpy(szEXT, ".jpg");
				gf_export_message(dumper, GF_OK, "Dumping JPEG image%s", szNum);
				break;
			case 0x6D:
				strcpy(szEXT, ".png");
				gf_export_message(dumper, GF_OK, "Dumping PNG image%s", szNum);
				break;
			case GPAC_OGG_MEDIA_OTI:
				strcpy(szEXT, ".theo");
				gf_export_message(dumper, GF_OK, "Dumping Theora video sample%s", szNum);
				break;
			default:
				strcpy(szEXT, ".raw");
				gf_export_message(dumper, GF_OK, "Dumping Unknown video sample%s (OTI %d)", szNum, dcfg->objectTypeIndication);
				break;
			}
			break;
		case GF_STREAM_AUDIO:
			switch (dcfg->objectTypeIndication) {
			case 0x66:
			case 0x67:
			case 0x68:
			case 0x40:
				strcpy(szEXT, ".aac");
				gf_export_message(dumper, GF_OK, "Dumping MPEG-%d AAC sample%s", (dcfg->objectTypeIndication==0x40) ? 4 : 2, szNum);
				break;
			case 0x69:
			case 0x6B:
				strcpy(szEXT, ".mp3");
				gf_export_message(dumper, GF_OK, "Dumping MPEG-1/2 Audio (MP3) sample%s", szNum);
				break;
			case GPAC_OGG_MEDIA_OTI:
				strcpy(szEXT, ".vorb");
				gf_export_message(dumper, GF_OK, "Dumping Vorbis audio sample%s", szNum);
				break;
			default:
				strcpy(szEXT, ".raw");
				gf_export_message(dumper, GF_OK, "Dumping Unknown audio sample%s (OTI %d)", szNum, dcfg->objectTypeIndication);
				break;
			}
			break;
		case GF_STREAM_SCENE:
			strcpy(szEXT, ".bifs"); gf_export_message(dumper, GF_OK, "Dumping BIFS sample%s", szNum); 
			break;
		case GF_STREAM_OD:
			strcpy(szEXT, ".od"); gf_export_message(dumper, GF_OK, "Dumping OD sample%s", szNum); 
			break;
		case GF_STREAM_MPEGJ:
			strcpy(szEXT, ".mpj"); gf_export_message(dumper, GF_OK, "Dumping MPEG-J sample%s", szNum); 
			break;
		case GF_STREAM_OCI:
			strcpy(szEXT, ".oci"); gf_export_message(dumper, GF_OK, "Dumping OCI sample%s", szNum); 
			break;
		case GF_STREAM_MPEG7:
			strcpy(szEXT, ".mp7"); gf_export_message(dumper, GF_OK, "Dumping MPEG7 sample%s", szNum); 
			break;
		case GF_STREAM_IPMP:
			strcpy(szEXT, ".ipmp"); gf_export_message(dumper, GF_OK, "Dumping IPMP sample%s", szNum); 
			break;
		case GF_STREAM_TEXT:
			strcpy(szEXT, ".tx3g"); gf_export_message(dumper, GF_OK, "Dumping 3GP Text sample%s", szNum); 
			break;
		default:
			gf_odf_desc_del((GF_Descriptor *) dcfg);
			return gf_export_message(dumper, GF_NOT_SUPPORTED, "Cannot dump systems track ID %d sample%s - use NHNT", dumper->trackID, szNum);
		}
		gf_odf_desc_del((GF_Descriptor *) dcfg);
	} else if ((m_stype==GF_ISOM_SUBTYPE_3GP_AMR) || (m_stype==GF_ISOM_SUBTYPE_3GP_AMR_WB)) {
		strcpy(szEXT, ".amr");
		gf_export_message(dumper, GF_OK, "Extracting AMR Audio sample%s", szNum);
	} else if (m_stype==GF_ISOM_SUBTYPE_3GP_H263) {
		gf_export_message(dumper, GF_OK, "Extracting H263 Video sample%s", szNum);
		strcpy(szEXT, ".263");
	} else if (m_stype==GF_ISOM_SUBTYPE_AVC_H264) {
		strcpy(szEXT, ".h264");
		gf_export_message(dumper, GF_OK, "Dumping MPEG-4 AVC-H264 Visual sample%s", szNum);
	} else if (m_type==GF_ISOM_MEDIA_FLASH) {
		gf_export_message(dumper, GF_OK, "Extracting Macromedia Flash Movie sample%s", szNum);
		strcpy(szEXT, ".swf");
	} else if (m_type==GF_ISOM_MEDIA_HINT) {
		return gf_export_hint(dumper);
	} else {
		strcpy(szEXT, ".");
		strcat(szEXT, gf_4cc_to_str(m_stype));
		udesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		switch (m_type) {
		case GF_ISOM_MEDIA_VISUAL: gf_export_message(dumper, GF_OK, "Extracting \'%s\' Video - Compressor %s", szEXT, udesc ? udesc->compressor_name: "Unknown"); break;
		case GF_ISOM_MEDIA_AUDIO: gf_export_message(dumper, GF_OK, "Extracting \'%s\' Audio - Compressor %s", szEXT, udesc ? udesc->compressor_name : "Unknown"); break;
		default:
			gf_export_message(dumper, GF_OK, "Extracting \'%s\' Track (type '%s') - Compressor %s sample%s", szEXT, gf_4cc_to_str(m_type), udesc ? udesc->compressor_name : "Unknown", szNum);
			break;
		}
		if (udesc) free(udesc);
	}
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_OK;

	if (dumper->sample_num) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, dumper->sample_num, &di);
		if (!samp) return GF_BAD_PARAM;
		sprintf(szName, "%s_%d%s", dumper->out_name, dumper->sample_num, szEXT);
		out = fopen(szName, "wb");
		bs = gf_bs_from_file(out, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs, samp->data, samp->dataLength);
		gf_isom_sample_del(&samp);
		gf_bs_del(bs);
		fclose(out);
		return GF_OK;
	}

	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		if (count>=1000) {
			sprintf(szName, "%s_%08d%s", dumper->out_name, i+1, szEXT);
		} else {
			sprintf(szName, "%s_%03d%s", dumper->out_name, i+1, szEXT);
		}
		out = fopen(szName, "wb");
		bs = gf_bs_from_file(out, GF_BITSTREAM_WRITE);
		gf_bs_write_data(bs, samp->data, samp->dataLength);
		gf_isom_sample_del(&samp);
		dump_progress(dumper, i+1, count);
		gf_bs_del(bs);
		fclose(out);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	return GF_OK;
}

/*QCP codec GUIDs*/
static const unsigned char *QCP_QCELP_GUID_1 = "\x41\x6D\x7F\x5E\x15\xB1\xD0\x11\xBA\x91\x00\x80\x5F\xB4\xB9\x7E";
static const unsigned char *QCP_EVRC_GUID = "\x8D\xD4\x89\xE6\x76\x90\xB5\x46\x91\xEF\x73\x6A\x51\x00\xCE\xB4";
static const unsigned char *QCP_SMV_GUID = "\x75\x2B\x7C\x8D\x97\xA7\x46\xED\x98\x5E\xD5\x3C\x8C\xC7\x5F\x84";


GF_Err gf_media_export_native(GF_MediaExporter *dumper)
{
	GF_DecoderConfig *dcfg;
	GF_GenericSampleDescription *udesc;
	char szName[1000], szEXT[5], GUID[16];
	FILE *out;
	u32 *qcp_rates;
	GF_AVCConfig *avccfg;
	GF_M4ADecSpecInfo a_cfg;
	GF_BitStream *bs;
	u32 track, i, di, count, m_type, m_stype, dsi_size, qcp_type;
	Bool is_ogg, has_qcp_pad;
	u32 aac_type, is_aac;
	char *dsi;
	QCPRateTable rtable[8];
	u32 rt_cnt;

	dsi_size = 0;
	dsi = NULL;
	avccfg = NULL;
	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	m_type = gf_isom_get_media_type(dumper->file, track);
	m_stype = gf_isom_get_media_subtype(dumper->file, track, 1);
	has_qcp_pad = 0;

	is_aac = aac_type = 0;
	qcp_type = 0;
	is_ogg = 0;
	udesc = NULL;
	dcfg = NULL;
	if ((m_stype==GF_ISOM_SUBTYPE_MPEG4) || (m_stype==GF_ISOM_SUBTYPE_MPEG4_CRYP)) 
		dcfg = gf_isom_get_decoder_config(dumper->file, track, 1);

	strcpy(szName, dumper->out_name ? dumper->out_name : "");
	if (dcfg) {
		switch (dcfg->streamType) {
		case GF_STREAM_VISUAL:
			switch (dcfg->objectTypeIndication) {
			case 0x20:
				dsi = dcfg->decoderSpecificInfo->data;
				dcfg->decoderSpecificInfo->data = NULL;
				dsi_size = dcfg->decoderSpecificInfo->dataLength;
				strcat(szName, ".cmp");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-4 Visual stream to cmp");
				break;
			case 0x21:
				avccfg = gf_isom_avc_config_get(dumper->file, track, 1);
				strcat(szName, ".h264");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-4 AVC-H264 stream to h264");
				break;
			case 0x6A:
				strcat(szName, ".m1v");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-1 Visual stream to h264");
				break;
			case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: 
				strcat(szName, ".m2v");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-2 Visual stream to h264");
				break;
			case 0x6C:
				strcat(szName, ".jpg");
				gf_export_message(dumper, GF_OK, "Extracting JPEG image");
				break;
			case 0x6D:
				strcat(szName, ".png");
				gf_export_message(dumper, GF_OK, "Extracting PNG image");
				break;
			case GPAC_OGG_MEDIA_OTI:
				strcat(szName, ".ogg");
				gf_export_message(dumper, GF_OK, "Extracting Ogg video");
				is_ogg = 1;
				break;
			default:
				gf_odf_desc_del((GF_Descriptor *) dcfg);
				return gf_export_message(dumper, GF_NOT_SUPPORTED, "Unknown media in track ID %d - use NHNT instead", dumper->trackID);
			}
			break;
		case GF_STREAM_AUDIO:
			switch (dcfg->objectTypeIndication) {
			case 0x66:
			case 0x67:
			case 0x68:
				dsi = dcfg->decoderSpecificInfo->data;
				dcfg->decoderSpecificInfo->data = NULL;
				dsi_size = dcfg->decoderSpecificInfo->dataLength;
				strcat(szName, ".aac");
				is_aac = 1;
				aac_type = dcfg->objectTypeIndication - 0x66;
				gf_export_message(dumper, GF_OK, "Extracting MPEG-2 AAC");
				break;
			case 0x40:
				dsi = dcfg->decoderSpecificInfo->data;
				dcfg->decoderSpecificInfo->data = NULL;
				dsi_size = dcfg->decoderSpecificInfo->dataLength;
				is_aac = 2;
				strcat(szName, ".aac");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-4 AAC");
				break;
			case 0x69:
			case 0x6B:
				strcat(szName, ".mp3");
				gf_export_message(dumper, GF_OK, "Extracting MPEG-1/2 Audio (MP3)");
				break;
			case GPAC_OGG_MEDIA_OTI:
				strcat(szName, ".ogg");
				is_ogg = 1;
				gf_export_message(dumper, GF_OK, "Extracting Ogg audio");
				break;
			case 0xE1:
				strcat(szName, ".qcp"); qcp_type = 1;
				memcpy(GUID, QCP_QCELP_GUID_1, sizeof(char)*16);
				gf_export_message(dumper, GF_OK, "Extracting QCELP-13K (QCP file)");
				break;
			case 0xA0:
				memcpy(GUID, QCP_EVRC_GUID, sizeof(char)*16);
				qcp_type = 3;
				if (dumper->flags & GF_EXPORT_PROBE_ONLY) dumper->flags |= GF_EXPORT_USE_QCP;
				break;
			case 0xA1:
				qcp_type = 2;
				memcpy(GUID, QCP_SMV_GUID, sizeof(char)*16);
				if (dumper->flags & GF_EXPORT_PROBE_ONLY) dumper->flags |= GF_EXPORT_USE_QCP;
				break;
			case 0xD1: 
				if (dcfg->decoderSpecificInfo && (dcfg->decoderSpecificInfo->dataLength==8)
					&& !strnicmp(dcfg->decoderSpecificInfo->data, "pvmm", 4)) {
					qcp_type = 3;
					memcpy(GUID, QCP_EVRC_GUID, sizeof(char)*16);
					if (dumper->flags & GF_EXPORT_PROBE_ONLY) dumper->flags |= GF_EXPORT_USE_QCP;
					break;
				}
				/*fall through*/
			default:
				gf_odf_desc_del((GF_Descriptor *) dcfg);
				return gf_export_message(dumper, GF_NOT_SUPPORTED, "Unknown audio in track ID %d - use NHNT", dumper->trackID);
			}
			break;
		default:
			gf_odf_desc_del((GF_Descriptor *) dcfg);
			return gf_export_message(dumper, GF_NOT_SUPPORTED, "Cannot extract systems track ID %d in raw format - use NHNT", dumper->trackID);
		}
		gf_odf_desc_del((GF_Descriptor *) dcfg);
	} else {
		if (m_stype==GF_ISOM_SUBTYPE_3GP_AMR) {
			strcat(szName, ".amr");
			gf_export_message(dumper, GF_OK, "Extracting AMR Audio");
		} else if (m_stype==GF_ISOM_SUBTYPE_3GP_AMR_WB) {
			strcat(szName, ".awb");
			gf_export_message(dumper, GF_OK, "Extracting AMR WideBand Audio");
		} else if (m_stype==GF_ISOM_SUBTYPE_3GP_QCELP) {
			strcat(szName, ".qcp"); qcp_type = 1;
			memcpy(GUID, QCP_QCELP_GUID_1, sizeof(char)*16);
			gf_export_message(dumper, GF_OK, "Extracting QCELP-13K (QCP file)");
		} else if (m_stype==GF_ISOM_SUBTYPE_3GP_EVRC) {
			qcp_type = 3;
			memcpy(GUID, QCP_EVRC_GUID, sizeof(char)*16);
			if (dumper->flags & GF_EXPORT_PROBE_ONLY) dumper->flags |= GF_EXPORT_USE_QCP;
		} else if (m_stype==GF_ISOM_SUBTYPE_3GP_SMV) {
			qcp_type = 2;
			memcpy(GUID, QCP_SMV_GUID, sizeof(char)*16);
			if (dumper->flags & GF_EXPORT_PROBE_ONLY) dumper->flags |= GF_EXPORT_USE_QCP;
		} else if (m_stype==GF_ISOM_SUBTYPE_3GP_H263) {
			gf_export_message(dumper, GF_OK, "Extracting H263 Video");
			strcat(szName, ".263");
		} else if (m_stype==GF_ISOM_SUBTYPE_AVC_H264) {
			avccfg = gf_isom_avc_config_get(dumper->file, track, 1);
			strcat(szName, ".h264");
			gf_export_message(dumper, GF_OK, "Extracting MPEG-4 AVC-H264 stream to h264");
		} else if (m_type==GF_ISOM_MEDIA_FLASH) {
			gf_export_message(dumper, GF_OK, "Extracting Macromedia Flash Movie");
			strcat(szName, ".swf");
		} else {
			strcat(szName, ".");
			strcat(szName, gf_4cc_to_str(m_stype));
			udesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
			if (udesc) {
				dsi = udesc->extension_buf; udesc->extension_buf = NULL;
				dsi_size = udesc->extension_buf_size;
			}
			switch (m_type) {
			case GF_ISOM_MEDIA_VISUAL: gf_export_message(dumper, GF_OK, "Extracting \'%s\' Video - Compressor %s", szEXT, udesc ? udesc->compressor_name : "Unknown"); break;
			case GF_ISOM_MEDIA_AUDIO: gf_export_message(dumper, GF_OK, "Extracting \'%s\' Audio - Compressor %s", szEXT, udesc ? udesc->compressor_name : "Unknown"); break;
			default:
				gf_export_message(dumper, GF_OK, "Extracting \'%s\' Track (type '%s') - Compressor %s", szEXT, gf_4cc_to_str(m_type), udesc ? udesc->compressor_name : "Unknown");
				break;
			}
			if (udesc) free(udesc);
		}
	}
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		if (dsi) free(dsi);
		if (avccfg) gf_odf_avc_cfg_del(avccfg);
		return GF_OK;
	}

	if (is_ogg) return gf_dump_to_ogg(dumper, szName, track);

	if (qcp_type>1) {
		if (dumper->flags & GF_EXPORT_USE_QCP) {
			strcat(szName, ".qcp");
			gf_export_message(dumper, GF_OK, "Extracting %s audio (QCP file)", (qcp_type==2) ? "SMV" : "EVRC");
		} else if (qcp_type==2) {
			strcat(szName, ".smv");
			gf_export_message(dumper, GF_OK, "Extracting SMV audio");
		} else {
			strcat(szName, ".evc");
			gf_export_message(dumper, GF_OK, "Extracting EVRC audio");
		}
	}
	out = fopen(szName, "wb");
	if (!out) {
		if (dsi) free(dsi);
		if (avccfg) gf_odf_avc_cfg_del(avccfg);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}
	bs = gf_bs_from_file(out, GF_BITSTREAM_WRITE);

	if (is_aac) {
		gf_m4a_get_config(dsi, dsi_size, &a_cfg);
		if (is_aac==2) aac_type = a_cfg.base_object_type - 1;
		free(dsi);
		dsi = NULL;
	}
	if (dsi) {
		gf_bs_write_data(bs, dsi, dsi_size);
		free(dsi);
	}
	if (avccfg) {
		count = gf_list_count(avccfg->sequenceParameterSets);
		for (i=0;i<count;i++) {
			GF_AVCConfigSlot *sl = gf_list_get(avccfg->sequenceParameterSets, i);
			gf_bs_write_u32(bs, 1);
			gf_bs_write_data(bs, sl->data, sl->size);
		}
		count = gf_list_count(avccfg->pictureParameterSets);
		for (i=0;i<count;i++) {
			GF_AVCConfigSlot *sl = gf_list_get(avccfg->pictureParameterSets, i);
			gf_bs_write_u32(bs, 1);
			gf_bs_write_data(bs, sl->data, sl->size);
		}
	}

	qcp_rates = NULL;
	count = gf_isom_get_sample_count(dumper->file, track);
	rt_cnt = 0;

	if (m_stype==GF_ISOM_SUBTYPE_3GP_AMR) gf_bs_write_data(bs, "#!AMR\n", 6);
	else if (m_stype==GF_ISOM_SUBTYPE_3GP_AMR_WB) gf_bs_write_data(bs, "#!AMR-WB\n", 9);
	else if ((qcp_type>1) && !(dumper->flags & GF_EXPORT_USE_QCP)) {
		if (qcp_type==2) gf_bs_write_data(bs, "#!SMV\n", 6);
		else gf_bs_write_data(bs, "#!EVRC\n", 7);
		qcp_type = 0;
	}

	/*QCP formatting*/
	if (qcp_type) {
		GF_ISOSample *samp;
		Bool needs_rate_octet;
		u32 tot_size, data_size, sample_size, avg_rate, agg_samp;
		u32 block_size = 160;
		u32 sample_rate = 8000;
		GF_3GPConfig *gpc = gf_isom_3gp_config_get(dumper->file, track, 1);
		sample_size = gf_isom_get_constant_sample_size(dumper->file, track);
		agg_samp = 1;
		if (gpc) {
			agg_samp = gpc->frames_per_sample;
			free(gpc);
		}

		if (qcp_type==1) {
			qcp_rates = (u32 *)GF_QCELP_RATE_TO_SIZE;
			rt_cnt = GF_QCELP_RATE_TO_SIZE_NB;
		} else {
			qcp_rates = (u32 *)GF_SMV_EVRC_RATE_TO_SIZE;
			rt_cnt = GF_SMV_EVRC_RATE_TO_SIZE_NB;
		}

		/*if cst size and no aggregation, skip table*/
		if (sample_size && !agg_samp) {
			data_size = sample_size*count;
		} else {
			/*dumps full table...*/
			for (i=0; i<rt_cnt; i++) {
				rtable[i].rate_idx = qcp_rates[2*i];
				rtable[i].pck_size = qcp_rates[2*i+1];
			}
			data_size = (u32) gf_isom_get_media_data_size(dumper->file, track);
		}

		/*check sample format - packetvideo doesn't include rate octet...*/
		needs_rate_octet = 1;
		samp = gf_isom_get_sample_info(dumper->file, track, 1, NULL, NULL);
		for (i=0; i<rt_cnt; i++) {
			if (qcp_rates[2*i+1]==samp->dataLength) {
				needs_rate_octet = 0;
				break;
			}
		}
		gf_isom_sample_del(&samp);
		if (needs_rate_octet) data_size += count;
		has_qcp_pad = (data_size % 2) ? 1 : 0;

		avg_rate = 8*data_size*sample_rate/count/block_size;
		/*QLCM + fmt + vrat + data*/
		tot_size = 4+ 8+150 + 8+8 + 8 + data_size;
		/*pad is included in riff size*/
		if (has_qcp_pad) tot_size++;

		gf_bs_write_data(bs, "RIFF", 4);
		gf_bs_write_u32_le(bs, tot_size);
		gf_bs_write_data(bs, "QLCM", 4);
		gf_bs_write_data(bs, "fmt ", 4);
		gf_bs_write_u32_le(bs, 150);/*fmt chunk size*/
		gf_bs_write_u8(bs, 1);
		gf_bs_write_u8(bs, 0);
		gf_bs_write_data(bs, GUID, 16);
		gf_bs_write_u16_le(bs, 1);
		memset(szName, 0, 80);
		strcpy(szName, (qcp_type==1) ? "QCELP-GPACExport" : ((qcp_type==2) ? "SMV-GPACExport" : "EVRC-GPACExport"));
		gf_bs_write_data(bs, szName, 80);
		gf_bs_write_u16_le(bs, avg_rate);
		gf_bs_write_u16_le(bs, sample_size);
		gf_bs_write_u16_le(bs, block_size);
		gf_bs_write_u16_le(bs, sample_rate);
		gf_bs_write_u16_le(bs, 16);
		gf_bs_write_u32_le(bs, rt_cnt);
		for (i=0; i<8; i++) {
			if (i<rt_cnt) {
				/*frame size MINUS rate octet*/
				gf_bs_write_u8(bs, rtable[i].pck_size - 1);
				gf_bs_write_u8(bs, rtable[i].rate_idx);
			} else {
				gf_bs_write_u16(bs, 0);
			}
		}
		memset(szName, 0, 80);
		gf_bs_write_data(bs, szName, 20);/*reserved*/
		gf_bs_write_data(bs, "vrat", 4);
		gf_bs_write_u32_le(bs, 8);/*vrat chunk size*/
		gf_bs_write_u32_le(bs, rt_cnt);
		gf_bs_write_u32_le(bs, count);
		gf_bs_write_data(bs, "data", 4);
		gf_bs_write_u32_le(bs, data_size);/*data chunk size*/

		qcp_type = needs_rate_octet ? 1 : 0;
	}


	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;
		/*AVC sample to NALU*/
		if (avccfg) {
			u32 j, nal_size, remain;
			unsigned char *ptr = samp->data;
			remain = samp->dataLength;
			while (remain) {
				nal_size = 0;
				for (j=0; j<avccfg->nal_unit_size; j++) {
					nal_size |= *ptr;
					if (j+1<avccfg->nal_unit_size) nal_size<<=8;
					remain--;
					ptr++;
				}
				gf_bs_write_u32(bs, 1);
				gf_bs_write_data(bs, ptr, nal_size);
				ptr += nal_size;
				remain -= nal_size;
			}
		}
		/*adts frame header*/
		else if (is_aac) {
			gf_bs_write_int(bs, 0xFFF, 12);/*sync*/
			gf_bs_write_int(bs, (is_aac==1) ? 1 : 0, 1);/*mpeg2 aac*/
			gf_bs_write_int(bs, 0, 2); /*layer*/
			gf_bs_write_int(bs, 1, 1); /* protection_absent*/
			gf_bs_write_int(bs, aac_type, 2);
			gf_bs_write_int(bs, a_cfg.base_sr_index, 4);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_write_int(bs, a_cfg.nb_chan, 3);
			gf_bs_write_int(bs, 0, 4);
			gf_bs_write_int(bs, 7+samp->dataLength, 13);
			gf_bs_write_int(bs, 0x7FF, 11);
			gf_bs_write_int(bs, 0, 2);
		}
		/*fix rate octet for QCP*/
		else if (qcp_type) {
			u32 j;
			for (j=0; j<rt_cnt; j++) {
				if (qcp_rates[2*j+1]==1+samp->dataLength) {
					gf_bs_write_u8(bs, qcp_rates[2*j]);
					break;
				}
			}
		}
		if (!avccfg) gf_bs_write_data(bs, samp->data, samp->dataLength);
		gf_isom_sample_del(&samp);
		dump_progress(dumper, i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	if (has_qcp_pad) gf_bs_write_u8(bs, 0);

	if (avccfg) gf_odf_avc_cfg_del(avccfg);
	gf_bs_del(bs);
	fclose(out);
	return GF_OK;
}

GF_Err gf_media_export_avi_track(GF_MediaExporter *dumper)
{
	GF_Err e;
	u32 max_size, tot_size, num_samples, i;
	s32 size;
	char *comp, *frame;
	char szOutFile[1024];	
	avi_t *in;
	FILE *fout;

	in = AVI_open_input_file(dumper->in_name, 1);
	if (!in) return gf_export_message(dumper, GF_URL_ERROR, "Unsupported avi file");
	fout = NULL;

	e = GF_OK;
	if (dumper->trackID==1) {
		Bool key;
		comp = AVI_video_compressor(in);
		if (!stricmp(comp, "DIVX") || !stricmp(comp, "DX50")	/*DivX*/
		|| !stricmp(comp, "XVID") /*XviD*/
		|| !stricmp(comp, "3iv2") /*3ivX*/
		|| !stricmp(comp, "fvfw") /*ffmpeg*/
		|| !stricmp(comp, "NDIG") /*nero*/
		|| !stricmp(comp, "MP4V") /*!! not tested*/
		|| !stricmp(comp, "M4CC") /*Divio - not tested*/
		|| !stricmp(comp, "PVMM") /*PacketVideo - not tested*/
		|| !stricmp(comp, "SEDG") /*Samsung - not tested*/
		|| !stricmp(comp, "RMP4") /*Sigma - not tested*/
		) {
			sprintf(szOutFile, "%s.cmp", dumper->out_name);
		} else if (!stricmp(comp, "VSSH")) {
			sprintf(szOutFile, "%s.h264", dumper->out_name);
		} else {
			sprintf(szOutFile, "%s.%s", dumper->out_name, comp);
		}
		gf_export_message(dumper, GF_OK, "Extracting AVI video (format %s) to %s", comp, szOutFile);

		fout = fopen(szOutFile, "wb");

		max_size = 0;
		frame = NULL;
		num_samples = AVI_video_frames(in);
		for (i=0; i<num_samples; i++) {
			size = AVI_frame_size(in, i);
			if (!size) {
				AVI_read_frame(in, NULL, &key);
				continue;
			}
			if ((u32) size > max_size) {
				frame = realloc(frame, sizeof(char) * size);
				max_size = size;
			}
			AVI_read_frame(in, frame, &key);
			if ((u32) size>4) fwrite(frame, 1, size, fout);
			dump_progress(dumper, i+1, num_samples);
		}
		free(frame);
		fclose(fout);
		fout = NULL;
		goto exit;
	}
	i = 0;
	tot_size = max_size = 0;
	while ((size = AVI_audio_size(in, i) )>0) {
		if (max_size<(u32) size) max_size=size;
		tot_size += size;
		i++;
	}
	frame = malloc(sizeof(char) * max_size);
	AVI_seek_start(in);
	AVI_set_audio_position(in, 0);

	switch (in->track[in->aptr].a_fmt) {
	case WAVE_FORMAT_PCM: comp = "pcm"; break;
	case WAVE_FORMAT_ADPCM: comp = "adpcm"; break;
	case WAVE_FORMAT_IBM_CVSD: comp = "cvsd"; break;
	case WAVE_FORMAT_ALAW: comp = "alaw"; break;
	case WAVE_FORMAT_MULAW: comp = "mulaw"; break;
	case WAVE_FORMAT_OKI_ADPCM: comp = "oki_adpcm"; break;
	case WAVE_FORMAT_DVI_ADPCM: comp = "dvi_adpcm"; break;
	case WAVE_FORMAT_DIGISTD: comp = "digistd"; break;
	case WAVE_FORMAT_YAMAHA_ADPCM: comp = "yam_adpcm"; break;
	case WAVE_FORMAT_DSP_TRUESPEECH: comp = "truespeech"; break;
	case WAVE_FORMAT_GSM610: comp = "gsm610"; break;
	case IBM_FORMAT_MULAW: comp = "ibm_mulaw"; break;
	case IBM_FORMAT_ALAW: comp = "ibm_alaw"; break;
	case IBM_FORMAT_ADPCM: comp = "ibm_adpcm"; break;
	case 0x55: comp = "mp3"; break;
	default: comp = "raw"; break;
	}
	sprintf(szOutFile, "%s.%s", dumper->out_name, comp);
	gf_export_message(dumper, GF_OK, "Extracting AVI %s audio", comp);
	fout = fopen(szOutFile, "wb");
	num_samples = 0;
	while (1) {
		Bool continuous;
		size = AVI_read_audio(in, frame, max_size, &continuous);
		if (!size) break;
		num_samples += size;
		fwrite(frame, 1, size, fout);
		dump_progress(dumper, num_samples, tot_size);

	}


exit:
	if (fout) fclose(fout);
	AVI_close(in);
	return e;
}

GF_Err gf_media_export_nhnt(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	char szName[1000];
	FILE *out_med, *out_inf, *out_nhnt;
	GF_BitStream *bs;
	Bool has_b_frames;
	u32 track, i, di, count, pos;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	esd = gf_isom_get_esd(dumper->file, track, 1);
	if (!esd) return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Invalid MPEG-4 stream in track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		return GF_OK;
	}

	sprintf(szName, "%s.media", dumper->out_name);
	out_med = gf_f64_open(szName, "wb");
	if (!out_med) {
		gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	sprintf(szName, "%s.nhnt", dumper->out_name);
	out_nhnt = fopen(szName, "wb");
	if (!out_nhnt) {
		fclose(out_med);
		gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}


	bs = gf_bs_from_file(out_nhnt, GF_BITSTREAM_WRITE);

	if (esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
		sprintf(szName, "%s.info", dumper->out_name);
		out_inf = fopen(szName, "wb");
		if (out_inf) fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, out_inf);
		fclose(out_inf);
	}

	/*write header*/
	/*'NHnt' format*/
	gf_bs_write_data(bs, "NHnt", 4);
	/*version 1*/
	gf_bs_write_u8(bs, 0);
	/*streamType*/
	gf_bs_write_u8(bs, esd->decoderConfig->streamType);
	/*OTI*/
	gf_bs_write_u8(bs, esd->decoderConfig->objectTypeIndication);
	/*reserved*/
	gf_bs_write_u16(bs, 0);
	/*bufferDB*/
	gf_bs_write_u24(bs, esd->decoderConfig->bufferSizeDB);
	/*avg BitRate*/
	gf_bs_write_u32(bs, esd->decoderConfig->avgBitrate);
	/*max bitrate*/
	gf_bs_write_u32(bs, esd->decoderConfig->maxBitrate);
	/*timescale*/
	gf_bs_write_u32(bs, esd->slConfig->timestampResolution);

	gf_odf_desc_del((GF_Descriptor *) esd);

	has_b_frames = gf_isom_has_time_offset(dumper->file, track);

	pos = 0;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;
		fwrite(samp->data, samp->dataLength, 1, out_med);
		
		/*dump nhnt info*/
		gf_bs_write_u24(bs, samp->dataLength);
		gf_bs_write_int(bs, samp->IsRAP, 1);
		/*AU start & end flag always true*/
		gf_bs_write_int(bs, 1, 1);
		gf_bs_write_int(bs, 1, 1);
		/*reserved*/
		gf_bs_write_int(bs, 0, 3);
		/*type - try to guess it*/
		if (has_b_frames) {
			if (samp->IsRAP) gf_bs_write_int(bs, 0, 2);
			/*if CTS offset, assime P*/
			else if (samp->CTS_Offset) gf_bs_write_int(bs, 1, 2);
			else gf_bs_write_int(bs, 2, 2);
		} else {
			gf_bs_write_int(bs, samp->IsRAP ? 0 : 1, 2);
		}
		gf_bs_write_u32(bs, pos);
		gf_bs_write_u32(bs, samp->DTS + samp->CTS_Offset);
		gf_bs_write_u32(bs, samp->DTS);

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		dump_progress(dumper, i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	fclose(out_med);
	gf_bs_del(bs);
	fclose(out_nhnt);
	return GF_OK;
}

static GF_Err MP4T_CopyTrack(GF_MediaExporter *dumper, GF_ISOFile *infile, u32 inTrackNum, GF_ISOFile *outfile, Bool ResetDependancies, Bool AddToIOD)
{
	GF_ESD *esd;
	GF_InitialObjectDescriptor *iod;
	u32 TrackID, newTk, descIndex, i, ts, rate, dur, pos, di, count, msubtype;
	GF_ISOSample *samp;

	if (!inTrackNum) {
		if (gf_isom_get_track_count(infile) != 1) return gf_export_message(dumper, GF_BAD_PARAM, "Please specify trackID to export");
		inTrackNum = 1;
	}
	//check the ID is available
	TrackID = gf_isom_get_track_id(infile, inTrackNum);
	newTk = gf_isom_get_track_by_id(outfile, TrackID);
	if (newTk) TrackID = 0;

	//get the ESD and remove dependancies
	esd = NULL;
	msubtype = gf_isom_get_media_subtype(infile, inTrackNum, 1);

	if (msubtype == GF_ISOM_SUBTYPE_MPEG4) {
		esd = gf_isom_get_esd(infile, inTrackNum, 1);
		if (esd && ResetDependancies) {
			esd->dependsOnESID = 0;
			esd->OCRESID = 0;
		}
	}

	newTk = gf_isom_new_track(outfile, TrackID, gf_isom_get_media_type(infile, inTrackNum), gf_isom_get_media_timescale(infile, inTrackNum));
	gf_isom_set_track_enabled(outfile, newTk, 1);

	if (esd) {
		gf_isom_new_mpeg4_description(outfile, newTk, esd, NULL, NULL, &descIndex);
		if ((esd->decoderConfig->streamType == GF_STREAM_VISUAL) || (esd->decoderConfig->streamType == GF_STREAM_SCENE)) {
			u32 w, h;
			gf_isom_get_visual_info(infile, inTrackNum, 1, &w, &h);
			/*this is because so many files have reserved values of 320x240 from v1 ... */
			if ((esd->decoderConfig->objectTypeIndication == 0x20) ) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				w = dsi.width;
				h = dsi.height;
			}
			gf_isom_set_visual_info(outfile, newTk, 1, w, h);
		}
		esd->decoderConfig->avgBitrate = 0;
		esd->decoderConfig->maxBitrate = 0;
	} else {
		gf_isom_clone_sample_description(outfile, newTk, infile, inTrackNum, 1, NULL, NULL, &descIndex);
	}

	pos = 0;
	rate = 0;
	ts = gf_isom_get_media_timescale(infile, inTrackNum);
	count = gf_isom_get_sample_count(infile, inTrackNum); 
	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample(infile, inTrackNum, i+1, &di);
		gf_isom_add_sample(outfile, newTk, descIndex, samp);
		if (esd) {
			rate += samp->dataLength;
			esd->decoderConfig->avgBitrate += samp->dataLength;
			if (esd->decoderConfig->bufferSizeDB<samp->dataLength) esd->decoderConfig->bufferSizeDB = samp->dataLength;
			if (samp->DTS - pos > ts) {
				if (esd->decoderConfig->maxBitrate<rate) esd->decoderConfig->maxBitrate = rate;
				rate = 0;
				pos = 0;
			}
		}
		gf_isom_sample_del(&samp);
		dump_progress(dumper, i, count);
	}
	dump_progress(dumper, count, count);

	if (msubtype == GF_ISOM_SUBTYPE_MPEG4_CRYP) {
		esd = gf_isom_get_esd(infile, inTrackNum, 1);
	} else if (msubtype == GF_ISOM_SUBTYPE_AVC_H264) {
		return gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0x0F);
	} 
	/*likely 3gp or any non-MPEG-4 isomedia file*/
	else if (!esd) return gf_isom_remove_root_od(outfile);

	dur = (u32) gf_isom_get_media_duration(outfile, newTk);
	if (!dur) dur = ts;
	esd->decoderConfig->maxBitrate *= 8;
	esd->decoderConfig->avgBitrate *= 8 * ts / dur;
	gf_isom_change_mpeg4_description(outfile, newTk, 1, esd);


	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(infile);
	switch (esd->decoderConfig->streamType) {
	case GF_STREAM_SCENE:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, iod->scene_profileAndLevel);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, iod->graphics_profileAndLevel);
		} else if (esd->decoderConfig->objectTypeIndication==0x20) {
			gf_export_message(dumper, GF_OK, "Warning: Scene PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, 0xFE);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, 0xFE);
		}
		break;
	case GF_STREAM_VISUAL:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, iod->visual_profileAndLevel);
		} else if (esd->decoderConfig->objectTypeIndication==0x20) {
			GF_M4VDecSpecInfo dsi;
			gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, dsi.VideoPL);
		} else {
			gf_export_message(dumper, GF_OK, "Warning: Visual PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0xFE);
		}
		break;
	case GF_STREAM_AUDIO:
		if (iod && (iod->tag==GF_ODF_IOD_TAG)) {
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, iod->audio_profileAndLevel);
		} else if (esd->decoderConfig->objectTypeIndication==0x40) {
			GF_M4ADecSpecInfo cfg;
			gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &cfg);
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, cfg.audioPL);
		} else {
			gf_export_message(dumper, GF_OK, "Warning: Audio PLs not found in original MP4 - defaulting to No Profile Specified");
			gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, 0xFE);
		}
	default:
		break;
	}
	if (iod) gf_odf_desc_del((GF_Descriptor *) iod);	
	gf_odf_desc_del((GF_Descriptor *)esd);

	if (AddToIOD) gf_isom_add_track_to_root_od(outfile, newTk);

	return GF_OK;
}

GF_Err gf_media_export_isom(GF_MediaExporter *dumper)
{
	GF_ISOFile *outfile;
	GF_Err e;
	Bool add_to_iod;
	char szName[1000], *ext;
	u32 track;
	u8 mode;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	if (gf_isom_get_media_type(dumper->file, dumper->trackID)==GF_ISOM_MEDIA_OD) {
		return gf_export_message(dumper, GF_BAD_PARAM, "Cannot extract OD track, result is  meaningless");
	}

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_MERGE_TRACKS;
		return GF_OK;
	}
	ext = (char *) gf_isom_get_filename(dumper->file);
	if (ext) ext = strrchr(ext, '.');
	sprintf(szName, "%s%s", dumper->out_name, ext ? ext : ".mp4");

	add_to_iod = 1;
	mode = GF_ISOM_WRITE_EDIT;
	if (dumper->flags & GF_EXPORT_MERGE_TRACKS) {
		FILE *t = fopen(szName, "rb");
		if (t) {
			add_to_iod = 0;
			mode = GF_ISOM_OPEN_EDIT;
			fclose(t);
		}
	}
	outfile = gf_isom_open(szName, mode, NULL);

	if (mode == GF_ISOM_WRITE_EDIT) {
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_AUDIO, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_VISUAL, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_GRAPHICS, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_SCENE, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_OD, 0xFF);
		gf_isom_set_pl_indication(outfile, GF_ISOM_PL_MPEGJ, 0xFF);
	}

	e = MP4T_CopyTrack(dumper, dumper->file, track, outfile, 1, add_to_iod);
	if (!add_to_iod) {
		u32 i;
		for (i=0; i<gf_isom_get_track_count(outfile); i++) {
			gf_isom_remove_track_from_root_od(outfile, i+1);
		}
	}
	if (e) gf_isom_delete(outfile);
	else gf_isom_close(outfile);

	return e;
}

GF_Err gf_media_export_avi(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	GF_ISOSample *samp;
	char szName[1000];
	avi_t *avi_out;
	char dumdata[1];
	u32 track, i, di, count, w, h, frame_d;
	GF_M4VDecSpecInfo dsi;
	Double FPS;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	esd = gf_isom_get_esd(dumper->file, track, 1);
	if (!esd) return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Invalid MPEG-4 stream in track ID %d", dumper->trackID);

	if ((esd->decoderConfig->streamType!=GF_STREAM_VISUAL) || 
	( (esd->decoderConfig->objectTypeIndication!=0x20) && (esd->decoderConfig->objectTypeIndication!=0x21)) ) {
		gf_odf_desc_del((GF_Descriptor*)esd);
		return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Track ID %d is not MPEG-4 Visual - cannot extract to AVI", dumper->trackID);
	}
	if (!esd->decoderConfig->decoderSpecificInfo) {
		gf_odf_desc_del((GF_Descriptor*)esd);
		return gf_export_message(dumper, GF_NON_COMPLIANT_BITSTREAM, "Missing decoder config for track ID %d", dumper->trackID);
	}
	if (dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_OK;

	sprintf(szName, "%s.avi", dumper->out_name);
	avi_out = AVI_open_output_file(szName);
	if (!avi_out) {
		gf_odf_desc_del((GF_Descriptor *)esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}
	/*ignore visual size info, get it from dsi*/
	gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
	w = dsi.width;
	h = dsi.height;

	/*compute FPS - note we assume constant frame rate without droped frames...*/
	count = gf_isom_get_sample_count(dumper->file, track);
	FPS = gf_isom_get_media_timescale(dumper->file, track);
	FPS *= (count-1);
	samp = gf_isom_get_sample(dumper->file, track, count, &di);
	FPS /= samp->DTS;
	gf_isom_sample_del(&samp);

	frame_d = 0;
	if (gf_isom_has_time_offset(dumper->file, track)) {
		u32 DTS, max_CTSO;
		DTS = max_CTSO = 0;
		for (i=0; i<count; i++) {
			samp = gf_isom_get_sample_info(dumper->file, track, i+1, NULL, NULL);
			if (!samp) break;
			if (samp->CTS_Offset>max_CTSO) max_CTSO = samp->CTS_Offset;
			DTS = samp->DTS;
			gf_isom_sample_del(&samp);
		}
		DTS /= (count-1);
		frame_d = max_CTSO / DTS;
		frame_d -= 1;
		/*dummy delay frame for xvid unpacked bitstreams*/
		dumdata[0] = 127;
	}

	/*AVC*/
	if (esd->decoderConfig->objectTypeIndication==0x21) {
		AVI_set_video(avi_out, w, h, FPS, "VSSH");
	} 
	/*MPEG4*/
	else {
		AVI_set_video(avi_out, w, h, FPS, "XVID");
	}
	gf_export_message(dumper, GF_OK, "Creating AVI file %d x %d @ %.2f FPS - 4CC \"XVID\"", w, h, FPS);
	if (frame_d) gf_export_message(dumper, GF_OK, "B-Frames detected - using unpacked bitstream with max B-VOP delta %d", frame_d);

	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;

		if (!i) {
			char *data = malloc(sizeof(char) * (samp->dataLength + esd->decoderConfig->decoderSpecificInfo->dataLength));
			memcpy(data, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			memcpy(data + esd->decoderConfig->decoderSpecificInfo->dataLength, samp->data, samp->dataLength);
			AVI_write_frame(avi_out, data, samp->dataLength + esd->decoderConfig->decoderSpecificInfo->dataLength, 1);
			free(data);
		} else {
			AVI_write_frame(avi_out, samp->data, samp->dataLength, samp->IsRAP);
		}
		gf_isom_sample_del(&samp);
		while (frame_d) {
			AVI_write_frame(avi_out, dumdata, 1, 0);
			frame_d--;
		}
		dump_progress(dumper, i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}

	gf_odf_desc_del((GF_Descriptor *) esd);
	AVI_close(avi_out);
	return GF_OK;
}

GF_Err gf_media_export_nhml(GF_MediaExporter *dumper)
{
	GF_ESD *esd;
	char szName[1000], szMedia[1000];
	FILE *med, *inf, *nhml;
	Bool full_dump;
	u32 track, i, di, count, pos;

	track = gf_isom_get_track_by_id(dumper->file, dumper->trackID);
	if (!track) return gf_export_message(dumper, GF_BAD_PARAM, "Invalid track ID %d", dumper->trackID);

	if (dumper->flags & GF_EXPORT_PROBE_ONLY) {
		dumper->flags |= GF_EXPORT_NHML_FULL;
		return GF_OK;
	}
	esd = gf_isom_get_esd(dumper->file, track, 1);
	full_dump = (dumper->flags & GF_EXPORT_NHML_FULL) ? 1 : 0;

	sprintf(szMedia, "%s.media", dumper->out_name);
	med = gf_f64_open(szMedia, "wb");
	if (!med) {
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szMedia);
	}

	sprintf(szName, "%s.nhml", dumper->out_name);
	nhml = fopen(szName, "wt");
	if (!nhml) {
		fclose(med);
		if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
		return gf_export_message(dumper, GF_IO_ERR, "Error opening %s for writing - check disk access & permissions", szName);
	}

	/*write header*/
	fprintf(nhml, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
	fprintf(nhml, "<NHNTStream version=\"1.0\" timeScale=\"%d\" ", gf_isom_get_media_timescale(dumper->file, track) );
	if (esd) {
		fprintf(nhml, "streamType=\"%d\" objectTypeIndication=\"%d\" ", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
		if (esd->decoderConfig->decoderSpecificInfo  && esd->decoderConfig->decoderSpecificInfo->data) {
			sprintf(szName, "%s.info", dumper->out_name);
			inf = fopen(szName, "wb");
			if (inf) fwrite(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 1, inf);
			fclose(inf);
			fprintf(nhml, "specificInfoFile=\"%s\" ", szName);
		}
		gf_odf_desc_del((GF_Descriptor *) esd);

		if (gf_isom_get_media_type(dumper->file, track)==GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			gf_isom_get_visual_info(dumper->file, track, 1, &w, &h);
			fprintf(nhml, "width=\"%d\" height=\"%d\" ", w, h);
		}
		else if (gf_isom_get_media_type(dumper->file, track)==GF_ISOM_MEDIA_AUDIO) {
			u32 sr, nb_ch;
			u8 bps;
			gf_isom_get_audio_info(dumper->file, track, 1, &sr, &nb_ch, &bps);
			fprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" ", sr, nb_ch);
		}

	} else {
		GF_GenericSampleDescription *sdesc = gf_isom_get_generic_sample_description(dumper->file, track, 1);
		u32 mtype = gf_isom_get_media_type(dumper->file, track);
		fprintf(nhml, "mediaType=\"%s\" mediaSubType=\"%s\" ", gf_4cc_to_str(mtype), gf_4cc_to_str(sdesc->codec_tag));
		if (mtype==GF_ISOM_MEDIA_VISUAL) {
			fprintf(nhml, "codecVendor=\"%s\" codecVersion=\"%d\" codecRevision=\"%d\" ", gf_4cc_to_str(sdesc->vendor_code), sdesc->version, sdesc->revision);
			fprintf(nhml, "width=\"%d\" height=\"%d\" compressorName=\"%s\" temporalQuality=\"%d\" spatialQuality=\"%d\" horizontalResolution=\"%d\" verticalResolution=\"%d\" bitDepth=\"%d\" ",
				sdesc->width, sdesc->height, sdesc->compressor_name, sdesc->temporal_quality, sdesc->spacial_quality, sdesc->h_res, sdesc->v_res, sdesc->depth);
		} else if (mtype==GF_ISOM_MEDIA_AUDIO) {
			fprintf(nhml, "codecVendor=\"%s\" codecVersion=\"%d\" codecRevision=\"%d\" ", gf_4cc_to_str(sdesc->vendor_code), sdesc->version, sdesc->revision);
			fprintf(nhml, "sampleRate=\"%d\" numChannels=\"%d\" bitsPerSample=\"%d\" ", sdesc->samplerate, sdesc->nb_channels, sdesc->bits_per_sample);
		}
		if (sdesc->extension_buf) {
			sprintf(szName, "%s.info", dumper->out_name);
			inf = fopen(szName, "wb");
			if (inf) fwrite(sdesc->extension_buf, sdesc->extension_buf_size, 1, inf);
			fclose(inf);
			fprintf(nhml, "specificInfoFile=\"%s\" ", szName);
			free(sdesc->extension_buf);
		}
		free(sdesc);
	}

	fprintf(nhml, "baseMediaFile=\"%s\" ", szMedia);
	fprintf(nhml, ">\n");


	pos = 0;
	count = gf_isom_get_sample_count(dumper->file, track);
	for (i=0; i<count; i++) {
		GF_ISOSample *samp = gf_isom_get_sample(dumper->file, track, i+1, &di);
		if (!samp) break;
		fwrite(samp->data, samp->dataLength, 1, med);

		fprintf(nhml, "<NHNTSample DTS=\"%d\" dataLength=\"%d\" ", samp->DTS, samp->dataLength);
		if (full_dump || samp->CTS_Offset) fprintf(nhml, "CTSOffset=\"%d\" ", samp->CTS_Offset);
		if (samp->IsRAP==1) fprintf(nhml, "isRAP=\"yes\" ");
		else if (samp->IsRAP==2) fprintf(nhml, "isSyncShadow=\"yes\" ");
		else if (full_dump) fprintf(nhml, "isRAP=\"no\" ");

		if (full_dump) fprintf(nhml, "mediaOffset=\"%d\" ", pos);
		fprintf(nhml, "/>\n");

		pos += samp->dataLength;
		gf_isom_sample_del(&samp);
		dump_progress(dumper, i+1, count);
		if (dumper->flags & GF_EXPORT_DO_ABORT) break;
	}
	fprintf(nhml, "</NHNTStream>\n");
	fclose(med);
	fclose(nhml);
	return GF_OK;
}

GF_Err gf_media_export(GF_MediaExporter *dumper)
{
	if (!dumper) return GF_BAD_PARAM;
	if (!dumper->out_name && !dumper->flags & GF_EXPORT_PROBE_ONLY) return GF_BAD_PARAM;
	
	if (dumper->flags & GF_EXPORT_NATIVE) return gf_media_export_native(dumper);
	else if (dumper->flags & GF_EXPORT_RAW_SAMPLES) return gf_media_export_samples(dumper);
	else if (dumper->flags & GF_EXPORT_NHNT) return gf_media_export_nhnt(dumper);
	else if (dumper->flags & GF_EXPORT_AVI) return gf_media_export_avi(dumper);
	else if (dumper->flags & GF_EXPORT_MP4) return gf_media_export_isom(dumper);
	else if (dumper->flags & GF_EXPORT_AVI_NATIVE) return gf_media_export_avi_track(dumper);
	else if (dumper->flags & GF_EXPORT_NHML) return gf_media_export_nhml(dumper);
	else return GF_BAD_PARAM;
}

#endif

