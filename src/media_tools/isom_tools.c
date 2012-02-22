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



#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/config_file.h>

#ifndef GPAC_DISABLE_ISOM

GF_Err gf_media_get_rfc_6381_codec_name(GF_ISOFile *movie, u32 track, char *szCodec)
{
	GF_ESD *esd;
	GF_AVCConfig *avcc;
	GF_AVCConfigSlot *sps;
	u32 subtype = gf_isom_get_media_subtype(movie, track, 1);

	switch (subtype) {
	case GF_ISOM_SUBTYPE_MPEG4:
		esd = gf_isom_get_esd(movie, track, 1);
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_AUDIO:
			if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
				/*5 first bits of AAC config*/
				u8 audio_object_type = (esd->decoderConfig->decoderSpecificInfo->data[0] & 0xF8) >> 3;
				sprintf(szCodec, "mp4a.%02x.%02x", esd->decoderConfig->objectTypeIndication, audio_object_type);
			} else {
				sprintf(szCodec, "mp4a.%02x", esd->decoderConfig->objectTypeIndication);
			}
			break;
		case GF_STREAM_VISUAL:
			if (esd->decoderConfig->decoderSpecificInfo) {
				GF_M4VDecSpecInfo dsi;
				gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
				sprintf(szCodec, "mp4v.%02x.%02x", esd->decoderConfig->objectTypeIndication, dsi.VideoPL);
			} else {
				sprintf(szCodec, "mp4v.%02x", esd->decoderConfig->objectTypeIndication);
			}
			break;
		default:
			sprintf(szCodec, "mp4s.%02x", esd->decoderConfig->objectTypeIndication);
			break;
		}
		gf_odf_desc_del((GF_Descriptor *)esd);
		return GF_OK;

	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_SVC_H264:
		avcc = gf_isom_avc_config_get(movie, track, 1);
		sps = gf_list_get(avcc->sequenceParameterSets, 0);
		sprintf(szCodec, "%s.%02x%02x%02x", gf_4cc_to_str(subtype), (u8) sps->data[1], (u8) sps->data[2], (u8) sps->data[3]);
		gf_odf_avc_cfg_del(avcc);
		return GF_OK;

	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_AUTHOR, ("[ISOM Tools] codec parameters not known - setting codecs string to default value \"%s\"\n", gf_4cc_to_str(subtype) ));
		sprintf(szCodec, "%s", gf_4cc_to_str(subtype));
		return GF_OK;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

static const u32 ISMA_VIDEO_OD_ID = 20;
static const u32 ISMA_AUDIO_OD_ID = 10;

static const u32 ISMA_VIDEO_ES_ID = 201;
static const u32 ISMA_AUDIO_ES_ID = 101;

static const char ISMA_BIFS_CONFIG[] = {0x00, 0x00, 0x60 };

/*ISMA audio*/
static const u8 ISMA_BIFS_AUDIO[] =
{
	0xC0, 0x10, 0x12, 0x81, 0x30, 0x2A, 0x05, 0x7C
};
/*ISMA video*/
static const u8 ISMA_GF_BIFS_VIDEO[] =
{
	0xC0, 0x10, 0x12, 0x60, 0x42, 0x82, 0x28, 0x29,
	0xD0, 0x4F, 0x00
};
/*ISMA audio-video*/
static const u8 ISMA_BIFS_AV[] =
{
	0xC0, 0x10, 0x12, 0x81, 0x30, 0x2A, 0x05, 0x72,
	0x60, 0x42, 0x82, 0x28, 0x29, 0xD0, 0x4F, 0x00
};

/*image only - uses same visual OD ID as video*/
static const u8 ISMA_BIFS_IMAGE[] =
{
	0xC0, 0x11, 0xA4, 0xCD, 0x53, 0x6A, 0x0A, 0x44,
	0x13, 0x00
};

/*ISMA audio-image*/
static const u8 ISMA_BIFS_AI[] =
{
	0xC0, 0x11, 0xA5, 0x02, 0x60, 0x54, 0x0A, 0xE4,
	0xCD, 0x53, 0x6A, 0x0A, 0x44, 0x13, 0x00
};


GF_EXPORT
GF_Err gf_media_make_isma(GF_ISOFile *mp4file, Bool keepESIDs, Bool keepImage, Bool no_ocr)
{
	u32 AudioTrack, VideoTrack, Tracks, i, mType, bifsT, odT, descIndex, VID, AID, bifsID, odID;
	u32 bifs, w, h;
	Bool is_image, image_track;
	GF_ESD *a_esd, *v_esd, *_esd;
	GF_ObjectDescriptor *od;
	GF_ODUpdate *odU;
	GF_ODCodec *codec;
	GF_ISOSample *samp;
	GF_BitStream *bs;
	u8 audioPL, visualPL;

	switch (gf_isom_get_mode(mp4file)) {
	case GF_ISOM_OPEN_EDIT:
	case GF_ISOM_OPEN_WRITE:
	case GF_ISOM_WRITE_EDIT:
		break;
	default:
		return GF_BAD_PARAM;
	}


	Tracks = gf_isom_get_track_count(mp4file);
	AID = VID = 0;
	is_image = 0;

	//search for tracks
	for (i=0; i<Tracks; i++) {
		GF_ESD *esd = gf_isom_get_esd(mp4file, i+1, 1);
		//remove from IOD
		gf_isom_remove_track_from_root_od(mp4file, i+1);

		mType = gf_isom_get_media_type(mp4file, i+1);
		switch (mType) {
		case GF_ISOM_MEDIA_VISUAL:
			image_track = 0;
			if (esd && ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_PNG)) )
				image_track = 1;

			/*remove image tracks if wanted*/
			if (keepImage || !image_track) {
				/*only ONE video stream possible with ISMA*/
				if (VID) {
					if (esd) gf_odf_desc_del((GF_Descriptor*)esd);
					GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA convert] More than one video track found, cannot convert file - remove extra track(s)\n"));
					return GF_NOT_SUPPORTED;
				}
				VID = gf_isom_get_track_id(mp4file, i+1);
				is_image = image_track;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA convert] Visual track ID %d: only one sample found, assuming image and removing track\n", gf_isom_get_track_id(mp4file, i+1) ) );
				gf_isom_remove_track(mp4file, i+1);
				i -= 1;
				Tracks = gf_isom_get_track_count(mp4file);
			}
			break;
		case GF_ISOM_MEDIA_AUDIO:
			if (AID) {
				if (esd) gf_odf_desc_del((GF_Descriptor*)esd);
				GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[ISMA convert] More than one audio track found, cannot convert file - remove extra track(s)\n") );
				return GF_NOT_SUPPORTED;
			}
			AID = gf_isom_get_track_id(mp4file, i+1);
			break;
		/*clean file*/
		default:
			if (mType==GF_ISOM_MEDIA_HINT) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA convert] Removing Hint track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA convert] Removing track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
			}
			gf_isom_remove_track(mp4file, i+1);
			i -= 1;
			Tracks = gf_isom_get_track_count(mp4file);
			break;
		}
		if (esd) gf_odf_desc_del((GF_Descriptor*)esd);
	}
	//no audio no video
	if (!AID && !VID) return GF_OK;

	/*reset all PLs*/
	visualPL = 0xFE;
	audioPL = 0xFE;

	od = (GF_ObjectDescriptor *) gf_isom_get_root_od(mp4file);
	if (od && (od->tag==GF_ODF_IOD_TAG)) {
		audioPL = ((GF_InitialObjectDescriptor*)od)->audio_profileAndLevel;
		visualPL = ((GF_InitialObjectDescriptor*)od)->visual_profileAndLevel;
	}
	if (od) gf_odf_desc_del((GF_Descriptor *)od);


	//create the OD AU
	bifs = 0;
	odU = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);

	a_esd = v_esd = NULL;

	gf_isom_set_root_od_id(mp4file, 1);

	bifsID = 1;
	odID = 2;
	if (keepESIDs) {
		bifsID = 1;
		while ((bifsID==AID) || (bifsID==VID)) bifsID++;
		odID = 2;
		while ((odID==AID) || (odID==VID) || (odID==bifsID)) odID++;

	}

	VideoTrack = gf_isom_get_track_by_id(mp4file, VID);
	AudioTrack = gf_isom_get_track_by_id(mp4file, AID);

	w = h = 0;
	if (VideoTrack) {
		bifs = 1;
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = ISMA_VIDEO_OD_ID;

		if (!keepESIDs && (VID != ISMA_VIDEO_ES_ID)) {
			gf_isom_set_track_id(mp4file, VideoTrack, ISMA_VIDEO_ES_ID);
		}

		v_esd = gf_isom_get_esd(mp4file, VideoTrack, 1);
		if (v_esd) {
			v_esd->OCRESID = no_ocr ? 0 : bifsID;

			gf_odf_desc_add_desc((GF_Descriptor *)od, (GF_Descriptor *)v_esd);
			gf_list_add(odU->objectDescriptors, od);

			gf_isom_get_track_layout_info(mp4file, VideoTrack, &w, &h, NULL, NULL, NULL);
			if (!w || !h) {
				gf_isom_get_visual_info(mp4file, VideoTrack, 1, &w, &h);
#ifndef GPAC_DISABLE_AV_PARSERS
				if ((v_esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) && (v_esd->decoderConfig->streamType==GF_STREAM_VISUAL)) {
					GF_M4VDecSpecInfo dsi;
					gf_m4v_get_config(v_esd->decoderConfig->decoderSpecificInfo->data, v_esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
					if (!is_image && (!w || !h)) {
						w = dsi.width;
						h = dsi.height;
						gf_isom_set_visual_info(mp4file, VideoTrack, 1, w, h);
						GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[ISMA convert] Adjusting visual track size to %d x %d\n", w, h));
					}
					if (dsi.par_num && (dsi.par_den!=dsi.par_num)) {
						w *= dsi.par_num;
						w /= dsi.par_den;
					}
					if (dsi.VideoPL) visualPL = dsi.VideoPL;
				}
#endif
			}
		}
	}

	if (AudioTrack) {
		od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->objectDescriptorID = ISMA_AUDIO_OD_ID;

		if (!keepESIDs && (AID != ISMA_AUDIO_ES_ID)) {
			gf_isom_set_track_id(mp4file, AudioTrack, ISMA_AUDIO_ES_ID);
		}

		a_esd = gf_isom_get_esd(mp4file, AudioTrack, 1);
		if (a_esd) {
			a_esd->OCRESID = no_ocr ? 0 : bifsID;

			if (!keepESIDs) a_esd->ESID = ISMA_AUDIO_ES_ID;
			gf_odf_desc_add_desc((GF_Descriptor *)od, (GF_Descriptor *)a_esd);
			gf_list_add(odU->objectDescriptors, od);
			if (!bifs) {
				bifs = 3;
			} else {
				bifs = 2;
			}

#ifndef GPAC_DISABLE_AV_PARSERS
			if (a_esd->decoderConfig->objectTypeIndication == GPAC_OTI_AUDIO_AAC_MPEG4) {
				GF_M4ADecSpecInfo cfg;
				gf_m4a_get_config(a_esd->decoderConfig->decoderSpecificInfo->data, a_esd->decoderConfig->decoderSpecificInfo->dataLength, &cfg);
				audioPL = cfg.audioPL;
			}
#endif
		}
	}

	/*update video cfg if needed*/
	if (v_esd) gf_isom_change_mpeg4_description(mp4file, VideoTrack, 1, v_esd);
	if (a_esd) gf_isom_change_mpeg4_description(mp4file, AudioTrack, 1, a_esd);

	/*likely 3GP or other files...*/
	if ((!a_esd && AudioTrack) || (!v_esd && VideoTrack)) return GF_OK;

	//get the OD sample
	codec = gf_odf_codec_new();
	samp = gf_isom_sample_new();
	gf_odf_codec_add_com(codec, (GF_ODCom *)odU);
	gf_odf_codec_encode(codec, 1);
	gf_odf_codec_get_au(codec, &samp->data, &samp->dataLength);
	gf_odf_codec_del(codec);
	samp->CTS_Offset = 0;
	samp->DTS = 0;
	samp->IsRAP = 1;

	/*create the OD track*/
	odT = gf_isom_new_track(mp4file, odID, GF_ISOM_MEDIA_OD, gf_isom_get_timescale(mp4file));
	if (!odT) return gf_isom_last_error(mp4file);

	_esd = gf_odf_desc_esd_new(SLPredef_MP4);
	_esd->decoderConfig->bufferSizeDB = samp->dataLength;
	_esd->decoderConfig->objectTypeIndication = GPAC_OTI_OD_V1;
	_esd->decoderConfig->streamType = GF_STREAM_OD;
	_esd->ESID = odID;
	_esd->OCRESID = no_ocr ? 0 : bifsID;
	gf_isom_new_mpeg4_description(mp4file, odT, _esd, NULL, NULL, &descIndex);
	gf_odf_desc_del((GF_Descriptor *)_esd);
	gf_isom_add_sample(mp4file, odT, 1, samp);
	gf_isom_sample_del(&samp);

	gf_isom_set_track_group(mp4file, odT, 1);

	/*create the BIFS track*/
	bifsT = gf_isom_new_track(mp4file, bifsID, GF_ISOM_MEDIA_SCENE, gf_isom_get_timescale(mp4file));
	if (!bifsT) return gf_isom_last_error(mp4file);

	_esd = gf_odf_desc_esd_new(SLPredef_MP4);
	_esd->decoderConfig->bufferSizeDB = sizeof(ISMA_BIFS_CONFIG);
	_esd->decoderConfig->objectTypeIndication = GPAC_OTI_SCENE_BIFS_V2;
	_esd->decoderConfig->streamType = GF_STREAM_SCENE;
	_esd->ESID = bifsID;
	_esd->OCRESID = 0;

	/*rewrite ISMA BIFS cfg*/
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*empty bifs stuff*/
	gf_bs_write_int(bs, 0, 17);
	/*command stream*/
	gf_bs_write_int(bs, 1, 1);
	/*in pixel metrics*/
	gf_bs_write_int(bs, 1, 1);
	/*with size*/
	gf_bs_write_int(bs, 1, 1);
	gf_bs_write_int(bs, w, 16);
	gf_bs_write_int(bs, h, 16);
	gf_bs_align(bs);
	gf_bs_get_content(bs, &_esd->decoderConfig->decoderSpecificInfo->data, &_esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_isom_new_mpeg4_description(mp4file, bifsT, _esd, NULL, NULL, &descIndex);
	gf_odf_desc_del((GF_Descriptor *)_esd);
	gf_bs_del(bs);
	gf_isom_set_visual_info(mp4file, bifsT, descIndex, w, h);

	samp = gf_isom_sample_new();
	samp->CTS_Offset = 0;
	samp->DTS = 0;
	switch (bifs) {
	case 1:
		if (is_image) {
			samp->data = (char *) ISMA_BIFS_IMAGE;
			samp->dataLength = 10;
		} else {
			samp->data = (char *) ISMA_GF_BIFS_VIDEO;
			samp->dataLength = 11;
		}
		break;
	case 2:
		if (is_image) {
			samp->data = (char *) ISMA_BIFS_AI;
			samp->dataLength = 15;
		} else {
			samp->data = (char *) ISMA_BIFS_AV;
			samp->dataLength = 16;
		}
		break;
	case 3:
		samp->data = (char *) ISMA_BIFS_AUDIO;
		samp->dataLength = 8;
		break;
	}

	samp->IsRAP = 1;

	gf_isom_add_sample(mp4file, bifsT, 1, samp);
	samp->data = NULL;
	gf_isom_sample_del(&samp);
	gf_isom_set_track_group(mp4file, bifsT, 1);

	gf_isom_set_track_enabled(mp4file, bifsT, 1);
	gf_isom_set_track_enabled(mp4file, odT, 1);
	gf_isom_add_track_to_root_od(mp4file, bifsT);
	gf_isom_add_track_to_root_od(mp4file, odT);

	gf_isom_set_pl_indication(mp4file, GF_ISOM_PL_SCENE, 1);
	gf_isom_set_pl_indication(mp4file, GF_ISOM_PL_GRAPHICS, 1);
	gf_isom_set_pl_indication(mp4file, GF_ISOM_PL_OD, 1);
	gf_isom_set_pl_indication(mp4file, GF_ISOM_PL_AUDIO, audioPL);
	gf_isom_set_pl_indication(mp4file, GF_ISOM_PL_VISUAL, (u8) (is_image ? 0xFE : visualPL));

	gf_isom_set_brand_info(mp4file, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_ISOM, 1);
	gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP5, 1);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_media_make_3gpp(GF_ISOFile *mp4file)
{
	u32 Tracks, i, mType, stype, nb_vid, nb_avc, nb_aud, nb_txt, nb_non_mp4, nb_dims;
	Bool is_3g2 = 0;

	switch (gf_isom_get_mode(mp4file)) {
	case GF_ISOM_OPEN_EDIT:
	case GF_ISOM_OPEN_WRITE:
	case GF_ISOM_WRITE_EDIT:
		break;
	default:
		return GF_BAD_PARAM;
	}

	Tracks = gf_isom_get_track_count(mp4file);
	nb_vid = nb_aud = nb_txt = nb_avc = nb_non_mp4 = nb_dims = 0;

	for (i=0; i<Tracks; i++) {
		gf_isom_remove_track_from_root_od(mp4file, i+1);

		mType = gf_isom_get_media_type(mp4file, i+1);
		stype = gf_isom_get_media_subtype(mp4file, i+1, 1);
		switch (mType) {
		case GF_ISOM_MEDIA_VISUAL:
			/*remove image tracks if wanted*/
			if (gf_isom_get_sample_count(mp4file, i+1)<=1) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Visual track ID %d: only one sample found, assuming image and removing track\n", gf_isom_get_track_id(mp4file, i+1) ));
				goto remove_track;
			}

			if (stype == GF_ISOM_SUBTYPE_MPEG4_CRYP) gf_isom_get_ismacryp_info(mp4file, i+1, 1, &stype, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

			switch (stype) {
			case GF_ISOM_SUBTYPE_3GP_H263:
				nb_vid++;
				nb_non_mp4 ++;
				break;
			case GF_ISOM_SUBTYPE_AVC_H264:
			case GF_ISOM_SUBTYPE_AVC2_H264:
			case GF_ISOM_SUBTYPE_SVC_H264:
				nb_vid++;
				nb_avc++;
				break;
			case GF_ISOM_SUBTYPE_MPEG4:
				{
					GF_ESD *esd = gf_isom_get_esd(mp4file, i+1, 1);
					/*both MPEG4-Video and H264/AVC are supported*/
					if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) ) {
						nb_vid++;
					} else {
						GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Video format not supported by 3GP - removing track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
						goto remove_track;
					}
					gf_odf_desc_del((GF_Descriptor *)esd);
				}
				break;
			default:
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Video format not supported by 3GP - removing track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
				goto remove_track;
			}
			break;
		case GF_ISOM_MEDIA_AUDIO:
			if (stype == GF_ISOM_SUBTYPE_MPEG4_CRYP) gf_isom_get_ismacryp_info(mp4file, i+1, 1, &stype, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
			switch (stype) {
			case GF_ISOM_SUBTYPE_3GP_EVRC:
			case GF_ISOM_SUBTYPE_3GP_QCELP:
			case GF_ISOM_SUBTYPE_3GP_SMV:
				is_3g2 = 1;
				nb_aud++;
				break;
			case GF_ISOM_SUBTYPE_3GP_AMR:
			case GF_ISOM_SUBTYPE_3GP_AMR_WB:
				nb_aud++;
				nb_non_mp4 ++;
				break;
			case GF_ISOM_SUBTYPE_MPEG4:
			{
				GF_ESD *esd = gf_isom_get_esd(mp4file, i+1, 1);
				switch (esd->decoderConfig->objectTypeIndication) {
				case GPAC_OTI_AUDIO_13K_VOICE:
				case GPAC_OTI_AUDIO_EVRC_VOICE:
				case GPAC_OTI_AUDIO_SMV_VOICE:
					is_3g2 = 1;
				case GPAC_OTI_AUDIO_AAC_MPEG4:
					nb_aud++;
					break;
				default:
					GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Audio format not supported by 3GP - removing track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
					goto remove_track;
				}
				gf_odf_desc_del((GF_Descriptor *)esd);
			}
				break;
			default:
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Audio format not supported by 3GP - removing track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
				goto remove_track;
			}
			break;

		case GF_ISOM_MEDIA_SUBT:
			gf_isom_set_media_type(mp4file, i+1, GF_ISOM_MEDIA_TEXT);
		case GF_ISOM_MEDIA_TEXT:
			nb_txt++;
			break;

		case GF_ISOM_MEDIA_SCENE:
			if (stype == GF_ISOM_MEDIA_DIMS) {
				nb_dims++;
				break;
			}
		/*clean file*/
		default:
			if (mType==GF_ISOM_MEDIA_HINT) {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Removing Hint track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Removing system track ID %d\n", gf_isom_get_track_id(mp4file, i+1) ));
			}

remove_track:
			gf_isom_remove_track(mp4file, i+1);
			i -= 1;
			Tracks = gf_isom_get_track_count(mp4file);
			break;
		}
	}

	/*no more IOD*/
	gf_isom_remove_root_od(mp4file);

	if (is_3g2) {
		gf_isom_set_brand_info(mp4file, GF_ISOM_BRAND_3G2A, 65536);
		gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP6, 0);
		gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP5, 0);
		gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GG6, 0);
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Setting major brand to 3GPP2\n"));
	} else {
		/*update FType*/
		if ((nb_vid>1) || (nb_aud>1) || (nb_txt>1)) {
			/*3GPP general purpose*/
			gf_isom_set_brand_info(mp4file, GF_ISOM_BRAND_3GG6, 1024);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP6, 0);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP5, 0);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP4, 0);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Setting major brand to 3GPP Generic file\n"));
		}
		/*commented for QT compatibility, although this is correct (qt doesn't understand 3GP6 brand)*/
		else if (nb_txt && 0) {
			gf_isom_set_brand_info(mp4file, GF_ISOM_BRAND_3GP6, 1024);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP5, 1);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP4, 1);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GG6, 0);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Setting major brand to 3GPP V6 file\n"));
		} else if (nb_avc) {
			gf_isom_set_brand_info(mp4file, GF_ISOM_BRAND_3GP6, 0/*1024*/);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_AVC1, 1);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP5, 0);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP4, 0);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Setting major brand to 3GPP V6 file + AVC compatible\n"));
		} else {
			gf_isom_set_brand_info(mp4file, nb_avc ? GF_ISOM_BRAND_3GP6 : GF_ISOM_BRAND_3GP5, 0/*1024*/);
			gf_isom_modify_alternate_brand(mp4file, nb_avc ? GF_ISOM_BRAND_3GP5 : GF_ISOM_BRAND_3GP6, 0);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GP4, 1);
			gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_3GG6, 0);
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[3GPP convert] Setting major brand to 3GPP V5 file\n"));
		}
	}
	/*add/remove MP4 brands and add isom*/
	gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_MP41, (u8) ((nb_avc||is_3g2||nb_non_mp4) ? 0 : 1));
	gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_MP42, (u8) (nb_non_mp4 ? 0 : 1));
	gf_isom_modify_alternate_brand(mp4file, GF_ISOM_BRAND_ISOM, 1);
	return GF_OK;
}

GF_Err gf_media_make_psp(GF_ISOFile *mp4)
{
	u32 i, count;
	u32 nb_a, nb_v;
	/*psp track UUID*/
	bin128 psp_track_uuid = {0x55, 0x53, 0x4D, 0x54, 0x21, 0xD2, 0x4F, 0xCE, 0xBB, 0x88, 0x69, 0x5C, 0xFA, 0xC9, 0xC7, 0x40};
	u8 psp_track_sig [] = {0x00, 0x00, 0x00, 0x1C, 0x4D, 0x54, 0x44, 0x54, 0x00, 0x01, 0x00, 0x12, 0x00, 0x00, 0x00, 0x0A, 0x55, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
	/*psp mov UUID*/
	//bin128 psp_uuid = {0x50, 0x52, 0x4F, 0x46, 0x21, 0xD2, 0x4F, 0xCE, 0xBB, 0x88, 0x69, 0x5C, 0xFA, 0xC9, 0xC7, 0x40};

	nb_a = nb_v = 0;
	count = gf_isom_get_track_count(mp4);
	for (i=0; i<count; i++) {
		switch (gf_isom_get_media_type(mp4, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
			nb_v++;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			nb_a++;
			break;
		}
	}
	if ((nb_v != 1) && (nb_a!=1)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_AUTHOR, ("[PSP convert] Movies need one audio track and one video track\n" ));
		return GF_BAD_PARAM;
	}
	for (i=0; i<count; i++) {
		switch (gf_isom_get_media_type(mp4, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUDIO:
			/*if no edit list, add one*/
			if (!gf_isom_get_edit_segment_count(mp4, i+1)) {
				gf_isom_remove_edit_segments(mp4, i+1);
				gf_isom_append_edit_segment(mp4, i+1, gf_isom_get_track_duration(mp4, i+1), 0, GF_ISOM_EDIT_NORMAL);
			}
			/*add PSP UUID*/
			gf_isom_remove_uuid(mp4, i+1, psp_track_uuid);
			gf_isom_add_uuid(mp4, i+1, psp_track_uuid, (char *) psp_track_sig, 28);
			break;
		default:
			GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[PSP convert] Removing track ID %d\n", gf_isom_get_track_id(mp4, i+1) ));
			gf_isom_remove_track(mp4, i+1);
			i -= 1;
			count -= 1;
			break;
		}
	}
	gf_isom_set_brand_info(mp4, GF_4CC('M','S','N','V'), 0);
	gf_isom_modify_alternate_brand(mp4, GF_4CC('M','S','N','V'), 1);
	return GF_OK;
}

typedef struct
{
	Bool is_ref_track;
	Bool done;
	u32 TrackID;
	u32 SampleNum, SampleCount;
	u32 FragmentLength;
	u32 OriginalTrack;
	u32 finalSampleDescriptionIndex;
	u32 TimeScale, MediaType, DefaultDuration, InitialTSOffset;
	u64 last_sample_cts, next_sample_dts;
	Bool all_sample_raps;
} TrackFragmenter;

static u64 get_next_sap_time(GF_ISOFile *input, u32 track, u32 sample_count, u32 sample_num)
{
	GF_ISOSample *samp;
	u64 time;
	Bool is_rap, has_roll;
	u32 i, found_sample = 0;
	for (i=sample_num; i<=sample_count; i++) {
		if (gf_isom_get_sample_sync(input, track, i)) {
			found_sample = i;
			break;
		}
		gf_isom_get_sample_rap_roll_info(input, track, i, &is_rap, &has_roll, NULL);
		if (is_rap || has_roll) {
			found_sample = i;
			break;
		}
	}
	if (!found_sample) return 0;
	samp = gf_isom_get_sample_info(input, track, found_sample, NULL, NULL);
	time = samp->DTS;
	gf_isom_sample_del(&samp);
	return time;
}


GF_EXPORT
GF_Err gf_media_fragment_file(GF_ISOFile *input, const char *output_file, const char *mpd_name, Double max_duration_sec, u32 dash_mode, Double dash_duration_sec, char *seg_rad_name, char *seg_ext, s32 subsegs_per_sidx, Bool daisy_chain_sidx, Bool use_url_template, Bool single_segment_mode, const char *dash_ctx_file, GF_ISOFile *sample_descs, u32 rep_idx)
{
	u8 NbBits;
	u32 i, TrackNum, descIndex, j, count, nb_sync, ref_track_id, nb_tracks_done;
	u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, nb_samp, nb_done;
	u32 nb_video, nb_audio, nb_text, nb_scene;
	u8 defaultPadding;
	u16 defaultDegradationPriority;
	GF_Err e;
	char sOpt[100], sKey[100];
	char szCodecs[200], szCodec[100];
	u32 cur_seg, fragment_index, nb_fragments, max_sap_type;
	GF_ISOFile *output;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u32 MaxFragmentDuration, MaxSegmentDuration, SegmentDuration, maxFragDurationOverSegment;
	Double average_duration, file_duration, period_duration, max_segment_duration;
	u32 nb_segments, width, height, sample_rate, nb_channels;
	char langCode[5];
	u32 index_start_range, index_end_range;
	Bool force_switch_segment = 0;
	Bool switch_segment = 0;
	Bool split_seg_at_rap = (dash_mode==2) ? 1 : 0;
	Bool split_at_rap = 0;
	Bool has_rap = 0;
	Bool next_sample_rap = 0;
	Bool flush_all_samples = 0;
	Bool simulation_pass = 0;
	u64 last_ref_cts = 0;
	u64 start_range, end_range, file_size, init_seg_size, ref_track_first_dts, ref_track_next_cts;
	u32 tfref_timescale = 0;
	u32 bandwidth = 0;
	TrackFragmenter *tf, *tfref;
	FILE *mpd = NULL;
	FILE *mpd_segs = NULL;
	char *SegName = NULL;
	const char *opt;
	GF_Config *dash_ctx = NULL;
	Bool store_dash_params = 0;
	Bool dash_moov_setup = 0;
	Bool segments_start_with_sap = 1;
	Bool first_sample_in_segment = 0;
	u32 *segments_info = NULL;
	u32 nb_segments_info = 0;
	Bool audio_only = 1;


	SegmentDuration = 0;
	nb_samp = 0;
	fragmenters = NULL;
	if (!seg_ext) seg_ext = "m4s";

	//create output file
	if (dash_mode) {
		u32 len;
		
		if (dash_ctx_file) {
			dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
			if (!dash_ctx) {
				FILE *t = fopen(dash_ctx_file, "wt");
				if (t) fclose(t);
				dash_ctx = gf_cfg_new(NULL, dash_ctx_file);
				
				if (dash_ctx) store_dash_params=1;
			}
		}
		len = strlen(output_file);
		len += 100;
		SegName = gf_malloc(sizeof(char)*len);
		if (!SegName) return GF_OUT_OF_MEM;

		opt = dash_ctx ? gf_cfg_get_key(dash_ctx, "DASH", "InitializationSegment") : NULL;
		if (opt) {
			output = gf_isom_open(opt, GF_ISOM_OPEN_CAT_FRAGMENTS, NULL);
			dash_moov_setup = 1;
		} else {
			strcpy(SegName, output_file);
			strcat(SegName, ".mp4");
			output = gf_isom_open(SegName, GF_ISOM_OPEN_WRITE, NULL);
		}
		if (!output) {
			e = gf_isom_last_error(NULL);
			goto err_exit;
		}

		if (store_dash_params) {
			gf_cfg_set_key(dash_ctx, "DASH", "InitializationSegment", SegName);
		}
		mpd = gf_f64_open(mpd_name, "a+t");
		mpd_segs = gf_temp_file_new();
	} else {
		output = gf_isom_open(output_file, GF_ISOM_OPEN_WRITE, NULL);
		if (!output) return gf_isom_last_error(NULL);
	}
	
	nb_sync = 0;
	nb_samp = 0;
	fragmenters = gf_list_new();

	if (! dash_moov_setup) {
		e = gf_isom_clone_movie(input, output, 0, 0);
		if (e) goto err_exit;
	}

	MaxFragmentDuration = (u32) (max_duration_sec * 1000);	
	MaxSegmentDuration = (u32) (dash_duration_sec * 1000);

	/*in single segment mode, only one big SIDX is written between the end of the moov and the first fragment. 
	To speed-up writing, we do a first fragmentation pass without writing any sample to compute the number of segments and fragments per segment
	in order to allocate / write to file the sidx before the fragmentation. The sidx will then be rewritten when closing the last segment*/
	if (single_segment_mode) simulation_pass = 1;
	index_start_range = index_end_range = 0;	

	tfref = NULL;
	file_duration = 0;
	width = height = sample_rate = nb_channels = 0;
	langCode[0]=0;
	langCode[4]=0;
	szCodecs[0] = 0;

	nb_video = nb_audio = nb_text = nb_scene = 0;
	//duplicates all tracks
	for (i=0; i<gf_isom_get_track_count(input); i++) {
		u32 _w, _h, _sr, _nb_ch;

		u32 mtype = gf_isom_get_media_type(input, i+1);
		if (mtype == GF_ISOM_MEDIA_HINT) continue;

		if (! dash_moov_setup) {
			e = gf_isom_clone_track(input, i+1, output, 0, &TrackNum);
			if (e) goto err_exit;

			if (gf_isom_is_track_in_root_od(input, i+1)) gf_isom_add_track_to_root_od(output, TrackNum);

			//if only one sample, don't fragment track
			count = gf_isom_get_sample_count(input, i+1);
			if (count==1) {
				sample = gf_isom_get_sample(input, i+1, 1, &descIndex);
				e = gf_isom_add_sample(output, TrackNum, 1, sample);
				gf_isom_sample_del(&sample);
				if (e) goto err_exit;

				continue;
			}
		} else {
			TrackNum = gf_isom_get_track_by_id(output, gf_isom_get_track_id(input, i+1));
			count = gf_isom_get_sample_count(input, i+1);
		}

		if (mtype == GF_ISOM_MEDIA_VISUAL) nb_video++;
		else if (mtype == GF_ISOM_MEDIA_AUDIO) nb_audio++;
		else if (mtype == GF_ISOM_MEDIA_TEXT) nb_text++;
		else if (mtype == GF_ISOM_MEDIA_SCENE) nb_scene++;
		else if (mtype == GF_ISOM_MEDIA_DIMS) nb_scene++;

		if (mtype != GF_ISOM_MEDIA_AUDIO) audio_only = 0;

		//setup fragmenters
		if (! dash_moov_setup) {
			gf_isom_get_fragment_defaults(input, i+1,
									 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);

			//new initialization segment, setup fragmentation
			e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum),
						defaultDescriptionIndex, defaultDuration,
						defaultSize, (u8) defaultRandomAccess,
						defaultPadding, defaultDegradationPriority);
			if (e) goto err_exit;

		} else {
			gf_isom_get_fragment_defaults(output, TrackNum,
									 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
		}

		gf_media_get_rfc_6381_codec_name(output, TrackNum, szCodec);
		if (strlen(szCodecs)) strcat(szCodecs, ",");
		strcat(szCodecs, szCodec);

		GF_SAFEALLOC(tf, TrackFragmenter);
		tf->TrackID = gf_isom_get_track_id(output, TrackNum);
		tf->SampleCount = gf_isom_get_sample_count(input, i+1);
		tf->OriginalTrack = i+1;
		tf->TimeScale = gf_isom_get_media_timescale(input, i+1);
		tf->MediaType = gf_isom_get_media_type(input, i+1);
		tf->DefaultDuration = defaultDuration;

		if (gf_isom_get_sync_point_count(input, i+1)>nb_sync) { 
			tfref = tf;
			nb_sync = gf_isom_get_sync_point_count(input, i+1);
		} else if (!gf_isom_has_sync_points(input, i+1)) {
			tf->all_sample_raps = 1;
		}

		tf->finalSampleDescriptionIndex = 1;

		/*figure out if we have an initial TS*/
		if (!dash_moov_setup) {
			if (gf_isom_get_edit_segment_count(input, i+1)) {
				u64 EditTime, SegmentDuration, MediaTime;
				u8 EditMode;
				gf_isom_get_edit_segment(input, i+1, 1, &EditTime, &SegmentDuration, &MediaTime, &EditMode);
				if (EditMode==GF_ISOM_EDIT_EMPTY) {
					tf->InitialTSOffset = (u32) (SegmentDuration * tf->TimeScale / gf_isom_get_timescale(input));
				}
				/*and remove edit segments*/
				gf_isom_remove_edit_segments(output, TrackNum);
			}
			/*locate sample description in list if given*/
			if (sample_descs) {
				u32 s_count;
				u32 sample_descs_track = gf_isom_get_track_by_id(sample_descs, tf->TrackID);
				if (!sample_descs_track) {
					e = GF_BAD_PARAM;
					goto err_exit;

				}

				//the initialization segment is not yet setup for fragmentation
				if (! gf_isom_is_track_fragmented(sample_descs, tf->TrackID)) {
					e = gf_isom_setup_track_fragment(sample_descs, sample_descs_track,
								defaultDescriptionIndex, defaultDuration,
								defaultSize, (u8) defaultRandomAccess,
								defaultPadding, defaultDegradationPriority);
					if (e) goto err_exit;
				}
				/*otherwise override the fragment defauls so that we are consistent with the shared init segment*/
				else {
					e = gf_isom_get_fragment_defaults(sample_descs, sample_descs_track,
											 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
					if (e) goto err_exit;

					e = gf_isom_change_track_fragment_defaults(output, TrackNum,
											 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, defaultPadding, defaultDegradationPriority);
					if (e) goto err_exit;


				}

				/*reset all sample desc and clone with new ones*/
				gf_isom_clone_sample_descriptions(output, TrackNum, sample_descs, sample_descs_track, 1);

				/*and search in new ones the new index*/
				s_count = gf_isom_get_sample_description_count(sample_descs, sample_descs_track); 
				if (s_count>1) {
					u32 k;
					/*remove all sample descs*/
					for (k=0; k<s_count; k++) {
						/*search in original file if we have the sample desc*/
						if (gf_isom_is_same_sample_description(input, TrackNum, 1, sample_descs, sample_descs_track, k+1)) {
							tf->finalSampleDescriptionIndex = k+1;
						}
					}
				}
			}
		}
		/*restore track decode times*/
		else {
			char *opt, sKey[100];
			sprintf(sKey, "TrackID_%d", tf->TrackID);
			opt = (char *)gf_cfg_get_key(dash_ctx, sKey, "NextDecodingTime");
			if (opt) tf->InitialTSOffset = atoi(opt);
		}
	
		switch (mtype) {
		case GF_ISOM_MEDIA_TEXT:
			gf_isom_get_media_language(input, i+1, langCode);
		case GF_ISOM_MEDIA_VISUAL:
			if (!tfref && (count >=10)) {
				tfref = tf;
			}
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_DIMS:
			gf_isom_get_track_layout_info(input, i+1, &_w, &_h, NULL, NULL, NULL);
			if (_w>width) width = _w;
			if (_h>height) height = _h;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			gf_isom_get_audio_info(input, i+1, 1, &_sr, &_nb_ch, NULL);
			if (_sr>sample_rate) sample_rate=_sr;
			if (_nb_ch>nb_channels) nb_channels = _nb_ch;
			gf_isom_get_media_language(input, i+1, langCode);
			break;
		}

		if (file_duration < ((Double) gf_isom_get_media_duration(input, i+1)) / tf->TimeScale ) {
			file_duration = ((Double) gf_isom_get_media_duration(input, i+1)) / tf->TimeScale;
		}
		gf_list_add(fragmenters, tf);

		nb_samp += count;
	}

	if (!tfref) tfref = gf_list_get(fragmenters, 0);
	tfref->is_ref_track = 1;
	tfref_timescale = tfref->TimeScale;
	ref_track_id = tfref->TrackID;
	if (tfref->all_sample_raps) split_seg_at_rap = 1;

	//flush movie
	e = gf_isom_finalize_for_fragment(output, dash_mode ? 1 : 0);
	if (e) goto err_exit;

	start_range = 0;
	file_size = gf_isom_get_file_size(output);
	end_range = file_size - 1;
	init_seg_size = file_size;

	if (dash_ctx) {
		if (store_dash_params) {
			char szVal[1024];
			sprintf(szVal, LLU, init_seg_size);
			gf_cfg_set_key(dash_ctx, "DASH", "InitializationSegmentSize", szVal);
		} else {
			const char *opt = gf_cfg_get_key(dash_ctx, "DASH", "InitializationSegmentSize");
			if (opt) init_seg_size = atoi(opt);
		}
	}

restart_fragmentation_pass:

	average_duration = 0;
	nb_segments = 0;

	nb_tracks_done = 0;
	ref_track_first_dts = (u64) -1;
	nb_done = 0;

	maxFragDurationOverSegment=0;
	if (dash_mode) switch_segment=1;

	if (!seg_rad_name) use_url_template = 0;

	cur_seg=1;
	fragment_index=1;
	period_duration = 0;
	split_at_rap = 0;
	has_rap = 0;
	next_sample_rap = 0;
	flush_all_samples = 0;
	force_switch_segment = 0;
	max_sap_type = 0;
	ref_track_next_cts = 0;

	/*setup previous URL list*/
	if (dash_ctx) {
		const char *opt;
		count = gf_cfg_get_key_count(dash_ctx, "URLs");
		for (i=0; i<count; i++) {
			const char *key_name = gf_cfg_get_key_name(dash_ctx, "URLs", i);
			opt = gf_cfg_get_key(dash_ctx, "URLs", key_name);
			fprintf(mpd_segs, "    %s\n", opt);	
		}

		opt = gf_cfg_get_key(dash_ctx, "DASH", "NextSegmentIndex");
		if (opt) cur_seg = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, "DASH", "NextFragmentIndex");
		if (opt) fragment_index = atoi(opt);
		opt = gf_cfg_get_key(dash_ctx, "DASH", "PeriodDuration");
		if (opt) period_duration = atof(opt);
	}
	gf_isom_set_next_moof_number(output, fragment_index);


	max_segment_duration = 0;
	nb_fragments=0;

	while ( (count = gf_list_count(fragmenters)) ) {

		if (switch_segment) {
			SegmentDuration = 0;
			switch_segment = 0;
			first_sample_in_segment = 1;

			if (simulation_pass) {
				segments_info = gf_realloc(segments_info, sizeof(u32) * (nb_segments_info+1) );
				segments_info[nb_segments_info] = 0;
				nb_segments_info++;
				e = GF_OK;
			} else {
				start_range = gf_isom_get_file_size(output);
				if (seg_rad_name) {
					sprintf(SegName, "%s%d.%s", seg_rad_name, cur_seg, seg_ext);
					e = gf_isom_start_segment(output, SegName);
					if (!use_url_template) {
						fprintf(mpd_segs, "     <SegmentURL media=\"%s\"/>\n", SegName);	
						if (dash_ctx) {
							char szKey[100], szVal[4046];
							sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
							sprintf(szVal, "<SegmentURL sourceURL=\"%s\"/>", SegName);
							gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
						}
					}
				} else {
					e = gf_isom_start_segment(output, NULL);
				}
			}

			cur_seg++;
			if (e) goto err_exit;
		} 

		maxFragDurationOverSegment=0;

		sample = NULL;

		if (simulation_pass) {
			segments_info[nb_segments_info-1] ++;
			e = GF_OK;
		} else {
			e = gf_isom_start_fragment(output, 1);
			if (e) goto err_exit;
	

			for (i=0; i<count; i++) {
				tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf->done) continue;
				gf_isom_set_traf_base_media_decode_time(output, tf->TrackID, tf->InitialTSOffset + tf->next_sample_dts);
			}
		
		}

		//process track by track
		for (i=0; i<count; i++) {
			Bool has_roll, is_rap;
			s32 roll_distance;
			u32 SAP_type = 0;
			/*start with ref*/
			if (tfref && split_seg_at_rap ) {
				if (i==0) {
					tf = tfref;
					has_rap=0;
				} else {
					tf = (TrackFragmenter *)gf_list_get(fragmenters, i-1);
					if (tf == tfref) {
						tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
					}
				}
			} else {
				tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
				if (tf == tfref) 
					has_rap=0;
			} 
			if (tf->done) continue;

			//ok write samples
			while (1) {
				Bool stop_frag = 0;

				/*first sample*/
				if (!sample) {
					sample = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 1, &descIndex);
					if (!sample) {
						e = gf_isom_last_error(input);
						goto err_exit;
					}

					/*also get SAP type - this is not needed if sample is not NULL as SAP tye was computed for "next sample" in previous loop*/
					if (sample->IsRAP) {
						SAP_type = 1;
					} else {
						SAP_type = 0;
						e = gf_isom_get_sample_rap_roll_info(input, tf->OriginalTrack, tf->SampleNum + 1, &is_rap, &has_roll, &roll_distance);
						if (e==GF_OK) {
							if (is_rap) SAP_type = 3;
							else if (has_roll && (roll_distance>=0) ) SAP_type = 4;
						}
					}
				}
				gf_isom_get_sample_padding_bits(input, tf->OriginalTrack, tf->SampleNum+1, &NbBits);

				next = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 2, &j);
				if (next) {
					defaultDuration = (u32) (next->DTS - sample->DTS);
				} else {
					defaultDuration = tf->DefaultDuration;
				}

				if (tf==tfref) {
					if (segments_start_with_sap && first_sample_in_segment ) {
						first_sample_in_segment = 0;
						if (! SAP_type) segments_start_with_sap = 0;
					}
					if (ref_track_first_dts > sample->DTS) 
						ref_track_first_dts = sample->DTS;

					if (next) {
						u64 next_cts = next->DTS + next->CTS_Offset;
						if (ref_track_next_cts<next_cts) {
							ref_track_next_cts = next_cts;
						}
					} else {
						u64 cts = gf_isom_get_media_duration(input, tf->OriginalTrack);
						if (cts>ref_track_next_cts) ref_track_next_cts = cts;
						else ref_track_next_cts += defaultDuration;
					}
				}

				if (SAP_type > max_sap_type) max_sap_type = SAP_type;

				if (simulation_pass) {
					e = GF_OK;
				} else {
					/*override descIndex with final index used in file*/
					descIndex = tf->finalSampleDescriptionIndex;
					e = gf_isom_fragment_add_sample(output, tf->TrackID, sample, descIndex,
									 defaultDuration, NbBits, 0);
					if (e) 
						goto err_exit;

					/*copy subsample information*/
					e = gf_isom_fragment_copy_subsample(output, tf->TrackID, input, tf->OriginalTrack, tf->SampleNum + 1);
					if (e) 
						goto err_exit;

					gf_set_progress("ISO File Fragmenting", nb_done, nb_samp);
					nb_done++;
				}

				tf->last_sample_cts = sample->DTS + sample->CTS_Offset;
				tf->next_sample_dts = sample->DTS + defaultDuration;

				gf_isom_sample_del(&sample);
				sample = next;
				tf->FragmentLength += defaultDuration;
				tf->SampleNum += 1;

				/*compute SAP type*/
				if (sample) {
					if (sample->IsRAP) {
						SAP_type = 1;
					} else {
						SAP_type = 0;
						e = gf_isom_get_sample_rap_roll_info(input, tf->OriginalTrack, tf->SampleNum + 1, &is_rap, &has_roll, NULL);
						if (e==GF_OK) {
							if (is_rap)
								SAP_type = 3;
							else if (has_roll && (roll_distance>=0) )
								SAP_type = 4;
						}
					}
				}

				if (next && SAP_type) {
					if (tf==tfref) {
						if (split_seg_at_rap) {
							u64 next_sap_time;
							/*duration of fragment if we add this rap*/
							u32 frag_dur = (tf->FragmentLength+defaultDuration)*1000/tf->TimeScale;
							next_sample_rap = 1;
							next_sap_time = get_next_sap_time(input, tf->OriginalTrack, tf->SampleCount, tf->SampleNum + 2);
							/*if no more SAP after this one, do not switch segment*/
							if (next_sap_time) {
								u32 scaler;
								/*this is the fragment duration from last sample added to next SAP*/
								frag_dur += (u32) (next_sap_time - tf->next_sample_dts - defaultDuration)*1000/tf->TimeScale;
								/*if media segment about to be produced is longer than max segment length, force segment split*/
								if (SegmentDuration + frag_dur > MaxSegmentDuration) {
									split_at_rap = 1;
									/*force new segment*/
									force_switch_segment = 1;
								}

								/*if adding this SAP will result in stoping the fragment "soon" after it, stop now and start with SAP
								if all samples are RAPs, just stop fragment if we exceed the requested duration by adding the next sample
								otherwise, take 3 samples (should be refined of course)*/
								scaler = 3;
								if (tf->all_sample_raps) scaler = 1;
								if ( (tf->FragmentLength + scaler*defaultDuration)*1000 >= MaxFragmentDuration * tf->TimeScale)
									stop_frag = 1;
							}
							if (split_at_rap && !tf->all_sample_raps) {
								stop_frag = 1;
								/*override fragment duration for the rest of this fragment*/
								MaxFragmentDuration = tf->FragmentLength*1000/tf->TimeScale;
							}
						} else if (!has_rap) {
							if (tf->FragmentLength == defaultDuration) has_rap = 2;
							else has_rap = 1;
						}
					} 
				} else {
					next_sample_rap = 0;
				}

				if (tf->SampleNum==tf->SampleCount) {
					stop_frag = 1;
				} else if (tf==tfref) {
					/*fragmenting on "clock" track: no drift control*/
                    if (tf->FragmentLength*1000 >= MaxFragmentDuration*tf->TimeScale)
						stop_frag = 1;
				}
				/*do not abort fragment if ref track is done, put everything in the last fragment*/
				else if (!flush_all_samples) {
					/*fragmenting on "non-clock" track: drift control*/
					if (tf->last_sample_cts * tfref_timescale >= last_ref_cts * tf->TimeScale)
						stop_frag = 1;
				}

				if (stop_frag) {
					gf_isom_sample_del(&next);
					sample = next = NULL;
					if (maxFragDurationOverSegment<=tf->FragmentLength*1000/tf->TimeScale) {
						maxFragDurationOverSegment = tf->FragmentLength*1000/tf->TimeScale;
					}
					tf->FragmentLength = 0;

					if (tf==tfref) last_ref_cts = tf->last_sample_cts;

					break;
				}
			}

			if (tf->SampleNum==tf->SampleCount) {
				tf->done = 1;
				nb_tracks_done++;
				if (tf == tfref) {
					tfref = NULL;
					flush_all_samples = 1;
				}
			}
		}

		if (dash_mode) {

			SegmentDuration += maxFragDurationOverSegment;
			maxFragDurationOverSegment=0;

			/*next fragment will exceed segment length, abort fragment now (all samples RAPs)*/
			if (tfref && tfref->all_sample_raps && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				force_switch_segment = 1;
			}

			if (force_switch_segment || ((SegmentDuration >= MaxSegmentDuration) && (!split_seg_at_rap || next_sample_rap))) {
				average_duration += SegmentDuration;
				nb_segments++;
				if (max_segment_duration * 1000 <= SegmentDuration) {
					max_segment_duration = SegmentDuration;
					max_segment_duration /= 1000;
				}


#if 0
				if (split_seg_at_rap) has_rap = 0;
				fprintf(stdout, "Seg#%d: Duration %d%s - Track times (ms): ", nb_segments, SegmentDuration, (has_rap == 2) ? " - Starts with RAP" : ((has_rap == 1) ? " - Contains RAP" : "") );
				for (i=0; i<count; i++) {
					tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
					fprintf(stdout, "%d ", (u32) ( (tf->last_sample_cts*1000.0)/tf->TimeScale) );
				}
				fprintf(stdout, "\n ");
#endif
				force_switch_segment=0;
				switch_segment=1;
				SegmentDuration=0;
				split_at_rap = 0;
				has_rap = 0;
				/*restore fragment duration*/
				MaxFragmentDuration = (u32) (max_duration_sec * 1000);

				if (!simulation_pass) {
					u64 idx_start_range, idx_end_range;
					
					gf_isom_close_segment(output, subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, daisy_chain_sidx, flush_all_samples ? 1 : 0, &idx_start_range, &idx_end_range);
					ref_track_first_dts = (u64) -1;

					if (!seg_rad_name) {
						file_size = gf_isom_get_file_size(output);
						end_range = file_size - 1;
						if (!single_segment_mode) {
							fprintf(mpd_segs, "     <SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>\n", start_range, end_range, idx_start_range, idx_end_range);	
							if (dash_ctx) {
								char szKey[100], szVal[4046];
								sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
								sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
								gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
							}
						}
					} else {
						file_size += gf_isom_get_file_size(output);
					}
				}
			} 
			/*next fragment will exceed segment length, abort fragment at next rap (possibly after MaxSegmentDuration)*/
			if (split_seg_at_rap && SegmentDuration && (SegmentDuration + MaxFragmentDuration >= MaxSegmentDuration)) {
				split_at_rap = 1;
			}
		}
		
		if (nb_tracks_done==count) break;
	}

	if (simulation_pass) {
		/*OK, we have all segments and frags per segments*/
		gf_isom_allocate_sidx(output, subsegs_per_sidx, daisy_chain_sidx, nb_segments_info, segments_info, &index_start_range, &index_end_range );
		gf_free(segments_info);
		segments_info = NULL;
		simulation_pass = 0;
		/*reset fragmenters*/
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = gf_list_get(fragmenters, i);
			tf->done = 0;
			tf->last_sample_cts = 0;
			tf->next_sample_dts = 0;
			tf->FragmentLength = 0;
			tf->SampleNum = 0;
			if (tf->is_ref_track) tfref = tf;
		}
		goto restart_fragmentation_pass;
	}

	if (dash_mode) {
		char buffer[1000];
		u32 h, m;
		Double s;

		/*flush last segment*/
		if (!switch_segment) {
			u64 idx_start_range, idx_end_range;

			if (max_segment_duration * 1000 <= SegmentDuration) {
				max_segment_duration = SegmentDuration;
				max_segment_duration /= 1000;
			}

			gf_isom_close_segment(output, subsegs_per_sidx, ref_track_id, ref_track_first_dts, ref_track_next_cts, daisy_chain_sidx, 1, &idx_start_range, &idx_end_range);
			nb_segments++;

			if (!seg_rad_name) {
				file_size = gf_isom_get_file_size(output);
				end_range = file_size - 1;
				if (!single_segment_mode) {
					fprintf(mpd_segs, "     <SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>\n", start_range, end_range, idx_start_range, idx_end_range);	
					if (dash_ctx) {
						char szKey[100], szVal[4046];
						sprintf(szKey, "UrlInfo%d", gf_cfg_get_key_count(dash_ctx, "URLs") + 1 );
						sprintf(szVal, "<SegmentURL mediaRange=\""LLD"-"LLD"\" indexRange=\""LLD"-"LLD"\"/>", start_range, end_range, idx_start_range, idx_end_range);
						gf_cfg_set_key(dash_ctx, "URLs", szKey, szVal);
					}
				}
			} else {
				file_size += gf_isom_get_file_size(output);
			}
		}

		period_duration += file_duration;
		h = (u32) (period_duration/3600);
		m = (u32) (period_duration-h*60)/60;
		s = period_duration - h*3600 - m*60;
		bandwidth = (u32) (file_size * 8 / file_duration);

		fprintf(mpd, "   <Representation id=\"%d\" mimeType=\"%s/mp4\" codecs=\"%s\"", rep_idx ? rep_idx : 1, audio_only ? "audio" : "video", szCodecs);
		if (width && height) fprintf(mpd, " width=\"%d\" height=\"%d\"", width, height);
		if (sample_rate && nb_channels) fprintf(mpd, " sampleRate=\"%d\" numChannels=\"%d\"", sample_rate, nb_channels);
		if (langCode[0]) fprintf(mpd, " lang=\"%s\"", langCode);
		if (segments_start_with_sap || split_seg_at_rap) {
			fprintf(mpd, " startWithSAP=\"%d\"", max_sap_type);
		} else {
			fprintf(mpd, " startWithSAP=\"false\"");
		}
		if (single_segment_mode && segments_start_with_sap) fprintf(mpd, " subsegmentStartsWithSAP=\"true\"");
		
		fprintf(mpd, " bandwidth=\"%d\"", bandwidth);		
		fprintf(mpd, ">\n");

		if (dash_ctx) {
			Double seg_dur;
			opt = gf_cfg_get_key(dash_ctx, "DASH", "MaxSegmentDuration");
			if (opt) {
				seg_dur = atof(opt);
				if (seg_dur < max_segment_duration) {
					sprintf(sOpt, "%f", max_segment_duration);
					gf_cfg_set_key(dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
					seg_dur = max_segment_duration;
				} else {
					max_segment_duration = seg_dur;
				}
			} else {
				sprintf(sOpt, "%f", max_segment_duration);
				gf_cfg_set_key(dash_ctx, "DASH", "MaxSegmentDuration", sOpt);
			}
		}

		if (use_url_template) {
			sprintf(SegName, "%s$Number$.%s", seg_rad_name, seg_ext);
			fprintf(mpd, "    <SegmentTemplate timescale=\"1000\" duration=\"%d\" media=\"%s\" startNumber=\"1\"", (u32) (max_segment_duration*1000), SegName);	
			if (!sample_descs) fprintf(mpd, " initialization=\"%s.mp4\"", output_file);	
			fprintf(mpd, "/>\n");
		} else if (single_segment_mode) {
			fprintf(mpd, "    <BaseURL>%s</BaseURL>\n", gf_isom_get_filename(output) );	
			fprintf(mpd, "    <SegmentBase indexRangeExact=\"true\" indexRange=\"%d-%d\"/>\n", index_start_range, index_end_range);	
		} else {
			if (!seg_rad_name) {
				fprintf(mpd, "    <BaseURL>%s.mp4</BaseURL>\n", output_file );	
			}
			fprintf(mpd, "    <SegmentList timescale=\"1000\" duration=\"%d\">\n", (u32) (max_segment_duration*1000));	
			//if (!sample_descs) 
			{
				fprintf(mpd, "     <Initialization");	
				if (!seg_rad_name) {
					fprintf(mpd, " range=\"0-"LLD"\"", init_seg_size-1);
				} else {
					fprintf(mpd, " sourceURL=\"%s.mp4\"", output_file);
				}
				fprintf(mpd, "/>\n");
			}
		}


		gf_f64_seek(mpd_segs, 0, SEEK_SET);
		while (!feof(mpd_segs)) {
			u32 r = fread(buffer, 1, 100, mpd_segs);
			gf_fwrite(buffer, 1, r, mpd);
		}

		if (!use_url_template && !single_segment_mode) {
			fprintf(mpd, "    </SegmentList>\n");
		}

		fprintf(mpd, "   </Representation>\n");
	}

	/*store context*/
	if (dash_ctx) {
		for (i=0; i<gf_list_count(fragmenters); i++) {
			tf = (TrackFragmenter *)gf_list_get(fragmenters, i);
			
			sprintf(sKey, "TrackID_%d", tf->TrackID);
			sprintf(sOpt, LLU, tf->InitialTSOffset + tf->next_sample_dts);
			gf_cfg_set_key(dash_ctx, sKey, "NextDecodingTime", sOpt);
		}
		sprintf(sOpt, "%d", cur_seg);
		gf_cfg_set_key(dash_ctx, "DASH", "NextSegmentIndex", sOpt);

		fragment_index = gf_isom_get_next_moof_number(output);
		sprintf(sOpt, "%d", fragment_index);
		gf_cfg_set_key(dash_ctx, "DASH", "NextFragmentIndex", sOpt);
		sprintf(sOpt, "%f", period_duration);
		gf_cfg_set_key(dash_ctx, "DASH", "PeriodDuration", sOpt);
	}

err_exit:
	if (fragmenters){
		while (gf_list_count(fragmenters)) {
			tf = (TrackFragmenter *)gf_list_get(fragmenters, 0);
			gf_free(tf);
			gf_list_rem(fragmenters, 0);
		}
		gf_list_del(fragmenters);
	}
	if (e) gf_isom_delete(output);
	else gf_isom_close(output);
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	if (SegName) gf_free(SegName);
	if (mpd) fclose(mpd);
	if (mpd_segs) fclose(mpd_segs);
	if (dash_ctx) gf_cfg_del(dash_ctx);
	return e;
}


GF_Err gf_media_mpd_start(char *mpd_name, char *title, Bool use_url_template, Bool single_segment, char *dash_ctx, GF_ISOFile *init_segment, Double period_duration)
{
	u32 h, m;
	Double s;
	FILE *mpd = fopen(mpd_name, "wt");
	if (!mpd) return GF_IO_ERR;

	h = (u32) (period_duration/3600);
	m = (u32) (period_duration-h*60)/60;
	s = period_duration - h*3600 - m*60;

	/*TODO what should we put for minBufferTime */
	fprintf(mpd, "<MPD type=\"static\" xmlns=\"urn:mpeg:DASH:schema:MPD:2011\" profiles=\"%s\" minBufferTime=\"PT1.5S\" mediaPresentationDuration=\"PT%dH%dM%.2fS\">\n", 
		single_segment ? "urn:mpeg:dash:profile:isoff-on-demand:2011" : "urn:mpeg:dash:profile:full:2011",
		h, m, s);
    fprintf(mpd, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">\n");
	if (title)
		fprintf(mpd, "  <Title>Media Presentation Description for file %s generated with GPAC </Title>\n", title);
    fprintf(mpd, " </ProgramInformation>\n");
    fprintf(mpd, " <Period start=\"PT0S\" duration=\"PT%dH%dM%.2fS\">\n", h, m, s);	
	fprintf(mpd, "  <AdaptationSet>\n");

	if (init_segment) {
		u32 i;
		char langCode[4];
		langCode[3] = 0;

		for (i=0; i<gf_isom_get_track_count(init_segment); i++) {
			u32 trackID = gf_isom_get_track_id(init_segment, i+1);

			gf_isom_get_media_language(init_segment, i+1, langCode);

			switch (gf_isom_get_media_type(init_segment, i+1) ) {
			case GF_ISOM_MEDIA_TEXT:
				fprintf(mpd, "    <ContentComponent id=\"%d\" contentType=\"text\" lang=\"%s\"/>\n", trackID, langCode);
				break;
			case GF_ISOM_MEDIA_VISUAL:
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"video\"/>\n", trackID);
				break;
			case GF_ISOM_MEDIA_SCENE:
			case GF_ISOM_MEDIA_DIMS:
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"application\" lang=\"%s\"/>\n", trackID, langCode);
				break;
			case GF_ISOM_MEDIA_AUDIO:
				fprintf(mpd, "   <ContentComponent id=\"%d\" contentType=\"audio\" lang=\"%s\"/>\n", trackID, langCode);
				break;
			}
		}

		if (use_url_template) {
			fprintf(mpd, "   <SegmentTemplate initialization=\"%s\"/>\n", gf_isom_get_filename(init_segment) );	
		} else if (0 && !single_segment) {
			fprintf(mpd, "   <SegmentList>\n");	
			fprintf(mpd, "    <Initialization sourceURL=\"%s\"/>\n", gf_isom_get_filename(init_segment) );	
			fprintf(mpd, "   </SegmentList>\n");	
		}
	}

	fclose(mpd);
	return GF_OK;
}

GF_Err gf_media_mpd_end(char *mpd_name)
{
	FILE *mpd = fopen(mpd_name, "a+t");
	if (!mpd_name) return GF_IO_ERR;

    fprintf(mpd, "  </AdaptationSet>\n");
    fprintf(mpd, " </Period>\n");
    fprintf(mpd, "</MPD>");

	fclose(mpd);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_media_import_chapters(GF_ISOFile *file, char *chap_file, Double import_fps)
{
	int readen=0;
	GF_Err e;
	u32 state, offset;
	u32 cur_chap;
	u64 ts;
	u32 i, h, m, s, ms, fr, fps;
	char line[1024];
	char szTitle[1024];
	FILE *f = gf_f64_open(chap_file, "rt");
	if (!f) return GF_URL_ERROR;

	readen = fread(line, 1, 4, f);
	if (readen < 4){
		e = GF_URL_ERROR;
		goto err_exit;
	}
	offset = 0;
	if ((line[0]==(char)(0xFF)) && (line[1]==(char)(0xFE))) {
		if (!line[2] && !line[3]){
			e = GF_NOT_SUPPORTED;
			goto err_exit;
		}
		offset = 2;
	} else if ((line[0]==(char)(0xFE)) && (line[1]==(char)(0xFF))) {
		if (!line[2] && !line[3]){
			e = GF_NOT_SUPPORTED;
			goto err_exit;
		}
		offset = 2;
	} else if ((line[0]==(char)(0xEF)) && (line[1]==(char)(0xBB)) && (line[2]==(char)(0xBF))) {
		/*we handle UTF8 as asci*/
		offset = 3;
	} else {
		offset = 0;
	}
	gf_f64_seek(f, offset, SEEK_SET);

	e = gf_isom_remove_chapter(file, 0, 0);
	if (e) goto err_exit;

	/*try to figure out the frame rate*/
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		GF_ISOSample *samp;
		u32 ts, inc;
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_VISUAL) continue;
		if (gf_isom_get_sample_count(file, i+1) < 20) continue;
		samp = gf_isom_get_sample_info(file, 1, 2, NULL, NULL);
		inc = (u32) samp->DTS;
		if (!inc) inc=1;
		ts = gf_isom_get_media_timescale(file, i+1);
		import_fps = ts;
		import_fps /= inc;
		gf_isom_sample_del(&samp);
		GF_LOG(GF_LOG_INFO, GF_LOG_AUTHOR, ("[Chapter import] Guessed video frame rate %g (%u:%u)\n", import_fps, ts, inc));
		break;
	}


	cur_chap = 0;
	ts = 0;
	state = 0;
	while (fgets(line, 1024, f) != NULL) {
		char *title = NULL;
		u32 off = 0;
		char *sL;
		while (1) {
			u32 len = strlen(line);
			if (!len) break;
			switch (line[len-1]) {
			case '\n': case '\t': case '\r': case ' ':
				line[len-1] = 0;
				continue;
			}
			break;
		}

		while (line[off]==' ') off++;
		if (!strlen(line+off)) continue;
		sL = line+off;

		szTitle[0] = 0;
		/*ZoomPlayer chapters*/
		if (!strnicmp(sL, "AddChapter(", 11)) {
			u32 nb_fr;
			sscanf(sL, "AddChapter(%u,%s)", &nb_fr, szTitle);
			ts = nb_fr;
			ts *= 1000;
			if (import_fps) ts = (u64) (((s64) ts ) / import_fps);
			else ts /= 25;
			sL = strchr(sL, ','); strcpy(szTitle, sL+1); sL = strrchr(szTitle, ')'); if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterBySecond(", 19)) {
			u32 nb_s;
			sscanf(sL, "AddChapterBySecond(%u,%s)", &nb_s, szTitle);
			ts = nb_s;
			ts *= 1000;
			sL = strchr(sL, ','); strcpy(szTitle, sL+1); sL = strrchr(szTitle, ')'); if (sL) sL[0] = 0;
		} else if (!strnicmp(sL, "AddChapterByTime(", 17)) {
			u32 h, m, s;
			sscanf(sL, "AddChapterByTime(%u,%u,%u,%s)", &h, &m, &s, szTitle);
			ts = 3600*h + 60*m + s;
			ts *= 1000;
			sL = strchr(sL, ',');
			if (sL) sL = strchr(sL+1, ',');
			if (sL) sL = strchr(sL+1, ',');
			strcpy(szTitle, sL+1); sL = strrchr(szTitle, ')'); if (sL) sL[0] = 0;
		}
		/*regular or SMPTE time codes*/
		else if ((strlen(sL)>=8) && (sL[2]==':') && (sL[5]==':')) {
			title = NULL;
			if (strlen(sL)==8) {
				sscanf(sL, "%02u:%02u:%02u", &h, &m, &s);
				ts = (h*3600 + m*60+s)*1000;
			}
			else {
				char szTS[20], *tok;
				strncpy(szTS, sL, 18);
				tok = strrchr(szTS, ' ');
				if (tok) {
					title = strchr(sL, ' ') + 1;
					while (title[0]==' ') title++;
					if (strlen(title)) strcpy(szTitle, title);
					tok[0] = 0;
				}
				ts = 0;
				h = m = s = ms = 0;

				if (sscanf(szTS, "%u:%u:%u;%u/%u", &h, &m, &s, &fr, &fps)==5) {
					ts = (h*3600 + m*60+s)*1000 + 1000*fr/fps;
				} else if (sscanf(szTS, "%u:%u:%u;%u", &h, &m, &s, &fr)==4) {
					ts = (h*3600 + m*60+s);
					if (import_fps)
						ts = (s64) (((import_fps*((s64)ts) + fr) * 1000 ) / import_fps);
					else
						ts = ((ts*25 + fr) * 1000 ) / 25;
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(szTS, "%u:%u:%u", &h, &m, &s) == 3) {
					ts = (h*3600 + m*60+s) * 1000;
				}
			}
		}
		/*CHAPTERX= and CHAPTERXNAME=*/
		else if (!strnicmp(sL, "CHAPTER", 7)) {
			u32 idx;
			char szTemp[20], *str;
			strncpy(szTemp, sL, 19);
			str = strrchr(szTemp, '=');
			if (!str) continue;
			str[0] = 0;
			strlwr(szTemp);
			idx = cur_chap;
			str = strchr(sL, '=');
			str++;
			if (strstr(szTemp, "name")) {
				sscanf(szTemp, "chapter%uname", &idx);
				strcpy(szTitle, str);
				if (idx!=cur_chap) {
					cur_chap=idx;
					state = 0;
				}
				state++;
			} else {
				sscanf(szTemp, "chapter%u", &idx);
				if (idx!=cur_chap) {
					cur_chap=idx;
					state = 0;
				}
				state++;

				ts = 0;
				h = m = s = ms = 0;
				if (sscanf(str, "%u:%u:%u.%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(str, "%u:%u:%u:%u", &h, &m, &s, &ms) == 4) {
					ts = (h*3600 + m*60+s)*1000+ms;
				} else if (sscanf(str, "%u:%u:%u", &h, &m, &s) == 3) {
					ts = (h*3600 + m*60+s) * 1000;
				}
			}
			if (state==2) {
				e = gf_isom_add_chapter(file, 0, ts, szTitle);
				if (e) goto err_exit;
				state = 0;
			}
			continue;
		}
		else continue;

		if (strlen(szTitle)) {
			e = gf_isom_add_chapter(file, 0, ts, szTitle);
		} else {
			e = gf_isom_add_chapter(file, 0, ts, NULL);
		}
		if (e) goto err_exit;
	}


err_exit:
	fclose(f);
	return e;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_ESD *gf_media_map_esd(GF_ISOFile *mp4, u32 track)
{
	u32 type;
	GF_GenericSampleDescription *udesc;
	GF_BitStream *bs;
	GF_ESD *esd;

	u32 subtype = gf_isom_get_media_subtype(mp4, track, 1);
	/*all types with an official MPEG-4 mapping*/
	switch (subtype) {
	case GF_ISOM_SUBTYPE_MPEG4:
	case GF_ISOM_SUBTYPE_MPEG4_CRYP:
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_SVC_H264:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		return gf_isom_get_esd(mp4, track, 1);
	}

	switch (gf_isom_get_media_type(mp4, track)) {
	case GF_ISOM_MEDIA_TEXT:
	case GF_ISOM_MEDIA_SUBT:
		return gf_isom_get_esd(mp4, track, 1);
	}

	if ((subtype == GF_ISOM_SUBTYPE_3GP_AMR) || (subtype == GF_ISOM_SUBTYPE_3GP_AMR_WB)) {
		GF_3GPConfig *gpc = gf_isom_3gp_config_get(mp4, track, 1);
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
		esd->ESID = gf_isom_get_track_id(mp4, track);
		esd->OCRESID = esd->ESID;
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		/*use private DSI*/
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_GENERIC;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*format ext*/
		gf_bs_write_u32(bs, subtype);
		gf_bs_write_u32(bs, (subtype == GF_ISOM_SUBTYPE_3GP_AMR) ? 8000 : 16000);
		gf_bs_write_u16(bs, 1);
		gf_bs_write_u16(bs, (subtype == GF_ISOM_SUBTYPE_3GP_AMR) ? 160 : 320);
		gf_bs_write_u8(bs, 16);
		gf_bs_write_u8(bs, gpc ? gpc->frames_per_sample : 0);
		if (gpc) gf_free(gpc);
		gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_bs_del(bs);
		return esd;
	}

	if (subtype == GF_ISOM_SUBTYPE_3GP_H263) {
		u32 w, h;
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
		esd->ESID = gf_isom_get_track_id(mp4, track);
		esd->OCRESID = esd->ESID;
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		/*use private DSI*/
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_GENERIC;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*format ext*/
		gf_bs_write_u32(bs, GF_4CC('h', '2', '6', '3'));
		gf_isom_get_visual_info(mp4, track, 1, &w, &h);
		gf_bs_write_u16(bs, w);
		gf_bs_write_u16(bs, h);
		gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_bs_del(bs);
		return esd;
	}

	if (subtype == GF_ISOM_SUBTYPE_AC3) {
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
		esd->ESID = gf_isom_get_track_id(mp4, track);
		esd->OCRESID = esd->ESID;
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_AUDIO_AC3;
		gf_odf_desc_del((GF_Descriptor*)esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = NULL;
		return esd;
	}

	if (subtype == GF_ISOM_SUBTYPE_3GP_DIMS) {
		GF_DIMSDescription dims;
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
		esd->ESID = gf_isom_get_track_id(mp4, track);
		esd->OCRESID = esd->ESID;
		esd->decoderConfig->streamType = GF_STREAM_SCENE;
		/*use private DSI*/
		esd->decoderConfig->objectTypeIndication = GPAC_OTI_SCENE_DIMS;
		gf_isom_get_dims_description(mp4, track, 1, &dims);
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*format ext*/
		gf_bs_write_u8(bs, dims.profile);
		gf_bs_write_u8(bs, dims.level);
		gf_bs_write_int(bs, dims.pathComponents, 4);
		gf_bs_write_int(bs, dims.fullRequestHost, 1);
		gf_bs_write_int(bs, dims.streamType, 1);
		gf_bs_write_int(bs, dims.containsRedundant, 2);
		gf_bs_write_data(bs, (char*)dims.textEncoding, strlen(dims.textEncoding)+1);
		gf_bs_write_data(bs, (char*)dims.contentEncoding, strlen(dims.contentEncoding)+1);
		gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
		gf_bs_del(bs);
		return esd;
	}

	type = gf_isom_get_media_type(mp4, track);
	if ((type != GF_ISOM_MEDIA_AUDIO) && (type != GF_ISOM_MEDIA_VISUAL)) return NULL;

	esd = gf_odf_desc_esd_new(0);
	esd->OCRESID = esd->ESID = gf_isom_get_track_id(mp4, track);
	esd->slConfig->useTimestampsFlag = 1;
	esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
	esd->decoderConfig->objectTypeIndication = GPAC_OTI_MEDIA_GENERIC;
	/*format ext*/
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, subtype);
	udesc = gf_isom_get_generic_sample_description(mp4, track, 1);
	if (type==GF_ISOM_MEDIA_AUDIO) {
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		gf_bs_write_u32(bs, udesc->samplerate);
		gf_bs_write_u16(bs, udesc->nb_channels);
		gf_bs_write_u16(bs, 0);
		gf_bs_write_u8(bs, udesc->bits_per_sample);
		gf_bs_write_u8(bs, 0);
	} else {
		esd->decoderConfig->streamType = GF_STREAM_VISUAL;
		gf_bs_write_u16(bs, udesc->width);
		gf_bs_write_u16(bs, udesc->height);
	}
	if (udesc && udesc->extension_buf_size) {
		gf_bs_write_data(bs, udesc->extension_buf, udesc->extension_buf_size);
		gf_free(udesc->extension_buf);
	}
	if (udesc) gf_free(udesc);
	gf_bs_get_content(bs, &esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength);
	gf_bs_del(bs);
	return esd;
}

#endif /*GPAC_DISABLE_ISOM*/

