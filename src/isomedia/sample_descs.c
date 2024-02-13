/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#ifndef GPAC_DISABLE_ISOM


GF_Err gf_isom_base_sample_entry_read(GF_SampleEntryBox *ptr, GF_BitStream *bs)
{
	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	if (!ptr->dataReferenceIndex) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISO file] dataReferenceIndex set to 0 in sample entry, overriding to 1\n"));
		ptr->dataReferenceIndex = 1;
	}
	return GF_OK;
}

void gf_isom_sample_entry_predestroy(GF_SampleEntryBox *ptr)
{
}

void gf_isom_sample_entry_init(GF_SampleEntryBox *ent)
{
	ent->internal_type = GF_ISOM_SAMPLE_ENTRY_GENERIC;
//	ent->internal_type = GF_ISOM_SAMPLE_ENTRY_MP4S;
}

void gf_isom_video_sample_entry_init(GF_VisualSampleEntryBox *ent)
{
	gf_isom_sample_entry_init((GF_SampleEntryBox*)ent);
	ent->internal_type = GF_ISOM_SAMPLE_ENTRY_VIDEO;
	ent->horiz_res = ent->vert_res = 0x00480000;
	ent->frames_per_sample = 1;
	ent->bit_depth = 0x18;
	ent->color_table_index = -1;
}

GF_Err gf_isom_video_sample_entry_read(GF_VisualSampleEntryBox *ptr, GF_BitStream *bs)
{
	GF_Err e;
	ISOM_DECREASE_SIZE(ptr, 78)

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ptr->version = gf_bs_read_u16(bs);
	ptr->revision = gf_bs_read_u16(bs);
	ptr->vendor = gf_bs_read_u32(bs);
	ptr->temporal_quality  = gf_bs_read_u32(bs);
	ptr->spatial_quality = gf_bs_read_u32(bs);
	ptr->Width = gf_bs_read_u16(bs);
	ptr->Height = gf_bs_read_u16(bs);
	ptr->horiz_res = gf_bs_read_u32(bs);
	ptr->vert_res = gf_bs_read_u32(bs);
	ptr->entry_data_size = gf_bs_read_u32(bs);
	ptr->frames_per_sample = gf_bs_read_u16(bs);
	gf_bs_read_data(bs, ptr->compressor_name, 32);
	ptr->compressor_name[32] = 0;
	ptr->bit_depth = gf_bs_read_u16(bs);
	ptr->color_table_index = gf_bs_read_u16(bs);
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
void gf_isom_video_sample_entry_write(GF_VisualSampleEntryBox *ptr, GF_BitStream *bs)
{
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);

	gf_bs_write_u16(bs, ptr->version);
	gf_bs_write_u16(bs, ptr->revision);
	gf_bs_write_u32(bs, ptr->vendor);
	gf_bs_write_u32(bs, ptr->temporal_quality);
	gf_bs_write_u32(bs, ptr->spatial_quality);
	gf_bs_write_u16(bs, ptr->Width);
	gf_bs_write_u16(bs, ptr->Height);
	gf_bs_write_u32(bs, ptr->horiz_res);
	gf_bs_write_u32(bs, ptr->vert_res);
	gf_bs_write_u32(bs, ptr->entry_data_size);
	gf_bs_write_u16(bs, ptr->frames_per_sample);
	gf_bs_write_data(bs, ptr->compressor_name, 32);
	gf_bs_write_u16(bs, ptr->bit_depth);
	gf_bs_write_u16(bs, ptr->color_table_index);


	if (gf_sys_old_arch_compat()) {
		//patch for old export
		GF_Box *clap = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_CLAP);
		GF_Box *pasp = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_PASP);
		GF_Box *colr = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_COLR);
		GF_Box *mdcv = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_MDCV);
		GF_Box *clli = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_CLLI);
		GF_Box *ccst = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_CCST);
		GF_Box *auxi = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_AUXI);
		GF_Box *rvcc = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_RVCC);
		GF_Box *sinf = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		GF_Box *btrt = gf_isom_box_find_child(ptr->child_boxes, GF_ISOM_BOX_TYPE_BTRT);
		GF_Box *fiel = gf_isom_box_find_child(ptr->child_boxes, GF_QT_BOX_TYPE_FIEL);
		GF_Box *gamma = gf_isom_box_find_child(ptr->child_boxes, GF_QT_BOX_TYPE_GAMA);

		if (clap) {
			gf_list_del_item(ptr->child_boxes, clap);
			gf_list_add(ptr->child_boxes, clap);
		}
		if (pasp) {
			gf_list_del_item(ptr->child_boxes, pasp);
			gf_list_add(ptr->child_boxes, pasp);
		}
		if (colr) {
			gf_list_del_item(ptr->child_boxes, colr);
			gf_list_add(ptr->child_boxes, colr);
		}
		if (fiel) {
			gf_list_del_item(ptr->child_boxes, fiel);
			gf_list_add(ptr->child_boxes, fiel);
		}
		if (gamma) {
			gf_list_del_item(ptr->child_boxes, gamma);
			gf_list_add(ptr->child_boxes, gamma);
		}
		if (mdcv) {
			gf_list_del_item(ptr->child_boxes, mdcv);
			gf_list_add(ptr->child_boxes, mdcv);
		}
		if (clli) {
			gf_list_del_item(ptr->child_boxes, clli);
			gf_list_add(ptr->child_boxes, clli);
		}
		if (ccst) {
			gf_list_del_item(ptr->child_boxes, ccst);
			gf_list_add(ptr->child_boxes, ccst);
		}
		if (auxi) {
			gf_list_del_item(ptr->child_boxes, auxi);
			gf_list_add(ptr->child_boxes, auxi);
		}
		if (rvcc) {
			gf_list_del_item(ptr->child_boxes, rvcc);
			gf_list_add(ptr->child_boxes, rvcc);
		}
		if (sinf) {
			gf_list_del_item(ptr->child_boxes, sinf);
			gf_list_add(ptr->child_boxes, sinf);
		}
		if (btrt) {
			gf_list_del_item(ptr->child_boxes, btrt);
			gf_list_add(ptr->child_boxes, btrt);
		}
	}

}

void gf_isom_video_sample_entry_size(GF_VisualSampleEntryBox *ent)
{
	ent->size += 78;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



void gf_isom_audio_sample_entry_init(GF_AudioSampleEntryBox *ptr)
{
	gf_isom_sample_entry_init((GF_SampleEntryBox*)ptr);
	ptr->internal_type = GF_ISOM_SAMPLE_ENTRY_AUDIO;
	ptr->channel_count = 2;
	ptr->bitspersample = 16;
}

GF_Err gf_isom_audio_sample_entry_read(GF_AudioSampleEntryBox *ptr, GF_BitStream *bs)
{
	GF_Err e;
	ISOM_DECREASE_SIZE(ptr, 28)

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ptr->version = gf_bs_read_u16(bs);
	ptr->revision = gf_bs_read_u16(bs);
	ptr->vendor = gf_bs_read_u32(bs);
	ptr->channel_count = gf_bs_read_u16(bs);
	ptr->bitspersample = gf_bs_read_u16(bs);
	ptr->compression_id = gf_bs_read_u16(bs);
	ptr->packet_size = gf_bs_read_u16(bs);
	ptr->samplerate_hi = gf_bs_read_u16(bs);
	ptr->samplerate_lo = gf_bs_read_u16(bs);

	if (!ptr->qtff_mode) return GF_OK;
	//QT only
	if (ptr->version==1) {
		ISOM_DECREASE_SIZE(ptr, 16)
		gf_bs_read_data(bs, (char *) ptr->extensions, 16);
	} else if (ptr->version==2) {
		ISOM_DECREASE_SIZE(ptr, 36)
		gf_bs_read_data(bs,  (char *) ptr->extensions, 36);
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Box *gf_isom_audio_sample_get_audio_codec_cfg_box(GF_AudioSampleEntryBox *ptr)
{
	GF_MPEGAudioSampleEntryBox *mpga = (GF_MPEGAudioSampleEntryBox *) ptr;
	switch (ptr->type) {
	case GF_ISOM_BOX_TYPE_MP4A:
		return (GF_Box *)mpga->esd;
	case GF_ISOM_BOX_TYPE_AC3:
	case GF_ISOM_BOX_TYPE_EC3:
		return (GF_Box *)mpga->cfg_ac3;
	case GF_ISOM_BOX_TYPE_OPUS:
		return (GF_Box *)mpga->cfg_opus;
	case GF_ISOM_BOX_TYPE_MHA1:
	case GF_ISOM_BOX_TYPE_MHA2:
		return (GF_Box *)mpga->cfg_mha;
	case GF_ISOM_BOX_TYPE_MLPA:
		return (GF_Box *)mpga->cfg_mlp;
	}
	return NULL;
}
void gf_isom_audio_sample_entry_write(GF_AudioSampleEntryBox *ptr, GF_BitStream *bs)
{
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);

	gf_bs_write_u16(bs, ptr->version);
	gf_bs_write_u16(bs, ptr->revision);
	gf_bs_write_u32(bs, ptr->vendor);
	gf_bs_write_u16(bs, ptr->channel_count);
	gf_bs_write_u16(bs, ptr->bitspersample);
	gf_bs_write_u16(bs, ptr->compression_id);
	gf_bs_write_u16(bs, ptr->packet_size);
	gf_bs_write_u16(bs, ptr->samplerate_hi);
	gf_bs_write_u16(bs, ptr->samplerate_lo);

	if (ptr->qtff_mode) {
		GF_Box *codec_ext = NULL;
		if (ptr->qtff_mode==GF_ISOM_AUDIO_QTFF_ON_NOEXT) {
			codec_ext = gf_isom_audio_sample_get_audio_codec_cfg_box(ptr);
		}

		if (ptr->version==1) {
			//direct copy of data
			if (ptr->qtff_mode==GF_ISOM_AUDIO_QTFF_ON_EXT_VALID) {
				gf_bs_write_data(bs,  (char *) ptr->extensions, 16);
			} else {
				gf_bs_write_u32(bs, codec_ext ? 1024 : 1);
				gf_bs_write_u32(bs, codec_ext ? 0 : ptr->bitspersample/8);
				gf_bs_write_u32(bs, codec_ext ? 0 : ptr->bitspersample/8*ptr->channel_count);
				gf_bs_write_u32(bs, codec_ext ? 0 : ptr->bitspersample <= 16 ? ptr->bitspersample/8 : 2);
			}
		} else if (ptr->version==2) {
			gf_bs_write_data(bs,  (char *) ptr->extensions, 36);
		}
	}
}

void gf_isom_audio_sample_entry_size(GF_AudioSampleEntryBox *ptr)
{
	ptr->size += 28;
	if (ptr->qtff_mode) {
		if (ptr->version==1) {
			ptr->size+=16;
		} else if (ptr->version==2) {
			ptr->size += 36;
		}
	}
}


#endif	/*GPAC_DISABLE_ISOM_WRITE*/



GF_EXPORT
GF_3GPConfig *gf_isom_3gp_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_3GPConfig *config, *res;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return NULL;

	config = NULL;
	entry = (GF_SampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return NULL;
	switch (entry->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return NULL;
		if (! ((GF_MPEGAudioSampleEntryBox*)entry)->cfg_3gpp) return NULL;
		config = & ((GF_MPEGAudioSampleEntryBox*)entry)->cfg_3gpp->cfg;
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return NULL;
		if (! ((GF_MPEGVisualSampleEntryBox*)entry)->cfg_3gpp) return NULL;
		config = & ((GF_MPEGVisualSampleEntryBox*)entry)->cfg_3gpp->cfg;
		break;
	default:
		return NULL;
	}
	if (!config) return NULL;

	res = (GF_3GPConfig*)gf_malloc(sizeof(GF_3GPConfig));
	if (res)
		memcpy(res, config, sizeof(GF_3GPConfig));
	return res;
}

GF_EXPORT
GF_AC3Config *gf_isom_ac3_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_AC3Config *res;
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return NULL;

	entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return NULL;
	if (!entry->cfg_ac3) return NULL;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return NULL;
	if ( (entry->cfg_ac3->type!=GF_ISOM_BOX_TYPE_DAC3) && (entry->cfg_ac3->type!=GF_ISOM_BOX_TYPE_DEC3) ) return NULL;

	res = (GF_AC3Config*)gf_malloc(sizeof(GF_AC3Config));
	if (res)
		memcpy(res, &entry->cfg_ac3->cfg, sizeof(GF_AC3Config));
	return res;
}

GF_EXPORT
GF_Err gf_isom_flac_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u8 **dsi, u32 *dsi_size)
{
	u32 type;
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (dsi) *dsi = NULL;
	if (dsi_size) *dsi_size = 0;
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;

	type = entry->type;
	if (type==GF_ISOM_BOX_TYPE_ENCA) {
		gf_isom_get_original_format_type(the_file, trackNumber, StreamDescriptionIndex, &type);
	}

	if (type!=GF_ISOM_BOX_TYPE_FLAC) return GF_BAD_PARAM;
	if (!entry->cfg_flac) return GF_OK;
	if (dsi) {
		*dsi = gf_malloc(sizeof(u8)*entry->cfg_flac->dataSize);
		memcpy(*dsi, entry->cfg_flac->data, entry->cfg_flac->dataSize);
	}
	if (dsi_size) *dsi_size = entry->cfg_flac->dataSize;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_truehd_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 *format_info, u32 *peak_data_rate)
{
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;
	if (entry->type != GF_ISOM_SUBTYPE_MLPA) return GF_BAD_PARAM;

	if (!entry->cfg_mlp) return GF_ISOM_INVALID_FILE;
	if (format_info) *format_info = entry->cfg_mlp->format_info;
	if (peak_data_rate) *peak_data_rate = entry->cfg_mlp->peak_data_rate;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_truehd_config_new(GF_ISOFile *the_file, u32 trackNumber, char *URLname, char *URNname, u32 format, u32 peak_rate, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGAudioSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	entry = (GF_MPEGAudioSampleEntryBox *)gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_MLPA);
	if (!entry) return GF_OUT_OF_MEM;
	entry->cfg_mlp = (GF_TrueHDConfigBox *)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DMLP);
	if (!entry->cfg_mlp) return GF_OUT_OF_MEM;
	entry->cfg_mlp->format_info = format;
	entry->cfg_mlp->peak_data_rate = (u16) peak_rate;
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	return e;
}
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_EXPORT
GF_Err gf_isom_opus_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_OpusConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGAudioSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	if (!cfg) return GF_BAD_PARAM;
	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	entry = (GF_MPEGAudioSampleEntryBox *)gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_OPUS);
	if (!entry) return GF_OUT_OF_MEM;
	entry->cfg_opus = (GF_OpusSpecificBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DOPS);
	if (!entry->cfg_opus) return GF_OUT_OF_MEM;
	entry->cfg_opus->opcfg = *cfg;
	entry->cfg_opus->opcfg.version = 0;

	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	return e;
}
#endif

static GF_Err gf_isom_opus_config_get_internal(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u8 **dsi, u32 *dsi_size, GF_OpusConfig *ocfg)
{
	u32 type;
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (dsi) *dsi = NULL;
	if (dsi_size) *dsi_size = 0;
	if (ocfg) memset(ocfg, 0, sizeof(GF_OpusConfig));

	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;

	type = entry->type;
	if (type==GF_ISOM_BOX_TYPE_ENCA) {
		gf_isom_get_original_format_type(the_file, trackNumber, StreamDescriptionIndex, &type);
	}
	if (type != GF_ISOM_SUBTYPE_OPUS)
		return GF_BAD_PARAM;

	if (!entry->cfg_opus)
		return GF_BAD_PARAM;

	if (dsi && dsi_size) {
		gf_odf_opus_cfg_write(&entry->cfg_opus->opcfg, dsi, dsi_size);
	}
	if (ocfg)
		*ocfg = entry->cfg_opus->opcfg;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_opus_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 **dsi, u32 *dsi_size)
{
	return gf_isom_opus_config_get_internal(isom_file, trackNumber, sampleDescriptionIndex, dsi, dsi_size, NULL);
}

GF_EXPORT
GF_Err gf_isom_opus_config_get_desc(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_OpusConfig *opcfg)
{
	return gf_isom_opus_config_get_internal(isom_file, trackNumber, sampleDescriptionIndex, NULL, NULL, opcfg);

}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_3gp_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_3GPConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	u32 cfg_type;
	GF_SampleDescriptionBox *stsd;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg) return GF_BAD_PARAM;

	switch (cfg->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
		if (trak->Media->handler->handlerType!=GF_ISOM_MEDIA_AUDIO) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_DAMR;
		break;
	case GF_ISOM_SUBTYPE_3GP_EVRC:
		if (trak->Media->handler->handlerType!=GF_ISOM_MEDIA_AUDIO) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_DEVC;
		break;
	case GF_ISOM_SUBTYPE_3GP_QCELP:
		if (trak->Media->handler->handlerType!=GF_ISOM_MEDIA_AUDIO) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_DQCP;
		break;
	case GF_ISOM_SUBTYPE_3GP_SMV:
		if (trak->Media->handler->handlerType!=GF_ISOM_MEDIA_AUDIO) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_DSMV;
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		if (!gf_isom_is_video_handler_type(trak->Media->handler->handlerType)) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_D263;
		break;
	case 0:
		return GF_BAD_PARAM;
	default:
		return GF_NOT_SUPPORTED;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stsd = trak->Media->information->sampleTable->SampleDescription;

	switch (cfg->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
	{
		GF_MPEGAudioSampleEntryBox *entry = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes, cfg->type);
		if (!entry) return GF_OUT_OF_MEM;
		entry->cfg_3gpp = (GF_3GPPConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, cfg_type);
		if (!entry->cfg_3gpp) {
			gf_isom_box_del((GF_Box *) entry);
			return GF_OUT_OF_MEM;
		}
		memcpy(&entry->cfg_3gpp->cfg, cfg, sizeof(GF_3GPConfig));
		entry->samplerate_hi = trak->Media->mediaHeader->timeScale;
		entry->dataReferenceIndex = dataRefIndex;
		*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	}
	break;
	case GF_ISOM_SUBTYPE_3GP_H263:
	{
		GF_MPEGVisualSampleEntryBox *entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes, cfg->type);
		if (!entry) return GF_OUT_OF_MEM;
		entry->cfg_3gpp = (GF_3GPPConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, cfg_type);
		if (!entry->cfg_3gpp) {
			gf_isom_box_del((GF_Box *) entry);
			return GF_OUT_OF_MEM;
		}
		memcpy(&entry->cfg_3gpp->cfg, cfg, sizeof(GF_3GPConfig));
		entry->dataReferenceIndex = dataRefIndex;
		*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	}
	break;
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_3gp_config_update(GF_ISOFile *the_file, u32 trackNumber, GF_3GPConfig *param, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_3GPConfig *cfg;
	GF_MPEGAudioSampleEntryBox *a_entry;
	GF_MPEGVisualSampleEntryBox *v_entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !param || !DescriptionIndex) return GF_BAD_PARAM;

	cfg = NULL;
	a_entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!a_entry) return GF_BAD_PARAM;
	v_entry = (GF_MPEGVisualSampleEntryBox *) a_entry;

	switch (a_entry->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		if (a_entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_ISOM_INVALID_FILE;
		cfg = &a_entry->cfg_3gpp->cfg;
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		if (v_entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_ISOM_INVALID_FILE;
		cfg = & v_entry->cfg_3gpp->cfg;
		break;
	default:
		break;
	}
	if (!cfg || (cfg->type != param->type)) return GF_BAD_PARAM;
	memcpy(cfg, param, sizeof(GF_3GPConfig));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_ac3_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AC3Config *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGAudioSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (cfg->is_ec3) {
		entry = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EC3);
		if (!entry) return GF_OUT_OF_MEM;
		entry->cfg_ac3 = (GF_AC3ConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DEC3);
	} else {
		entry = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_AC3);
		if (!entry) return GF_OUT_OF_MEM;
		entry->cfg_ac3 = (GF_AC3ConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DAC3);
	}
	if (!entry->cfg_ac3) {
		gf_isom_box_del((GF_Box *) entry);
		return GF_OUT_OF_MEM;
	}
	memcpy(&entry->cfg_ac3->cfg, cfg, sizeof(GF_AC3Config));
	entry->samplerate_hi = trak->Media->mediaHeader->timeScale;
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}

GF_EXPORT
GF_Err gf_isom_ac3_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AC3Config *cfg)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGAudioSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg || !sampleDescriptionIndex) return GF_BAD_PARAM;

	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (!entry->cfg_ac3) return GF_BAD_PARAM;

	if (entry->cfg_ac3->cfg.is_ec3) {
		if (!cfg->is_ec3) return GF_BAD_PARAM;
	} else {
		if (cfg->is_ec3) return GF_BAD_PARAM;
	}
	memcpy(&entry->cfg_ac3->cfg, cfg, sizeof(GF_AC3Config));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_flac_config_new(GF_ISOFile *the_file, u32 trackNumber, u8 *metadata, u32 metadata_size, const char *URLname, const char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGAudioSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	entry = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FLAC);
	if (!entry) return GF_OUT_OF_MEM;
	entry->cfg_flac = (GF_FLACConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DFLA);

	if (!entry->cfg_flac) {
		gf_isom_box_del((GF_Box *) entry);
		return GF_OUT_OF_MEM;
	}
	entry->cfg_flac->dataSize = metadata_size;
	entry->cfg_flac->data = gf_malloc(sizeof(u8)*metadata_size);
	memcpy(entry->cfg_flac->data, metadata, sizeof(u8)*metadata_size);
	entry->samplerate_hi = trak->Media->mediaHeader->timeScale;
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}



GF_EXPORT
GF_Err gf_isom_new_mj2k_description(GF_ISOFile *the_file, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_len)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex=0;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	GF_MPEGVisualSampleEntryBox *entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->SampleDescription->child_boxes, GF_ISOM_BOX_TYPE_MJP2);
	if (!entry) return GF_OUT_OF_MEM;
	entry->jp2h = (GF_J2KHeaderBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_JP2H);
	if (!entry->jp2h) {
		gf_isom_box_del_parent(&trak->Media->information->sampleTable->SampleDescription->child_boxes, (GF_Box *) entry);
		return GF_OUT_OF_MEM;
	}
	if (dsi && dsi_len) {
		GF_BitStream *bs = gf_bs_new(dsi, dsi_len, GF_BITSTREAM_READ);
		entry->jp2h->size = dsi_len;
		gf_isom_box_read((GF_Box *)entry->jp2h, bs);
		gf_bs_del(bs);
	}
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}

#endif	/*GPAC_DISABLE_ISOM_WRITE*/



GF_EXPORT
GF_Err gf_isom_get_dims_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_DIMSDescription *desc)
{
	GF_DIMSSampleEntryBox *dims;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !descriptionIndex || !desc) return GF_BAD_PARAM;

	dims = (GF_DIMSSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex-1);
	if (!dims) return GF_BAD_PARAM;
	if (dims->type != GF_ISOM_BOX_TYPE_DIMS) return GF_BAD_PARAM;

	memset(desc, 0, sizeof(GF_DIMSDescription));
	if (dims->config) {
		desc->profile = dims->config->profile;
		desc->level = dims->config->level;
		desc->pathComponents = dims->config->pathComponents;
		desc->fullRequestHost = dims->config->fullRequestHost;
		desc->containsRedundant = dims->config->containsRedundant;
		desc->streamType = dims->config->streamType;
		desc->textEncoding = dims->config->textEncoding;
		desc->contentEncoding = dims->config->contentEncoding;
	}
	if (dims->scripts) {
		desc->content_script_types = dims->scripts->content_script_types;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_new_dims_description(GF_ISOFile *movie, u32 trackNumber, GF_DIMSDescription *desc, const char *URLname, const char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_DIMSSampleEntryBox *dims;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_SCENE) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	dims = (GF_DIMSSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DIMS);
	if (!dims) return GF_OUT_OF_MEM;
	dims->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, dims);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	dims->config = (GF_DIMSSceneConfigBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIMC);
	if (!dims->config) return GF_OUT_OF_MEM;

	dims->config->profile = desc->profile;
	dims->config->level = desc->level;
	dims->config->pathComponents = desc->pathComponents;
	dims->config->fullRequestHost = desc->fullRequestHost;
	dims->config->containsRedundant = desc->containsRedundant;
	if (!dims->config->containsRedundant) dims->config->containsRedundant = 1;
	dims->config->streamType = desc->streamType;
	dims->config->textEncoding = gf_strdup(desc->textEncoding ? desc->textEncoding : "");
	dims->config->contentEncoding = gf_strdup(desc->contentEncoding ? desc->contentEncoding : "");

	if (desc->content_script_types) {
		dims->scripts = (GF_DIMSScriptTypesBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIST);
		if (!dims->scripts) return GF_OUT_OF_MEM;
		dims->scripts->content_script_types = gf_strdup(desc->content_script_types);
	}
	return e;
}

#if 0 //unused
GF_Err gf_isom_update_dims_description(GF_ISOFile *movie, u32 trackNumber, GF_DIMSDescription *desc, char *URLname, char *URNname, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_DIMSSampleEntryBox *dims;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !desc || !DescriptionIndex) return GF_BAD_PARAM;

	dims = (GF_DIMSSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!dims) return GF_BAD_PARAM;
	if (dims->type != GF_ISOM_BOX_TYPE_DIMS) return GF_BAD_PARAM;
	if (!dims->config) {
		dims->config = (GF_DIMSSceneConfigBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIMC);
		if (!dims->config) return GF_OUT_OF_MEM;
	}

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	dims->config->profile = desc->profile;
	dims->config->level = desc->level;
	dims->config->pathComponents = desc->pathComponents;
	dims->config->fullRequestHost = desc->fullRequestHost;
	dims->config->containsRedundant = desc->containsRedundant;
	dims->config->streamType = desc->streamType;

	if (dims->config->textEncoding) gf_free(dims->config->textEncoding);
	dims->config->textEncoding = gf_strdup(desc->textEncoding ? desc->textEncoding : "");

	if (dims->config->contentEncoding) gf_free(dims->config->contentEncoding);
	dims->config->contentEncoding = gf_strdup(desc->contentEncoding ? desc->contentEncoding : "");

	if (desc->content_script_types) {
		if (!dims->scripts) {
			dims->scripts = (GF_DIMSScriptTypesBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIST);
			if (!dims->scripts) return GF_OUT_OF_MEM;
		}
		if (dims->scripts->content_script_types) gf_free(dims->scripts->content_script_types);
		dims->scripts->content_script_types = gf_strdup(desc->content_script_types ? desc->content_script_types  :"");
	} else if (dims->scripts) {
		gf_isom_box_del_parent(&dims->child_boxes, (GF_Box *) dims->scripts);
		dims->scripts = NULL;
	}
	return GF_OK;
}
#endif

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_EXPORT
GF_Err gf_isom_get_udts_config(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_UDTSConfig *cfg)
{
	GF_Box *sample_entry;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	GF_UDTSSpecificBox *udts = NULL;

	if (!trak || !descriptionIndex || !cfg) return GF_BAD_PARAM;
	sample_entry = (GF_Box *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex-1);
	if (sample_entry && sample_entry->type == GF_ISOM_BOX_TYPE_DTSX)
		udts = (GF_UDTSSpecificBox *)gf_list_get(sample_entry->child_boxes, 0);

	memset(cfg, 0, sizeof(GF_UDTSConfig));
	if (!udts)
		return GF_BAD_PARAM;

	cfg->DecoderProfileCode = udts->cfg.DecoderProfileCode;
	cfg->FrameDurationCode = udts->cfg.FrameDurationCode;
	cfg->MaxPayloadCode = udts->cfg.MaxPayloadCode;
	cfg->NumPresentationsCode = udts->cfg.NumPresentationsCode;
	cfg->ChannelMask = udts->cfg.ChannelMask;
	cfg->BaseSamplingFrequencyCode = udts->cfg.BaseSamplingFrequencyCode;
	cfg->SampleRateMod = udts->cfg.SampleRateMod;
	cfg->RepresentationType = udts->cfg.RepresentationType;
	cfg->StreamIndex = udts->cfg.RepresentationType;
	return GF_OK;
}


GF_Err LSR_UpdateESD(GF_LASeRSampleEntryBox *lsr, GF_ESD *esd)
{
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)lsr, GF_TRUE);

	if (lsr->descr) gf_isom_box_del_parent(&lsr->child_boxes, (GF_Box *) lsr->descr);
	lsr->descr = NULL;
	btrt->avgBitrate = esd->decoderConfig->avgBitrate;
	btrt->maxBitrate = esd->decoderConfig->maxBitrate;
	btrt->bufferSizeDB = esd->decoderConfig->bufferSizeDB;

	if (gf_list_count(esd->IPIDataSet)
	        || gf_list_count(esd->IPMPDescriptorPointers)
	        || esd->langDesc
	        || gf_list_count(esd->extensionDescriptors)
	        || esd->ipiPtr || esd->qos || esd->RegDescriptor) {

		lsr->descr = (GF_MPEG4ExtensionDescriptorsBox *)gf_isom_box_new_parent(&lsr->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
		if (!lsr->descr) return GF_OUT_OF_MEM;
		if (esd->RegDescriptor) {
			gf_list_add(lsr->descr->descriptors, esd->RegDescriptor);
			esd->RegDescriptor = NULL;
		}
		if (esd->qos) {
			gf_list_add(lsr->descr->descriptors, esd->qos);
			esd->qos = NULL;
		}
		if (esd->ipiPtr) {
			gf_list_add(lsr->descr->descriptors, esd->ipiPtr);
			esd->ipiPtr= NULL;
		}

		while (gf_list_count(esd->IPIDataSet)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPIDataSet, 0);
			gf_list_rem(esd->IPIDataSet, 0);
			gf_list_add(lsr->descr->descriptors, desc);
		}
		while (gf_list_count(esd->IPMPDescriptorPointers)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPMPDescriptorPointers, 0);
			gf_list_rem(esd->IPMPDescriptorPointers, 0);
			gf_list_add(lsr->descr->descriptors, desc);
		}
		if (esd->langDesc) {
			gf_list_add(lsr->descr->descriptors, esd->langDesc);
			esd->langDesc = NULL;
		}
		while (gf_list_count(esd->extensionDescriptors)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->extensionDescriptors, 0);
			gf_list_rem(esd->extensionDescriptors, 0);
			gf_list_add(lsr->descr->descriptors, desc);
		}
	}

	/*update GF_AVCConfig*/
	if (!lsr->lsr_config) {
		lsr->lsr_config = (GF_LASERConfigurationBox *)gf_isom_box_new_parent(&lsr->child_boxes,  GF_ISOM_BOX_TYPE_LSRC);
		if (!lsr->lsr_config) return GF_OUT_OF_MEM;
	}
	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		lsr->lsr_config->hdr = gf_realloc(lsr->lsr_config->hdr, sizeof(char) * esd->decoderConfig->decoderSpecificInfo->dataLength);
		lsr->lsr_config->hdr_size = esd->decoderConfig->decoderSpecificInfo->dataLength;
		memcpy(lsr->lsr_config->hdr, esd->decoderConfig->decoderSpecificInfo->data, sizeof(char)*esd->decoderConfig->decoderSpecificInfo->dataLength);
	}
	gf_odf_desc_del((GF_Descriptor *)esd);
	return GF_OK;
}

/* MetadataSampleEntry */
GF_EXPORT
GF_Err gf_isom_get_xml_metadata_description(GF_ISOFile *file, u32 track, u32 sampleDescription,
        const char **_namespace, const char **schema_loc, const char **content_encoding)
{
	GF_TrackBox *trak;
	GF_MetaDataSampleEntryBox *ptr;
	if (_namespace) *_namespace = NULL;
	if (content_encoding) *content_encoding = NULL;
	if (schema_loc) *schema_loc = NULL;
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !sampleDescription) return GF_BAD_PARAM;
	ptr = (GF_MetaDataSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescription-1);
	if (!ptr) return GF_BAD_PARAM;

	if (schema_loc) *schema_loc = ptr->xml_schema_loc;
	if (_namespace) *_namespace = ptr->xml_namespace;
	if (content_encoding) *content_encoding = ptr->content_encoding;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_new_xml_metadata_description(GF_ISOFile *movie, u32 trackNumber,
        const char *_namespace, const char *schema_loc, const char *content_encoding,
        u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MetaDataSampleEntryBox *metad;
	char *URLname = NULL;
	char *URNname = NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !_namespace)
		return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_MPEG_SUBT:
	case GF_ISOM_MEDIA_META:
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	metad = (GF_MetaDataSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_METX);
	if (!metad) return GF_OUT_OF_MEM;

	metad->dataReferenceIndex = dataRefIndex;
	metad->xml_namespace = gf_strdup(_namespace);
	if (content_encoding) metad->content_encoding = gf_strdup(content_encoding);
	if (schema_loc) metad->xml_schema_loc = gf_strdup(schema_loc);

	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, metad);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/* XMLSubtitleSampleEntry */
GF_EXPORT
GF_Err gf_isom_xml_subtitle_get_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex,
        const char **xmlnamespace, const char **xml_schema_loc, const char **mimes)
{
	GF_TrackBox *trak;
	GF_MetaDataSampleEntryBox *entry;
	if (xmlnamespace) *xmlnamespace = NULL;
	if (xml_schema_loc) *xml_schema_loc = NULL;
	if (mimes) *mimes = NULL;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MetaDataSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
 
	if ((entry->type!=GF_ISOM_BOX_TYPE_STPP) && (entry->type!=GF_ISOM_BOX_TYPE_METX)) {
		return GF_BAD_PARAM;
	}

	if (entry->mime_type) {
		if (mimes) *mimes = entry->mime_type;
	}
	if (entry->xml_schema_loc) {
		if (xml_schema_loc) *xml_schema_loc = entry->xml_schema_loc;
	}
	if (xmlnamespace)
		*xmlnamespace = entry->xml_namespace;
	return GF_OK;
}


GF_EXPORT
const char *gf_isom_subtitle_get_mime(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_TextConfigBox *mime;
	GF_MetaDataSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return NULL;

	entry = (GF_MetaDataSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return NULL;

	mime = (GF_TextConfigBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_MIME);
	if (!mime) return NULL;
	return mime->config;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_subtitle_set_mime(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const char *codec_params)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_TextConfigBox *mime;
	GF_MetaDataSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MetaDataSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;

	mime = (GF_TextConfigBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_MIME);
	if (!mime) {
		if (!codec_params) return GF_OK;
		mime = (GF_TextConfigBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_MIME);
		if (!mime) return GF_OUT_OF_MEM;
	}
	if (!codec_params) {
		gf_isom_box_del_parent(&entry->child_boxes, (GF_Box *)mime);
		return GF_OK;
	}
	if (mime->config) gf_free(mime->config);
	mime->config = gf_strdup(codec_params);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_new_xml_subtitle_description(GF_ISOFile  *movie, u32 trackNumber,
        const char *xmlnamespace, const char *xml_schema_loc, const char *mimes,
        u32 *outDescriptionIndex)
{
	GF_TrackBox                 *trak;
	GF_Err                      e;
	u32                         dataRefIndex;
	GF_MetaDataSampleEntryBox *stpp;
	char *URLname = NULL;
	char *URNname = NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_MPEG_SUBT:
	case GF_ISOM_MEDIA_META:
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!xmlnamespace) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("XML (Subtitle, Metadata or Text) SampleEntry: namespace is mandatory. Abort.\n"));
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stpp = (GF_MetaDataSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STPP);
	if (!stpp) return GF_OUT_OF_MEM;
	stpp->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, stpp);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	stpp->xml_namespace = gf_strdup(xmlnamespace);
	if (xml_schema_loc) stpp->xml_schema_loc = gf_strdup(xml_schema_loc); //optional
	if (mimes) stpp->mime_type = gf_strdup(mimes); //optional
	return e;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


/* SimpleTextSampleEntry: also used for MetadataTextSampleEntry and SubtitleTextSampleEntry */
GF_EXPORT
GF_Err gf_isom_stxt_get_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex,
                                    const char **mime, const char **encoding, const char **config)
{
	GF_TrackBox *trak;
	GF_MetaDataSampleEntryBox *entry;
	if (mime) *mime = NULL;
	if (config) *config = NULL;
	if (encoding) *encoding = NULL;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MetaDataSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry ||
	        ((entry->type!=GF_ISOM_BOX_TYPE_STXT) &&
	         (entry->type!=GF_ISOM_BOX_TYPE_METT) &&
	         (entry->type!=GF_ISOM_BOX_TYPE_SBTT))) {
		return GF_BAD_PARAM;
	}

	if (entry->config) {
		if (config) *config = entry->config->config;
	}
	if (entry->mime_type) {
		if (mime) *mime = entry->mime_type;
	}
	if (entry->content_encoding) {
		if (encoding) *encoding = entry->content_encoding;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_new_stxt_description(GF_ISOFile *movie, u32 trackNumber, u32 type,
                                    const char *mime, const char *encoding, const char * config,
                                    u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MetaDataSampleEntryBox *sample_entry;
	char *URLname = NULL;
	char *URNname = NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_MPEG_SUBT:
	case GF_ISOM_MEDIA_META:
	case GF_ISOM_MEDIA_SCENE:
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}
	switch (type) {
	case GF_ISOM_SUBTYPE_SBTT:
	case GF_ISOM_SUBTYPE_STXT:
	case GF_ISOM_SUBTYPE_METT:
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("SampleEntry shall be either Metadata, Subtitle or SimpleText. Abort.\n"));
		return GF_BAD_PARAM;
	}

	if (!mime) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Text (Metadata, Subtitle or SimpleText) missing mime, using text/plain.\n"));
		mime = "text/plain";
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	sample_entry = (GF_MetaDataSampleEntryBox *) gf_isom_box_new(type);
	if (!sample_entry) return GF_OUT_OF_MEM;
	sample_entry->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, sample_entry);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	sample_entry->mime_type = gf_strdup(mime);
	if (encoding) sample_entry->content_encoding = gf_strdup(encoding);
	if (config) {
		sample_entry->config = (GF_TextConfigBox*) gf_isom_box_new_parent(&sample_entry->child_boxes, GF_ISOM_BOX_TYPE_TXTC);
		if (!sample_entry->config) return GF_OUT_OF_MEM;
		sample_entry->config->config = gf_strdup(config);
		if (!sample_entry->config->config) return GF_OUT_OF_MEM;
	}
	return e;
}

#if 0 //unused

/*! updates simple streaming text config
\param isom_file the target ISO file
\param trackNumber the target track
\param encoding the text encoding, if any
\param config the configuration string, if any
\param sampleDescriptionIndex the target sample description index
\return error if any
*/
GF_Err gf_isom_update_stxt_description(GF_ISOFile *movie, u32 trackNumber,
                                       const char *encoding, const char *config,
                                       u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MetaDataSampleEntryBox *sample_entry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;

	sample_entry = (GF_MetaDataSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!sample_entry) return GF_BAD_PARAM;
	if (sample_entry->type != GF_ISOM_BOX_TYPE_METT &&
	        sample_entry->type != GF_ISOM_BOX_TYPE_SBTT &&
	        sample_entry->type != GF_ISOM_BOX_TYPE_STXT) {
		return GF_BAD_PARAM;
	}

	if (!sample_entry->config) {
		sample_entry->config = (GF_TextConfigBox*) gf_isom_box_new_parent(&sample_entry->child_boxes, GF_ISOM_BOX_TYPE_TXTC);
		if (!sample_entry->config) return GF_OUT_OF_MEM;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (sample_entry->config->config) {
		gf_free(sample_entry->config->config);
	}
	sample_entry->config->config = gf_strdup(config);

	if (sample_entry->content_encoding) {
		gf_free(sample_entry->content_encoding);
	}
	if (encoding) {
		sample_entry->content_encoding = gf_strdup(encoding);
	}
	return GF_OK;
}
#endif


#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_VTT

GF_WebVTTSampleEntryBox *gf_webvtt_isom_get_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex)
{
	GF_WebVTTSampleEntryBox *wvtt;
	GF_TrackBox *trak;

	if (!descriptionIndex) return NULL;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return NULL;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
	case GF_ISOM_MEDIA_MPEG_SUBT:
		break;
	default:
		return NULL;
	}

	wvtt = (GF_WebVTTSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex - 1);
	if (!wvtt) return NULL;
	switch (wvtt->type) {
	case GF_ISOM_BOX_TYPE_WVTT:
		break;
	default:
		return NULL;
	}
	return wvtt;
}

GF_EXPORT
const char *gf_isom_get_webvtt_config(GF_ISOFile *file, u32 track, u32 index)
{
	GF_WebVTTSampleEntryBox *wvtt = gf_webvtt_isom_get_description(file, track, index);
	if (!wvtt) return NULL;
	return wvtt->config ? wvtt->config->string : NULL;
}

#endif /*GPAC_DISABLE_VTT*/

#ifndef GPAC_DISABLE_ISOM_WRITE

#ifndef GPAC_DISABLE_VTT

#if 0 //unused
/*! updates a WebVTT sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param config the WebVTT configuration string
\return error if any
*/
GF_Err gf_isom_update_webvtt_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, const char *config)
{
	GF_Err e;
	GF_WebVTTSampleEntryBox *wvtt;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
	case GF_ISOM_MEDIA_MPEG_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	wvtt = (GF_WebVTTSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex - 1);
	if (!wvtt) return GF_BAD_PARAM;
	switch (wvtt->type) {
	case GF_ISOM_BOX_TYPE_WVTT:
		break;
	default:
		return GF_BAD_PARAM;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (wvtt->config) gf_isom_box_del_parent(&wvtt->child_boxes, (GF_Box *)wvtt->config);
	wvtt->config = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_VTTC_CONFIG, config, &wvtt->child_boxes);
	return GF_OK;
}
#endif


GF_Err gf_isom_new_webvtt_description(GF_ISOFile *movie, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, const char *config)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_WebVTTSampleEntryBox *wvtt;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
	case GF_ISOM_MEDIA_MPEG_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	wvtt = (GF_WebVTTSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_WVTT);
	if (!wvtt) return GF_OUT_OF_MEM;
	wvtt->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, wvtt);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	if (config) {
		wvtt->config = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_VTTC_CONFIG, config, &wvtt->child_boxes);
	}
	return e;
}

#endif /*GPAC_DISABLE_VTT*/
#endif //GPAC_DISABLE_ISOM_WRITE

GF_BitRateBox *gf_isom_sample_entry_get_bitrate(GF_SampleEntryBox *ent, Bool create)
{
	u32 i=0;
	GF_BitRateBox *a;
	while ((a = (GF_BitRateBox *)gf_list_enum(ent->child_boxes, &i))) {
		if (a->type==GF_ISOM_BOX_TYPE_BTRT) return a;
	}
	if (!create) return NULL;
	a = (GF_BitRateBox *) gf_isom_box_new_parent(&ent->child_boxes, GF_ISOM_BOX_TYPE_BTRT);
	return a;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_update_bitrate(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, u32 average_bitrate, u32 max_bitrate, u32 decode_buffer_size)
{
	GF_BitRateBox *a;
	GF_Err e;
	GF_SampleEntryBox *ent;
	u32 i, count;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);

	if (!trak || !trak->Media) return GF_BAD_PARAM;

	count = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	for (i=0; i<count; i++) {
		u32 ent_type;
		GF_ProtectionSchemeInfoBox *sinf;
		GF_ESDBox *esds=NULL;
		if (sampleDescriptionIndex && (sampleDescriptionIndex!=i+1)) continue;

		ent = (GF_SampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (!ent) return GF_BAD_PARAM;

		ent_type = ent->type;
		switch (ent_type) {
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCS:
			sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_SINF);
			if (sinf && sinf->original_format)
				ent_type = sinf->original_format->data_format;
			break;
		}
		switch (ent_type) {
		case GF_ISOM_BOX_TYPE_MP4V:
			esds = ((GF_MPEGVisualSampleEntryBox *)ent)->esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4A:
			esds = ((GF_MPEGAudioSampleEntryBox *)ent)->esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4S:
			esds = ((GF_MPEGSampleEntryBox *) ent)->esd;
			break;
		}
		//using mpeg4 esd
		if (esds) {
			if (esds->desc && esds->desc->decoderConfig) {
				esds->desc->decoderConfig->avgBitrate = average_bitrate;
				esds->desc->decoderConfig->maxBitrate = max_bitrate;
				if (decode_buffer_size)
					esds->desc->decoderConfig->bufferSizeDB = decode_buffer_size;
			}
			continue;
		}
		
		//using BTRT
		if (!max_bitrate && average_bitrate) max_bitrate = average_bitrate;
		a = gf_isom_sample_entry_get_bitrate(ent, max_bitrate ? GF_TRUE : GF_FALSE);

		if (!max_bitrate) {
			if (a) {
				gf_isom_box_del_parent(&ent->child_boxes, (GF_Box *) a);
			}
		} else {
			a->avgBitrate = average_bitrate;
			a->maxBitrate = max_bitrate;
			if (decode_buffer_size)
				a->bufferSizeDB = decode_buffer_size;
		}
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_tmcd_config_new(GF_ISOFile *the_file, u32 trackNumber, u32 fps_num, u32 fps_den, s32 frames_per_counter_tick, Bool is_drop, Bool is_counter, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_Box *tmcd;
	GF_GenericMediaHeaderInfoBox *gmin;
	GF_TimeCodeMediaInformationBox *tcmi;
	GF_TimeCodeSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, NULL, NULL, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, NULL, NULL, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	gmin = (GF_GenericMediaHeaderInfoBox *) gf_isom_box_find_child(trak->Media->information->InfoHeader->child_boxes, GF_QT_BOX_TYPE_GMIN);
	if (!gmin) {
		gmin = (GF_GenericMediaHeaderInfoBox *) gf_isom_box_new_parent(&trak->Media->information->InfoHeader->child_boxes, GF_QT_BOX_TYPE_GMIN);
		if (!gmin) return GF_OUT_OF_MEM;
	}

	tmcd = gf_isom_box_find_child(trak->Media->information->InfoHeader->child_boxes, GF_QT_BOX_TYPE_TMCD);
	if (!tmcd) {
		//default container box, use GMHD to create it
		tmcd = gf_isom_box_new_parent(&trak->Media->information->InfoHeader->child_boxes, GF_ISOM_BOX_TYPE_GMHD);
		if (!tmcd) return GF_OUT_OF_MEM;
		tmcd->type = GF_QT_BOX_TYPE_TMCD;
	}
	tcmi = (GF_TimeCodeMediaInformationBox *) gf_isom_box_find_child(tmcd->child_boxes, GF_QT_BOX_TYPE_TCMI);
	if (!tcmi) {
		tcmi = (GF_TimeCodeMediaInformationBox *) gf_isom_box_new_parent(&tmcd->child_boxes, GF_QT_BOX_TYPE_TCMI);
		if (!tcmi) return GF_OUT_OF_MEM;
	}

	entry = (GF_TimeCodeSampleEntryBox *) gf_isom_box_new_ex(GF_QT_BOX_TYPE_TMCD, GF_ISOM_BOX_TYPE_STSD, GF_FALSE, GF_FALSE, GF_FALSE);
	if (!entry) return GF_OUT_OF_MEM;
	entry->flags = 0;
	if (is_drop) entry->flags |= 0x00000001;
	if (is_counter) entry->flags |= 0x00000008;

	entry->timescale = fps_num;
	entry->frame_duration = fps_den;
	entry->frames_per_counter_tick = (u8) frames_per_counter_tick;
	
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}

GF_Err gf_isom_new_mpha_description(GF_ISOFile *movie, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_size, u32 mha_subtype)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGAudioSampleEntryBox *mpa;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_AUDIO:
		break;
	default:
		return GF_BAD_PARAM;
	}
	switch (mha_subtype) {
	case GF_ISOM_BOX_TYPE_MHA1:
	case GF_ISOM_BOX_TYPE_MHA2:
		if (!dsi || (dsi_size<6)) return GF_BAD_PARAM;
		break;
	case GF_ISOM_BOX_TYPE_MHM1:
	case GF_ISOM_BOX_TYPE_MHM2:
		if (dsi_size && (dsi_size<6)) return GF_BAD_PARAM;
		break;
	default:
		return GF_BAD_PARAM;
	}

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	mpa = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(mha_subtype);
	if (!mpa) return GF_OUT_OF_MEM;
	mpa->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, mpa);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	if (dsi) {
		mpa->cfg_mha = (GF_MHAConfigBox *) gf_isom_box_new_parent(&mpa->child_boxes, GF_ISOM_BOX_TYPE_MHAC);
		if (!mpa->cfg_mha) return GF_OUT_OF_MEM;
		mpa->cfg_mha->configuration_version = dsi[0];
		mpa->cfg_mha->mha_pl_indication = dsi[1];
		mpa->cfg_mha->reference_channel_layout = dsi[2];
		mpa->cfg_mha->mha_config_size = dsi[3];
		mpa->cfg_mha->mha_config_size <<= 8;
		mpa->cfg_mha->mha_config_size |= dsi[4];
		mpa->cfg_mha->mha_config = gf_malloc(sizeof(u8) * mpa->cfg_mha->mha_config_size);
		if (!mpa->cfg_mha->mha_config) return GF_OUT_OF_MEM;
		memcpy(mpa->cfg_mha->mha_config, dsi+5, dsi_size-5);
	}
	return GF_OK;
}

#endif	/*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_Err gf_isom_get_tmcd_config(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, u32 *tmcd_flags, u32 *tmcd_fps_num, u32 *tmcd_fps_den, u32 *tmcd_fpt)
{
	GF_TimeCodeSampleEntryBox *tmcd;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !descriptionIndex) return GF_BAD_PARAM;

	tmcd = (GF_TimeCodeSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex-1);
	if (!tmcd) return GF_BAD_PARAM;
	if (tmcd->type != GF_QT_BOX_TYPE_TMCD) return GF_BAD_PARAM;

	if (tmcd_flags) *tmcd_flags = tmcd->flags;
	if (tmcd_fps_num) *tmcd_fps_num = tmcd->timescale;
	if (tmcd_fps_den) *tmcd_fps_den = tmcd->frame_duration;
	if (tmcd_fpt) *tmcd_fpt = tmcd->frames_per_counter_tick;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_pcm_config(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, u32 *flags, u32 *pcm_size)
{
	GF_AudioSampleEntryBox *aent;
	GF_PCMConfigBox *pcmC;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !descriptionIndex) return GF_BAD_PARAM;

	aent = (GF_AudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex-1);
	if (!aent) return GF_BAD_PARAM;
	if ((aent->type != GF_ISOM_BOX_TYPE_IPCM) && (aent->type != GF_ISOM_BOX_TYPE_FPCM)) {
		Bool is_le = GF_FALSE;
		GF_Box *b = gf_isom_box_find_child(aent->child_boxes, GF_QT_BOX_TYPE_WAVE);
		//if no wave, assume big endian
		if (b) {
			GF_ChromaInfoBox *enda = (GF_ChromaInfoBox *)gf_isom_box_find_child(b->child_boxes, GF_QT_BOX_TYPE_ENDA);
			is_le = (enda && enda->chroma) ? GF_TRUE : GF_FALSE;
		}
		u32 bits=0;
		switch (aent->type) {
		case GF_QT_SUBTYPE_FL32: bits = 32; break;
		case GF_QT_SUBTYPE_FL64: bits = 64; break;
		case GF_QT_SUBTYPE_TWOS: bits = 16; is_le = GF_FALSE; break;
		case GF_QT_SUBTYPE_SOWT: bits = 16; is_le = GF_TRUE; break;
		case GF_QT_SUBTYPE_IN24: bits = 24; break;
		case GF_QT_SUBTYPE_IN32: bits = 32; break;
		default:
			return GF_BAD_PARAM;
		}
		if (pcm_size) *pcm_size = bits;
		if (flags) *flags = is_le ? 1 : 0;
		return GF_OK;
	}

	pcmC = (GF_PCMConfigBox *) gf_isom_box_find_child(aent->child_boxes, GF_ISOM_BOX_TYPE_PCMC);
	if (!pcmC) return GF_NON_COMPLIANT_BITSTREAM;

	if (flags) *flags = pcmC->format_flags;
	if (pcm_size) *pcm_size = pcmC->PCM_sample_size;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_lpcm_config(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, Double *sample_rate, u32 *nb_channels, u32 *flags, u32 *pcm_size)
{
	GF_AudioSampleEntryBox *aent;
	GF_TrackBox *trak;
	GF_BitStream *bs;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !descriptionIndex) return GF_BAD_PARAM;

	aent = (GF_AudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, descriptionIndex-1);
	if (!aent) return GF_BAD_PARAM;
	if (aent->type != GF_QT_SUBTYPE_LPCM)
		return GF_BAD_PARAM;
	if (aent->version != 2)
		return GF_BAD_PARAM;

	bs = gf_bs_new(aent->extensions, 36, GF_BITSTREAM_READ);
	/*offset*/gf_bs_read_u32(bs);
	Double sr = gf_bs_read_double(bs);
	if (sample_rate) *sample_rate = sr;
	u32 nb_ch = gf_bs_read_u32(bs);
	if (nb_channels) *nb_channels = nb_ch;
	/*res*/gf_bs_read_u32(bs);
	u32 constBitsPerChannel = gf_bs_read_u32(bs);
	if (pcm_size) *pcm_size=constBitsPerChannel;
	u32 PCM_flags = gf_bs_read_u32(bs);
	if (flags) *flags = PCM_flags;
	/*constBytesPerAudioPacket*/gf_bs_read_u32(bs);
	/*constLPCMFramesPerAudioPacket*/gf_bs_read_u32(bs);
	gf_bs_del(bs);
	return GF_OK;
}



#endif /*GPAC_DISABLE_ISOM*/
