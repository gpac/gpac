/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/internal/media_dev.h>
#endif

#ifndef GPAC_DISABLE_ISOM


Bool gf_isom_is_nalu_based_entry(GF_MediaBox *mdia, GF_SampleEntryBox *_entry)
{
	GF_MPEGVisualSampleEntryBox *entry;
	if (mdia->handler->handlerType != GF_ISOM_MEDIA_VISUAL) return 0;
	if (_entry->type==GF_ISOM_BOX_TYPE_GNRV) return 0;
	entry = (GF_MPEGVisualSampleEntryBox*)_entry;
	if (!entry) return 0;
	if (entry->avc_config || entry->svc_config || entry->hevc_config) return 1;
	return 0;
}


static void rewrite_nalus_list(GF_List *nalus, GF_BitStream *bs, Bool rewrite_start_codes, u32 nal_unit_size_field)
{
	u32 i, count = gf_list_count(nalus);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(nalus, i);
		if (rewrite_start_codes) gf_bs_write_u32(bs, 1);
		else gf_bs_write_int(bs, sl->size, 8*nal_unit_size_field);
		gf_bs_write_data(bs, sl->data, sl->size);
	}				
}

/* FIXME : unused function
static void merge_nalus_list(GF_List  *src, GF_List *dst)
{
	u32 i, count = gf_list_count(src);
	for (i=0; i<count; i++) {
		void *p = gf_list_get(src, i);
		if (p) gf_list_add(dst, p);
	}
}


static void merge_nalus(GF_MPEGVisualSampleEntryBox *entry, GF_List *sps, GF_List *pps)
{
	if (entry->avc_config) {
		merge_nalus_list(entry->avc_config->config->sequenceParameterSets, sps);
		merge_nalus_list(entry->avc_config->config->sequenceParameterSetExtensions, sps);
		merge_nalus_list(entry->avc_config->config->pictureParameterSets, pps);
	}
	if (entry->svc_config) {
		merge_nalus_list(entry->svc_config->config->sequenceParameterSets, sps);
		merge_nalus_list(entry->svc_config->config->pictureParameterSets, pps);
	}
}*/

/* Rewrite mode:
 * mode = 0: playback
 * mode = 1: streaming
 */
GF_Err gf_isom_nalu_sample_rewrite(GF_MediaBox *mdia, GF_ISOSample *sample, u32 sampleNumber, GF_MPEGVisualSampleEntryBox *entry)
{
	Bool is_hevc = 0;
	GF_Err e = GF_OK;
	GF_ISOSample *ref_samp;
	GF_BitStream *src_bs, *ref_bs, *dst_bs, *ps_bs;
	u64 offset;
	u32 ref_nalu_size, data_offset, data_length, copy_size, nal_size, max_size, di, nal_unit_size_field, cur_extract_mode, extractor_mode, ref_extract_mode;
	Bool rewrite_ps, rewrite_start_codes, insert_vdrd_code;
	u8 ref_track_ID, ref_track_num;
	s8 sample_offset, nal_type;
	u32 nal_hdr;
	u32 temporal_id = 0;
	char *buffer;
	GF_TrackBox *ref_trak;
	GF_ISOFile *file = mdia->mediaTrack->moov->mov;

	src_bs = ref_bs = dst_bs = ps_bs = NULL;
	ref_samp = NULL;
	buffer = NULL;
	rewrite_ps = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG) ? 1 : 0;
	if (! sample->IsRAP) rewrite_ps = 0;
	rewrite_start_codes = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG) ? 1 : 0;
	insert_vdrd_code = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_VDRD_FLAG) ? 1 : 0;
	if (!entry->svc_config) insert_vdrd_code = 0;
	extractor_mode = mdia->mediaTrack->extractor_mode&0x0000FFFF;

	if (extractor_mode != GF_ISOM_NALU_EXTRACT_LAYER_ONLY)
		insert_vdrd_code = 0;

	if (extractor_mode == GF_ISOM_NALU_EXTRACT_INSPECT) {
		if (!rewrite_ps && !rewrite_start_codes)
			return GF_OK;
	}

	if (!entry) return GF_BAD_PARAM;
	nal_unit_size_field = 0;
	/*if svc rewrire*/
	if (entry->svc_config && entry->svc_config->config) nal_unit_size_field = entry->svc_config->config->nal_unit_size;
	/*if mvc rewrire*/

	/*otherwise do nothing*/
	else if (!rewrite_ps && !rewrite_start_codes) {
		return GF_OK;
	}

	if (!nal_unit_size_field) {
		if (entry->avc_config) nal_unit_size_field = entry->avc_config->config->nal_unit_size;
		else if (entry->hevc_config) {
			nal_unit_size_field = entry->hevc_config->config->nal_unit_size;
			is_hevc = 1;
		}
	}
	if (!nal_unit_size_field) return GF_ISOM_INVALID_FILE;

	dst_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ps_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	src_bs = gf_bs_new(sample->data, sample->dataLength, GF_BITSTREAM_READ);
	max_size = 4096;

	/*rewrite start code with NALU delim*/
	if (rewrite_start_codes) {

		//we are SVC, don't write NALU delim, only insert VDRD NALU
		if (insert_vdrd_code) {
			gf_bs_write_int(dst_bs, 1, 32);
			gf_bs_write_int(dst_bs, GF_AVC_NALU_VDRD , 8);
		}
		//AVC/HEVC base, insert NALU delim
		else {
			gf_bs_write_int(dst_bs, 1, 32);
			if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
				gf_bs_write_int(dst_bs, 0, 1);
				gf_bs_write_int(dst_bs, GF_HEVC_NALU_ACCESS_UNIT, 6);
				gf_bs_write_int(dst_bs, 0, 6);
				gf_bs_write_int(dst_bs, 1, 3); //nuh_temporal_id_plus1 - cannot be 0, we use 1 by default, and overwrite it if needed at the end

				/*pic-type - by default we signal all slice types possible*/
				gf_bs_write_int(dst_bs, 2, 3);
				gf_bs_write_int(dst_bs, 0, 5);
#endif
			} else {
				gf_bs_write_int(dst_bs, (sample->data[0] & 0x60) | GF_AVC_NALU_ACCESS_UNIT, 8);
				gf_bs_write_int(dst_bs, 0xF0 , 8); /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;
			}
		}
	}

	if (rewrite_ps) {
		if (is_hevc) {
			u32 i, count;
			count = gf_list_count(entry->hevc_config->config->param_array);
			for (i=0; i<count; i++) {
				GF_HEVCParamArray *ar = gf_list_get(entry->hevc_config->config->param_array, i);
				rewrite_nalus_list(ar->nalus, ps_bs, rewrite_start_codes, nal_unit_size_field);
			}

			/*little optimization if we are not asked to start codes: copy over the sample*/
			if (!rewrite_start_codes) {
				if (ps_bs) {
					u8 nal_type = (sample->data[nal_unit_size_field] & 0x7E) >> 1;
					//temp fix - if we detect xPS in the begining of the sample do NOT copy the ps bitstream
					//this is not correct since we are not sure whether they are the same xPS or not, but it crashes openHEVC ...
					switch (nal_type) {
					case GF_HEVC_NALU_VID_PARAM:
					case GF_HEVC_NALU_SEQ_PARAM:
					case GF_HEVC_NALU_PIC_PARAM:
						break;
					default:
						gf_bs_transfer(dst_bs, ps_bs);
						break;
					}
					gf_bs_del(ps_bs);
					ps_bs = NULL;
				}
				gf_bs_write_data(dst_bs, sample->data, sample->dataLength);
				gf_free(sample->data);
				sample->data = NULL;
				gf_bs_get_content(dst_bs, &sample->data, &sample->dataLength);
				gf_bs_del(src_bs);
				gf_bs_del(dst_bs);
				return GF_OK;
			}
		} else {
			if (entry->avc_config) {
				rewrite_nalus_list(entry->avc_config->config->sequenceParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
				rewrite_nalus_list(entry->avc_config->config->sequenceParameterSetExtensions, ps_bs, rewrite_start_codes, nal_unit_size_field);
				rewrite_nalus_list(entry->avc_config->config->pictureParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
			}

			/*add svc config */
			if (entry->svc_config) {
				rewrite_nalus_list(entry->svc_config->config->sequenceParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
				rewrite_nalus_list(entry->svc_config->config->pictureParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
			}
		}
	}

	/*little optimization if we are not asked to rewrite extractors or start codes: copy over the sample*/
	if (!entry->svc_config && !rewrite_start_codes && !rewrite_ps) {
		if (ps_bs)
		{
			gf_bs_transfer(dst_bs, ps_bs);
			gf_bs_del(ps_bs);
			ps_bs = NULL;
		}
		gf_bs_write_data(dst_bs, sample->data, sample->dataLength);
		gf_free(sample->data);
		sample->data = NULL;
		gf_bs_get_content(dst_bs, &sample->data, &sample->dataLength);
		gf_bs_del(src_bs);
		gf_bs_del(dst_bs);
		return GF_OK;
	}

	buffer = (char *)gf_malloc(sizeof(char)*max_size);

	while (gf_bs_available(src_bs)) {
		nal_size = gf_bs_read_int(src_bs, 8*nal_unit_size_field);
		if (nal_size>max_size) {
			buffer = (char*) gf_realloc(buffer, sizeof(char)*nal_size);
			max_size = nal_size;
		}
		if (is_hevc) {
			nal_hdr = gf_bs_read_u16(src_bs);
			nal_type = (nal_hdr&0x7E00) >> 9;
		} else {
			nal_hdr = gf_bs_read_u8(src_bs);
			nal_type = nal_hdr & 0x1F;
		} 

		if (is_hevc) {
			if (ps_bs) {
				gf_bs_transfer(dst_bs, ps_bs);
				gf_bs_del(ps_bs);
				ps_bs = NULL;
			}

#ifndef GPAC_DISABLE_HEVC
			/*we already wrote this stuff*/
			if (nal_type==GF_HEVC_NALU_ACCESS_UNIT) {
				gf_bs_skip_bytes(src_bs, nal_size-2);
				continue;
			}
			switch (nal_type) {
			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_TSA_R:
			case GF_HEVC_NALU_SLICE_STSA_R:
				if (temporal_id < (nal_hdr & 0x7))
					temporal_id = (nal_hdr & 0x7);
				break;
			}
#endif
			
			/*rewrite nal*/
			gf_bs_read_data(src_bs, buffer, nal_size-2);
			if (rewrite_start_codes) 
				gf_bs_write_u32(dst_bs, 1);
			else
				gf_bs_write_int(dst_bs, nal_size, 8*nal_unit_size_field);

			gf_bs_write_u16(dst_bs, nal_hdr);
			gf_bs_write_data(dst_bs, buffer, nal_size-2);

			continue;
		} 
		
		switch(nal_type) {
			case GF_AVC_NALU_ACCESS_UNIT:
				/*we already wrote this stuff*/
				gf_bs_skip_bytes(src_bs, nal_size-1);
				continue;		
			//extractor
			case 31:
				switch (extractor_mode) {
					case 0:
						gf_bs_read_int(src_bs, 24); //3 bytes of NALUHeader in extractor
						ref_track_ID = gf_bs_read_u8(src_bs);
						sample_offset = (s8) gf_bs_read_int(src_bs, 8);
						data_offset = gf_bs_read_u32(src_bs);
						data_length = gf_bs_read_u32(src_bs);

						ref_track_num = gf_isom_get_track_by_id(file, ref_track_ID);
						if (!ref_track_num) {
							e = GF_BAD_PARAM;
							goto exit;
						}
						cur_extract_mode = gf_isom_get_nalu_extract_mode(file, ref_track_num);
						ref_extract_mode = GF_ISOM_NALU_EXTRACT_INSPECT;
						if (rewrite_ps)
							ref_extract_mode |= GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG;
						gf_isom_set_nalu_extract_mode(file, ref_track_num, ref_extract_mode);
						ref_trak = gf_isom_get_track_from_file(file, ref_track_num);
						if (!ref_trak) {
							e =  GF_BAD_PARAM;
							goto exit;
						}
						ref_samp = gf_isom_sample_new();
						if (!ref_samp) {
							e = GF_IO_ERR;
							goto exit;
						}

						//fixeme - we assume that the sampleNumber in the refered track is the same as this sample number
						//are there cases were this wouldn't be the case ?
						if (sample_offset < -sample_offset) 
							sample_offset = 0;
						e = Media_GetSample(ref_trak->Media, sampleNumber + sample_offset, &ref_samp, &di, 0, NULL);
						if (e)
							goto exit;
						if (rewrite_start_codes) {
							gf_bs_write_int(dst_bs, 1, 32);
							if (is_hevc) {
								gf_bs_write_int(dst_bs, 0, 1);
								gf_bs_write_int(dst_bs, GF_HEVC_NALU_ACCESS_UNIT, 6);
								gf_bs_write_int(dst_bs, 0, 9);
								/*pic-type - by default we signal all slice types possible*/
								gf_bs_write_int(dst_bs, 2, 3);
								gf_bs_write_int(dst_bs, 0, 5);
							} else {
								gf_bs_write_int(dst_bs, (ref_samp->data[0] & 0x60) | GF_AVC_NALU_ACCESS_UNIT, 8);
								gf_bs_write_int(dst_bs, 0xF0 , 8); /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;
							}
						}
						ref_bs = gf_bs_new(ref_samp->data, ref_samp->dataLength, GF_BITSTREAM_READ);
						offset = 0;
						while (gf_bs_available(ref_bs)) {
							if (gf_bs_get_position(ref_bs) < data_offset) {
								ref_nalu_size = gf_bs_read_int(ref_bs, 8*nal_unit_size_field);
								offset += ref_nalu_size + nal_unit_size_field;
								if ((offset > data_offset) || (offset >= gf_bs_get_size(ref_bs))) {
									e = GF_BAD_PARAM;
									goto exit;
								}

								e = gf_bs_seek(ref_bs, offset);
								if (e)
									goto exit;
								continue;
							}
							ref_nalu_size = gf_bs_read_int(ref_bs, 8*nal_unit_size_field);
							copy_size = data_length ? data_length : ref_nalu_size;
							assert(copy_size <= ref_nalu_size);
							nal_hdr = gf_bs_read_u8(ref_bs); //rewrite NAL type
							if ((copy_size-1)>max_size) {
								buffer = (char*)gf_realloc(buffer, sizeof(char)*(copy_size-1));
								max_size = copy_size-1;
							}			
							gf_bs_read_data(ref_bs, buffer, copy_size-1);

							if (rewrite_start_codes) 
								gf_bs_write_u32(dst_bs, 1);
							else
								gf_bs_write_int(dst_bs, copy_size, 8*nal_unit_size_field);

							gf_bs_write_u8(dst_bs, nal_hdr);
							gf_bs_write_data(dst_bs, buffer, copy_size-1);
						}

						gf_isom_sample_del(&ref_samp);
						ref_samp = NULL;
						gf_bs_del(ref_bs);
						ref_bs = NULL;
						gf_isom_set_nalu_extract_mode(file, ref_track_num, cur_extract_mode);
						break;
					case 1:
						//skip to end of this NALU
						gf_bs_skip_bytes(src_bs, nal_size-1);
						continue;
					case 2:
						gf_bs_read_data(src_bs, buffer, nal_size-1);
						if (rewrite_start_codes) 
							gf_bs_write_u32(dst_bs, 1);
						else
							gf_bs_write_int(dst_bs, nal_size, 8*nal_unit_size_field);

						gf_bs_write_u8(dst_bs, nal_hdr);
						gf_bs_write_data(dst_bs, buffer, nal_size-1);
						break;
				}
				break;
//			case GF_AVC_NALU_SEI:
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_PIC_PARAM:
			case GF_AVC_NALU_SEQ_PARAM_EXT:
			case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
				// we will rewrite the sps/pps if and only if there is no sps/pps in bistream
				if (ps_bs) {
					gf_bs_del(ps_bs);
					ps_bs = NULL;
				}
			default:
				if (ps_bs) {
					gf_bs_transfer(dst_bs, ps_bs);
					gf_bs_del(ps_bs);
					ps_bs = NULL;
				}
				gf_bs_read_data(src_bs, buffer, nal_size-1);
				if (rewrite_start_codes) 
					gf_bs_write_u32(dst_bs, 1);
				else
					gf_bs_write_int(dst_bs, nal_size, 8*nal_unit_size_field);

				gf_bs_write_u8(dst_bs, nal_hdr);
				gf_bs_write_data(dst_bs, buffer, nal_size-1);
		}
	}

	/*done*/
	gf_free(sample->data);
	sample->data = NULL;
	gf_bs_get_content(dst_bs, &sample->data, &sample->dataLength);

	/*rewrite temporal ID of AU Ddelim NALU (first one)*/
	if (rewrite_start_codes && is_hevc && temporal_id) {
		sample->data[6] = (sample->data[6] & 0xF8) | (temporal_id+1);
	}


exit:
	if (ref_samp) gf_isom_sample_del(&ref_samp);
	if (src_bs) gf_bs_del(src_bs);
	if (ref_bs) gf_bs_del(ref_bs);
	if (dst_bs) gf_bs_del(dst_bs);
	if (buffer) gf_free(buffer);
	return e;
}

GF_HEVCConfig *HEVC_DuplicateConfig(GF_HEVCConfig *cfg)
{
	char *data;
	u32 data_size;
	GF_HEVCConfig *new_cfg;
	GF_BitStream *bs;
	
	if (!cfg) return NULL;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_odf_hevc_cfg_write_bs(cfg, bs);

	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);

	new_cfg = gf_odf_hevc_cfg_read_bs(bs);
	gf_bs_del(bs);
	gf_free(data);
	return new_cfg;
}

static GF_AVCConfig *AVC_DuplicateConfig(GF_AVCConfig *cfg)
{
	u32 i, count;
	GF_AVCConfigSlot *p1, *p2;
	GF_AVCConfig *cfg_new = gf_odf_avc_cfg_new();
	cfg_new->AVCLevelIndication = cfg->AVCLevelIndication;
	cfg_new->AVCProfileIndication = cfg->AVCProfileIndication;
	cfg_new->configurationVersion = cfg->configurationVersion;
	cfg_new->nal_unit_size = cfg->nal_unit_size;
	cfg_new->profile_compatibility = cfg->profile_compatibility;
	cfg_new->complete_representation = cfg->complete_representation;
	cfg_new->chroma_bit_depth = cfg->chroma_bit_depth;
	cfg_new->luma_bit_depth = cfg->luma_bit_depth;
	cfg_new->chroma_format = cfg->chroma_format;

	count = gf_list_count(cfg->sequenceParameterSets);
	for (i=0; i<count; i++) {
		p1 = (GF_AVCConfigSlot*)gf_list_get(cfg->sequenceParameterSets, i);
		p2 = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
		p2->size = p1->size;
		p2->data = (char *)gf_malloc(sizeof(char)*p1->size);
		memcpy(p2->data, p1->data, sizeof(char)*p1->size);
		gf_list_add(cfg_new->sequenceParameterSets, p2);
	}

	count = gf_list_count(cfg->pictureParameterSets);
	for (i=0; i<count; i++) {
		p1 = (GF_AVCConfigSlot*)gf_list_get(cfg->pictureParameterSets, i);
		p2 = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
		p2->size = p1->size;
		p2->data = (char*)gf_malloc(sizeof(char)*p1->size);
		memcpy(p2->data, p1->data, sizeof(char)*p1->size);
		gf_list_add(cfg_new->pictureParameterSets, p2);
	}

	if (cfg->sequenceParameterSetExtensions) {
		cfg_new->sequenceParameterSetExtensions = gf_list_new();
		count = gf_list_count(cfg->sequenceParameterSetExtensions);
		for (i=0; i<count; i++) {
			p1 = (GF_AVCConfigSlot*)gf_list_get(cfg->sequenceParameterSetExtensions, i);
			p2 = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			p2->size = p1->size;
			p2->data = (char*)gf_malloc(sizeof(char)*p1->size);
			memcpy(p2->data, p1->data, sizeof(char)*p1->size);
			gf_list_add(cfg_new->sequenceParameterSetExtensions, p2);
		}
	}
	return cfg_new;	
}


void AVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *avc)
{
	GF_AVCConfig *avcc, *svcc;
	if (avc->emul_esd) gf_odf_desc_del((GF_Descriptor *)avc->emul_esd);
	avc->emul_esd = gf_odf_desc_esd_new(2);
	avc->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	/*AVC OTI is 0x21, AVC parameter set stream OTI (not supported in gpac) is 0x22, SVC OTI is 0x24*/
	/*if we have only SVC stream, set objectTypeIndication to AVC OTI; else set it to AVC OTI*/
	if (avc->svc_config && !avc->avc_config)
		avc->emul_esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_SVC;
	else
		avc->emul_esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_AVC;
	if (avc->bitrate) {
		avc->emul_esd->decoderConfig->bufferSizeDB = avc->bitrate->bufferSizeDB;
		avc->emul_esd->decoderConfig->avgBitrate = avc->bitrate->avgBitrate;
		avc->emul_esd->decoderConfig->maxBitrate = avc->bitrate->maxBitrate;
	}
	if (avc->descr) {
		u32 i=0; 
		GF_Descriptor *desc,*clone;
		i=0;
		while ((desc = (GF_Descriptor *)gf_list_enum(avc->descr->descriptors, &i))) {
			clone = NULL;
			gf_odf_desc_copy(desc, &clone);
			if (gf_odf_desc_add_desc((GF_Descriptor *)avc->emul_esd, clone) != GF_OK) 
				gf_odf_desc_del(clone);
		}
	}
	if (avc->avc_config) {
		avcc = avc->avc_config->config ? AVC_DuplicateConfig(avc->avc_config->config) : NULL;
		/*merge SVC config*/
		if (avc->svc_config) {
			svcc = AVC_DuplicateConfig(avc->svc_config->config);
			while (gf_list_count(svcc->sequenceParameterSets)) {
				GF_AVCConfigSlot *p = (GF_AVCConfigSlot*)gf_list_get(svcc->sequenceParameterSets, 0);
				gf_list_rem(svcc->sequenceParameterSets, 0);
				gf_list_add(avcc->sequenceParameterSets, p);
			}
			while (gf_list_count(svcc->pictureParameterSets)) {
				GF_AVCConfigSlot *p = (GF_AVCConfigSlot*)gf_list_get(svcc->pictureParameterSets, 0);
				gf_list_rem(svcc->pictureParameterSets, 0);
				gf_list_add(avcc->pictureParameterSets, p);
			}
			gf_odf_avc_cfg_del(svcc);
		}
		if (avcc) {
			gf_odf_avc_cfg_write(avcc, &avc->emul_esd->decoderConfig->decoderSpecificInfo->data, &avc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_odf_avc_cfg_del(avcc);
		}
	} else if (avc->svc_config) {
		svcc = AVC_DuplicateConfig(avc->svc_config->config);
		gf_odf_avc_cfg_write(svcc, &avc->emul_esd->decoderConfig->decoderSpecificInfo->data, &avc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_odf_avc_cfg_del(svcc);
	}
}

void HEVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *hevc)
{
	if (hevc->emul_esd) gf_odf_desc_del((GF_Descriptor *)hevc->emul_esd);
	hevc->emul_esd = gf_odf_desc_esd_new(2);
	hevc->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	hevc->emul_esd->decoderConfig->objectTypeIndication = GPAC_OTI_VIDEO_HEVC;
	if (hevc->bitrate) {
		hevc->emul_esd->decoderConfig->bufferSizeDB = hevc->bitrate->bufferSizeDB;
		hevc->emul_esd->decoderConfig->avgBitrate = hevc->bitrate->avgBitrate;
		hevc->emul_esd->decoderConfig->maxBitrate = hevc->bitrate->maxBitrate;
	}
	if (hevc->descr) {
		u32 i=0; 
		GF_Descriptor *desc,*clone;
		i=0;
		while ((desc = (GF_Descriptor *)gf_list_enum(hevc->descr->descriptors, &i))) {
			clone = NULL;
			gf_odf_desc_copy(desc, &clone);
			if (gf_odf_desc_add_desc((GF_Descriptor *)hevc->emul_esd, clone) != GF_OK) 
				gf_odf_desc_del(clone);
		}
	}
	if (hevc->hevc_config && hevc->hevc_config->config) {
		gf_odf_hevc_cfg_write(hevc->hevc_config->config, &hevc->emul_esd->decoderConfig->decoderSpecificInfo->data, &hevc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
	}
}

GF_Err AVC_HEVC_UpdateESD(GF_MPEGVisualSampleEntryBox *avc, GF_ESD *esd)
{
	if (!avc->bitrate) avc->bitrate = (GF_MPEG4BitRateBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_BTRT);
	if (avc->descr) gf_isom_box_del((GF_Box *) avc->descr);
	avc->descr = NULL;
	avc->bitrate->avgBitrate = esd->decoderConfig->avgBitrate;
	avc->bitrate->maxBitrate = esd->decoderConfig->maxBitrate;
	avc->bitrate->bufferSizeDB = esd->decoderConfig->bufferSizeDB;

	if (gf_list_count(esd->IPIDataSet)
		|| gf_list_count(esd->IPMPDescriptorPointers)
		|| esd->langDesc
		|| gf_list_count(esd->extensionDescriptors)
		|| esd->ipiPtr || esd->qos || esd->RegDescriptor) {

		avc->descr = (GF_MPEG4ExtensionDescriptorsBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_M4DS);
		if (esd->RegDescriptor) { gf_list_add(avc->descr->descriptors, esd->RegDescriptor); esd->RegDescriptor = NULL; }
		if (esd->qos) { gf_list_add(avc->descr->descriptors, esd->qos); esd->qos = NULL; }
		if (esd->ipiPtr) { gf_list_add(avc->descr->descriptors, esd->ipiPtr); esd->ipiPtr= NULL; }

		while (gf_list_count(esd->IPIDataSet)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPIDataSet, 0);
			gf_list_rem(esd->IPIDataSet, 0);
			gf_list_add(avc->descr->descriptors, desc);
		}
		while (gf_list_count(esd->IPMPDescriptorPointers)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPMPDescriptorPointers, 0);
			gf_list_rem(esd->IPMPDescriptorPointers, 0);
			gf_list_add(avc->descr->descriptors, desc);
		}
		if (esd->langDesc) {
			gf_list_add(avc->descr->descriptors, esd->langDesc);
			esd->langDesc = NULL;
		}
		while (gf_list_count(esd->extensionDescriptors)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->extensionDescriptors, 0);
			gf_list_rem(esd->extensionDescriptors, 0);
			gf_list_add(avc->descr->descriptors, desc);
		}
	}

	/*update GF_AVCConfig*/
	if (!avc->svc_config) {
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_HEVC) {
			if (!avc->hevc_config) avc->hevc_config = (GF_HEVCConfigurationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_HVCC);
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				if (avc->hevc_config->config) gf_odf_hevc_cfg_del(avc->hevc_config->config);
				avc->hevc_config->config = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			}
		} else {
			if (!avc->avc_config) avc->avc_config = (GF_AVCConfigurationBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				if (avc->avc_config->config) gf_odf_avc_cfg_del(avc->avc_config->config);
				avc->avc_config->config = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			}
		}

	}
	gf_odf_desc_del((GF_Descriptor *)esd);
	if (avc->hevc_config) {
		HEVC_RewriteESDescriptor(avc);
	} else {
		AVC_RewriteESDescriptor(avc);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_EXPORT
GF_Err gf_isom_avc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_AVC1);
	if (!entry) return GF_OUT_OF_MEM;
	entry->avc_config = (GF_AVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
	entry->avc_config->config = AVC_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	AVC_RewriteESDescriptor(entry);
	return e;
}

static GF_Err gf_isom_avc_config_update_ex(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, u32 op_type)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	switch (op_type) {
	/*AVCC replacement*/
	case 0:
		if (!cfg) return GF_BAD_PARAM;
		if (!entry->avc_config) entry->avc_config = (GF_AVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_AVCC);
		if (entry->avc_config->config) gf_odf_avc_cfg_del(entry->avc_config->config);
		entry->avc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_AVC1;
		break;
	/*SVCC replacement*/
	case 1:
		if (!cfg) return GF_BAD_PARAM;
		if (!entry->svc_config) entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
		if (entry->svc_config->config) gf_odf_avc_cfg_del(entry->svc_config->config);
		entry->svc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_AVC1;
		break;
	/*SVCC replacement and AVC removal*/
	case 2:
		if (!cfg) return GF_BAD_PARAM;
		if (entry->avc_config) {
			gf_isom_box_del((GF_Box*)entry->avc_config);
			entry->avc_config = NULL;
		}
		if (!entry->svc_config) entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
		if (entry->svc_config->config) gf_odf_avc_cfg_del(entry->svc_config->config);
		entry->svc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_SVC1;
		break;
	/*AVCC removal and switch to avc3*/
	case 3:
		if (!entry->avc_config || !entry->avc_config->config) 
			return GF_BAD_PARAM;

		if (entry->svc_config) {
			gf_isom_box_del((GF_Box*)entry->svc_config);
			entry->svc_config = NULL;
		}

		while (gf_list_count(entry->avc_config->config->sequenceParameterSets)) {
			GF_AVCConfigSlot *sl = gf_list_get(entry->avc_config->config->sequenceParameterSets, 0);
			gf_list_rem(entry->avc_config->config->sequenceParameterSets, 0);
			if (sl->data) gf_free(sl->data);
			gf_free(sl);
		}

		while (gf_list_count(entry->avc_config->config->pictureParameterSets)) {
			GF_AVCConfigSlot *sl = gf_list_get(entry->avc_config->config->pictureParameterSets, 0);
			gf_list_rem(entry->avc_config->config->pictureParameterSets, 0);
			if (sl->data) gf_free(sl->data);
			gf_free(sl);
		}

		if (entry->type == GF_ISOM_BOX_TYPE_AVC1)
			entry->type = GF_ISOM_BOX_TYPE_AVC3;
		else if (entry->type == GF_ISOM_BOX_TYPE_AVC2)
			entry->type = GF_ISOM_BOX_TYPE_AVC4;
		break;
	}
	AVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_avc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, NULL, 3);
}

GF_EXPORT
GF_Err gf_isom_avc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, 0);
}

GF_Err gf_isom_svc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, Bool is_add)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, is_add ? 1 : 2);
}

GF_Err gf_isom_svc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (entry->svc_config) {
			gf_isom_box_del((GF_Box*)entry->svc_config);
			entry->svc_config = NULL;
		}
	AVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_ipod_compatible(GF_ISOFile *the_file, u32 trackNumber)
{
	static const u8 ipod_ext[][16] = { { 0x6B, 0x68, 0x40, 0xF2, 0x5F, 0x24, 0x4F, 0xC5, 0xBA, 0x39, 0xA5, 0x1B, 0xCF, 0x03, 0x23, 0xF3} };
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, 0);
	if (!entry) return GF_OK;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
		break;
	default:
		return GF_OK;
	}

	if (!entry->ipod_ext) entry->ipod_ext = (GF_UnknownUUIDBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
	memcpy(entry->ipod_ext->uuid, ipod_ext, sizeof(u8)*16);
	entry->ipod_ext->dataSize = 0;
	return GF_OK;
}

GF_Err gf_isom_svc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SVC1);
	if (!entry) return GF_OUT_OF_MEM;
	entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_SVCC);
	entry->svc_config->config = AVC_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	AVC_RewriteESDescriptor(entry);
	return e;
}

GF_Err gf_isom_hevc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_HEVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !cfg) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HVC1);
	if (!entry) return GF_OUT_OF_MEM;
	entry->hevc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_HVCC);
	entry->hevc_config->config = HEVC_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	HEVC_RewriteESDescriptor(entry);
	return e;
}

static
GF_Err gf_isom_hevc_config_update_ex(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, u32 operand_type)
{
	u32 i, array_incomplete;
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!entry->hevc_config) entry->hevc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_HVCC);

	if (cfg) {
		if (entry->hevc_config->config) gf_odf_hevc_cfg_del(entry->hevc_config->config);
		entry->hevc_config->config = HEVC_DuplicateConfig(cfg);
	} else {
		operand_type=1;
	}
	array_incomplete = 0;
	for (i=0; i<gf_list_count(entry->hevc_config->config->param_array); i++) {
		GF_HEVCParamArray *ar = gf_list_get(entry->hevc_config->config->param_array, i);
	
		/*we want to force hev1*/
		if (operand_type==1) ar->array_completeness = 0;

		if (!ar->array_completeness) {
			array_incomplete = 1;
		}
	}

	entry->type = array_incomplete ? GF_ISOM_BOX_TYPE_HEV1 : GF_ISOM_BOX_TYPE_HVC1;

	HEVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_hevc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, 0);;
}

GF_EXPORT
GF_Err gf_isom_hevc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, NULL, 1);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_AVCConfig *gf_isom_avc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_avc_svc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_AVCTYPE_NONE)
		return NULL;

	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return NULL;

	if (!entry->avc_config) return NULL;
	return AVC_DuplicateConfig(entry->avc_config->config);
}

GF_EXPORT
GF_HEVCConfig *gf_isom_hevc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;

	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
		break;
	default:
		return NULL;
	}
	if (!entry->hevc_config) return NULL;
	return HEVC_DuplicateConfig(entry->hevc_config->config);
}

GF_EXPORT
GF_AVCConfig *gf_isom_svc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_avc_svc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_AVCTYPE_NONE)
		return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
	if (!entry->svc_config) return NULL;
	return AVC_DuplicateConfig(entry->svc_config->config);
}


GF_EXPORT
u32 gf_isom_get_avc_svc_type(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	u32 type;
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_ISOM_AVCTYPE_NONE;
	if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_VISUAL) return GF_ISOM_AVCTYPE_NONE;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, DescriptionIndex-1);
	type = entry->type;
	
	if (type == GF_ISOM_BOX_TYPE_ENCV) {
		GF_ProtectionInfoBox *sinf = (GF_ProtectionInfoBox *) gf_list_get(entry->protections, 0);
		if (sinf && sinf->original_format) type = sinf->original_format->data_format;
	}

	switch (type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
		break;
	default:
		return GF_ISOM_AVCTYPE_NONE;
	}
	if (entry->avc_config && !entry->svc_config) return GF_ISOM_AVCTYPE_AVC_ONLY;
	if (entry->avc_config && entry->svc_config) return GF_ISOM_AVCTYPE_AVC_SVC;
	if (!entry->avc_config && entry->svc_config) return GF_ISOM_AVCTYPE_SVC_ONLY;
	return GF_ISOM_AVCTYPE_NONE;
}


void btrt_del(GF_Box *s)
{
	GF_MPEG4BitRateBox *ptr = (GF_MPEG4BitRateBox *)s;
	if (ptr) gf_free(ptr);
}
GF_Err btrt_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_MPEG4BitRateBox *ptr = (GF_MPEG4BitRateBox *)s;
	ptr->bufferSizeDB = gf_bs_read_u32(bs);
	ptr->maxBitrate = gf_bs_read_u32(bs);
	ptr->avgBitrate = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *btrt_New()
{
	GF_MPEG4BitRateBox *tmp = (GF_MPEG4BitRateBox *) gf_malloc(sizeof(GF_MPEG4BitRateBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MPEG4BitRateBox));
	tmp->type = GF_ISOM_BOX_TYPE_BTRT;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err btrt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_MPEG4BitRateBox *ptr = (GF_MPEG4BitRateBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->bufferSizeDB);
	gf_bs_write_u32(bs, ptr->maxBitrate);
	gf_bs_write_u32(bs, ptr->avgBitrate);
	return GF_OK;
}
GF_Err btrt_Size(GF_Box *s)
{
	GF_Err e;
	GF_MPEG4BitRateBox *ptr = (GF_MPEG4BitRateBox *)s;
	e = gf_isom_box_get_size(s);
	ptr->size += 12;
	return e;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void m4ds_del(GF_Box *s)
{
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *)s;
	gf_odf_desc_list_del(ptr->descriptors);
	gf_list_del(ptr->descriptors);
	gf_free(ptr);
}
GF_Err m4ds_Read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	char *enc_od;
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *)s;
	u32 od_size = (u32) ptr->size;
	if (!od_size) return GF_OK;
	enc_od = (char *)gf_malloc(sizeof(char) * od_size);
	gf_bs_read_data(bs, enc_od, od_size);
	e = gf_odf_desc_list_read((char *)enc_od, od_size, ptr->descriptors);
	gf_free(enc_od);
	return e;
}
GF_Box *m4ds_New()
{
	GF_MPEG4ExtensionDescriptorsBox *tmp = (GF_MPEG4ExtensionDescriptorsBox *) gf_malloc(sizeof(GF_MPEG4ExtensionDescriptorsBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MPEG4ExtensionDescriptorsBox));
	tmp->type = GF_ISOM_BOX_TYPE_M4DS;
	tmp->descriptors = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err m4ds_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	char *enc_ods;
	u32 enc_od_size;
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	enc_ods = NULL;
	enc_od_size = 0;
	e = gf_odf_desc_list_write(ptr->descriptors, &enc_ods, &enc_od_size);
	if (e) return e;
	if (enc_od_size) {
		gf_bs_write_data(bs, enc_ods, enc_od_size);
		gf_free(enc_ods);
	}
	return GF_OK;
}
GF_Err m4ds_Size(GF_Box *s)
{
	GF_Err e;
	u32 descSize;
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *)s;
	e = gf_isom_box_get_size(s);
	if (!e) e = gf_odf_desc_list_size(ptr->descriptors, &descSize);
	ptr->size += descSize;
	return e;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void avcc_del(GF_Box *s)
{
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *)s;
	if (ptr->config) gf_odf_avc_cfg_del(ptr->config);
	gf_free(ptr);
}

GF_Err avcc_Read(GF_Box *s, GF_BitStream *bs)
{
	u32 i, count;
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *)s;

	if (ptr->config) gf_odf_avc_cfg_del(ptr->config);
	ptr->config = gf_odf_avc_cfg_new();
	ptr->config->configurationVersion = gf_bs_read_u8(bs);
	ptr->config->AVCProfileIndication = gf_bs_read_u8(bs);
	ptr->config->profile_compatibility = gf_bs_read_u8(bs);
	ptr->config->AVCLevelIndication = gf_bs_read_u8(bs);
	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		gf_bs_read_int(bs, 6);
	} else {
		ptr->config->complete_representation = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 5);
	}
	ptr->config->nal_unit_size = 1 + gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 3);
	count = gf_bs_read_int(bs, 5);

	ptr->size -= 7; //including 2nd count

	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *) gf_malloc(sizeof(GF_AVCConfigSlot));
		sl->size = gf_bs_read_u16(bs);
		sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
		gf_bs_read_data(bs, sl->data, sl->size);
		gf_list_add(ptr->config->sequenceParameterSets, sl);
		ptr->size -= 2+sl->size;
	}

	count = gf_bs_read_u8(bs);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
		sl->size = gf_bs_read_u16(bs);
		sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
		gf_bs_read_data(bs, sl->data, sl->size);
		gf_list_add(ptr->config->pictureParameterSets, sl);
		ptr->size -= 2+sl->size;
	}

	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {

		switch (ptr->config->AVCProfileIndication) {
		case 100:
		case 110:
		case 122:
		case 144:
			if (!ptr->size) {
#ifndef GPAC_DISABLE_AV_PARSERS
				AVCState avc;
				s32 idx, vui_flag_pos;
				GF_AVCConfigSlot *sl = gf_list_get(ptr->config->sequenceParameterSets, 0);
				idx = gf_media_avc_read_sps(sl->data+1, sl->size-1, &avc, 0, (u32 *) &vui_flag_pos);
				if (idx>=0) {
					ptr->config->chroma_format = avc.sps[idx].chroma_format;
					ptr->config->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
					ptr->config->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
				}
#else
				/*set default values ...*/
				ptr->config->chroma_format = 1;
				ptr->config->luma_bit_depth = 8;
				ptr->config->chroma_bit_depth = 8;
#endif
				return GF_OK;
			}
			gf_bs_read_int(bs, 6);
			ptr->config->chroma_format = gf_bs_read_int(bs, 2);
			gf_bs_read_int(bs, 5);
			ptr->config->luma_bit_depth = 8 + gf_bs_read_int(bs, 3);
			gf_bs_read_int(bs, 5);
			ptr->config->chroma_bit_depth = 8 + gf_bs_read_int(bs, 3);

			count = gf_bs_read_int(bs, 8);
			ptr->size -= 4;
			if (count*2 > ptr->size) {
				//ffmpeg just ignores this part while allocating bytes (filled with garbage?)
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("AVCC: invalid numOfSequenceParameterSetExt value. Skipping.\n"));
				return GF_OK;
			}
			if (count) {
				ptr->config->sequenceParameterSetExtensions = gf_list_new();
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
					sl->size = gf_bs_read_u16(bs);
					sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
					gf_bs_read_data(bs, sl->data, sl->size);
					gf_list_add(ptr->config->sequenceParameterSetExtensions, sl);
					ptr->size -= sl->size + 2;
				}
			}
			break;
		}
	}
	return GF_OK;
}

GF_Box *avcc_New()
{
	GF_AVCConfigurationBox *tmp = (GF_AVCConfigurationBox *) gf_malloc(sizeof(GF_AVCConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_AVCConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_AVCC;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err avcc_Write(GF_Box *s, GF_BitStream *bs)
{
	u32 i, count;
	GF_Err e;
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *) s;
	if (!s) return GF_BAD_PARAM;
	if (!ptr->config) return GF_OK;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	gf_bs_write_u8(bs, ptr->config->configurationVersion);
	gf_bs_write_u8(bs, ptr->config->AVCProfileIndication);
	gf_bs_write_u8(bs, ptr->config->profile_compatibility);
	gf_bs_write_u8(bs, ptr->config->AVCLevelIndication);
	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		gf_bs_write_int(bs, 0x3F, 6);
	} else {
		gf_bs_write_int(bs, ptr->config->complete_representation, 1);
		gf_bs_write_int(bs, 0x1F, 5);
	}
	gf_bs_write_int(bs, ptr->config->nal_unit_size - 1, 2);
	gf_bs_write_int(bs, 0x7, 3);
	count = gf_list_count(ptr->config->sequenceParameterSets);
	gf_bs_write_int(bs, count, 5);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *) gf_list_get(ptr->config->sequenceParameterSets, i);
		gf_bs_write_u16(bs, sl->size);
		gf_bs_write_data(bs, sl->data, sl->size);
	}

	count = gf_list_count(ptr->config->pictureParameterSets);
	gf_bs_write_u8(bs, count);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *) gf_list_get(ptr->config->pictureParameterSets, i);
		gf_bs_write_u16(bs, sl->size);
		gf_bs_write_data(bs, sl->data, sl->size);
	}


	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		switch (ptr->config->AVCProfileIndication) {
		case 100:
		case 110:
		case 122:
		case 144:
			gf_bs_write_int(bs, 0xFF, 6);
			gf_bs_write_int(bs, ptr->config->chroma_format, 2);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, ptr->config->luma_bit_depth - 8, 3);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, ptr->config->chroma_bit_depth - 8, 3);

			count = ptr->config->sequenceParameterSetExtensions ? gf_list_count(ptr->config->sequenceParameterSetExtensions) : 0;
			gf_bs_write_u8(bs, count);
			for (i=0; i<count; i++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *) gf_list_get(ptr->config->sequenceParameterSetExtensions, i);
				gf_bs_write_u16(bs, sl->size);
				gf_bs_write_data(bs, sl->data, sl->size);
			}
			break;
		}
	}
	return GF_OK;
}
GF_Err avcc_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, count;
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (!ptr->config) {
		ptr->size = 0;
		return e;
	}
	ptr->size += 7;
	count = gf_list_count(ptr->config->sequenceParameterSets);
	for (i=0; i<count; i++) 
		ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->sequenceParameterSets, i))->size;

	count = gf_list_count(ptr->config->pictureParameterSets);
	for (i=0; i<count; i++) 
		ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->pictureParameterSets, i))->size;

	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		switch (ptr->config->AVCProfileIndication) {
		case 100:
		case 110:
		case 122:
		case 144:
			ptr->size += 4;
			count = ptr->config->sequenceParameterSetExtensions ?gf_list_count(ptr->config->sequenceParameterSetExtensions) : 0;
			for (i=0; i<count; i++)	
				ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->sequenceParameterSetExtensions, i))->size;
			break;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void hvcc_del(GF_Box *s)
{
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox*)s;
	if (ptr->config) gf_odf_hevc_cfg_del(ptr->config);
	gf_free(ptr);
}

GF_Err hvcc_Read(GF_Box *s, GF_BitStream *bs)
{
	u64 pos;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *)s;

	if (ptr->config) gf_odf_hevc_cfg_del(ptr->config);
	
	pos = gf_bs_get_position(bs);
	ptr->config = gf_odf_hevc_cfg_read_bs(bs);
	pos = gf_bs_get_position(bs) - pos ;
	if (pos < ptr->size)
		ptr->size -= (u32) pos;

	return GF_OK;
}
GF_Box *hvcc_New()
{
	GF_HEVCConfigurationBox *tmp = (GF_HEVCConfigurationBox *) gf_malloc(sizeof(GF_HEVCConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_HEVCConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_HVCC;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hvcc_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *) s;
	if (!s) return GF_BAD_PARAM;
	if (!ptr->config) return GF_OK;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return gf_odf_hevc_cfg_write_bs(ptr->config, bs);
}
GF_Err hvcc_Size(GF_Box *s)
{
	GF_Err e;
	u32 i, count, j, subcount;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *)s;
	e = gf_isom_box_get_size(s);
	if (e) return e;
	if (!ptr->config) {
		ptr->size = 0;
		return e;
	}
	ptr->size += 23;

	count = gf_list_count(ptr->config->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = gf_list_get(ptr->config->param_array, i);
		ptr->size += 3;
		subcount = gf_list_count(ar->nalus);
		for (j=0; j<subcount; j++) {
			ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ar->nalus, j))->size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#endif /*GPAC_DISABLE_ISOM*/
