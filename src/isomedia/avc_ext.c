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
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/internal/media_dev.h>
#endif

#ifndef GPAC_DISABLE_ISOM


Bool gf_isom_is_nalu_based_entry(GF_MediaBox *mdia, GF_SampleEntryBox *_entry)
{
	GF_MPEGVisualSampleEntryBox *entry;
	if (!gf_isom_is_video_subtype(mdia->handler->handlerType))
		return GF_FALSE;
	if (!_entry) return GF_FALSE;
	entry = (GF_MPEGVisualSampleEntryBox*)_entry;

	switch (_entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_SVC2:
	case GF_ISOM_BOX_TYPE_MVC1:
	case GF_ISOM_BOX_TYPE_MVC2:
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_LHV1:
	case GF_ISOM_BOX_TYPE_LHE1:
	case GF_ISOM_BOX_TYPE_MHV1:
	case GF_ISOM_BOX_TYPE_MHC1:
	case GF_ISOM_BOX_TYPE_HVT1:
	case GF_ISOM_BOX_TYPE_LHT1:
		return GF_TRUE;
	case GF_ISOM_BOX_TYPE_GNRV:
	case GF_ISOM_BOX_TYPE_GNRA:
	case GF_ISOM_BOX_TYPE_GNRM:
		return GF_FALSE;
	default:
		break;
	}

	if (entry->avc_config || entry->svc_config || entry->mvc_config || entry->hevc_config || entry->lhvc_config) {
		GF_ProtectionSchemeInfoBox *schi = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		if (!schi || !schi->scheme_type) return GF_TRUE;
		switch (schi->scheme_type->scheme_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
			return GF_TRUE;
		default:
			break;
		}
	}
	return GF_FALSE;
}


static void rewrite_nalus_list(GF_List *nalus, GF_BitStream *bs, Bool rewrite_start_codes, u32 nal_unit_size_field)
{
	u32 i, count = gf_list_count(nalus);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(nalus, i);
		if (rewrite_start_codes) gf_bs_write_u32(bs, 1);
		else gf_bs_write_int(bs, sl->size, 8*nal_unit_size_field);
		gf_bs_write_data(bs, sl->data, sl->size);
	}
}


static GF_Err process_extractor(GF_ISOFile *file, GF_MediaBox *mdia, u32 sampleNumber, u64 sampleDTS, u32 nal_size, u16 nal_hdr, u32 nal_unit_size_field, Bool is_hevc, Bool rewrite_ps, Bool rewrite_start_codes, u32 extractor_mode)
{
	GF_Err e;
	u32 di, ref_track_index, ref_track_num, data_offset, data_length, cur_extract_mode, ref_extract_mode, ref_nalu_size, nb_bytes_nalh;
	GF_TrackReferenceTypeBox *dpnd;
	GF_TrackBox *ref_trak;
	s8 sample_offset;
	u32 last_byte, ref_sample_num, prev_ref_sample_num;
	Bool header_written = GF_FALSE;
	nb_bytes_nalh = is_hevc ? 2 : 1;

	switch (extractor_mode) {
	case 0:
		last_byte = (u32) gf_bs_get_position(mdia->nalu_parser) + nal_size - (is_hevc ? 2 : 1);
		if (!is_hevc) gf_bs_read_int(mdia->nalu_parser, 24); //1 byte for HEVC , 3 bytes for AVC of NALUHeader in extractor
		while (gf_bs_get_position(mdia->nalu_parser) < last_byte) {
			u32 xmode = 0;
			//hevc extractors use constructors
			if (is_hevc) xmode = gf_bs_read_u8(mdia->nalu_parser);
			if (xmode) {
				u8 done=0, len = gf_bs_read_u8(mdia->nalu_parser);
				while (done<len) {
					u8 c = gf_bs_read_u8(mdia->nalu_parser);
					done++;
					if (header_written) {
						gf_bs_write_u8(mdia->nalu_out_bs, c);
					} else if (done==nal_unit_size_field) {
						if (rewrite_start_codes) {
							gf_bs_write_int(mdia->nalu_out_bs, 1, 32);
						} else {
							gf_bs_write_u8(mdia->nalu_out_bs, c);
						}
						header_written = GF_TRUE;
					} else if (!rewrite_start_codes) {
						gf_bs_write_u8(mdia->nalu_out_bs, c);
					}
				}
				continue;
			}

			ref_track_index = gf_bs_read_u8(mdia->nalu_parser);
			sample_offset = (s8) gf_bs_read_int(mdia->nalu_parser, 8);
			data_offset = gf_bs_read_int(mdia->nalu_parser, nal_unit_size_field*8);
			data_length = gf_bs_read_int(mdia->nalu_parser, nal_unit_size_field*8);

			Track_FindRef(mdia->mediaTrack, GF_ISOM_REF_SCAL, &dpnd);
			ref_track_num = 0;
			if (dpnd && ref_track_index && (ref_track_index<=dpnd->trackIDCount))
				ref_track_num = gf_isom_get_track_by_id(file, dpnd->trackIDs[ref_track_index-1]);

			if (!ref_track_num) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("ISOBMF: Extractor target track is not present in file - skipping.\n"));
				return GF_OK;
			}

			cur_extract_mode = gf_isom_get_nalu_extract_mode(file, ref_track_num);

			//we must be in inspect mode only otherwise the reference sample will not be the one stored on file (change in start codes, PS inserted or other NALUs inserted)
			//and this will corrupt extraction (wrong data offsets)
			ref_extract_mode = GF_ISOM_NALU_EXTRACT_INSPECT;
			gf_isom_set_nalu_extract_mode(file, ref_track_num, ref_extract_mode);

			ref_trak = gf_isom_get_track_from_file(file, ref_track_num);
			if (!ref_trak) return GF_ISOM_INVALID_FILE;

			if (!mdia->extracted_samp) {
				mdia->extracted_samp = gf_isom_sample_new();
				if (!mdia->extracted_samp) return GF_IO_ERR;
			}
			if (!mdia->extracted_bs) {
				mdia->extracted_bs = gf_bs_new("a", 1, GF_BITSTREAM_READ);
				if (!mdia->extracted_bs) return GF_IO_ERR;
			}

			e = stbl_findEntryForTime(ref_trak->Media->information->sampleTable, sampleDTS, 0, &ref_sample_num, &prev_ref_sample_num);
			if (e) return e;
			if (!ref_sample_num) ref_sample_num = prev_ref_sample_num;
			if (!ref_sample_num) return GF_ISOM_INVALID_FILE;
			if ((sample_offset<0) && (ref_sample_num > (u32) -sample_offset)) return GF_ISOM_INVALID_FILE;
			ref_sample_num = (u32) ( (s32) ref_sample_num + sample_offset);

			e = Media_GetSample(ref_trak->Media, ref_sample_num, &mdia->extracted_samp, &di, GF_FALSE, NULL);
			if (e) return e;
			if (!mdia->extracted_samp->alloc_size)
				mdia->extracted_samp->alloc_size = mdia->extracted_samp->dataLength;
#if 0
			if (!header_written && rewrite_start_codes) {
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
#endif
			gf_bs_reassign_buffer(mdia->extracted_bs, mdia->extracted_samp->data + data_offset, mdia->extracted_samp->dataLength - data_offset);

			if (mdia->extracted_samp->dataLength - data_offset >= data_length) {

				while (data_length && gf_bs_available(mdia->extracted_bs)) {
					if (!header_written) {
						ref_nalu_size = gf_bs_read_int(mdia->extracted_bs, 8*nal_unit_size_field);

						if (!data_length)
							data_length = ref_nalu_size + nal_unit_size_field;

						assert(data_length>nal_unit_size_field);
						data_length -= nal_unit_size_field;
						if (data_length > gf_bs_available(mdia->extracted_bs)) {
							data_length = (u32)gf_bs_available(mdia->extracted_bs);
						}
					} else {
						ref_nalu_size = data_length;
					}

					if (ref_nalu_size > mdia->tmp_nal_copy_buffer_alloc) {
						mdia->tmp_nal_copy_buffer_alloc = ref_nalu_size;
						mdia->tmp_nal_copy_buffer = (char*) gf_realloc(mdia->tmp_nal_copy_buffer, sizeof(char) * ref_nalu_size );
					}
					gf_bs_read_data(mdia->extracted_bs, mdia->tmp_nal_copy_buffer, ref_nalu_size);

					if (!header_written) {
						if (rewrite_start_codes)
							gf_bs_write_u32(mdia->nalu_out_bs, 1);
						else
							gf_bs_write_int(mdia->nalu_out_bs, ref_nalu_size, 8*nal_unit_size_field);
					}
					assert(data_length >= ref_nalu_size);
					gf_bs_write_data(mdia->nalu_out_bs, mdia->tmp_nal_copy_buffer, ref_nalu_size);
					data_length -= ref_nalu_size;

					header_written = GF_FALSE;

				}
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("ISOBMF: Extractor size is larger than refered sample size - skipping.\n"));
			}
			gf_isom_set_nalu_extract_mode(file, ref_track_num, cur_extract_mode);

			if (!is_hevc) break;
		}
		break;
	case 1:
		//skip to end of this NALU
		gf_bs_skip_bytes(mdia->nalu_parser, nal_size - nb_bytes_nalh);
		break;
	case 2:
		if (nal_size - nb_bytes_nalh > mdia->tmp_nal_copy_buffer_alloc) {
			mdia->tmp_nal_copy_buffer_alloc = nal_size - nb_bytes_nalh;
			mdia->tmp_nal_copy_buffer = (char*) gf_realloc(mdia->tmp_nal_copy_buffer, sizeof(char) * (nal_size - nb_bytes_nalh) );
		}
		gf_bs_read_data(mdia->nalu_parser, mdia->tmp_nal_copy_buffer, nal_size - nb_bytes_nalh);
		if (rewrite_start_codes)
			gf_bs_write_u32(mdia->nalu_out_bs, 1);
		else
			gf_bs_write_int(mdia->nalu_out_bs, nal_size, 8*nal_unit_size_field);

		gf_bs_write_u8(mdia->nalu_out_bs, nal_hdr);
		gf_bs_write_data(mdia->nalu_out_bs, mdia->tmp_nal_copy_buffer, nal_size - nb_bytes_nalh);
		break;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_HEVC
/* returns the SAP type as defined in the 14496-12 specification */
static SAPType sap_type_from_nal_type(u8 nal_type) {
	switch (nal_type) {
	case GF_HEVC_NALU_SLICE_CRA:
		return SAP_TYPE_3;
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
		return SAP_TYPE_1;
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
		return SAP_TYPE_2;
	default:
		return RAP_NO;
	}
}
#endif

static SAPType is_sample_idr(GF_MediaBox *mdia, GF_ISOSample *sample, GF_MPEGVisualSampleEntryBox *entry)
{
	Bool is_hevc = GF_FALSE;
	u32 nalu_size_field = 0;
	if (entry->avc_config && entry->avc_config->config) nalu_size_field = entry->avc_config->config->nal_unit_size;
	else if (entry->svc_config && entry->svc_config->config) nalu_size_field = entry->svc_config->config->nal_unit_size;
	else if (entry->mvc_config && entry->mvc_config->config) nalu_size_field = entry->mvc_config->config->nal_unit_size;
	else if (entry->hevc_config && entry->hevc_config->config) {
		nalu_size_field = entry->hevc_config->config->nal_unit_size;
		is_hevc = GF_TRUE;
	}
	else if (entry->lhvc_config && entry->lhvc_config->config) {
		nalu_size_field = entry->lhvc_config->config->nal_unit_size;
		is_hevc = GF_TRUE;
	}
	if (!nalu_size_field) return RAP_NO;

	if (!mdia->nalu_parser)
		mdia->nalu_parser = gf_bs_new(sample->data, sample->dataLength, GF_BITSTREAM_READ);
	else
		gf_bs_reassign_buffer(mdia->nalu_parser, sample->data, sample->dataLength);

	if (!mdia->nalu_parser) return RAP_NO;

	while (gf_bs_available(mdia->nalu_parser)) {
		u8 nal_type;
		u32 size = gf_bs_read_int(mdia->nalu_parser, 8*nalu_size_field);

		if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
			u16 nal_hdr = gf_bs_read_u16(mdia->nalu_parser);
			nal_type = (nal_hdr&0x7E00) >> 9;

			switch (nal_type) {
			case GF_HEVC_NALU_SLICE_CRA:
				return SAP_TYPE_3;
			case GF_HEVC_NALU_SLICE_IDR_N_LP:
			case GF_HEVC_NALU_SLICE_BLA_N_LP:
				return SAP_TYPE_1;
			case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			case GF_HEVC_NALU_SLICE_BLA_W_DLP:
			case GF_HEVC_NALU_SLICE_BLA_W_LP:
				return SAP_TYPE_2;
			case GF_HEVC_NALU_ACCESS_UNIT:
			case GF_HEVC_NALU_FILLER_DATA:
			case GF_HEVC_NALU_SEI_PREFIX:
			case GF_HEVC_NALU_VID_PARAM:
			case GF_HEVC_NALU_SEQ_PARAM:
			case GF_HEVC_NALU_PIC_PARAM:
				break;
			default:
				return RAP_NO;
			}
			gf_bs_skip_bytes(mdia->nalu_parser, size - 2);
#endif
		} else {
			u8 nal_hdr = gf_bs_read_u8(mdia->nalu_parser);
			nal_type = nal_hdr & 0x1F;

			switch (nal_type) {
			/*			case GF_AVC_NALU_SEQ_PARAM:
						case GF_AVC_NALU_PIC_PARAM:
						case GF_AVC_NALU_SEQ_PARAM_EXT:
						case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		*/
			case GF_AVC_NALU_IDR_SLICE:
				return SAP_TYPE_1;
			case GF_AVC_NALU_ACCESS_UNIT:
			case GF_AVC_NALU_FILLER_DATA:
			case GF_AVC_NALU_SEI:
				break;
			default:
				return RAP_NO;
			}
			gf_bs_skip_bytes(mdia->nalu_parser, size - 1);
		}
	}
	return RAP_NO;
}

static void nalu_merge_ps(GF_BitStream *ps_bs, Bool rewrite_start_codes, u32 nal_unit_size_field, GF_MPEGVisualSampleEntryBox *entry, Bool is_hevc, Bool *has_vps)
{
	u32 i, count;
	if (is_hevc) {
		if (entry->hevc_config) {
			count = gf_list_count(entry->hevc_config->config->param_array);
			for (i=0; i<count; i++) {
				GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(entry->hevc_config->config->param_array, i);
				if (ar->type == GF_HEVC_NALU_VID_PARAM) {
					if (! *has_vps)  *has_vps = GF_TRUE;
					else continue;
				}
				rewrite_nalus_list(ar->nalus, ps_bs, rewrite_start_codes, nal_unit_size_field);
			}
		}
		if (entry->lhvc_config) {
			count = gf_list_count(entry->lhvc_config->config->param_array);
			for (i=0; i<count; i++) {
				GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(entry->lhvc_config->config->param_array, i);
				if (ar->type == GF_HEVC_NALU_VID_PARAM) {
					if (! *has_vps)  *has_vps = GF_TRUE;
					else continue;
				}
				rewrite_nalus_list(ar->nalus, ps_bs, rewrite_start_codes, nal_unit_size_field);
			}
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
		/*add mvc config */
		if (entry->mvc_config) {
			rewrite_nalus_list(entry->mvc_config->config->sequenceParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
			rewrite_nalus_list(entry->mvc_config->config->pictureParameterSets, ps_bs, rewrite_start_codes, nal_unit_size_field);
		}
	}
}


GF_Err gf_isom_nalu_sample_rewrite(GF_MediaBox *mdia, GF_ISOSample *sample, u32 sampleNumber, GF_MPEGVisualSampleEntryBox *entry)
{
	Bool is_hevc = GF_FALSE;
	//if only one sync given in the sample sync table, insert sps/pps/vps before cra/bla in hevc
//	Bool check_cra_bla = (mdia->information->sampleTable->SyncSample && mdia->information->sampleTable->SyncSample->nb_entries>1) ? 0 : 1;
	Bool check_cra_bla = GF_TRUE;
	Bool insert_nalu_delim = GF_TRUE;
	Bool force_sei_inspect = GF_FALSE;
	GF_Err e = GF_OK;
	GF_BitStream *sei_suffix_bs = NULL;
	Bool ps_transfered = GF_FALSE;
	u32 nal_size, nal_unit_size_field, extractor_mode;
	Bool rewrite_ps, rewrite_start_codes, insert_vdrd_code;
	s8 nal_type;
	u32 nal_hdr, sabt_ref, i, track_num;
	u32 temporal_id = 0;
	GF_ISOFile *file = mdia->mediaTrack->moov->mov;
	GF_TrackReferenceTypeBox *scal = NULL;

	Track_FindRef(mdia->mediaTrack, GF_ISOM_REF_SCAL, &scal);

	rewrite_ps = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG) ? GF_TRUE : GF_FALSE;
	rewrite_start_codes = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG) ? GF_TRUE : GF_FALSE;
	insert_vdrd_code = (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_VDRD_FLAG) ? GF_TRUE : GF_FALSE;
	if (!entry->svc_config && !entry->mvc_config && !entry->lhvc_config) insert_vdrd_code = GF_FALSE;
	extractor_mode = mdia->mediaTrack->extractor_mode&0x0000FFFF;

	if (mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_TILE_ONLY) {
		insert_nalu_delim = GF_FALSE;
	}

	track_num = 1 + gf_list_find(mdia->mediaTrack->moov->trackList, mdia->mediaTrack);

	if ( (extractor_mode != GF_ISOM_NALU_EXTRACT_INSPECT) && !(mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_TILE_ONLY) ) {
		u32 ref_track, di;
		//aggregate all sabt samples with the same DTS
		if (entry->lhvc_config && !entry->hevc_config && !(mdia->mediaTrack->extractor_mode & GF_ISOM_NALU_EXTRACT_LAYER_ONLY)) {
			GF_ISOSample *base_samp;
			if (gf_isom_get_reference_count(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_SCAL) <= 0) {
				//FIXME - for now we only support two layers (base + enh) in implicit
				if ( gf_isom_get_reference_count(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_BASE) >= 1) {
					gf_isom_get_reference(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_BASE, 1, &ref_track);
					switch (gf_isom_get_media_subtype(mdia->mediaTrack->moov->mov , ref_track, 1)) {
					case GF_ISOM_SUBTYPE_HVC1:
					case GF_ISOM_SUBTYPE_HVC2:
					case GF_ISOM_SUBTYPE_HEV1:
					case GF_ISOM_SUBTYPE_HEV2:

						if (!mdia->extracted_samp) {
							mdia->extracted_samp = gf_isom_sample_new();
							if (!mdia->extracted_samp) return GF_OUT_OF_MEM;
						}

						base_samp = gf_isom_get_sample_ex(mdia->mediaTrack->moov->mov, ref_track, sampleNumber + mdia->mediaTrack->sample_count_at_seg_start, &di, mdia->extracted_samp);
						if (base_samp && base_samp->data) {
							if (!sample->alloc_size || (sample->alloc_size<sample->dataLength+base_samp->dataLength) ) {
								sample->data = gf_realloc(sample->data, sample->dataLength+base_samp->dataLength);
								if (sample->alloc_size) sample->alloc_size = sample->dataLength+base_samp->dataLength;
							}
							memmove(sample->data + base_samp->dataLength, sample->data , sample->dataLength);
							memcpy(sample->data, base_samp->data, base_samp->dataLength);
							sample->dataLength += base_samp->dataLength;
						}
						Track_FindRef(mdia->mediaTrack, GF_ISOM_REF_BASE, &scal);
						break;
					}
				}
			}
		}

		sabt_ref = gf_isom_get_reference_count(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_SABT);
		if ((s32) sabt_ref > 0) {
			force_sei_inspect = GF_TRUE;
			for (i=0; i<sabt_ref; i++) {
				GF_ISOSample *tile_samp;
				gf_isom_get_reference(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_SABT, i+1, &ref_track);

				if (!mdia->extracted_samp) {
					mdia->extracted_samp = gf_isom_sample_new();
					if (!mdia->extracted_samp) return GF_OUT_OF_MEM;
				}

				tile_samp = gf_isom_get_sample_ex(mdia->mediaTrack->moov->mov, ref_track, sampleNumber + mdia->mediaTrack->sample_count_at_seg_start, &di, mdia->extracted_samp);
				if (tile_samp  && tile_samp ->data) {
					if (!sample->alloc_size || (sample->alloc_size<sample->dataLength+tile_samp->dataLength) ) {
						sample->data = gf_realloc(sample->data, sample->dataLength+tile_samp->dataLength);
						if (sample->alloc_size) sample->alloc_size = sample->dataLength+tile_samp->dataLength;
					}
					memcpy(sample->data + sample->dataLength, tile_samp->data, tile_samp->dataLength);
					sample->dataLength += tile_samp->dataLength;
				}
			}
		}
	}

	if ( gf_isom_get_reference_count(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_TBAS) >= 1) {
		u32 ref_track;
		u32 idx = gf_list_find(mdia->information->sampleTable->SampleDescription->child_boxes, entry);
		GF_TrackBox *tbas;
		gf_isom_get_reference(mdia->mediaTrack->moov->mov, track_num, GF_ISOM_REF_TBAS, 1, &ref_track);
		tbas = (GF_TrackBox *)gf_list_get(mdia->mediaTrack->moov->trackList, ref_track-1);
		entry = gf_list_get(tbas->Media->information->sampleTable->SampleDescription->child_boxes, idx);
	}


	if (sample->IsRAP < SAP_TYPE_2) {
		if (mdia->information->sampleTable->no_sync_found || (!sample->IsRAP && check_cra_bla) ) {
			sample->IsRAP = is_sample_idr(mdia, sample, entry);
		}
	}
	if (!sample->IsRAP)
		rewrite_ps = GF_FALSE;

	if (extractor_mode != GF_ISOM_NALU_EXTRACT_LAYER_ONLY)
		insert_vdrd_code = GF_FALSE;

	//this is a compatible HEVC, don't insert VDRD, insert NALU delim
	if (entry->lhvc_config && entry->hevc_config)
		insert_vdrd_code = GF_FALSE;

	if (extractor_mode == GF_ISOM_NALU_EXTRACT_INSPECT) {
		if (!rewrite_ps && !rewrite_start_codes)
			return GF_OK;
	}

	if (!entry) return GF_BAD_PARAM;
	nal_unit_size_field = 0;
	/*if svc rewrite*/
	if (entry->svc_config && entry->svc_config->config)
		nal_unit_size_field = entry->svc_config->config->nal_unit_size;
	/*if mvc rewrite*/
	if (entry->mvc_config && entry->mvc_config->config)
		nal_unit_size_field = entry->mvc_config->config->nal_unit_size;

	/*if lhvc rewrite*/
	else if (entry->lhvc_config && entry->lhvc_config->config)  {
		is_hevc = GF_TRUE;
		nal_unit_size_field = entry->lhvc_config->config->nal_unit_size;
	}

	/*otherwise do nothing*/
	else if (!rewrite_ps && !rewrite_start_codes && !scal && !force_sei_inspect) {
		return GF_OK;
	}

	if (!nal_unit_size_field) {
		if (entry->avc_config) nal_unit_size_field = entry->avc_config->config->nal_unit_size;
		else if (entry->hevc_config || entry->lhvc_config ) {
			nal_unit_size_field = entry->lhvc_config ? entry->lhvc_config->config->nal_unit_size : entry->hevc_config->config->nal_unit_size;
			is_hevc = GF_TRUE;
		}
	}

	if (!nal_unit_size_field) return GF_ISOM_INVALID_FILE;

	//setup PS rewriter
	if (!mdia->nalu_ps_bs)
		mdia->nalu_ps_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_seek(mdia->nalu_ps_bs, 0);

	//setup sample reader
	if (mdia->in_sample_buffer_alloc<sample->dataLength) {
		mdia->in_sample_buffer_alloc = sample->dataLength;
		mdia->in_sample_buffer = gf_realloc(mdia->in_sample_buffer, sample->dataLength);
	}
	memcpy(mdia->in_sample_buffer, sample->data, sample->dataLength);

	if (!mdia->nalu_parser) {
		mdia->nalu_parser = gf_bs_new(mdia->in_sample_buffer, sample->dataLength, GF_BITSTREAM_READ);
		if (!mdia->nalu_parser && sample->data) return GF_ISOM_INVALID_FILE;
	} else {
		e = gf_bs_reassign_buffer(mdia->nalu_parser, mdia->in_sample_buffer, sample->dataLength);
		if (e) return e;
	}
	//setup ouput
	if (!mdia->nalu_out_bs) {
		u8 *output;
		u32 outSize;
		mdia->nalu_out_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_get_content(mdia->nalu_out_bs, &output, &outSize);
	}

	gf_bs_reassign_buffer(mdia->nalu_out_bs, sample->data, sample->alloc_size ? sample->alloc_size : sample->dataLength);

	/*rewrite start code with NALU delim*/
	if (rewrite_start_codes) {

		//we are SVC, don't write NALU delim, only insert VDRD NALU
		if (insert_vdrd_code) {
			if (is_hevc) {
				//spec is not clear here, we don't insert an NALU AU delimiter before the layer starts since it breaks openHEVC
//				insert_nalu_delim=0;
			} else {
				gf_bs_write_int(mdia->nalu_out_bs, 1, 32);
				gf_bs_write_int(mdia->nalu_out_bs, GF_AVC_NALU_VDRD , 8);
				insert_nalu_delim=0;
			}
		}

		//AVC/HEVC base, insert NALU delim
		if (insert_nalu_delim) {
			gf_bs_write_int(mdia->nalu_out_bs, 1, 32);
			if (is_hevc) {
#ifndef GPAC_DISABLE_HEVC
				gf_bs_write_int(mdia->nalu_out_bs, 0, 1);
				gf_bs_write_int(mdia->nalu_out_bs, GF_HEVC_NALU_ACCESS_UNIT, 6);
				gf_bs_write_int(mdia->nalu_out_bs, insert_vdrd_code ? 1 : 0, 6); //we should pick the layerID of the following nalus ...
				gf_bs_write_int(mdia->nalu_out_bs, 1, 3); //nuh_temporal_id_plus1 - cannot be 0, we use 1 by default, and overwrite it if needed at the end

				/*pic-type - by default we signal all slice types possible*/
				gf_bs_write_int(mdia->nalu_out_bs, 2, 3);
				gf_bs_write_int(mdia->nalu_out_bs, 0, 5);
#endif
			} else {
				gf_bs_write_int(mdia->nalu_out_bs, (sample->data[0] & 0x60) | GF_AVC_NALU_ACCESS_UNIT, 8);
				gf_bs_write_int(mdia->nalu_out_bs, 0xF0 , 8); /*7 "all supported NALUs" (=111) + rbsp trailing (10000)*/;
			}
		}
	}

	if (rewrite_ps) {
		Bool has_vps = GF_FALSE;
		//in inspect mode or single-layer mode just use the xPS from this layer
		if (extractor_mode == GF_ISOM_NALU_EXTRACT_DEFAULT) {
			u32 i;

			if (scal) {
				for (i=0; i<scal->trackIDCount; i++) {
					GF_TrackBox *a_track = GetTrackbyID(mdia->mediaTrack->moov, scal->trackIDs[i]);
					GF_MPEGVisualSampleEntryBox *an_entry = NULL;
					if (a_track && a_track->Media && a_track->Media->information && a_track->Media->information->sampleTable && a_track->Media->information->sampleTable->SampleDescription)
						an_entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(a_track->Media->information->sampleTable->SampleDescription->child_boxes, 0);

					if (an_entry)
						nalu_merge_ps(mdia->nalu_ps_bs, rewrite_start_codes, nal_unit_size_field, an_entry, is_hevc, &has_vps);
				}
			}
		}
		nalu_merge_ps(mdia->nalu_ps_bs, rewrite_start_codes, nal_unit_size_field, entry, is_hevc, &has_vps);


		if (is_hevc) {
			/*little optimization if we are not asked to start codes: copy over the sample*/
			if (!rewrite_start_codes && !entry->lhvc_config && !scal) {
				if (! ps_transfered) {
					u8 nal_type = (sample->data[nal_unit_size_field] & 0x7E) >> 1;
					//temp fix - if we detect xPS in the beginning of the sample do NOT copy the ps bitstream
					//this is not correct since we are not sure whether they are the same xPS or not, but it crashes openHEVC ...
					switch (nal_type) {
#ifndef GPAC_DISABLE_HEVC
					case GF_HEVC_NALU_VID_PARAM:
					case GF_HEVC_NALU_SEQ_PARAM:
					case GF_HEVC_NALU_PIC_PARAM:
						break;
#endif
					default:
						gf_bs_transfer(mdia->nalu_out_bs, mdia->nalu_ps_bs, GF_TRUE);
						break;
					}
				}
				gf_bs_write_data(mdia->nalu_out_bs, mdia->in_sample_buffer, sample->dataLength);
				gf_bs_get_content_no_truncate(mdia->nalu_out_bs, &sample->data, &sample->dataLength, &sample->alloc_size);

				return GF_OK;
			}
		}
	} else {
		ps_transfered = GF_TRUE;
	}

	/*little optimization if we are not asked to rewrite extractors or start codes: copy over the sample*/
	if (!scal && !rewrite_start_codes && !rewrite_ps && !force_sei_inspect) {
		if (! ps_transfered)
		{
			gf_bs_transfer(mdia->nalu_out_bs, mdia->nalu_ps_bs, GF_TRUE);
		}
		gf_bs_write_data(mdia->nalu_out_bs, mdia->in_sample_buffer, sample->dataLength);
		gf_bs_get_content_no_truncate(mdia->nalu_out_bs, &sample->data, &sample->dataLength, &sample->alloc_size);
		return GF_OK;
	}

	if (!mdia->tmp_nal_copy_buffer) {
		mdia->tmp_nal_copy_buffer = gf_malloc(sizeof(char) * 4096);
		mdia->tmp_nal_copy_buffer_alloc = 4096;
	}


	while (gf_bs_available(mdia->nalu_parser)) {
		nal_size = gf_bs_read_int(mdia->nalu_parser, 8*nal_unit_size_field);
		if (gf_bs_get_position(mdia->nalu_parser) + nal_size > sample->dataLength) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("Sample %u (size %u) rewrite: corrupted NAL Unit (size %u)\n", sampleNumber, sample->dataLength, nal_size));
			goto exit;
		}
		if (nal_size > mdia->tmp_nal_copy_buffer_alloc) {
			mdia->tmp_nal_copy_buffer_alloc = nal_size;
			mdia->tmp_nal_copy_buffer = (char*) gf_realloc(mdia->tmp_nal_copy_buffer, sizeof(char)*nal_size);
		}
		if (is_hevc) {
			nal_hdr = gf_bs_read_u16(mdia->nalu_parser);
			nal_type = (nal_hdr&0x7E00) >> 9;
		} else {
			nal_hdr = gf_bs_read_u8(mdia->nalu_parser);
			nal_type = nal_hdr & 0x1F;
		}

		if (is_hevc) {
			GF_BitStream *write_to_bs = mdia->nalu_out_bs;
			if (!ps_transfered) {
				gf_bs_transfer(mdia->nalu_out_bs, mdia->nalu_ps_bs, GF_TRUE);
				ps_transfered = GF_TRUE;
			}

#ifndef GPAC_DISABLE_HEVC
			/*we already wrote this stuff*/
			if (nal_type==GF_HEVC_NALU_ACCESS_UNIT) {
				gf_bs_skip_bytes(mdia->nalu_parser, nal_size-2);
				continue;
			}
			switch (nal_type) {
			//extractor
			case 49:
				e = process_extractor(file, mdia, sampleNumber, sample->DTS, nal_size, nal_hdr, nal_unit_size_field, GF_TRUE, rewrite_ps, rewrite_start_codes, extractor_mode);
				if (e) goto exit;
				break;

			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_TSA_R:
			case GF_HEVC_NALU_SLICE_STSA_R:
				if (temporal_id < (nal_hdr & 0x7))
					temporal_id = (nal_hdr & 0x7);
				/*rewrite nal*/
				gf_bs_read_data(mdia->nalu_parser, mdia->tmp_nal_copy_buffer, nal_size-2);
				if (rewrite_start_codes)
					gf_bs_write_u32(mdia->nalu_out_bs, 1);
				else
					gf_bs_write_int(mdia->nalu_out_bs, nal_size, 8*nal_unit_size_field);

				gf_bs_write_u16(mdia->nalu_out_bs, nal_hdr);
				gf_bs_write_data(mdia->nalu_out_bs, mdia->tmp_nal_copy_buffer, nal_size-2);
				break;

			case GF_HEVC_NALU_SLICE_BLA_W_LP:
			case GF_HEVC_NALU_SLICE_BLA_W_DLP:
			case GF_HEVC_NALU_SLICE_BLA_N_LP:
			case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			case GF_HEVC_NALU_SLICE_IDR_N_LP:
			case GF_HEVC_NALU_SLICE_CRA:
				//insert xPS before CRA/BLA
				if (check_cra_bla && !sample->IsRAP) {
					sample->IsRAP = sap_type_from_nal_type(nal_type);
					if (sei_suffix_bs) gf_bs_del(sei_suffix_bs);
					return gf_isom_nalu_sample_rewrite(mdia, sample, sampleNumber, entry);
				}
			default:
				/*rewrite nal*/
				if (nal_size<2) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Invalid nal size %d in sample %d\n", nal_type, sampleNumber));
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}

				gf_bs_read_data(mdia->nalu_parser, mdia->tmp_nal_copy_buffer, nal_size-2);

				if (nal_type==GF_HEVC_NALU_SEI_SUFFIX) {
					if (!sei_suffix_bs) sei_suffix_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
					write_to_bs = sei_suffix_bs;
				}

				if (rewrite_start_codes)
					gf_bs_write_u32(write_to_bs, 1);
				else
					gf_bs_write_int(write_to_bs, nal_size, 8*nal_unit_size_field);

				gf_bs_write_u16(write_to_bs, nal_hdr);
				gf_bs_write_data(write_to_bs, mdia->tmp_nal_copy_buffer, nal_size-2);
			}
#endif

			//done with HEVC
			continue;
		}

		switch(nal_type) {
		case GF_AVC_NALU_ACCESS_UNIT:
			/*we already wrote this stuff*/
			gf_bs_skip_bytes(mdia->nalu_parser, nal_size-1);
			continue;
		//extractor
		case 31:
			e = process_extractor(file, mdia, sampleNumber, sample->DTS, nal_size, nal_hdr, nal_unit_size_field, GF_FALSE, rewrite_ps, rewrite_start_codes, extractor_mode);
			if (e) goto exit;
			break;
//			case GF_AVC_NALU_SEI:
		case GF_AVC_NALU_SEQ_PARAM:
		case GF_AVC_NALU_PIC_PARAM:
		case GF_AVC_NALU_SEQ_PARAM_EXT:
		case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
			// we will rewrite the sps/pps if and only if there is no sps/pps in bistream
			if (!ps_transfered) {
				ps_transfered = GF_TRUE;
			}
		default:
			if (!ps_transfered) {
				gf_bs_transfer(mdia->nalu_out_bs, mdia->nalu_ps_bs, GF_TRUE);
				ps_transfered = GF_TRUE;
			}
			gf_bs_read_data(mdia->nalu_parser, mdia->tmp_nal_copy_buffer, nal_size-1);
			if (rewrite_start_codes)
				gf_bs_write_u32(mdia->nalu_out_bs, 1);
			else
				gf_bs_write_int(mdia->nalu_out_bs, nal_size, 8*nal_unit_size_field);

			gf_bs_write_u8(mdia->nalu_out_bs, nal_hdr);
			gf_bs_write_data(mdia->nalu_out_bs, mdia->tmp_nal_copy_buffer, nal_size-1);
		}
	}

	if (sei_suffix_bs) {
		gf_bs_transfer(mdia->nalu_out_bs, sei_suffix_bs, GF_FALSE);
	}
	/*done*/
	gf_bs_get_content_no_truncate(mdia->nalu_out_bs, &sample->data, &sample->dataLength, &sample->alloc_size);

	/*rewrite temporal ID of AU Ddelim NALU (first one)*/
	if (rewrite_start_codes && is_hevc && temporal_id) {
		sample->data[6] = (sample->data[6] & 0xF8) | (temporal_id+1);
	}


exit:
	if (sei_suffix_bs)
		gf_bs_del(sei_suffix_bs);

	return e;
}

GF_HEVCConfig *HEVC_DuplicateConfig(GF_HEVCConfig *cfg)
{
	u8 *data;
	u32 data_size;
	GF_HEVCConfig *new_cfg;
	GF_BitStream *bs;

	if (!cfg) return NULL;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_odf_hevc_cfg_write_bs(cfg, bs);

	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);

	new_cfg = gf_odf_hevc_cfg_read_bs(bs, cfg->is_lhvc);
	new_cfg->is_lhvc = cfg->is_lhvc;
	gf_bs_del(bs);
	gf_free(data);
	return new_cfg;
}

static GF_AVCConfig *AVC_DuplicateConfig(GF_AVCConfig *cfg)
{
	u32 i, count;
	GF_AVCConfigSlot *p1, *p2;
	GF_AVCConfig *cfg_new;
	if (!cfg)
		return NULL;
	cfg_new = gf_odf_avc_cfg_new();
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
		p2->id = p1->id;
		p2->data = (char *)gf_malloc(sizeof(char)*p1->size);
		memcpy(p2->data, p1->data, sizeof(char)*p1->size);
		gf_list_add(cfg_new->sequenceParameterSets, p2);
	}

	count = gf_list_count(cfg->pictureParameterSets);
	for (i=0; i<count; i++) {
		p1 = (GF_AVCConfigSlot*)gf_list_get(cfg->pictureParameterSets, i);
		p2 = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
		p2->size = p1->size;
		p2->id = p1->id;
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
			p2->id = p1->id;
			p2->data = (char*)gf_malloc(sizeof(char)*p1->size);
			memcpy(p2->data, p1->data, sizeof(char)*p1->size);
			gf_list_add(cfg_new->sequenceParameterSetExtensions, p2);
		}
	}
	return cfg_new;
}

static void merge_avc_config(GF_AVCConfig *dst_cfg, GF_AVCConfig *src_cfg)
{
	GF_AVCConfig *cfg;
	if (!src_cfg || !dst_cfg) return;
	cfg = AVC_DuplicateConfig(src_cfg);
	if (!cfg) return;
	
	while (gf_list_count(cfg->sequenceParameterSets)) {
		GF_AVCConfigSlot *p = (GF_AVCConfigSlot*)gf_list_get(cfg->sequenceParameterSets, 0);
		gf_list_rem(cfg->sequenceParameterSets, 0);
		gf_list_insert(dst_cfg->sequenceParameterSets, p, 0);
	}
	while (gf_list_count(cfg->pictureParameterSets)) {
		GF_AVCConfigSlot *p = (GF_AVCConfigSlot*)gf_list_get(cfg->pictureParameterSets, 0);
		gf_list_rem(cfg->pictureParameterSets, 0);
		gf_list_insert(dst_cfg->pictureParameterSets, p, 0);
	}
	gf_odf_avc_cfg_del(cfg);
}

void merge_hevc_config(GF_HEVCConfig *dst_cfg, GF_HEVCConfig *src_cfg, Bool force_insert)
{
	GF_HEVCConfig *cfg = HEVC_DuplicateConfig(src_cfg);
	//merge all xPS
	u32 i, j, count = cfg->param_array ? gf_list_count(cfg->param_array) : 0;
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar_h = NULL;
		u32 count2 = dst_cfg->param_array ? gf_list_count(dst_cfg->param_array) : 0;
		GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(cfg->param_array, i);
		for (j=0; j<count2; j++) {
			ar_h = (GF_HEVCParamArray*)gf_list_get(dst_cfg->param_array, j);
			if (ar_h->type==ar->type) {
				break;
			}
			ar_h = NULL;
		}
		if (!ar_h) {
			gf_list_add(dst_cfg->param_array, ar);
			gf_list_rem(cfg->param_array, i);
			count--;
			i--;
		} else {
			while (gf_list_count(ar->nalus)) {
				GF_AVCConfigSlot *p = (GF_AVCConfigSlot*)gf_list_get(ar->nalus, 0);
				gf_list_rem(ar->nalus, 0);
				if (force_insert)
					gf_list_insert(ar_h->nalus, p, 0);
				else
					gf_list_add(ar_h->nalus, p);
			}

		}
	}
	gf_odf_hevc_cfg_del(cfg);

#define CHECK_CODE(__code)	if (dst_cfg->__code < src_cfg->__code) dst_cfg->__code = src_cfg->__code;

	CHECK_CODE(configurationVersion)
	CHECK_CODE(profile_idc)
	CHECK_CODE(profile_space)
	CHECK_CODE(tier_flag)
	CHECK_CODE(general_profile_compatibility_flags)
	CHECK_CODE(progressive_source_flag)
	CHECK_CODE(interlaced_source_flag)
	CHECK_CODE(constraint_indicator_flags)
	CHECK_CODE(level_idc)
	CHECK_CODE(min_spatial_segmentation_idc)

}

void merge_all_config(GF_AVCConfig *avc_cfg, GF_HEVCConfig *hevc_cfg, GF_MediaBox *mdia)
{
	u32 i;
	GF_TrackReferenceTypeBox *scal = NULL;
	Track_FindRef(mdia->mediaTrack, GF_ISOM_REF_SCAL, &scal);

	if (!scal) return;

	for (i=0; i<scal->trackIDCount; i++) {
		GF_TrackBox *a_track = GetTrackbyID(mdia->mediaTrack->moov, scal->trackIDs[i]);
		GF_MPEGVisualSampleEntryBox *an_entry = NULL;
		if (a_track && a_track->Media && a_track->Media->information && a_track->Media->information->sampleTable && a_track->Media->information->sampleTable->SampleDescription)
			an_entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(a_track->Media->information->sampleTable->SampleDescription->child_boxes, 0);

		if (!an_entry) continue;

		if (avc_cfg && an_entry->svc_config && an_entry->svc_config->config)
			merge_avc_config(avc_cfg, an_entry->svc_config->config);

		if (avc_cfg && an_entry->mvc_config && an_entry->mvc_config->config)
			merge_avc_config(avc_cfg, an_entry->mvc_config->config);

		if (avc_cfg && an_entry->avc_config && an_entry->avc_config->config)
			merge_avc_config(avc_cfg, an_entry->avc_config->config);

		if (hevc_cfg && an_entry->lhvc_config && an_entry->lhvc_config->config)
			merge_hevc_config(hevc_cfg, an_entry->lhvc_config->config, GF_TRUE);

		if (hevc_cfg && an_entry->hevc_config && an_entry->hevc_config->config)
			merge_hevc_config(hevc_cfg, an_entry->hevc_config->config, GF_TRUE);
	}

	if (hevc_cfg) hevc_cfg->is_lhvc = GF_FALSE;
}

void AVC_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *avc, GF_MediaBox *mdia)
{
	GF_AVCConfig *avcc, *svcc, *mvcc;
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)avc, GF_FALSE);

	if (avc->emul_esd) gf_odf_desc_del((GF_Descriptor *)avc->emul_esd);
	avc->emul_esd = gf_odf_desc_esd_new(2);
	avc->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	/*AVC OTI is 0x21, AVC parameter set stream OTI (not supported in gpac) is 0x22, SVC OTI is 0x24*/
	/*if we have only SVC stream, set objectTypeIndication to AVC OTI; else set it to AVC OTI*/
	if (avc->svc_config && !avc->avc_config)
		avc->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_SVC;
	else if (avc->mvc_config && !avc->avc_config)
		avc->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_MVC;
	else
		avc->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_AVC;

	if (btrt) {
		avc->emul_esd->decoderConfig->bufferSizeDB = btrt->bufferSizeDB;
		avc->emul_esd->decoderConfig->avgBitrate = btrt->avgBitrate;
		avc->emul_esd->decoderConfig->maxBitrate = btrt->maxBitrate;
	}
	GF_MPEG4ExtensionDescriptorsBox *mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_find_child(avc->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
	if (mdesc) {
		u32 i=0;
		GF_Descriptor *desc,*clone;
		i=0;
		while ((desc = (GF_Descriptor *)gf_list_enum(mdesc->descriptors, &i))) {
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
			merge_avc_config(avcc, avc->svc_config->config);
		}
		/*merge MVC config*/
		if (avc->mvc_config) {
			merge_avc_config(avcc, avc->mvc_config->config);
		}
		if (avcc) {
			if (mdia) merge_all_config(avcc, NULL, mdia);

			gf_odf_avc_cfg_write(avcc, &avc->emul_esd->decoderConfig->decoderSpecificInfo->data, &avc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_odf_avc_cfg_del(avcc);
		}
	} else if (avc->svc_config) {
		svcc = AVC_DuplicateConfig(avc->svc_config->config);

		if (mdia) merge_all_config(svcc, NULL, mdia);

		gf_odf_avc_cfg_write(svcc, &avc->emul_esd->decoderConfig->decoderSpecificInfo->data, &avc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_odf_avc_cfg_del(svcc);
	}
	else if (avc->mvc_config) {
		mvcc = AVC_DuplicateConfig(avc->mvc_config->config);

		if (mdia) merge_all_config(mvcc, NULL, mdia);

		gf_odf_avc_cfg_write(mvcc, &avc->emul_esd->decoderConfig->decoderSpecificInfo->data, &avc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_odf_avc_cfg_del(mvcc);
	}
}

void AVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *avc)
{
	AVC_RewriteESDescriptorEx(avc, NULL);
}

void HEVC_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *hevc, GF_MediaBox *mdia)
{
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)hevc, GF_FALSE);

	if (hevc->emul_esd) gf_odf_desc_del((GF_Descriptor *)hevc->emul_esd);
	hevc->emul_esd = gf_odf_desc_esd_new(2);
	hevc->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	hevc->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_HEVC;

	if (btrt) {
		hevc->emul_esd->decoderConfig->bufferSizeDB = btrt->bufferSizeDB;
		hevc->emul_esd->decoderConfig->avgBitrate = btrt->avgBitrate;
		hevc->emul_esd->decoderConfig->maxBitrate = btrt->maxBitrate;
	}
	GF_MPEG4ExtensionDescriptorsBox *mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_find_child(hevc->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
	if (mdesc) {
		u32 i=0;
		GF_Descriptor *desc,*clone;
		i=0;
		while ((desc = (GF_Descriptor *)gf_list_enum(mdesc->descriptors, &i))) {
			clone = NULL;
			gf_odf_desc_copy(desc, &clone);
			if (gf_odf_desc_add_desc((GF_Descriptor *)hevc->emul_esd, clone) != GF_OK)
				gf_odf_desc_del(clone);
		}
	}

	if (hevc->hevc_config || hevc->lhvc_config) {
		GF_HEVCConfig *hcfg = HEVC_DuplicateConfig(hevc->hevc_config ? hevc->hevc_config->config : hevc->lhvc_config->config);

		if (hevc->hevc_config && hevc->lhvc_config) {
			//merge LHVC config to HEVC conf, so we add entry rather than insert
			merge_hevc_config(hcfg, hevc->lhvc_config->config, GF_FALSE);
		}

		if (mdia) merge_all_config(NULL, hcfg, mdia);

		if (hcfg) {
			if (mdia && ((mdia->mediaTrack->extractor_mode&0x0000FFFF) != GF_ISOM_NALU_EXTRACT_INSPECT)) {
				hcfg->is_lhvc=GF_FALSE;
			}

			gf_odf_hevc_cfg_write(hcfg, &hevc->emul_esd->decoderConfig->decoderSpecificInfo->data, &hevc->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_odf_hevc_cfg_del(hcfg);
		}
	}
}
void HEVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *hevc)
{
	HEVC_RewriteESDescriptorEx(hevc, NULL);
}

GF_Err AVC_HEVC_UpdateESD(GF_MPEGVisualSampleEntryBox *avc, GF_ESD *esd)
{
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)avc, GF_TRUE);

	GF_MPEG4ExtensionDescriptorsBox *mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_find_child(avc->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
	if (mdesc) {
		gf_isom_box_del_parent(&avc->child_boxes, (GF_Box *) mdesc);
	}
	btrt->avgBitrate = esd->decoderConfig->avgBitrate;
	btrt->maxBitrate = esd->decoderConfig->maxBitrate;
	btrt->bufferSizeDB = esd->decoderConfig->bufferSizeDB;

	if (gf_list_count(esd->IPIDataSet)
	        || gf_list_count(esd->IPMPDescriptorPointers)
	        || esd->langDesc
	        || gf_list_count(esd->extensionDescriptors)
	        || esd->ipiPtr || esd->qos || esd->RegDescriptor) {

		GF_MPEG4ExtensionDescriptorsBox *mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_new_parent(&avc->child_boxes, GF_ISOM_BOX_TYPE_M4DS);

		if (esd->RegDescriptor) {
			gf_list_add(mdesc->descriptors, esd->RegDescriptor);
			esd->RegDescriptor = NULL;
		}
		if (esd->qos) {
			gf_list_add(mdesc->descriptors, esd->qos);
			esd->qos = NULL;
		}
		if (esd->ipiPtr) {
			gf_list_add(mdesc->descriptors, esd->ipiPtr);
			esd->ipiPtr= NULL;
		}

		while (gf_list_count(esd->IPIDataSet)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPIDataSet, 0);
			gf_list_rem(esd->IPIDataSet, 0);
			gf_list_add(mdesc->descriptors, desc);
		}
		while (gf_list_count(esd->IPMPDescriptorPointers)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->IPMPDescriptorPointers, 0);
			gf_list_rem(esd->IPMPDescriptorPointers, 0);
			gf_list_add(mdesc->descriptors, desc);
		}
		if (esd->langDesc) {
			gf_list_add(mdesc->descriptors, esd->langDesc);
			esd->langDesc = NULL;
		}
		while (gf_list_count(esd->extensionDescriptors)) {
			GF_Descriptor *desc = (GF_Descriptor *)gf_list_get(esd->extensionDescriptors, 0);
			gf_list_rem(esd->extensionDescriptors, 0);
			gf_list_add(mdesc->descriptors, desc);
		}
	}


	if (!avc->lhvc_config && (esd->decoderConfig->objectTypeIndication==GF_CODECID_HEVC)) {
		if (!avc->hevc_config) avc->hevc_config = (GF_HEVCConfigurationBox *)gf_isom_box_new_parent(&avc->child_boxes, GF_ISOM_BOX_TYPE_HVCC);
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			if (avc->hevc_config->config) gf_odf_hevc_cfg_del(avc->hevc_config->config);
			avc->hevc_config->config = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
		}
	}
	else if (!avc->svc_config && !avc->mvc_config && (esd->decoderConfig->objectTypeIndication==GF_CODECID_AVC)) {
		if (!avc->avc_config) avc->avc_config = (GF_AVCConfigurationBox *)gf_isom_box_new_parent(&avc->child_boxes, GF_ISOM_BOX_TYPE_AVCC);
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			if (avc->avc_config->config) gf_odf_avc_cfg_del(avc->avc_config->config);
			avc->avc_config->config = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
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



static GF_AV1Config* AV1_DuplicateConfig(GF_AV1Config const * const cfg) {
	u32 i = 0;
	GF_AV1Config *out = gf_malloc(sizeof(GF_AV1Config));

	out->marker = cfg->marker;
	out->version = cfg->version;
	out->seq_profile = cfg->seq_profile;
	out->seq_level_idx_0 = cfg->seq_level_idx_0;
	out->seq_tier_0 = cfg->seq_tier_0;
	out->high_bitdepth = cfg->high_bitdepth;
	out->twelve_bit = cfg->twelve_bit;
	out->monochrome = cfg->monochrome;
	out->chroma_subsampling_x = cfg->chroma_subsampling_x;
	out->chroma_subsampling_y = cfg->chroma_subsampling_y;
	out->chroma_sample_position = cfg->chroma_sample_position;

	out->initial_presentation_delay_present = cfg->initial_presentation_delay_present;
	out->initial_presentation_delay_minus_one = cfg->initial_presentation_delay_minus_one;
	out->obu_array = gf_list_new();
	for (i = 0; i<gf_list_count(cfg->obu_array); ++i) {
		GF_AV1_OBUArrayEntry *dst = gf_malloc(sizeof(GF_AV1_OBUArrayEntry)), *src = gf_list_get(cfg->obu_array, i);
		dst->obu_length = src->obu_length;
		dst->obu_type = src->obu_type;
		dst->obu = gf_malloc((size_t)dst->obu_length);
		memcpy(dst->obu, src->obu, (size_t)src->obu_length);
		gf_list_add(out->obu_array, dst);
	}
	return out;
}

void AV1_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *av1, GF_MediaBox *mdia)
{
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)av1, GF_FALSE);

	if (av1->emul_esd) gf_odf_desc_del((GF_Descriptor *)av1->emul_esd);
	av1->emul_esd = gf_odf_desc_esd_new(2);
	av1->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	av1->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_AV1;

	if (btrt) {
		av1->emul_esd->decoderConfig->bufferSizeDB = btrt->bufferSizeDB;
		av1->emul_esd->decoderConfig->avgBitrate = btrt->avgBitrate;
		av1->emul_esd->decoderConfig->maxBitrate = btrt->maxBitrate;
	}
	if (av1->av1_config) {
		GF_AV1Config *av1_cfg = AV1_DuplicateConfig(av1->av1_config->config);
		if (av1_cfg) {
			gf_odf_av1_cfg_write(av1_cfg, &av1->emul_esd->decoderConfig->decoderSpecificInfo->data, &av1->emul_esd->decoderConfig->decoderSpecificInfo->dataLength);
			gf_odf_av1_cfg_del(av1_cfg);
		}
	}
}

void AV1_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *av1)
{
	AV1_RewriteESDescriptorEx(av1, NULL);
}



static GF_VPConfig* VP_DuplicateConfig(GF_VPConfig const * const cfg)
{
	GF_VPConfig *out = gf_odf_vp_cfg_new();
	if (out) {
		out->profile = cfg->profile;
		out->level = cfg->level;
		out->bit_depth = cfg->bit_depth;
		out->chroma_subsampling = cfg->chroma_subsampling;
		out->video_fullRange_flag = cfg->video_fullRange_flag;
		out->colour_primaries = cfg->colour_primaries;
		out->transfer_characteristics = cfg->transfer_characteristics;
		out->matrix_coefficients = cfg->matrix_coefficients;
	}

	return out;
}

void VP9_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *vp9, GF_MediaBox *mdia)
{
	GF_BitRateBox *btrt = gf_isom_sample_entry_get_bitrate((GF_SampleEntryBox *)vp9, GF_FALSE);

	if (vp9->emul_esd) gf_odf_desc_del((GF_Descriptor *)vp9->emul_esd);
	vp9->emul_esd = gf_odf_desc_esd_new(2);
	vp9->emul_esd->decoderConfig->streamType = GF_STREAM_VISUAL;
	vp9->emul_esd->decoderConfig->objectTypeIndication = GF_CODECID_VP9;

	if (btrt) {
		vp9->emul_esd->decoderConfig->bufferSizeDB = btrt->bufferSizeDB;
		vp9->emul_esd->decoderConfig->avgBitrate = btrt->avgBitrate;
		vp9->emul_esd->decoderConfig->maxBitrate = btrt->maxBitrate;
	}

	if (vp9->vp_config) {
		GF_VPConfig *vp9_cfg = VP_DuplicateConfig(vp9->vp_config->config);
		if (vp9_cfg) {
			gf_odf_vp_cfg_write(vp9_cfg, &vp9->emul_esd->decoderConfig->decoderSpecificInfo->data, &vp9->emul_esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
			gf_odf_vp_cfg_del(vp9_cfg);
		}
	}
}

void VP9_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *vp9)
{
	VP9_RewriteESDescriptorEx(vp9, NULL);
}



#ifndef GPAC_DISABLE_ISOM_WRITE
GF_EXPORT
GF_Err gf_isom_avc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleDescriptionBox *stsd;
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
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!the_file->keep_utc && !gf_sys_is_test_mode() )
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	stsd = trak->Media->information->sampleTable->SampleDescription;

	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_AVC1);
	if (!entry) return GF_OUT_OF_MEM;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);

	entry->avc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_AVCC);
	entry->avc_config->config = AVC_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	AVC_RewriteESDescriptor(entry);
	return e;
}

static GF_Err gf_isom_avc_config_update_ex(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, u32 op_type, Bool keep_xps)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 i;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_MVC1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	switch (op_type) {
	/*AVCC replacement*/
	case 0:
		if (!cfg) return GF_BAD_PARAM;
		if (!entry->avc_config) entry->avc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_AVCC);
		if (entry->avc_config->config) gf_odf_avc_cfg_del(entry->avc_config->config);
		entry->avc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_AVC1;
		break;
	/*SVCC replacement*/
	case 1:
		if (!cfg) return GF_BAD_PARAM;
		if (!entry->svc_config) entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_SVCC);
		if (entry->svc_config->config) gf_odf_avc_cfg_del(entry->svc_config->config);
		entry->svc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_AVC1;
		break;
	/*SVCC replacement and AVC removal*/
	case 2:
		if (!cfg) return GF_BAD_PARAM;
		if (entry->avc_config) {
			gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->avc_config);
			entry->avc_config = NULL;
		}
		if (!entry->svc_config) entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_SVCC);
		if (entry->svc_config->config) gf_odf_avc_cfg_del(entry->svc_config->config);
		entry->svc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_SVC1;
		break;
	/*AVCC removal and switch to avc3*/
	case 3:
		if (!entry->avc_config || !entry->avc_config->config)
			return GF_BAD_PARAM;
		if (!keep_xps) {
			for (i=0; i<3; i++) {
				GF_AVCConfigurationBox *cfg = entry->avc_config;
				if (i==1) cfg = entry->svc_config;
				else if (i==2) cfg = entry->mvc_config;
				if (!cfg) continue;


				while (gf_list_count(cfg->config->sequenceParameterSets)) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(cfg->config->sequenceParameterSets, 0);
					gf_list_rem(cfg->config->sequenceParameterSets, 0);
					if (sl->data) gf_free(sl->data);
					gf_free(sl);
				}

				while (gf_list_count(cfg->config->pictureParameterSets)) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(cfg->config->pictureParameterSets, 0);
					gf_list_rem(cfg->config->pictureParameterSets, 0);
					if (sl->data) gf_free(sl->data);
					gf_free(sl);
				}

				while (gf_list_count(cfg->config->sequenceParameterSetExtensions)) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(cfg->config->sequenceParameterSetExtensions, 0);
					gf_list_rem(cfg->config->sequenceParameterSetExtensions, 0);
					if (sl->data) gf_free(sl->data);
					gf_free(sl);
				}
			}
		}

		if (entry->type == GF_ISOM_BOX_TYPE_AVC1)
			entry->type = GF_ISOM_BOX_TYPE_AVC3;
		else if (entry->type == GF_ISOM_BOX_TYPE_AVC2)
			entry->type = GF_ISOM_BOX_TYPE_AVC4;
		break;
	/*MVCC replacement*/
	case 4:
		if (!cfg) return GF_BAD_PARAM;
		if (!entry->mvc_config) entry->mvc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_MVCC);
		if (entry->mvc_config->config) gf_odf_avc_cfg_del(entry->mvc_config->config);
		entry->mvc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_AVC1;
		break;
	/*MVCC replacement and AVC removal*/
	case 5:
		if (!cfg) return GF_BAD_PARAM;
		if (entry->avc_config) {
			gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->avc_config);
			entry->avc_config = NULL;
		}
		if (!entry->mvc_config) entry->mvc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_MVCC);
		if (entry->mvc_config->config) gf_odf_avc_cfg_del(entry->mvc_config->config);
		entry->mvc_config->config = AVC_DuplicateConfig(cfg);
		entry->type = GF_ISOM_BOX_TYPE_MVC1;
		break;
	}
	AVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_avc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, Bool keep_xps)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, NULL, 3, keep_xps);
}

GF_EXPORT
GF_Err gf_isom_avc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, 0, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_svc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, Bool is_add)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, is_add ? 1 : 2, GF_FALSE);
}

GF_Err gf_isom_mvc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, Bool is_add)
{
	return gf_isom_avc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, is_add ? 4 : 5, GF_FALSE);
}

static GF_Err gf_isom_svc_mvc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, Bool is_mvc)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_MVC1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (is_mvc && entry->mvc_config) {
		gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->mvc_config);
		entry->mvc_config = NULL;
	}
	else if (!is_mvc && entry->svc_config) {
		gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->svc_config);
		entry->svc_config = NULL;
	}
	AVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_Err gf_isom_svc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	return gf_isom_svc_mvc_config_del(the_file, trackNumber, DescriptionIndex, GF_FALSE);
}

GF_Err gf_isom_mvc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	return gf_isom_svc_mvc_config_del(the_file, trackNumber, DescriptionIndex, GF_TRUE);
}


GF_EXPORT
GF_Err gf_isom_set_ipod_compatible(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, 0);
	if (!entry) return GF_OK;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_MVC1:
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVT1:
		break;
	default:
		return GF_OK;
	}

	if (!entry->ipod_ext) entry->ipod_ext = (GF_UnknownUUIDBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_UUID);
	memcpy(entry->ipod_ext->uuid, GF_ISOM_IPOD_EXT, sizeof(u8)*16);
	entry->ipod_ext->dataSize = 0;
	return GF_OK;
}

static GF_Err gf_isom_svc_mvc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, Bool is_mvc, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_SampleDescriptionBox *stsd;
	GF_MPEGVisualSampleEntryBox *entry;

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

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	if (is_mvc) {
		entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_MVC1);
		if (!entry) return GF_OUT_OF_MEM;
		entry->mvc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_MVCC);
		entry->mvc_config->config = AVC_DuplicateConfig(cfg);
	} else {
		entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes,GF_ISOM_BOX_TYPE_SVC1);
		if (!entry) return GF_OUT_OF_MEM;
		entry->svc_config = (GF_AVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes,GF_ISOM_BOX_TYPE_SVCC);
		entry->svc_config->config = AVC_DuplicateConfig(cfg);
	}
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	AVC_RewriteESDescriptor(entry);
	return e;
}

GF_EXPORT
GF_Err gf_isom_svc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	return gf_isom_svc_mvc_config_new(the_file, trackNumber, cfg, GF_FALSE, URLname, URNname,outDescriptionIndex);
}

GF_EXPORT
GF_Err gf_isom_mvc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	return gf_isom_svc_mvc_config_new(the_file, trackNumber, cfg, GF_TRUE, URLname, URNname,outDescriptionIndex);
}


GF_EXPORT
GF_Err gf_isom_hevc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_HEVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_SampleDescriptionBox *stsd;
	GF_MPEGVisualSampleEntryBox *entry;

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

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_HVC1);
	if (!entry) return GF_OUT_OF_MEM;
	entry->hevc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_HVCC);
	entry->hevc_config->config = HEVC_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	HEVC_RewriteESDescriptor(entry);
	return e;
}

GF_EXPORT
GF_Err gf_isom_vp_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_VPConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex, u32 vpx_type)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_SampleDescriptionBox *stsd;
	GF_MPEGVisualSampleEntryBox *entry;

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

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *)gf_isom_box_new_parent(&stsd->child_boxes, vpx_type);
	if (!entry) return GF_OUT_OF_MEM;
	
	entry->vp_config = (GF_VPConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_VPCC);
	if (!entry->vp_config) return GF_OUT_OF_MEM;
	entry->vp_config->config = VP_DuplicateConfig(cfg);
	strncpy(entry->compressor_name, "\012VPC Coding", sizeof(entry->compressor_name)-1);
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	return e;
}

GF_EXPORT
GF_Err gf_isom_av1_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AV1Config *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MPEGVisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

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

	stsd = trak->Media->information->sampleTable->SampleDescription;
	//create a new entry
	entry = (GF_MPEGVisualSampleEntryBox *)gf_isom_box_new_parent(&stsd->child_boxes, GF_ISOM_BOX_TYPE_AV01);
	if (!entry) return GF_OUT_OF_MEM;
	entry->av1_config = (GF_AV1ConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_AV1C);
	if (!entry->av1_config) return GF_OUT_OF_MEM;
	entry->av1_config->config = AV1_DuplicateConfig(cfg);
	entry->dataReferenceIndex = dataRefIndex;
	*outDescriptionIndex = gf_list_count(stsd->child_boxes);
	return e;
}


typedef enum
{
	GF_ISOM_HVCC_UPDATE = 0,
	GF_ISOM_HVCC_SET_INBAND,
	GF_ISOM_HVCC_SET_TILE,
	GF_ISOM_HVCC_SET_TILE_BASE_TRACK,
	GF_ISOM_HVCC_SET_LHVC,
	GF_ISOM_HVCC_SET_LHVC_WITH_BASE,
	GF_ISOM_HVCC_SET_LHVC_WITH_BASE_BACKWARD,
	GF_ISOM_HVCC_SET_HEVC_TILE_BASE,
	GF_ISOM_LHCC_SET_INBAND
} HevcConfigUpdateType;

static Bool hevc_cleanup_config(GF_HEVCConfig *cfg, HevcConfigUpdateType operand_type, Bool keep_xps)
{
	u32 i;
	Bool array_incomplete = (operand_type==GF_ISOM_HVCC_SET_INBAND) ? 1 : 0;
	if (!cfg) return 0;

	for (i=0; i<gf_list_count(cfg->param_array); i++) {
		GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(cfg->param_array, i);

		/*we want to force hev1*/
		if (operand_type==GF_ISOM_HVCC_SET_INBAND) {
			ar->array_completeness = 0;
			if (keep_xps) {
				array_incomplete=1;
				continue;
			}

			while (gf_list_count(ar->nalus)) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(ar->nalus, 0);
				gf_list_rem(ar->nalus, 0);
				if (sl->data) gf_free(sl->data);
				gf_free(sl);
			}
			gf_list_del(ar->nalus);
			gf_free(ar);
			ar=NULL;
			gf_list_rem(cfg->param_array, i);
			i--;
			continue;
		}
		if (ar && !ar->array_completeness)
			array_incomplete = 1;
	}
	return array_incomplete;
}

static
GF_Err gf_isom_hevc_config_update_ex(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, u32 operand_type, Bool keep_xps)
{
	u32 array_incomplete;
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_BAD_PARAM;
	stsd = trak->Media->information->sampleTable->SampleDescription;
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(stsd->child_boxes, DescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_LHV1:
	case GF_ISOM_BOX_TYPE_LHE1:
	case GF_ISOM_BOX_TYPE_HVT1:
		break;
	default:
		return GF_BAD_PARAM;
	}


	if (operand_type == GF_ISOM_HVCC_SET_TILE_BASE_TRACK) {
		if (entry->type==GF_ISOM_BOX_TYPE_HVC1)
			entry->type = GF_ISOM_BOX_TYPE_HVC2;
		else if (entry->type==GF_ISOM_BOX_TYPE_HEV1)
			entry->type = GF_ISOM_BOX_TYPE_HEV2;
	} else if (operand_type == GF_ISOM_HVCC_SET_TILE) {
		if (!entry->hevc_config) entry->hevc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_HVCC);
		if (entry->hevc_config->config) gf_odf_hevc_cfg_del(entry->hevc_config->config);
		entry->hevc_config->config = NULL;
		entry->type = GF_ISOM_BOX_TYPE_HVT1;
	} else if (operand_type < GF_ISOM_HVCC_SET_LHVC) {
		if ((operand_type != GF_ISOM_HVCC_SET_INBAND) && !entry->hevc_config)
			entry->hevc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_HVCC);

		if (cfg) {
			if (entry->hevc_config->config) gf_odf_hevc_cfg_del(entry->hevc_config->config);
			entry->hevc_config->config = HEVC_DuplicateConfig(cfg);
		} else if (operand_type != GF_ISOM_HVCC_SET_TILE) {
			operand_type=GF_ISOM_HVCC_SET_INBAND;
		}
		array_incomplete = (operand_type==GF_ISOM_HVCC_SET_INBAND) ? 1 : 0;
		if (entry->hevc_config && hevc_cleanup_config(entry->hevc_config->config, operand_type, keep_xps)) {
			array_incomplete=1;
		}

		if (entry->lhvc_config && hevc_cleanup_config(entry->lhvc_config->config, operand_type, keep_xps))
			array_incomplete=1;

		switch (entry->type) {
		case GF_ISOM_BOX_TYPE_HEV1:
		case GF_ISOM_BOX_TYPE_HVC1:
			entry->type = array_incomplete ? GF_ISOM_BOX_TYPE_HEV1 : GF_ISOM_BOX_TYPE_HVC1;
			break;
		case GF_ISOM_BOX_TYPE_HEV2:
		case GF_ISOM_BOX_TYPE_HVC2:
			entry->type = array_incomplete ? GF_ISOM_BOX_TYPE_HEV2 : GF_ISOM_BOX_TYPE_HVC2;
			break;
		case GF_ISOM_BOX_TYPE_LHE1:
		case GF_ISOM_BOX_TYPE_LHV1:
			entry->type = array_incomplete ? GF_ISOM_BOX_TYPE_LHE1 : GF_ISOM_BOX_TYPE_LHV1;
			break;
		}
	} else {

		/*SVCC replacement/removal with HEVC base, backward compatible signaling*/
		if ((operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE_BACKWARD) || (operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE) || (operand_type==GF_ISOM_HVCC_SET_HEVC_TILE_BASE) ) {
			if (!entry->hevc_config) return GF_BAD_PARAM;
			if (!cfg) {
				if (entry->lhvc_config) {
					gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->lhvc_config);
					entry->lhvc_config = NULL;
				}
				if (entry->type==GF_ISOM_BOX_TYPE_LHE1) entry->type = (operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE) ? GF_ISOM_BOX_TYPE_HEV2 : GF_ISOM_BOX_TYPE_HEV1;
				else if (entry->type==GF_ISOM_BOX_TYPE_HEV1) entry->type = (operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE) ? GF_ISOM_BOX_TYPE_HEV2 : GF_ISOM_BOX_TYPE_HEV1;
				else entry->type =  (operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE) ? GF_ISOM_BOX_TYPE_HVC2 : GF_ISOM_BOX_TYPE_HVC1;
			} else {
				if (operand_type != GF_ISOM_HVCC_SET_HEVC_TILE_BASE) {
					if (!entry->lhvc_config) entry->lhvc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_LHVC);
					if (entry->lhvc_config->config) gf_odf_hevc_cfg_del(entry->lhvc_config->config);
					entry->lhvc_config->config = HEVC_DuplicateConfig(cfg);
				}
				
				if (operand_type==GF_ISOM_HVCC_SET_LHVC_WITH_BASE_BACKWARD) {
					if (entry->type==GF_ISOM_BOX_TYPE_HEV2) entry->type = GF_ISOM_BOX_TYPE_HEV1;
					else entry->type = GF_ISOM_BOX_TYPE_HVC1;
				} else {
					if (entry->type==GF_ISOM_BOX_TYPE_HEV1) entry->type = GF_ISOM_BOX_TYPE_HEV2;
					else entry->type = GF_ISOM_BOX_TYPE_HVC2;
				}
			}
		}
		/*LHEVC track without base*/
		else if (operand_type==GF_ISOM_HVCC_SET_LHVC) {
			if (entry->hevc_config) {
				gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)entry->hevc_config);
				entry->hevc_config=NULL;
			}
			if (!cfg) return GF_BAD_PARAM;

			if (!entry->lhvc_config) entry->lhvc_config = (GF_HEVCConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_LHVC);
			if (entry->lhvc_config->config) gf_odf_hevc_cfg_del(entry->lhvc_config->config);
			entry->lhvc_config->config = HEVC_DuplicateConfig(cfg);

			if ((entry->type==GF_ISOM_BOX_TYPE_HEV1) || (entry->type==GF_ISOM_BOX_TYPE_HEV2)) entry->type = GF_ISOM_BOX_TYPE_LHE1;
			else entry->type = GF_ISOM_BOX_TYPE_LHV1;
		}
		/*LHEVC inband, no config change*/
		else if (operand_type==GF_ISOM_LHCC_SET_INBAND) {
			entry->type = GF_ISOM_BOX_TYPE_LHE1;
		}
	}

	HEVC_RewriteESDescriptor(entry);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_hevc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, GF_ISOM_HVCC_UPDATE, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_hevc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, Bool keep_xps)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, NULL, GF_ISOM_HVCC_SET_INBAND, keep_xps);
}

GF_EXPORT
GF_Err gf_isom_lhvc_force_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, NULL, GF_ISOM_LHCC_SET_INBAND, GF_FALSE);
}


GF_EXPORT
GF_Err gf_isom_hevc_set_tile_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, Bool is_base_track)
{
	return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, is_base_track ? GF_ISOM_HVCC_SET_TILE_BASE_TRACK : GF_ISOM_HVCC_SET_TILE, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_lhvc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, GF_ISOMLHEVCTrackType track_type)
{
	if (cfg) cfg->is_lhvc = GF_TRUE;
	switch (track_type) {
	case GF_ISOM_LEHVC_ONLY:
		return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, GF_ISOM_HVCC_SET_LHVC, GF_FALSE);
	case GF_ISOM_LEHVC_WITH_BASE:
		return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, GF_ISOM_HVCC_SET_LHVC_WITH_BASE, GF_FALSE);
	case GF_ISOM_LEHVC_WITH_BASE_BACKWARD:
		return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, GF_ISOM_HVCC_SET_LHVC_WITH_BASE_BACKWARD, GF_FALSE);
	case GF_ISOM_HEVC_TILE_BASE:
		return gf_isom_hevc_config_update_ex(the_file, trackNumber, DescriptionIndex, cfg, GF_ISOM_HVCC_SET_HEVC_TILE_BASE, GF_FALSE);
	default:
		return GF_BAD_PARAM;
	}
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_Box *gf_isom_clone_config_box(GF_Box *box)
{
	u8 *data=NULL;
	u32 size=0;
	GF_Err e;
	GF_Box *clone=NULL;
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = gf_isom_box_size(box);
	e = gf_isom_box_write(box, bs);
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	if (!e) {
		bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		e = gf_isom_box_parse(&clone, bs);
		gf_bs_del(bs);
	}
	if (data)
		gf_free(data);
	if (e) {
		if (clone)
			gf_isom_box_del(clone);
		clone = NULL;
	}
	return clone;
}

GF_EXPORT
GF_AVCConfig *gf_isom_avc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_avc_svc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_AVCTYPE_NONE)
		return NULL;

	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return NULL;

	if (!entry->avc_config) return NULL;
	return AVC_DuplicateConfig(entry->avc_config->config);
}

GF_EXPORT
GF_HEVCConfig *gf_isom_hevc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	if (gf_isom_get_reference_count(the_file, trackNumber, GF_ISOM_REF_TBAS)) {
		u32 ref_track;
		GF_Err e = gf_isom_get_reference(the_file, trackNumber, GF_ISOM_REF_TBAS, 1, &ref_track);
		if (e == GF_OK) {
			trackNumber = ref_track;
		}
	}
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_hevc_lhvc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_HEVCTYPE_NONE)
		return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
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
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
	if (!entry->svc_config) return NULL;
	return AVC_DuplicateConfig(entry->svc_config->config);
}


GF_EXPORT
GF_AVCConfig *gf_isom_mvc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_avc_svc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_AVCTYPE_NONE)
		return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
	if (!entry->mvc_config) return NULL;
	return AVC_DuplicateConfig(entry->mvc_config->config);
}

GF_EXPORT
GF_AV1Config *gf_isom_av1_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	if (gf_isom_get_reference_count(the_file, trackNumber, GF_ISOM_REF_TBAS)) {
		u32 ref_track;
		GF_Err e = gf_isom_get_reference(the_file, trackNumber, GF_ISOM_REF_TBAS, 1, &ref_track);
		if (e == GF_OK) {
			trackNumber = ref_track;
		}
	}
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex - 1);
	if (!entry || !entry->av1_config) return NULL;
	return AV1_DuplicateConfig(entry->av1_config->config);
}

GF_EXPORT
GF_VPConfig *gf_isom_vp_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex - 1);
	if (!entry || !entry->vp_config) return NULL;
	return VP_DuplicateConfig(entry->vp_config->config);
}

GF_EXPORT
u32 gf_isom_get_avc_svc_type(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	u32 type;
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_ISOM_AVCTYPE_NONE;
	if (!gf_isom_is_video_subtype(trak->Media->handler->handlerType))
		return GF_ISOM_AVCTYPE_NONE;

	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return GF_ISOM_AVCTYPE_NONE;

	type = entry->type;

	if (type == GF_ISOM_BOX_TYPE_ENCV) {
		GF_ProtectionSchemeInfoBox *sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		if (sinf && sinf->original_format) type = sinf->original_format->data_format;
	}
	else if (type == GF_ISOM_BOX_TYPE_RESV) {
		if (entry->rinf && entry->rinf->original_format) type = entry->rinf->original_format->data_format;
	}

	switch (type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_MVC1:
		break;
	default:
		return GF_ISOM_AVCTYPE_NONE;
	}
	if (entry->avc_config && !entry->svc_config && !entry->mvc_config) return GF_ISOM_AVCTYPE_AVC_ONLY;
	if (entry->avc_config && entry->svc_config) return GF_ISOM_AVCTYPE_AVC_SVC;
	if (entry->avc_config && entry->mvc_config) return GF_ISOM_AVCTYPE_AVC_MVC;
	if (!entry->avc_config && entry->svc_config) return GF_ISOM_AVCTYPE_SVC_ONLY;
	if (!entry->avc_config && entry->mvc_config) return GF_ISOM_AVCTYPE_MVC_ONLY;
	return GF_ISOM_AVCTYPE_NONE;
}

GF_EXPORT
u32 gf_isom_get_hevc_lhvc_type(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	u32 type;
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return GF_ISOM_HEVCTYPE_NONE;
	if (!gf_isom_is_video_subtype(trak->Media->handler->handlerType))
		return GF_ISOM_HEVCTYPE_NONE;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return GF_ISOM_AVCTYPE_NONE;
	type = entry->type;

	if (type == GF_ISOM_BOX_TYPE_ENCV) {
		GF_ProtectionSchemeInfoBox *sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		if (sinf && sinf->original_format) type = sinf->original_format->data_format;
	}
	else if (type == GF_ISOM_BOX_TYPE_RESV) {
		if (entry->rinf && entry->rinf->original_format) type = entry->rinf->original_format->data_format;
	}

	switch (type) {
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_LHV1:
	case GF_ISOM_BOX_TYPE_LHE1:
	case GF_ISOM_BOX_TYPE_HVT1:
		break;
	default:
		return GF_ISOM_HEVCTYPE_NONE;
	}
	if (entry->hevc_config && !entry->lhvc_config) return GF_ISOM_HEVCTYPE_HEVC_ONLY;
	if (entry->hevc_config && entry->lhvc_config) return GF_ISOM_HEVCTYPE_HEVC_LHVC;
	if (!entry->hevc_config && entry->lhvc_config) return GF_ISOM_HEVCTYPE_LHVC_ONLY;
	return GF_ISOM_HEVCTYPE_NONE;
}

GF_EXPORT
GF_HEVCConfig *gf_isom_lhvc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_HEVCConfig *lhvc;
	GF_OperatingPointsInformation *oinf=NULL;
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !DescriptionIndex) return NULL;
	if (gf_isom_get_hevc_lhvc_type(the_file, trackNumber, DescriptionIndex)==GF_ISOM_HEVCTYPE_NONE)
		return NULL;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return NULL;
	if (!entry->lhvc_config) return NULL;
	lhvc = HEVC_DuplicateConfig(entry->lhvc_config->config);
	if (!lhvc) return NULL;

	gf_isom_get_oinf_info(the_file, trackNumber, &oinf);
	if (oinf) {
		LHEVC_ProfileTierLevel *ptl = (LHEVC_ProfileTierLevel *)gf_list_last(oinf->profile_tier_levels);
		if (ptl) {
			lhvc->profile_space  = ptl->general_profile_space;
			lhvc->tier_flag = ptl->general_tier_flag;
			lhvc->profile_idc = ptl->general_profile_idc;
			lhvc->general_profile_compatibility_flags = ptl->general_profile_compatibility_flags;
			lhvc->constraint_indicator_flags = ptl->general_constraint_indicator_flags;
		}
	}
	return lhvc;
}


void btrt_box_del(GF_Box *s)
{
	GF_BitRateBox *ptr = (GF_BitRateBox *)s;
	if (ptr) gf_free(ptr);
}
GF_Err btrt_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_BitRateBox *ptr = (GF_BitRateBox *)s;
	ptr->bufferSizeDB = gf_bs_read_u32(bs);
	ptr->maxBitrate = gf_bs_read_u32(bs);
	ptr->avgBitrate = gf_bs_read_u32(bs);
	return GF_OK;
}
GF_Box *btrt_box_new()
{
	GF_BitRateBox *tmp = (GF_BitRateBox *) gf_malloc(sizeof(GF_BitRateBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_BitRateBox));
	tmp->type = GF_ISOM_BOX_TYPE_BTRT;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err btrt_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_BitRateBox *ptr = (GF_BitRateBox *) s;
	if (!s) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_u32(bs, ptr->bufferSizeDB);
	gf_bs_write_u32(bs, ptr->maxBitrate);
	gf_bs_write_u32(bs, ptr->avgBitrate);
	return GF_OK;
}
GF_Err btrt_box_size(GF_Box *s)
{
	GF_BitRateBox *ptr = (GF_BitRateBox *)s;
	ptr->size += 12;
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void m4ds_box_del(GF_Box *s)
{
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *)s;
	gf_odf_desc_list_del(ptr->descriptors);
	gf_list_del(ptr->descriptors);
	gf_free(ptr);
}
GF_Err m4ds_box_read(GF_Box *s, GF_BitStream *bs)
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
GF_Box *m4ds_box_new()
{
	GF_MPEG4ExtensionDescriptorsBox *tmp = (GF_MPEG4ExtensionDescriptorsBox *) gf_malloc(sizeof(GF_MPEG4ExtensionDescriptorsBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_MPEG4ExtensionDescriptorsBox));
	tmp->type = GF_ISOM_BOX_TYPE_M4DS;
	tmp->descriptors = gf_list_new();
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err m4ds_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	u8 *enc_ods;
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
GF_Err m4ds_box_size(GF_Box *s)
{
	GF_Err e;
	u32 descSize = 0;
	GF_MPEG4ExtensionDescriptorsBox *ptr = (GF_MPEG4ExtensionDescriptorsBox *)s;
	e = gf_odf_desc_list_size(ptr->descriptors, &descSize);
	ptr->size += descSize;
	return e;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void avcc_box_del(GF_Box *s)
{
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *)s;
	if (ptr->config) gf_odf_avc_cfg_del(ptr->config);
	ptr->config = NULL;
	gf_free(ptr);
}

GF_Err avcc_box_read(GF_Box *s, GF_BitStream *bs)
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
		if (gf_bs_available(bs) < sl->size) {
			gf_free(sl);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("AVCC: Not enough bits to parse. Aborting.\n"));
			return GF_ISOM_INVALID_FILE;
		}
		sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
		gf_bs_read_data(bs, sl->data, sl->size);
		gf_list_add(ptr->config->pictureParameterSets, sl);
		ptr->size -= 2+sl->size;
	}

	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		if (gf_avc_is_rext_profile(ptr->config->AVCProfileIndication)) {
			if (!ptr->size) {
#ifndef GPAC_DISABLE_AV_PARSERS
				AVCState avc;
				s32 idx;
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot*)gf_list_get(ptr->config->sequenceParameterSets, 0);
				idx = sl ? gf_media_avc_read_sps(sl->data+1, sl->size-1, &avc, 0, NULL) : -1;

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
					if (gf_bs_available(bs) < sl->size) {
						gf_free(sl);
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("AVCC: Not enough bits to parse. Aborting.\n"));
						return GF_ISOM_INVALID_FILE;
					}
					sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
					gf_bs_read_data(bs, sl->data, sl->size);
					gf_list_add(ptr->config->sequenceParameterSetExtensions, sl);
					ptr->size -= sl->size + 2;
				}
			}
		}
	}
	return GF_OK;
}

GF_Box *avcc_box_new()
{
	GF_AVCConfigurationBox *tmp = (GF_AVCConfigurationBox *) gf_malloc(sizeof(GF_AVCConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_AVCConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_AVCC;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err avcc_box_write(GF_Box *s, GF_BitStream *bs)
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
		if (gf_avc_is_rext_profile(ptr->config->AVCProfileIndication)) {
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
		}
	}
	return GF_OK;
}
GF_Err avcc_box_size(GF_Box *s)
{
	u32 i, count;
	GF_AVCConfigurationBox *ptr = (GF_AVCConfigurationBox *)s;

	if (!ptr->config) {
		ptr->size = 0;
		return GF_OK;
	}
	ptr->size += 7;
	count = gf_list_count(ptr->config->sequenceParameterSets);
	for (i=0; i<count; i++)
		ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->sequenceParameterSets, i))->size;

	count = gf_list_count(ptr->config->pictureParameterSets);
	for (i=0; i<count; i++)
		ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->pictureParameterSets, i))->size;

	if (ptr->type==GF_ISOM_BOX_TYPE_AVCC) {
		if (gf_avc_is_rext_profile(ptr->config->AVCProfileIndication)) {
			ptr->size += 4;
			count = ptr->config->sequenceParameterSetExtensions ?gf_list_count(ptr->config->sequenceParameterSetExtensions) : 0;
			for (i=0; i<count; i++)
				ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ptr->config->sequenceParameterSetExtensions, i))->size;
		}
	}
	return GF_OK;
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/



void hvcc_box_del(GF_Box *s)
{
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox*)s;
	if (ptr->config) gf_odf_hevc_cfg_del(ptr->config);
	gf_free(ptr);
}

GF_Err hvcc_box_read(GF_Box *s, GF_BitStream *bs)
{
	u64 pos;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *)s;

	if (ptr->config) gf_odf_hevc_cfg_del(ptr->config);

	pos = gf_bs_get_position(bs);
	ptr->config = gf_odf_hevc_cfg_read_bs(bs, (s->type == GF_ISOM_BOX_TYPE_HVCC) ? GF_FALSE : GF_TRUE);
	pos = gf_bs_get_position(bs) - pos ;
	if (pos < ptr->size)
		ptr->size -= (u32) pos;

	return ptr->config ? GF_OK : GF_ISOM_INVALID_FILE;
}

GF_Box *hvcc_box_new()
{
	GF_HEVCConfigurationBox *tmp = (GF_HEVCConfigurationBox *) gf_malloc(sizeof(GF_HEVCConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_HEVCConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_HVCC;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err hvcc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *) s;
	if (!s) return GF_BAD_PARAM;
	if (!ptr->config) return GF_OK;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return gf_odf_hevc_cfg_write_bs(ptr->config, bs);
}

GF_Err hvcc_box_size(GF_Box *s)
{
	u32 i, count, j, subcount;
	GF_HEVCConfigurationBox *ptr = (GF_HEVCConfigurationBox *)s;

	if (!ptr->config) {
		ptr->size = 0;
		return GF_OK;
	}

	if (!ptr->config->is_lhvc)
		ptr->size += 23;
	else
		ptr->size += 6;

	count = gf_list_count(ptr->config->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(ptr->config->param_array, i);
		ptr->size += 3;
		subcount = gf_list_count(ar->nalus);
		for (j=0; j<subcount; j++) {
			ptr->size += 2 + ((GF_AVCConfigSlot *)gf_list_get(ar->nalus, j))->size;
		}
	}
	return GF_OK;
}

GF_Box *av1c_box_new() {
	GF_AV1ConfigurationBox *tmp = (GF_AV1ConfigurationBox *)gf_malloc(sizeof(GF_AV1ConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_AV1ConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_AV1C;
	return (GF_Box *)tmp;
}

void av1c_box_del(GF_Box *s) {
	GF_AV1ConfigurationBox *ptr = (GF_AV1ConfigurationBox*)s;
	if (ptr->config) gf_odf_av1_cfg_del(ptr->config);
	gf_free(ptr);
}

GF_Err av1c_box_read(GF_Box *s, GF_BitStream *bs)
{
	u64 pos, read;
	GF_AV1ConfigurationBox *ptr = (GF_AV1ConfigurationBox*)s;

	if (ptr->config) gf_odf_av1_cfg_del(ptr->config);

	pos = gf_bs_get_position(bs);

	ptr->config = gf_odf_av1_cfg_read_bs_size(bs, (u32) ptr->size);

	read = gf_bs_get_position(bs) - pos;

	if (read < ptr->size)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOBMFF] AV1ConfigurationBox: read only "LLU" bytes (expected "LLU").\n", read, ptr->size));
	if (read > ptr->size)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] AV1ConfigurationBox overflow read "LLU" bytes, of box size "LLU".\n", read, ptr->size));

	return GF_OK;
}

GF_Err av1c_box_write(GF_Box *s, GF_BitStream *bs) {
	GF_Err e;
	GF_AV1ConfigurationBox *ptr = (GF_AV1ConfigurationBox*)s;
	if (!s) return GF_BAD_PARAM;
	if (!ptr->config) return GF_BAD_PARAM;
	e = gf_isom_box_write_header(s, bs);
	if (e) return e;

	return gf_odf_av1_cfg_write_bs(ptr->config, bs);
}

GF_Err av1c_box_size(GF_Box *s) {
	u32 i;
	GF_AV1ConfigurationBox *ptr = (GF_AV1ConfigurationBox *)s;

	if (!ptr->config) {
		ptr->size = 0;
		return GF_BAD_PARAM;
	}

	ptr->size += 4;

	for (i = 0; i < gf_list_count(ptr->config->obu_array); ++i) {
		GF_AV1_OBUArrayEntry *a = gf_list_get(ptr->config->obu_array, i);
		ptr->size += a->obu_length;
	}

	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/




void vpcc_box_del(GF_Box *s)
{
	GF_VPConfigurationBox *ptr = (GF_VPConfigurationBox*)s;
	if (ptr->config) gf_odf_vp_cfg_del(ptr->config);
	ptr->config = NULL;
	gf_free(ptr);
}

GF_Err vpcc_box_read(GF_Box *s, GF_BitStream *bs)
{
	u64 pos;
	GF_VPConfigurationBox *ptr = (GF_VPConfigurationBox *)s;

	if (ptr->config) gf_odf_vp_cfg_del(ptr->config);
	ptr->config = NULL;

	pos = gf_bs_get_position(bs);
	ptr->config = gf_odf_vp_cfg_read_bs(bs, ptr->version == 0);
	pos = gf_bs_get_position(bs) - pos ;

	if (pos < ptr->size)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOBMFF] VPConfigurationBox: read only "LLU" bytes (expected "LLU").\n", pos, ptr->size));
	if (pos > ptr->size)
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] VPConfigurationBox overflow read "LLU" bytes, of box size "LLU".\n", pos, ptr->size));

	return ptr->config ? GF_OK : GF_ISOM_INVALID_FILE;
}

GF_Box *vpcc_box_new()
{
	GF_VPConfigurationBox *tmp = (GF_VPConfigurationBox *) gf_malloc(sizeof(GF_VPConfigurationBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_VPConfigurationBox));
	tmp->type = GF_ISOM_BOX_TYPE_VPCC;
	tmp->version = 1;
	return (GF_Box *)tmp;
}

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err vpcc_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_VPConfigurationBox *ptr = (GF_VPConfigurationBox *) s;
	if (!s) return GF_BAD_PARAM;
	if (!ptr->config) return GF_OK;

	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	return gf_odf_vp_cfg_write_bs(ptr->config, bs, ptr->version == 0);
}
#endif

GF_Err vpcc_box_size(GF_Box *s)
{
	GF_VPConfigurationBox *ptr = (GF_VPConfigurationBox *)s;

	if (!ptr->config) {
		ptr->size = 0;
		return GF_OK;
	}

	if (ptr->version == 0) {
		ptr->size += 6;
	} else {
		if (ptr->config->codec_initdata_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] VPConfigurationBox: codec_initdata_size MUST be 0, was %d\n", ptr->config->codec_initdata_size));
			return GF_ISOM_INVALID_FILE;
		}

		ptr->size += 8;
	}

	return GF_OK;
}


GF_Box *SmDm_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_SMPTE2086MasteringDisplayMetadataBox, GF_ISOM_BOX_TYPE_SMDM);
	return (GF_Box *)tmp;
}

void SmDm_box_del(GF_Box *a)
{
	GF_SMPTE2086MasteringDisplayMetadataBox *p = (GF_SMPTE2086MasteringDisplayMetadataBox *)a;
	gf_free(p);
}

GF_Err SmDm_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_SMPTE2086MasteringDisplayMetadataBox *p = (GF_SMPTE2086MasteringDisplayMetadataBox *)s;
	p->primaryRChromaticity_x = gf_bs_read_u16(bs);
	p->primaryRChromaticity_y = gf_bs_read_u16(bs);
	p->primaryGChromaticity_x = gf_bs_read_u16(bs);
	p->primaryGChromaticity_y = gf_bs_read_u16(bs);
	p->primaryBChromaticity_x = gf_bs_read_u16(bs);
	p->primaryBChromaticity_y = gf_bs_read_u16(bs);
	p->whitePointChromaticity_x = gf_bs_read_u16(bs);
	p->whitePointChromaticity_y = gf_bs_read_u16(bs);
	p->luminanceMax = gf_bs_read_u32(bs);
	p->luminanceMin = gf_bs_read_u32(bs);

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err SmDm_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_SMPTE2086MasteringDisplayMetadataBox *p = (GF_SMPTE2086MasteringDisplayMetadataBox*)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, p->primaryRChromaticity_x);
	gf_bs_write_u16(bs, p->primaryRChromaticity_y);
	gf_bs_write_u16(bs, p->primaryGChromaticity_x);
	gf_bs_write_u16(bs, p->primaryGChromaticity_y);
	gf_bs_write_u16(bs, p->primaryBChromaticity_x);
	gf_bs_write_u16(bs, p->primaryBChromaticity_y);
	gf_bs_write_u16(bs, p->whitePointChromaticity_x);
	gf_bs_write_u16(bs, p->whitePointChromaticity_y);
	gf_bs_write_u32(bs, p->luminanceMax);
	gf_bs_write_u32(bs, p->luminanceMin);

	return GF_OK;
}

GF_Err SmDm_box_size(GF_Box *s)
{
	GF_SMPTE2086MasteringDisplayMetadataBox *p = (GF_SMPTE2086MasteringDisplayMetadataBox*)s;
	p->size += 24;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_Box *CoLL_box_new()
{
	ISOM_DECL_BOX_ALLOC(GF_VPContentLightLevelBox, GF_ISOM_BOX_TYPE_COLL);
	return (GF_Box *)tmp;
}

void CoLL_box_del(GF_Box *a)
{
	GF_VPContentLightLevelBox *p = (GF_VPContentLightLevelBox *)a;
	gf_free(p);
}

GF_Err CoLL_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_VPContentLightLevelBox *p = (GF_VPContentLightLevelBox *)s;
	p->maxCLL = gf_bs_read_u16(bs);
	p->maxFALL = gf_bs_read_u16(bs);

	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err CoLL_box_write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_VPContentLightLevelBox *p = (GF_VPContentLightLevelBox*)s;
	e = gf_isom_full_box_write(s, bs);
	if (e) return e;

	gf_bs_write_u16(bs, p->maxCLL);
	gf_bs_write_u16(bs, p->maxFALL);

	return GF_OK;
}

GF_Err CoLL_box_size(GF_Box *s)
{
	GF_VPContentLightLevelBox *p = (GF_VPContentLightLevelBox*)s;
	p->size += 4;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_OperatingPointsInformation *gf_isom_oinf_new_entry()
{
	GF_OperatingPointsInformation* ptr;
	GF_SAFEALLOC(ptr, GF_OperatingPointsInformation);
	if (ptr) {
		ptr->profile_tier_levels = gf_list_new();
		ptr->operating_points = gf_list_new();
		ptr->dependency_layers = gf_list_new();
	}
	return ptr;

}

void gf_isom_oinf_del_entry(void *entry)
{
	GF_OperatingPointsInformation* ptr = (GF_OperatingPointsInformation *)entry;
	if (!ptr) return;
	if (ptr->profile_tier_levels) {
		while (gf_list_count(ptr->profile_tier_levels)) {
			LHEVC_ProfileTierLevel *ptl = (LHEVC_ProfileTierLevel *)gf_list_get(ptr->profile_tier_levels, 0);
			gf_free(ptl);
			gf_list_rem(ptr->profile_tier_levels, 0);
		}
		gf_list_del(ptr->profile_tier_levels);
	}
	if (ptr->operating_points) {
		while (gf_list_count(ptr->operating_points)) {
			LHEVC_OperatingPoint *op = (LHEVC_OperatingPoint *)gf_list_get(ptr->operating_points, 0);
			gf_free(op);
			gf_list_rem(ptr->operating_points, 0);
		}
		gf_list_del(ptr->operating_points);
	}
	if (ptr->dependency_layers) {
		while (gf_list_count(ptr->dependency_layers)) {
			LHEVC_DependentLayer *dep = (LHEVC_DependentLayer *)gf_list_get(ptr->dependency_layers, 0);
			gf_free(dep);
			gf_list_rem(ptr->dependency_layers, 0);
		}
		gf_list_del(ptr->dependency_layers);
	}
	gf_free(ptr);
	return;
}

GF_Err gf_isom_oinf_read_entry(void *entry, GF_BitStream *bs)
{
	GF_OperatingPointsInformation* ptr = (GF_OperatingPointsInformation *)entry;
	u32 i, j, count;

	if (!ptr) return GF_BAD_PARAM;
	ptr->scalability_mask = gf_bs_read_u16(bs);
	gf_bs_read_int(bs, 2);//reserved
	count = gf_bs_read_int(bs, 6);
	for (i = 0; i < count; i++) {
		LHEVC_ProfileTierLevel *ptl;
		GF_SAFEALLOC(ptl, LHEVC_ProfileTierLevel);
		if (!ptl) return GF_OUT_OF_MEM;
		ptl->general_profile_space = gf_bs_read_int(bs, 2);
		ptl->general_tier_flag= gf_bs_read_int(bs, 1);
		ptl->general_profile_idc = gf_bs_read_int(bs, 5);
		ptl->general_profile_compatibility_flags = gf_bs_read_u32(bs);
		ptl->general_constraint_indicator_flags = gf_bs_read_long_int(bs, 48);
		ptl->general_level_idc = gf_bs_read_u8(bs);
		gf_list_add(ptr->profile_tier_levels, ptl);
	}
	count = gf_bs_read_u16(bs);
	for (i = 0; i < count; i++) {
		LHEVC_OperatingPoint *op;
		GF_SAFEALLOC(op, LHEVC_OperatingPoint);
		if (!op) return GF_OUT_OF_MEM;
		op->output_layer_set_idx = gf_bs_read_u16(bs);
		op->max_temporal_id = gf_bs_read_u8(bs);
		op->layer_count = gf_bs_read_u8(bs);
		if (op->layer_count > ARRAY_LENGTH(op->layers_info))
			return GF_NON_COMPLIANT_BITSTREAM;
		for (j = 0; j < op->layer_count; j++) {
			op->layers_info[j].ptl_idx = gf_bs_read_u8(bs);
			op->layers_info[j].layer_id = gf_bs_read_int(bs, 6);
			op->layers_info[j].is_outputlayer = gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE;
			op->layers_info[j].is_alternate_outputlayer = gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE;
		}
		op->minPicWidth = gf_bs_read_u16(bs);
		op->minPicHeight = gf_bs_read_u16(bs);
		op->maxPicWidth = gf_bs_read_u16(bs);
		op->maxPicHeight = gf_bs_read_u16(bs);
		op->maxChromaFormat = gf_bs_read_int(bs, 2);
		op->maxBitDepth = gf_bs_read_int(bs, 3) + 8;
		gf_bs_read_int(bs, 1);//reserved
		op->frame_rate_info_flag = gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE;
		op->bit_rate_info_flag = gf_bs_read_int(bs, 1) ? GF_TRUE : GF_FALSE;
		if (op->frame_rate_info_flag) {
			op->avgFrameRate = gf_bs_read_u16(bs);
			gf_bs_read_int(bs, 6); //reserved
			op->constantFrameRate = gf_bs_read_int(bs, 2);
		}
		if (op->bit_rate_info_flag) {
			op->maxBitRate = gf_bs_read_u32(bs);
			op->avgBitRate = gf_bs_read_u32(bs);
		}
		gf_list_add(ptr->operating_points, op);
	}
	count = gf_bs_read_u8(bs);
	for (i = 0; i < count; i++) {
		LHEVC_DependentLayer *dep;
		GF_SAFEALLOC(dep, LHEVC_DependentLayer);
		if (!dep) return GF_OUT_OF_MEM;
		dep->dependent_layerID = gf_bs_read_u8(bs);
		dep->num_layers_dependent_on = gf_bs_read_u8(bs);
		if (dep->num_layers_dependent_on > ARRAY_LENGTH(dep->dependent_on_layerID)) {
			gf_free(dep);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		for (j = 0; j < dep->num_layers_dependent_on; j++)
			dep->dependent_on_layerID[j] = gf_bs_read_u8(bs);
		for (j = 0; j < 16; j++) {
			if (ptr->scalability_mask & (1 << j))
				dep->dimension_identifier[j] = gf_bs_read_u8(bs);
		}
		gf_list_add(ptr->dependency_layers, dep);
	}

	return GF_OK;
}

GF_Err gf_isom_oinf_write_entry(void *entry, GF_BitStream *bs)
{
	GF_OperatingPointsInformation* ptr = (GF_OperatingPointsInformation *)entry;
	u32 i, j, count;
	if (!ptr) return GF_OK;

	gf_bs_write_u16(bs, ptr->scalability_mask);
	gf_bs_write_int(bs, 0xFF, 2);//reserved
	count=gf_list_count(ptr->profile_tier_levels);
	gf_bs_write_int(bs, count, 6);
	for (i = 0; i < count; i++) {
		LHEVC_ProfileTierLevel *ptl = (LHEVC_ProfileTierLevel *)gf_list_get(ptr->profile_tier_levels, i);
		gf_bs_write_int(bs, ptl->general_profile_space, 2);
		gf_bs_write_int(bs, ptl->general_tier_flag, 1);
		gf_bs_write_int(bs, ptl->general_profile_idc, 5);
		gf_bs_write_u32(bs, ptl->general_profile_compatibility_flags);
		gf_bs_write_long_int(bs, ptl->general_constraint_indicator_flags, 48);
		gf_bs_write_u8(bs, ptl->general_level_idc);
	}
	count=gf_list_count(ptr->operating_points);
	gf_bs_write_u16(bs, count);
	for (i = 0; i < count; i++) {
		LHEVC_OperatingPoint *op = (LHEVC_OperatingPoint *)gf_list_get(ptr->operating_points, i);
		gf_bs_write_u16(bs, op->output_layer_set_idx);
		gf_bs_write_u8(bs, op->max_temporal_id);
		gf_bs_write_u8(bs, op->layer_count);
		for (j = 0; j < op->layer_count; j++) {
			gf_bs_write_u8(bs, op->layers_info[j].ptl_idx);
			gf_bs_write_int(bs, op->layers_info[j].layer_id, 6);
			op->layers_info[j].is_outputlayer ? gf_bs_write_int(bs, 0x1, 1)  : gf_bs_write_int(bs, 0x0, 1);
			op->layers_info[j].is_alternate_outputlayer ? gf_bs_write_int(bs, 0x1, 1)  : gf_bs_write_int(bs, 0x0, 1);
		}
		gf_bs_write_u16(bs, op->minPicWidth);
		gf_bs_write_u16(bs, op->minPicHeight);
		gf_bs_write_u16(bs, op->maxPicWidth);
		gf_bs_write_u16(bs, op->maxPicHeight);
		gf_bs_write_int(bs, op->maxChromaFormat, 2);
		gf_bs_write_int(bs, op->maxBitDepth - 8, 3);
		gf_bs_write_int(bs, 0x1, 1);//resereved
		op->frame_rate_info_flag ? gf_bs_write_int(bs, 0x1, 1)  : gf_bs_write_int(bs, 0x0, 1);
		op->bit_rate_info_flag ? gf_bs_write_int(bs, 0x1, 1)  : gf_bs_write_int(bs, 0x0, 1);
		if (op->frame_rate_info_flag) {
			gf_bs_write_u16(bs, op->avgFrameRate);
			gf_bs_write_int(bs, 0xFF, 6); //reserved
			gf_bs_write_int(bs, op->constantFrameRate, 2);
		}
		if (op->bit_rate_info_flag) {
			gf_bs_write_u32(bs, op->maxBitRate);
			gf_bs_write_u32(bs, op->avgBitRate);
		}
	}
	count=gf_list_count(ptr->dependency_layers);
	gf_bs_write_u8(bs, count);
	for (i = 0; i < count; i++) {
		LHEVC_DependentLayer *dep = (LHEVC_DependentLayer *)gf_list_get(ptr->dependency_layers, i);
		gf_bs_write_u8(bs, dep->dependent_layerID);
		gf_bs_write_u8(bs, dep->num_layers_dependent_on);
		for (j = 0; j < dep->num_layers_dependent_on; j++)
			gf_bs_write_u8(bs, dep->dependent_on_layerID[j]);
		for (j = 0; j < 16; j++) {
			if (ptr->scalability_mask & (1 << j))
				gf_bs_write_u8(bs, dep->dimension_identifier[j]);
		}
	}

	return GF_OK;
}

u32 gf_isom_oinf_size_entry(void *entry)
{
	GF_OperatingPointsInformation* ptr = (GF_OperatingPointsInformation *)entry;
	u32 size = 0, i ,j, count;
	if (!ptr) return 0;

	size += 3; //scalability_mask + reserved + num_profile_tier_level
	count=gf_list_count(ptr->profile_tier_levels);
	size += count * 12; //general_profile_space + general_tier_flag + general_profile_idc + general_profile_compatibility_flags + general_constraint_indicator_flags + general_level_idc
	size += 2;//num_operating_points
	count=gf_list_count(ptr->operating_points);
	for (i = 0; i < count; i++) {
		LHEVC_OperatingPoint *op = (LHEVC_OperatingPoint *)gf_list_get(ptr->operating_points, i);
		size += 2/*output_layer_set_idx*/ + 1/*max_temporal_id*/ + 1/*layer_count*/;
		size += op->layer_count * 2;
		size += 9;
		if (op->frame_rate_info_flag) {
			size += 3;
		}
		if (op->bit_rate_info_flag) {
			size += 8;
		}
	}
	size += 1;//max_layer_count
	count=gf_list_count(ptr->dependency_layers);
	for (i = 0; i < count; i++) {
		LHEVC_DependentLayer *dep = (LHEVC_DependentLayer *)gf_list_get(ptr->dependency_layers, i);
		size += 1/*dependent_layerID*/ + 1/*num_layers_dependent_on*/;
		size += dep->num_layers_dependent_on * 1;//dependent_on_layerID
		for (j = 0; j < 16; j++) {
			if (ptr->scalability_mask & (1 << j))
				size += 1;//dimension_identifier
		}
	}
	return size;
}


GF_LHVCLayerInformation *gf_isom_linf_new_entry()
{
	GF_LHVCLayerInformation* ptr;
	GF_SAFEALLOC(ptr, GF_LHVCLayerInformation);
	if (ptr) ptr->num_layers_in_track = gf_list_new();

	return ptr;

}

void gf_isom_linf_del_entry(void *entry)
{
	GF_LHVCLayerInformation* ptr = (GF_LHVCLayerInformation *)entry;
	if (!ptr) return;
	while (gf_list_count(ptr->num_layers_in_track)) {
		LHVCLayerInfoItem *li = (LHVCLayerInfoItem *)gf_list_get(ptr->num_layers_in_track, 0);
		gf_free(li);
		gf_list_rem(ptr->num_layers_in_track, 0);
	}
	gf_list_del(ptr->num_layers_in_track);
	gf_free(ptr);
	return;
}

GF_Err gf_isom_linf_read_entry(void *entry, GF_BitStream *bs)
{
	GF_LHVCLayerInformation* ptr = (GF_LHVCLayerInformation *)entry;
	u32 i, count;

	if (!ptr) return GF_BAD_PARAM;
	gf_bs_read_int(bs, 2);
	count = gf_bs_read_int(bs, 6);
	for (i = 0; i < count; i++) {
		LHVCLayerInfoItem *li;
		GF_SAFEALLOC(li, LHVCLayerInfoItem);
		if (!li) return GF_OUT_OF_MEM;
		gf_bs_read_int(bs, 4);
		li->layer_id = gf_bs_read_int(bs, 6);
		li->min_TemporalId = gf_bs_read_int(bs, 3);
		li->max_TemporalId = gf_bs_read_int(bs, 3);
		gf_bs_read_int(bs, 1);
		li->sub_layer_presence_flags = gf_bs_read_int(bs, 7);
		gf_list_add(ptr->num_layers_in_track, li);
	}
	return GF_OK;
}

GF_Err gf_isom_linf_write_entry(void *entry, GF_BitStream *bs)
{
	GF_LHVCLayerInformation* ptr = (GF_LHVCLayerInformation *)entry;
	u32 i, count;
	if (!ptr) return GF_OK;

	gf_bs_write_int(bs, 0, 2);
	count=gf_list_count(ptr->num_layers_in_track);
	gf_bs_write_int(bs, count, 6);
	for (i = 0; i < count; i++) {
		LHVCLayerInfoItem *li = (LHVCLayerInfoItem *)gf_list_get(ptr->num_layers_in_track, i);
		gf_bs_write_int(bs, 0, 4);
		gf_bs_write_int(bs, li->layer_id, 6);
		gf_bs_write_int(bs, li->min_TemporalId, 3);
		gf_bs_write_int(bs, li->max_TemporalId, 3);
		gf_bs_write_int(bs, 0, 1);
		gf_bs_write_int(bs, li->sub_layer_presence_flags, 7);
	}
	return GF_OK;
}

u32 gf_isom_linf_size_entry(void *entry)
{
	GF_LHVCLayerInformation* ptr = (GF_LHVCLayerInformation *)entry;
	u32 size = 0, count;
	if (!ptr) return 0;

	size += 1;
	count=gf_list_count(ptr->num_layers_in_track);
	size += count * 3;
	return size;
}


#endif /*GPAC_DISABLE_ISOM*/
