/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
	if (ptr->size < 78) return GF_ISOM_INVALID_FILE;
	ptr->size -= 78;
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


	if (gf_sys_is_test_mode()) {
		//pacth for old export
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
	if (ptr->size<28) return GF_ISOM_INVALID_FILE;

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

	ptr->size -= 28;
	if (!ptr->is_qtff) return GF_OK;
	//QT only
	if (ptr->version==1) {
		if (ptr->size<16) return GF_ISOM_INVALID_FILE;
		gf_bs_read_data(bs, (char *) ptr->extensions, 16);
		ptr->size-=16;
	} else if (ptr->version==2) {
		if (ptr->size<36) return GF_ISOM_INVALID_FILE;
		gf_bs_read_data(bs,  (char *) ptr->extensions, 36);
		ptr->size -= 36;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

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

	if (ptr->is_qtff) {
		GF_Box *esds = NULL;
		if (ptr->is_qtff==1) {
			if ((ptr->type==GF_ISOM_BOX_TYPE_MP4A) && ((GF_MPEGAudioSampleEntryBox*)ptr)->esd) {
				esds = (GF_Box *) ((GF_MPEGAudioSampleEntryBox*)ptr)->esd;
			}
		}

		if (ptr->version==1) {
			//direct copy of data
			if (ptr->is_qtff==2) {
				gf_bs_write_data(bs,  (char *) ptr->extensions, 16);
			} else {
				gf_bs_write_u32(bs, esds ? 1024 : 1);
				gf_bs_write_u32(bs, esds ? 0 : ptr->bitspersample);
				gf_bs_write_u32(bs, esds ? 0 : ptr->bitspersample*ptr->channel_count);
				gf_bs_write_u32(bs, esds ? 0 : ptr->bitspersample/8);
			}
		} else if (ptr->version==2) {
			gf_bs_write_data(bs,  (char *) ptr->extensions, 36);
		}
	}
}

void gf_isom_audio_sample_entry_size(GF_AudioSampleEntryBox *ptr)
{
	ptr->size += 28;
	if (ptr->is_qtff) {
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
		if (! ((GF_MPEGAudioSampleEntryBox*)entry)->cfg_3gpp) return NULL;
		config = & ((GF_MPEGAudioSampleEntryBox*)entry)->cfg_3gpp->cfg;
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
		if (! ((GF_MPEGVisualSampleEntryBox*)entry)->cfg_3gpp) return NULL;
		config = & ((GF_MPEGVisualSampleEntryBox*)entry)->cfg_3gpp->cfg;
		break;
	default:
		return NULL;
	}
	if (!config) return NULL;

	res = (GF_3GPConfig*)gf_malloc(sizeof(GF_3GPConfig));
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
	if (!entry || !entry->cfg_ac3) return NULL;
	if ( (entry->type!=GF_ISOM_BOX_TYPE_AC3) && (entry->type!=GF_ISOM_BOX_TYPE_EC3) ) return NULL;
	if ( (entry->cfg_ac3->type!=GF_ISOM_BOX_TYPE_DAC3) && (entry->cfg_ac3->type!=GF_ISOM_BOX_TYPE_DEC3) ) return NULL;

	res = (GF_AC3Config*)gf_malloc(sizeof(GF_AC3Config));
	memcpy(res, &entry->cfg_ac3->cfg, sizeof(GF_AC3Config));
	return res;
}

GF_EXPORT
GF_Err gf_isom_flac_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u8 **dsi, u32 *dsi_size)
{
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (dsi) *dsi = NULL;
	if (dsi_size) *dsi_size = 0;
	if (!trak || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_MPEGAudioSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (entry->type!=GF_ISOM_BOX_TYPE_FLAC) return GF_BAD_PARAM;
	if (!entry->cfg_flac) return GF_OK;
	if (dsi) {
		*dsi = gf_malloc(sizeof(u8)*entry->cfg_flac->dataSize);
		memcpy(*dsi, entry->cfg_flac->data, entry->cfg_flac->dataSize);
	}
	if (dsi_size) *dsi_size = entry->cfg_flac->dataSize;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_EXPORT
GF_Err gf_isom_3gp_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_3GPConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
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
		if (!gf_isom_is_video_subtype(trak->Media->handler->handlerType)) return GF_BAD_PARAM;
		cfg_type = GF_ISOM_BOX_TYPE_D263;
		break;
	case 0:
		return GF_BAD_PARAM;
	default:
		return GF_NOT_SUPPORTED;
	}

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
		cfg = &a_entry->cfg_3gpp->cfg;
		break;
	case GF_ISOM_SUBTYPE_3GP_H263:
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
GF_Err gf_isom_ac3_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AC3Config *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
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
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
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
GF_Err gf_isom_flac_config_new(GF_ISOFile *the_file, u32 trackNumber, u8 *metadata, u32 metadata_size, char *URLname, char *URNname, u32 *outDescriptionIndex)
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
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
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
GF_Err gf_isom_new_mj2k_description(GF_ISOFile *the_file, u32 trackNumber, char *URLname, char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_len)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex=0;

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
GF_Err gf_isom_new_dims_description(GF_ISOFile *movie, u32 trackNumber, GF_DIMSDescription *desc, char *URLname, char *URNname, u32 *outDescriptionIndex)
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
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	dims = (GF_DIMSSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DIMS);
	dims->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, dims);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	dims->config = (GF_DIMSSceneConfigBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIMC);
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
		dims->scripts->content_script_types = gf_strdup(desc->content_script_types);
	}
	return e;
}

GF_EXPORT
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
	if (!dims->config)
		dims->config = (GF_DIMSSceneConfigBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIMC);

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
		if (!dims->scripts)
			dims->scripts = (GF_DIMSScriptTypesBox*) gf_isom_box_new_parent(&dims->child_boxes, GF_ISOM_BOX_TYPE_DIST);
		if (dims->scripts->content_script_types) gf_free(dims->scripts->content_script_types);
		dims->scripts->content_script_types = gf_strdup(desc->content_script_types ? desc->content_script_types  :"");
	} else if (dims->scripts) {
		gf_isom_box_del_parent(&dims->child_boxes, (GF_Box *) dims->scripts);
		dims->scripts = NULL;
	}
	return e;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

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
	if (!lsr->lsr_config) lsr->lsr_config = (GF_LASERConfigurationBox *)gf_isom_box_new_parent(&lsr->child_boxes,  GF_ISOM_BOX_TYPE_LSRC);
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
	*_namespace = NULL;
	*content_encoding = NULL;
	*schema_loc = NULL;
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

GF_Err gf_isom_update_xml_metadata_description(GF_ISOFile *movie, u32 trackNumber,
        const char *schema_loc, const char *encoding,
        u32 DescriptionIndex)
{
	/* TODO */
	return GF_NOT_SUPPORTED;
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

#ifndef GPAC_DISABLE_ISOM_WRITE

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
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!xmlnamespace) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("XML (Subtitle, Metadata or Text) SampleEntry: namespace is mandatory. Abort.\n"));
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
	stpp->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, stpp);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	stpp->xml_namespace = gf_strdup(xmlnamespace);
	if (xml_schema_loc) stpp->xml_schema_loc = gf_strdup(xml_schema_loc); //optional
	if (mimes) stpp->mime_type = gf_strdup(mimes); //optional
	return e;
}

GF_Err gf_isom_update_xml_subtitle_description(GF_ISOFile *movie, u32 trackNumber,
        u32 descriptionIndex, GF_GenericSubtitleSampleDescriptor *desc)
{
	GF_TrackBox *trak;
	GF_Err      e;

	if (!descriptionIndex || !desc) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_MPEG_SUBT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("SampleEntry shall be either Metadata, Subtitle or SimpleText. Abort.\n"));
		return GF_BAD_PARAM;
	}

	if (!mime) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("Text (Metadata, Subtitle or SimpleText) SampleEntry: mime is mandatory. Using text/plain.\n"));
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
	sample_entry->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, sample_entry);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	sample_entry->mime_type = gf_strdup(mime);
	if (encoding) sample_entry->content_encoding = gf_strdup(encoding);
	if (config) {
		sample_entry->config = (GF_TextConfigBox*) gf_isom_box_new_parent(&sample_entry->child_boxes, GF_ISOM_BOX_TYPE_TXTC);
		sample_entry->config->config = gf_strdup(config);
	}
	return e;
}


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

	if (!sample_entry->config)
		sample_entry->config = (GF_TextConfigBox*) gf_isom_box_new_parent(&sample_entry->child_boxes, GF_ISOM_BOX_TYPE_TXTC);

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
	return e;
}
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
	if (wvtt) {
		if (!movie->keep_utc)
			trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

		if (wvtt->config) gf_isom_box_del_parent(&wvtt->child_boxes, (GF_Box *)wvtt->config);
		wvtt->config = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_VTTC_CONFIG, config, &wvtt->child_boxes);
	} else {
		e = GF_BAD_PARAM;
	}
	return e;
}

GF_Err gf_isom_new_webvtt_description(GF_ISOFile *movie, u32 trackNumber, char *URLname, char *URNname, u32 *outDescriptionIndex, const char *config)
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
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	wvtt = (GF_WebVTTSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_WVTT);
	wvtt->dataReferenceIndex = dataRefIndex;
	gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, wvtt);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	if (config) {
		wvtt->config = (GF_StringBox *)boxstring_new_with_data(GF_ISOM_BOX_TYPE_VTTC_CONFIG, config, &wvtt->child_boxes);
	}
	return e;
}

#endif /*GPAC_DISABLE_VTT*/

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



#endif /*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
