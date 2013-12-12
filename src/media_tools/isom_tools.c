/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/config_file.h>

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_EXPORT
GF_Err gf_media_change_par(GF_ISOFile *file, u32 track, s32 ar_num, s32 ar_den)
{
	u32 tk_w, tk_h, stype;
	GF_Err e;

	e = gf_isom_get_visual_info(file, track, 1, &tk_w, &tk_h);
	if (e) return e;

	stype = gf_isom_get_media_subtype(file, track, 1);
	if ((stype==GF_ISOM_SUBTYPE_AVC_H264) 
		|| (stype==GF_ISOM_SUBTYPE_AVC2_H264) 
		|| (stype==GF_ISOM_SUBTYPE_AVC3_H264) 
		|| (stype==GF_ISOM_SUBTYPE_AVC4_H264) 
	) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_AVCConfig *avcc = gf_isom_avc_config_get(file, track, 1);
		gf_media_avc_change_par(avcc, ar_num, ar_den);
		e = gf_isom_avc_config_update(file, track, 1, avcc);
		gf_odf_avc_cfg_del(avcc);
		if (e) return e;
#endif
	}
#if !defined(GPAC_DISABLE_HEVC) && !defined(GPAC_DISABLE_AV_PARSERS)
	else if (stype==GF_4CC('h','v','c','1')) {
		GF_HEVCConfig *hvcc = gf_isom_hevc_config_get(file, track, 1);
		gf_media_hevc_change_par(hvcc, ar_num, ar_den);
		e = gf_isom_hevc_config_update(file, track, 1, hvcc);
		gf_odf_hevc_cfg_del(hvcc);
		if (e) return e;
	}
#endif
	else if (stype==GF_ISOM_SUBTYPE_MPEG4) {
		GF_ESD *esd = gf_isom_get_esd(file, track, 1);
		if (!esd || !esd->decoderConfig || (esd->decoderConfig->streamType!=4) ) {
			if (esd)  gf_odf_desc_del((GF_Descriptor *) esd);
			return GF_NOT_SUPPORTED;
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
			e = gf_m4v_rewrite_par(&esd->decoderConfig->decoderSpecificInfo->data, &esd->decoderConfig->decoderSpecificInfo->dataLength, ar_num, ar_den);
			if (!e) e = gf_isom_change_mpeg4_description(file, track, 1, esd);
			gf_odf_desc_del((GF_Descriptor *) esd);
			if (e) return e;
		}
#endif
	} else {
		if (gf_isom_get_media_type(file, track)==GF_ISOM_MEDIA_VISUAL) {
			return GF_NOT_SUPPORTED;
		}
		return GF_BAD_PARAM;
	}

	e = gf_isom_set_pixel_aspect_ratio(file, track, 1, ar_num, ar_den);
	if (e) return e;

	if ((ar_den>=0) && (ar_num>=0)) {
		if (ar_den) tk_w = tk_w * ar_num / ar_den;
		else if (ar_num) tk_h = tk_h * ar_den / ar_num;
	}
	/*revert to full frame for track layout*/
	else {
		e = gf_isom_get_visual_info(file, track, 1, &tk_w, &tk_h);
		if (e) return e;
	}
	return gf_isom_set_track_layout_info(file, track, tk_w<<16, tk_h<<16, 0, 0, 0);
}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_Err gf_media_get_file_hash(const char *file, u8 hash[20]) 
{
#ifdef GPAC_DISABLE_CORE_TOOLS
	return GF_NOT_SUPPORTED;
#else
	u8 block[1024];
	u32 read;
	u64 size, tot;
	FILE *in;
	GF_SHA1Context *ctx;
#ifndef GPAC_DISABLE_ISOM
	GF_BitStream *bs = NULL;
	Bool is_isom = gf_isom_probe_file(file);
#endif

	in = gf_f64_open(file, "rb");
	gf_f64_seek(in, 0, SEEK_END);
	size = gf_f64_tell(in);
	gf_f64_seek(in, 0, SEEK_SET);

	ctx = gf_sha1_starts();
	tot = 0;
#ifndef GPAC_DISABLE_ISOM
	if (is_isom) bs = gf_bs_from_file(in, GF_BITSTREAM_READ);
#endif

	while (tot<size) {
#ifndef GPAC_DISABLE_ISOM
		if (is_isom) {
			u64 box_size = gf_bs_peek_bits(bs, 32, 0);
			u32 box_type = gf_bs_peek_bits(bs, 32, 4);

			/*till end of file*/
			if (!box_size) box_size = size-tot;
			/*64-bit size*/
			else if (box_size==1) box_size = gf_bs_peek_bits(bs, 64, 8);

			/*skip all MutableDRMInformation*/
			if (box_type==GF_4CC('m','d','r','i')) {
				gf_bs_skip_bytes(bs, box_size);
				tot += box_size;
			} else {
				u32 bsize = 0;
				while (bsize<box_size) {
					u32 to_read = (u32) ((box_size-bsize<1024) ? (box_size-bsize) : 1024);
					gf_bs_read_data(bs, (char *) block, to_read);
					gf_sha1_update(ctx, block, to_read);
					bsize += to_read;
				}
				tot += box_size;
			}
		} else
#endif
		{
			read = (u32) fread(block, 1, 1024, in);
			gf_sha1_update(ctx, block, read);
			tot += read;
		}
	}
	gf_sha1_finish(ctx, hash);
#ifndef GPAC_DISABLE_ISOM
	if (bs) gf_bs_del(bs);
#endif
	fclose(in);
	return GF_OK;
#endif
}

#ifndef GPAC_DISABLE_ISOM

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
			case GF_ISOM_SUBTYPE_AVC3_H264:
			case GF_ISOM_SUBTYPE_AVC4_H264:
			case GF_ISOM_SUBTYPE_SVC_H264:
				nb_vid++;
				nb_avc++;
				break;
			case GF_ISOM_SUBTYPE_MPEG4:
				{
					GF_ESD *esd = gf_isom_get_esd(mp4file, i+1, 1);
					/*both MPEG4-Video and H264/AVC/SVC are supported*/
					if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)) {
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

GF_EXPORT
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
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
	case GF_ISOM_SUBTYPE_SVC_H264:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
	case GF_ISOM_SUBTYPE_HVC1:
	case GF_ISOM_SUBTYPE_HEV1:
	case GF_ISOM_SUBTYPE_HVC2:
	case GF_ISOM_SUBTYPE_HEV2:
	case GF_ISOM_SUBTYPE_SHC1:
	case GF_ISOM_SUBTYPE_SHV1:
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
		GF_AC3Config *ac3 = gf_isom_ac3_config_get(mp4, track, 1);
		esd = gf_odf_desc_esd_new(0);
		esd->slConfig->timestampResolution = gf_isom_get_media_timescale(mp4, track);
		esd->ESID = gf_isom_get_track_id(mp4, track);
		esd->OCRESID = esd->ESID;
		esd->decoderConfig->streamType = GF_STREAM_AUDIO;
		esd->decoderConfig->objectTypeIndication = (ac3 && ac3->is_ec3) ? GPAC_OTI_AUDIO_AC3_ENHANCED : GPAC_OTI_AUDIO_AC3;
		gf_odf_desc_del((GF_Descriptor*)esd->decoderConfig->decoderSpecificInfo);
		esd->decoderConfig->decoderSpecificInfo = NULL;
		if (ac3) gf_free(ac3);
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
		gf_bs_write_data(bs, (char*)dims.textEncoding, (u32) strlen(dims.textEncoding)+1);
		gf_bs_write_data(bs, (char*)dims.contentEncoding, (u32) strlen(dims.contentEncoding)+1);
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

#ifndef GPAC_DISABLE_MEDIA_IMPORT
static s32 gf_get_DQId(GF_ISOFile *file, u32 track)
{
	GF_AVCConfig *svccfg;
	GF_ISOSample *samp;
	u32 di = 0, cur_extract_mode;
	char *buffer;
	GF_BitStream *bs;
	u32 max_size = 4096;
	u32 size, nalu_size_length;
	u8 nal_type;
	s32 DQId=0;

	samp = NULL;
	bs = NULL;
	cur_extract_mode = gf_isom_get_nalu_extract_mode(file, track);
	gf_isom_set_nalu_extract_mode(file, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	svccfg = gf_isom_svc_config_get(file, track, 1);
	if (!svccfg)
	{
		DQId = 0;
		goto exit;
	}
	samp = gf_isom_get_sample(file, track, 1, &di);
	if (!samp)
	{
		DQId = -1;
		goto exit;
	}
	bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	nalu_size_length = 8 * svccfg->nal_unit_size;
	while (gf_bs_available(bs))
	{
		size = gf_bs_read_int(bs, nalu_size_length);
		if (size>max_size) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*size);
			max_size = size;
		}
		gf_bs_read_data(bs, buffer, size);
		nal_type = buffer[0] & 0x1F;
		if (nal_type == GF_AVC_NALU_SVC_SLICE)
		{
			DQId = buffer[2] & 0x7F;
			goto exit;
		}
	}
exit:
	if (svccfg) gf_odf_avc_cfg_del(svccfg);
	if (samp) gf_isom_sample_del(&samp);
	if (buffer) gf_free(buffer);
	if (bs) gf_bs_del(bs);
	gf_isom_set_nalu_extract_mode(file, track, cur_extract_mode);
	return DQId;;
}

static Bool gf_isom_has_svc_explicit(GF_ISOFile *file, u32 track)
{
	GF_AVCConfig *svccfg;
	GF_AVCConfigSlot *slc;
	u32 i;
	u8 type;
	Bool ret = 0;

	svccfg = gf_isom_svc_config_get(file, track, 1);
	if (!svccfg)
		return 0;

	for (i = 0; i < gf_list_count(svccfg->sequenceParameterSets); i++)
	{
		slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->sequenceParameterSets, i);
		type = slc->data[0] & 0x1F;
		if (type == GF_AVC_NALU_SEQ_PARAM)
		{
			ret = 1;
			break;
		}
	}

	if (svccfg) gf_odf_avc_cfg_del(svccfg);
	return ret;
}


static u32 gf_isom_get_track_id_max(GF_ISOFile *file)
{
	u32 num_track, i, trackID;
	u32 max_id = 0;

	num_track = gf_isom_get_track_count(file);
	for (i = 1; i <= num_track; i++)
	{
		trackID = gf_isom_get_track_id(file, i);
		if (max_id < trackID)
			max_id = trackID;
	}

	return max_id;
}

/* Split SVC layers */
GF_EXPORT
GF_Err gf_media_split_svc(GF_ISOFile *file, u32 track, Bool splitAll)
{
	GF_AVCConfig *svccfg, *cfg;
	u32 num_svc_track, num_sample, svc_track, dst_track, ref_trackID, ref_trackNum, max_id, di, width, height, size, nalu_size_length, i, j, t, max_size, num_pps, num_sps, num_subseq, NALUnitHeader, data_offset, data_length, count, timescale, cur_extract_mode;
	GF_Err e; 
	GF_AVCConfigSlot *slc, *sl;
	AVCState avc;
	s32 sps_id, pps_id;
	GF_ISOSample *samp, *dst_samp;
	GF_BitStream *bs, *dst_bs;
	GF_BitStream ** sample_bs;
	u8 nal_type, nal_hdr, track_ref_index;
	char *buffer;
	s32 *sps_track, *sps, *pps;
	u64 offset;
	Bool is_splited;
	Bool *first_sample_track, *is_subseq_pps;
	u64 *first_DTS_track;
	s8 sample_offset;

	max_size = 4096;
	e = GF_OK;
	samp = dst_samp = NULL;
	bs = NULL;
	sample_bs = NULL;
	sps_track = sps = pps = NULL;
	first_DTS_track = NULL;
	first_sample_track = is_subseq_pps = NULL;
	buffer = NULL;
	cfg = NULL;
	num_svc_track=0;
	cur_extract_mode = gf_isom_get_nalu_extract_mode(file, track);
	gf_isom_set_nalu_extract_mode(file, track, GF_ISOM_NALU_EXTRACT_INSPECT);
	svccfg = gf_isom_svc_config_get(file, track, 1);
	if (!svccfg)
	{
		e = GF_OK;
		goto exit;
	}
	num_sps = gf_list_count(svccfg->sequenceParameterSets);
	if (!num_sps)
	{
		e = GF_OK;
		goto exit;
	}
	num_pps = gf_list_count(svccfg->pictureParameterSets);
	if ((gf_isom_get_avc_svc_type(file, track, 1) == GF_ISOM_AVCTYPE_SVC_ONLY) && !gf_isom_has_svc_explicit(file, track))
		is_splited = 1;
	else
		is_splited = 0;
	num_subseq = gf_isom_has_svc_explicit(file, track) ? num_sps - 1 : num_sps;

	if (is_splited)
	{
		/*this track has only one SVC ...*/
		if (num_sps == 1)
		{
			/*use 'all' mode -> stop*/
			if (splitAll)
				goto exit;
			/*use 'base' mode -> merge SVC tracks*/
			else
			{
				e = gf_media_merge_svc(file, track, 0);
				goto exit;
			}
		}
		/*this file has been in 'splitbase' mode*/
		else if (!splitAll)
			goto exit;

	}

	timescale = gf_isom_get_media_timescale(file, track);
	num_svc_track = splitAll ? num_subseq : 1;
	max_id = gf_isom_get_track_id_max(file);
	di = 0;
	
	memset(&avc, 0, sizeof(AVCState));
	avc.sps_active_idx = -1;
	nalu_size_length = 8 * svccfg->nal_unit_size;
	/*read all sps, but we need only the subset sequence parameter sets*/
	sps =  (s32 *) gf_malloc(num_subseq * sizeof(s32));
	sps_track = (s32 *) gf_malloc(num_subseq * sizeof(s32));
	count = 0;
	for (i = 0; i < num_sps; i++)
	{
		slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->sequenceParameterSets, i);
		nal_type = slc->data[0] & 0x1F;
		sps_id = gf_media_avc_read_sps(slc->data, slc->size, &avc, 0, NULL);
		if (sps_id < 0) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		if (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM)
		{
			sps[count] = sps_id;
			sps_track[count] = i;
			count++;
		}
	}
	/*for testing*/
	assert(count == num_subseq);
	/*read all pps*/
	pps =  (s32 *) gf_malloc(num_pps * sizeof(s32));
	for (j = 0; j < num_pps; j++)
	{
		slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->pictureParameterSets, j);
		pps_id = gf_media_avc_read_pps(slc->data, slc->size, &avc);
		if (pps_id < 0) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto exit;
		}
		pps[j] = pps_id;
	}
	if (!is_splited)
		ref_trackID = gf_isom_get_track_id(file, track);
	else
	{
		gf_isom_get_reference(file, track, GF_ISOM_REF_BASE, 1, &ref_trackNum);
		ref_trackID = gf_isom_get_track_id(file, ref_trackNum);
	}

	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	/*read first sample for determinating the order of SVC tracks*/
	count = 0;
	samp = gf_isom_get_sample(file, track, 1, &di);
	if (!samp)
	{
		e = gf_isom_last_error(file);
		goto exit;
	}
	bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	offset = 0;
	is_subseq_pps = (Bool *) gf_malloc(num_pps*sizeof(Bool));
	for (i = 0; i < num_pps; i++)
		is_subseq_pps[i] = 0;
	while (gf_bs_available(bs))
	{
		size = gf_bs_read_int(bs, nalu_size_length);
		if (size>max_size) {
			buffer = (char*)gf_realloc(buffer, sizeof(char)*size);
			max_size = size;
		}
		nal_hdr = gf_bs_read_u8(bs);
		nal_type = nal_hdr & 0x1F;
		gf_media_avc_parse_nalu(bs, nal_hdr, &avc);
		e = gf_bs_seek(bs, offset+nalu_size_length/8);
		if (e)
			goto exit;
		gf_bs_read_data(bs, buffer, size);
		offset += size + nalu_size_length/8;
		if (nal_type == GF_AVC_NALU_SVC_SLICE)
		{
			for (i = 0; i < num_pps; i++)
			{
				if (avc.s_info.pps->id == pps[i])
				{
					is_subseq_pps[i] = 1;
					break;
				}
			}
			if ((count > 0) && (avc.s_info.pps->sps_id == sps[count-1]))
				continue;
			/*verify the order of SPS, reorder if necessary*/
			if (avc.s_info.pps->sps_id != sps[count])
			{
				for (i = count+1; i < num_subseq; i++)
				{
					/*swap two SPS*/
					if (avc.s_info.pps->sps_id == sps[i])
					{
						sps[i] = sps[count];
						sps[count] = avc.s_info.pps->sps_id;
						sps_track[count] = i;
						break;
					}
				}
			}
			count++;
		}
	}
	gf_bs_del(bs);
	bs = NULL;

	gf_isom_sample_del(&samp);
	samp = NULL;
	
	for (t = 0; t < num_svc_track; t++)
	{
		e = GF_OK;
		svc_track = gf_isom_new_track(file, t+1+max_id, GF_ISOM_MEDIA_VISUAL, timescale);
		if (!svc_track) 
		{
			e = gf_isom_last_error(file);
			goto exit;
		}
		gf_isom_set_track_enabled(file, svc_track, 1);
		gf_isom_set_track_reference(file, svc_track, GF_ISOM_REF_BASE, ref_trackID);
		cfg = gf_odf_avc_cfg_new();
		cfg->complete_representation = 1; //SVC
		/*this layer depends on the base layer and the lower layers*/
		gf_isom_set_track_reference(file, svc_track, GF_ISOM_REF_SCAL, ref_trackID);
		for (i = 0; i < t; i++)
			gf_isom_set_track_reference(file, svc_track, GF_ISOM_REF_SCAL, i+1+max_id);

		e = gf_isom_svc_config_new(file, svc_track, cfg, NULL, NULL, &di);
		if (e)
			goto exit;
		if (splitAll)
		{
			sps_id = sps[t];
			width = avc.sps[sps_id].width;
			height = avc.sps[sps_id].height;
			gf_isom_set_visual_info(file, svc_track, di, width, height);
			cfg->configurationVersion = 1;
			cfg->chroma_bit_depth = 8 + avc.sps[sps_id].chroma_bit_depth_m8;
			cfg->chroma_format = avc.sps[sps_id].chroma_format;
			cfg->luma_bit_depth = 8 + avc.sps[sps_id].luma_bit_depth_m8;
			cfg->profile_compatibility = avc.sps[sps_id].prof_compat;
			cfg->AVCLevelIndication = avc.sps[sps_id].level_idc;
			cfg->AVCProfileIndication = avc.sps[sps_id].profile_idc;
			cfg->nal_unit_size = svccfg->nal_unit_size;
			slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->sequenceParameterSets, sps_track[t]);
			sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			sl->id = slc->id;
			sl->size = slc->size;
			sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
			memcpy(sl->data, slc->data, sizeof(char)*sl->size);
			gf_list_add(cfg->sequenceParameterSets, sl);
			for (j = 0; j < num_pps; j++)
			{
				pps_id = pps[j];
				if (is_subseq_pps[j] && (avc.pps[pps_id].sps_id == sps_id))
				{
					slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->pictureParameterSets, j);
					sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
					sl->id = slc->id;
					sl->size = slc->size;
					sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
					memcpy(sl->data, slc->data, sizeof(char)*sl->size);
					gf_list_add(cfg->pictureParameterSets, sl);
				}
			}
		}
		else
		{
			for (i = 0; i < num_subseq; i++)
			{
				sps_id = sps[i];
				width = avc.sps[sps_id].width;
				height = avc.sps[sps_id].height;
				gf_isom_set_visual_info(file, svc_track, di, width, height);
				cfg->configurationVersion = 1;
				cfg->chroma_bit_depth = 8 + avc.sps[sps_id].chroma_bit_depth_m8;
				cfg->chroma_format = avc.sps[sps_id].chroma_format;
				cfg->luma_bit_depth = 8 + avc.sps[sps_id].luma_bit_depth_m8;
				cfg->profile_compatibility = avc.sps[sps_id].prof_compat;
				cfg->AVCLevelIndication = avc.sps[sps_id].level_idc;
				cfg->AVCProfileIndication = avc.sps[sps_id].profile_idc;
				cfg->nal_unit_size = svccfg->nal_unit_size;
				slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->sequenceParameterSets, sps_track[i]);
				sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
				sl->id = slc->id;
				sl->size = slc->size;
				sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
				memcpy(sl->data, slc->data, sizeof(char)*sl->size);
				gf_list_add(cfg->sequenceParameterSets, sl);
				for (j = 0; j < num_pps; j++)
				{
					pps_id = pps[j];
					if (avc.pps[pps_id].sps_id == sps_id)
					{
						slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->pictureParameterSets, j);
						sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
						sl->id = slc->id;
						sl->size = slc->size;
						sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
						memcpy(sl->data, slc->data, sizeof(char)*sl->size);
						gf_list_add(cfg->pictureParameterSets, sl);
					}
				}
			}
		}
		e = gf_isom_svc_config_update(file, svc_track, 1, cfg, 0);
		if (e)
			goto exit;
		gf_odf_avc_cfg_del(cfg);
		cfg = NULL;
	}

	num_sample = gf_isom_get_sample_count(file, track);
	first_sample_track = (Bool *) gf_malloc((num_svc_track+1) * sizeof(Bool));
	for (t = 0; t <= num_svc_track; t++)
		first_sample_track[t] = 1;
	first_DTS_track = (u64 *) gf_malloc((num_svc_track+1) * sizeof(u64));
	for (t = 0; t <= num_svc_track; t++)
		first_DTS_track[t] = 0;
	for (i = 1; i <= num_sample; i++)
	{
		/*reset*/
		memset(buffer, 0, max_size);
		
		samp = gf_isom_get_sample(file, track, i, &di);
		if (!samp)
		{
			e = GF_IO_ERR;
			goto exit;
		}

		/* Create (num_svc_track) SVC bitstreams + 1 AVC bitstream*/
		sample_bs = (GF_BitStream **) gf_malloc(sizeof(GF_BitStream *) * (num_svc_track+1));
		for (j = 0; j <= num_svc_track; j++)
			sample_bs[j] = (GF_BitStream *) gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		/*write extractor*/
		for (t = 0; t < num_svc_track; t++)
		{
			//reference to base layer
			gf_bs_write_int(sample_bs[t+1], 14, nalu_size_length); // extractor 's size = 14
			NALUnitHeader = 0; //reset
			NALUnitHeader |= 0x1F000000; // NALU type = 31
			gf_bs_write_u32(sample_bs[t+1], NALUnitHeader);
			track_ref_index = (u8) gf_isom_has_track_reference(file, t+1+max_id, GF_ISOM_REF_SCAL, ref_trackID);
			if (!track_ref_index)
			{
				e = GF_CORRUPTED_DATA;
				goto exit;
			}
			gf_bs_write_u8(sample_bs[t+1], track_ref_index);
			sample_offset = 0;
			gf_bs_write_u8(sample_bs[t+1], sample_offset);
			data_offset = 0;
			gf_bs_write_u32(sample_bs[t+1], data_offset);
			data_length = 0;
			gf_bs_write_u32(sample_bs[t+1], data_length);
			//reference to previous layer(s)
			for (j = 0; j < t; j++)
			{
				gf_bs_write_int(sample_bs[t+1], 14, nalu_size_length);
				NALUnitHeader = 0;
				NALUnitHeader |= 0x1F000000;
				gf_bs_write_u32(sample_bs[t+1], NALUnitHeader);
				track_ref_index = (u8) gf_isom_has_track_reference(file, t+1+max_id, GF_ISOM_REF_SCAL, j+1+max_id);
				if (!track_ref_index)
				{
					e = GF_CORRUPTED_DATA;
					goto exit;
				}
				gf_bs_write_u8(sample_bs[t+1], track_ref_index);
				sample_offset = 0;
				gf_bs_write_u8(sample_bs[t+1], sample_offset);
				data_offset = (j+1) * (nalu_size_length/8 + 14); // (nalu_size_length/8) bytes of NALU length field + 14 bytes of extractor per layer
				gf_bs_write_u32(sample_bs[t+1], data_offset);
				data_length = 0;
				gf_bs_write_u32(sample_bs[t+1], data_length);
			}
		}

		bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
		offset = 0;
		while (gf_bs_available(bs))
		{
			size = gf_bs_read_int(bs, nalu_size_length);
			if (size>max_size) {
				buffer = (char*)gf_realloc(buffer, sizeof(char)*size);
				max_size = size;
			}
			nal_hdr = gf_bs_read_u8(bs);
			nal_type = nal_hdr & 0x1F;
			gf_media_avc_parse_nalu(bs, nal_hdr, &avc);
			e = gf_bs_seek(bs, offset+nalu_size_length/8);
			if (e)
				goto exit;
			gf_bs_read_data(bs, buffer, size);
			offset += size + nalu_size_length/8;
					
			switch (nal_type) {
				case GF_AVC_NALU_PIC_PARAM:
					pps_id = gf_media_avc_read_pps(buffer, size, &avc);;
					j = 0;
					dst_track = 0;
					while (j < num_pps)
					{
						if (pps_id == pps[j])
							break;
						j++;
					}
					if ((j < num_pps) && (is_subseq_pps[j]))
					{
						if (splitAll)
						{
							for (t = 0; t < num_svc_track; t++)
							{
								if (sps[t] == avc.pps[pps_id].sps_id)
								{
									dst_track = t + 1;
									break;
								}
							}
						}
						else
							dst_track = 1;
					}
					dst_bs = sample_bs[dst_track];
					break;
				case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
					sps_id = gf_media_avc_read_sps(buffer, size, &avc, 0, NULL);
					dst_track = 0;
					if (splitAll)
					{
						for (t = 0; t < num_svc_track; t++)
						{
							if (sps[t] == sps_id)
							{
								dst_track = t + 1;
								break;
							}
						}
					}
					else
						dst_track = 1;
					dst_bs = sample_bs[dst_track];
					break;
				case GF_AVC_NALU_SVC_SLICE:
					dst_track = 0;
					if (splitAll)
					{
						for (t = 0; t < num_svc_track; t++)
						{
							if (sps[t] == (avc.s_info.pps)->sps_id)
							{
								dst_track = t + 1;
								break;
							}
						}
					}
					else
						dst_track = 1;
					dst_bs = sample_bs[dst_track];
					break;
				default:
					dst_bs = sample_bs[0];
			}

			gf_bs_write_int(dst_bs, size, nalu_size_length);
			gf_bs_write_data(dst_bs, buffer, size);
		}

		for (j = 0; j <= num_svc_track; j++)
		{
			if (gf_bs_get_position(sample_bs[j]))
			{
				if (first_sample_track[j])
				{
					first_sample_track[j] = 0;
					first_DTS_track[j] = samp->DTS;
				}
				dst_samp = gf_isom_sample_new();
				dst_samp->CTS_Offset = samp->CTS_Offset;
				dst_samp->DTS = samp->DTS - first_DTS_track[j];
				dst_samp->IsRAP = samp->IsRAP;
				gf_bs_get_content(sample_bs[j], &dst_samp->data, &dst_samp->dataLength);
				if (j) //SVC
					e = gf_isom_add_sample(file, track+j, di, dst_samp);
				else
					e = gf_isom_update_sample(file, track, i, dst_samp, 1);
				if (e)
					goto exit;
				gf_isom_sample_del(&dst_samp);
				dst_samp = NULL;
			}
			gf_bs_del(sample_bs[j]);
			sample_bs[j] = NULL;
		}
		gf_free(sample_bs);
		sample_bs = NULL;
		gf_bs_del(bs);
		bs = NULL;
		gf_isom_sample_del(&samp);
		samp = NULL;
	}

	/*add Editlist entry if DTS of the first sample is not zero*/
	for (t = 0; t <= num_svc_track; t++)
	{
		if (first_DTS_track[t])
		{
			u32 media_ts, moov_ts, offset;
			u64 dur;
			media_ts = gf_isom_get_media_timescale(file, t);
			moov_ts = gf_isom_get_timescale(file);
			offset = (u32)(first_DTS_track[t]) * moov_ts / media_ts;
			dur = gf_isom_get_media_duration(file, t) * moov_ts / media_ts;
			gf_isom_set_edit_segment(file, t, 0, offset, 0, GF_ISOM_EDIT_EMPTY);
			gf_isom_set_edit_segment(file, t, offset, dur, 0, GF_ISOM_EDIT_NORMAL);
		}
	}
	
	/*if this is a merged file*/
	if (!is_splited)
	{
		/*a normal stream: delete SVC config*/
		if (!gf_isom_has_svc_explicit(file, track))
		{
			gf_isom_svc_config_del(file, track, 1);
		}
		else
		{
			s32 shift=0;
			
			for (i = 0; i < gf_list_count(svccfg->sequenceParameterSets); i++)
			{
				slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->sequenceParameterSets, i);
				sps_id = gf_media_avc_read_sps(slc->data, slc->size, &avc, 0, NULL);
				if (sps_id < 0) {
					e = GF_NON_COMPLIANT_BITSTREAM;
					goto exit;
				}
				nal_type = slc->data[0] & 0x1F;
				if (nal_type == GF_AVC_NALU_SVC_SUBSEQ_PARAM)
				{
					gf_list_rem(svccfg->sequenceParameterSets, i);
					gf_free(slc->data);
					gf_free(slc);
					i--;
				}
			}

			for (j = 0; j < gf_list_count(svccfg->pictureParameterSets); j++)
				{
					slc = (GF_AVCConfigSlot *)gf_list_get(svccfg->pictureParameterSets, j);
					pps_id = gf_media_avc_read_pps(slc->data, slc->size, &avc);
					if (pps_id < 0) {
						e = GF_NON_COMPLIANT_BITSTREAM;
						goto exit;
					}
					if (is_subseq_pps[j+shift])
					{
						gf_list_rem(svccfg->pictureParameterSets, j);
						gf_free(slc->data);
						gf_free(slc);
						j--;
					}
				}
			e = gf_isom_svc_config_update(file, track, 1, svccfg, 0);
			if (e)
				goto exit;
		}
	}
	/*if this is as splited file: delete this track*/
	else
	{
		gf_isom_remove_track(file, track);
	}

exit:
	if (svccfg) gf_odf_avc_cfg_del(svccfg);
	if (cfg) gf_odf_avc_cfg_del(cfg);
	if (samp) gf_isom_sample_del(&samp);
	if (dst_samp) gf_isom_sample_del(&dst_samp);
	if (bs) gf_bs_del(bs);
	if (sample_bs)
	{
		for (i = 0; i <= num_svc_track; i++)
			gf_bs_del(sample_bs[i]);
		gf_free(sample_bs);
	}
	if (sps_track) gf_free(sps_track);
	if (sps) gf_free(sps);
	if (pps) gf_free(pps);
	if (first_sample_track) gf_free(first_sample_track);
	if (first_DTS_track) gf_free(first_DTS_track);
	if (buffer) gf_free(buffer);
	if (is_subseq_pps) gf_free(is_subseq_pps);
	gf_isom_set_nalu_extract_mode(file, track, cur_extract_mode);
	return e;
}

/* Merge SVC layers*/
GF_EXPORT
GF_Err gf_media_merge_svc(GF_ISOFile *file, u32 track, Bool mergeAll)
{
	GF_AVCConfig *svccfg, *cfg;
	u32 merge_track,  num_track, num_sample, size, i, t, di, max_size, nalu_size_length, ref_trackNum, ref_trackID, count, width, height, nb_EditList, media_ts, moov_ts;
	GF_ISOSample *avc_samp, *samp, *dst_samp;
	GF_BitStream *bs, *dst_bs;
	GF_Err e;
	char *buffer;
	s32 *DQId;
	u32 *list_track_sorted, *cur_sample, *max_sample;
	u64 *DTS_offset;
	u64 EditTime, SegmentDuration, MediaTime;
	u8 EditMode, nal_type;
	Bool first_sample;
	u64 first_DTS, offset, dur; 
	GF_AVCConfigSlot *slc, *sl;

	e = GF_OK;
	di = 1;
	max_size = 4096;
	width = height = 0;
	avc_samp = samp = dst_samp = NULL;
	svccfg = cfg = NULL;
	buffer = NULL;
	bs = dst_bs = NULL;
	DQId = NULL;
	list_track_sorted = cur_sample = max_sample = NULL;
	DTS_offset = NULL;
	
	if (gf_isom_get_avc_svc_type(file, track, 1) == GF_ISOM_AVCTYPE_AVC_SVC)
		goto exit;

	num_track = gf_isom_get_track_count(file);
	if (num_track == 1) 
		goto exit;
	gf_isom_get_reference(file, track, GF_ISOM_REF_BASE, 1, &ref_trackNum);
	ref_trackID = gf_isom_get_track_id(file, ref_trackNum);
	if (!ref_trackID)
	{
		e = GF_ISOM_INVALID_MEDIA;
		goto exit;
	}
	
	list_track_sorted = (u32 *) gf_malloc(num_track * sizeof(u32));
	DQId = (s32 *) gf_malloc(num_track * sizeof(s32));
	count = 0;
	for (t = 1; t <= num_track; t++) {
		u32 pos = 0;
		s32 track_DQId = gf_get_DQId(file, t);
		if (track_DQId < 0) {
			e = GF_ISOM_INVALID_MEDIA;
			goto exit;
		}

		if (!gf_isom_has_track_reference(file, t, GF_ISOM_REF_BASE, ref_trackID))
		{
			if (t != ref_trackNum) continue;
			else if (!mergeAll) continue;
		}

		while ((pos < count ) && (DQId[pos] <= track_DQId))
			pos++;
		for (i = count; i > pos; i--)
		{
			list_track_sorted[i] = list_track_sorted[i-1];
			DQId[i] = DQId[i-1];
		}
		list_track_sorted[pos] = t;
		DQId[pos] = track_DQId;
		count++;
	}

	merge_track = list_track_sorted[0];
	gf_isom_set_track_enabled(file, merge_track, 1);
	/*rewrite svccfg*/
	svccfg = gf_odf_avc_cfg_new();
	svccfg->complete_representation = 1; 
	/*rewrite visual info*/
	if (!mergeAll)
	{
		for (t = 0; t < count; t++)
			gf_isom_get_visual_info(file, list_track_sorted[t], 1, &width, &height);
		gf_isom_set_visual_info(file, merge_track, 1, width, height);
	}

	for (t = 0; t < count; t++)
	{
		cfg = gf_isom_svc_config_get(file, list_track_sorted[t], 1);
		if (!cfg)
			continue;
		svccfg->configurationVersion = 1;
		svccfg->chroma_bit_depth = cfg->chroma_bit_depth;
		svccfg->chroma_format = cfg->chroma_format;
		svccfg->luma_bit_depth = cfg->luma_bit_depth;
		svccfg->profile_compatibility = cfg->profile_compatibility;
		svccfg->AVCLevelIndication = cfg->AVCLevelIndication;
		svccfg->AVCProfileIndication = cfg->AVCProfileIndication;
		svccfg->nal_unit_size = cfg->nal_unit_size;
		for (i = 0; i < gf_list_count(cfg->sequenceParameterSets); i++)
		{
			slc = (GF_AVCConfigSlot *)gf_list_get(cfg->sequenceParameterSets, i);
			sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			sl->id = slc->id;
			sl->size = slc->size;
			sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
			memcpy(sl->data, slc->data, sizeof(char)*sl->size);
			gf_list_add(svccfg->sequenceParameterSets, sl);
		}
		for (i = 0; i < gf_list_count(cfg->pictureParameterSets); i++)
		{
			slc = (GF_AVCConfigSlot *)gf_list_get(cfg->pictureParameterSets, i);
			sl = (GF_AVCConfigSlot*)gf_malloc(sizeof(GF_AVCConfigSlot));
			sl->id = slc->id;
			sl->size = slc->size;
			sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
			memcpy(sl->data, slc->data, sizeof(char)*sl->size);
			gf_list_add(svccfg->pictureParameterSets, sl);
		}
		if (mergeAll)
		{
			gf_isom_svc_config_update(file, merge_track, 1, svccfg, 1);
		}
		else
			gf_isom_svc_config_update(file, merge_track, 1, svccfg, 0);
		gf_odf_avc_cfg_del(cfg);
		cfg = NULL;
	}

	cur_sample = (u32 *) gf_malloc(count * sizeof(u32));
	max_sample = (u32 *) gf_malloc(count * sizeof(u32));
	for (t = 0; t < count; t++)
	{
		cur_sample[t] = 1;
		max_sample[t] = gf_isom_get_sample_count(file, list_track_sorted[t]);
	}

	DTS_offset = (u64 *) gf_malloc(count * sizeof(u64));
	for (t = 0; t < count; t++)
	{
		nb_EditList = gf_isom_get_edit_segment_count(file, list_track_sorted[t]);
		if (!nb_EditList)
			DTS_offset[t] = 0;
		else
		{
			media_ts = gf_isom_get_media_timescale(file, list_track_sorted[t]);
			moov_ts = gf_isom_get_timescale(file);
			for (i = 1; i <= nb_EditList; i++)
			{
				e = gf_isom_get_edit_segment(file, list_track_sorted[t], i, &EditTime, &SegmentDuration, &MediaTime, &EditMode);
				if (e) goto exit;

				if (!EditMode)
				{
					DTS_offset[t] = SegmentDuration * media_ts / moov_ts;
				}
			}
		}
	}

	num_sample = gf_isom_get_sample_count(file, ref_trackNum);
	nalu_size_length = 8 * svccfg->nal_unit_size;
	first_sample = 1;
	first_DTS = 0;
	buffer = (char*)gf_malloc(sizeof(char) * max_size);
	for (t = 1; t <= num_track; t++)
		gf_isom_set_nalu_extract_mode(file, t, GF_ISOM_NALU_EXTRACT_INSPECT);
	for (i = 1; i <= num_sample; i++)
	{
		dst_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		/*add extractor if nessassary*/
		if (!mergeAll)
		{
			u32 NALUnitHeader = 0;
			u8 track_ref_index;
			s8 sample_offset;
			u32 data_offset;
			u32 data_length;

			gf_bs_write_int(dst_bs, 14, nalu_size_length); // extractor 's size = 14
			NALUnitHeader |= 0x1F000000; // NALU type = 31
			gf_bs_write_u32(dst_bs, NALUnitHeader);
			track_ref_index = (u8) gf_isom_has_track_reference(file, merge_track, GF_ISOM_REF_SCAL, ref_trackID);
			if (!track_ref_index)
			{
				e = GF_CORRUPTED_DATA;
				goto exit;
			}
			gf_bs_write_u8(dst_bs, track_ref_index);
			sample_offset = 0;
			gf_bs_write_u8(dst_bs, sample_offset);
			data_offset = 0;
			gf_bs_write_u32(dst_bs, data_offset);
			data_length = 0;
			gf_bs_write_u32(dst_bs, data_length);
		}

		avc_samp = gf_isom_get_sample(file, ref_trackNum, i, &di);
		if (!avc_samp) {
			e = gf_isom_last_error(file);
			goto exit;
		}

		for (t = 0; t < count; t++)
		{
			if (cur_sample[t] > max_sample[t])
				continue;
			samp = gf_isom_get_sample(file, list_track_sorted[t], cur_sample[t], &di);
			if (!samp) {
				e = gf_isom_last_error(file);
				goto exit;
			}

			if ((samp->DTS + DTS_offset[t]) != avc_samp->DTS)
				continue;
			bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
			/*reset*/
			memset(buffer, 0, sizeof(char) * max_size);
			while (gf_bs_available(bs))
			{
				size = gf_bs_read_int(bs, nalu_size_length);
				if (size>max_size) {
					buffer = (char*)gf_realloc(buffer, sizeof(char)*size);
					max_size = size;
				}			
				gf_bs_read_data(bs, buffer, size);
				nal_type = buffer[0] & 0x1F;
				/*skip extractor*/
				if (nal_type == 31)
					continue;			
				/*copy to new bitstream*/
				gf_bs_write_int(dst_bs, size, nalu_size_length);
				gf_bs_write_data(dst_bs, buffer, size);
			}
			gf_bs_del(bs);
			bs = NULL;
			gf_isom_sample_del(&samp);
			samp = NULL;
			cur_sample[t]++;
		}
		
		/*add sapmle to track*/
		if (gf_bs_get_position(dst_bs))
		{
			if (first_sample)
			{
				first_DTS = avc_samp->DTS;
				first_sample = 0;
			}
			dst_samp = gf_isom_sample_new();
			dst_samp->CTS_Offset = avc_samp->CTS_Offset;
			dst_samp->DTS = avc_samp->DTS - first_DTS;
			dst_samp->IsRAP = avc_samp->IsRAP;
			gf_bs_get_content(dst_bs, &dst_samp->data, &dst_samp->dataLength);
			e = gf_isom_update_sample(file, merge_track, i, dst_samp, 1);
			if (e)
				goto exit;
		}
		gf_isom_sample_del(&avc_samp);
		avc_samp = NULL;
		gf_bs_del(dst_bs);
		dst_bs = NULL;
		gf_isom_sample_del(&dst_samp);
		dst_samp = NULL;
	}

	/*Add EditList if nessessary*/
	if (!first_DTS)
	{
		media_ts = gf_isom_get_media_timescale(file, merge_track);
		moov_ts = gf_isom_get_timescale(file);
		offset = (u32)(first_DTS) * moov_ts / media_ts;
		dur = gf_isom_get_media_duration(file, merge_track) * moov_ts / media_ts;
		gf_isom_set_edit_segment(file, merge_track, 0, offset, 0, GF_ISOM_EDIT_EMPTY);
		gf_isom_set_edit_segment(file, merge_track, offset, dur, 0, GF_ISOM_EDIT_NORMAL);
	}

	/*Delete SVC track(s) that references to ref_track*/
	for (t = 1; t <= num_track; t++)
	{
		if (gf_isom_has_track_reference(file, t, GF_ISOM_REF_BASE, ref_trackID) && (t != merge_track))
		{
			gf_isom_remove_track(file, t);
			num_track--; //we removed one track from file
			t--;
		}
	}

exit:
	if (avc_samp) gf_isom_sample_del(&avc_samp);
	if (samp) gf_isom_sample_del(&samp);
	if (dst_samp) gf_isom_sample_del(&dst_samp);
	if (svccfg) gf_odf_avc_cfg_del(svccfg);
	if (cfg) gf_odf_avc_cfg_del(cfg);
	if (bs) gf_bs_del(bs);
	if (dst_bs) gf_bs_del(dst_bs);
	if (buffer) gf_free(buffer);
	if (DQId) gf_free(DQId);
	if (list_track_sorted) gf_free(list_track_sorted);
	if (cur_sample) gf_free(cur_sample);
	if (max_sample) gf_free(max_sample);
	if (DTS_offset) gf_free(DTS_offset);
	for (t = 1; t <= gf_isom_get_track_count(file); t++)
		gf_isom_set_nalu_extract_mode(file, t, GF_ISOM_NALU_EXTRACT_DEFAULT);
	return e;
}


/* Split SHVC layers */
static GF_HEVCParamArray *alloc_hevc_param_array(GF_HEVCConfig *hevc_cfg, u8 type) 
{
	GF_HEVCParamArray *ar;
	u32 i, count = hevc_cfg->param_array ? gf_list_count(hevc_cfg->param_array) : 0;
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = gf_list_get(hevc_cfg->param_array, i);
		if (ar->type==type) return ar;
	}
	GF_SAFEALLOC(ar, GF_HEVCParamArray);
	ar->nalus = gf_list_new();
	ar->type = type;
	gf_list_add(hevc_cfg->param_array, ar);
	return ar;
}

typedef struct
{
	u32 track_num;
	u32 layer_id;
	GF_HEVCConfig *shvccfg;
	GF_BitStream *bs;
	u32 data_offset, data_size;
	u32 temporal_id;
} SHVCTrackInfo;

GF_EXPORT
GF_Err gf_media_split_shvc(GF_ISOFile *file, u32 track, Bool splitAll, Bool use_extractors)
{
	SHVCTrackInfo sti[64];
	GF_HEVCConfig *hevccfg, *shvccfg;
	u32 i, count, cur_extract_mode, j, k;
	char *nal_data=NULL;
	u32 nal_alloc_size;
	GF_Err e = GF_OK;

	hevccfg = gf_isom_hevc_config_get(file, track, 1);
	shvccfg = gf_isom_shvc_config_get(file, track, 1);
	if (!shvccfg) {
		if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
		return GF_OK;
	}

	cur_extract_mode = gf_isom_get_nalu_extract_mode(file, track);
	gf_isom_set_nalu_extract_mode(file, track, GF_ISOM_NALU_EXTRACT_INSPECT);

	memset(sti, 0, sizeof(sti));
	sti[0].track_num = track;

	//split all SPS/PPS/VPS from svccfg
	count = gf_list_count(shvccfg->param_array);
	for (i=0; i<count; i++) {
		u32 count2;
		GF_HEVCParamArray *s_ar;
		GF_HEVCParamArray *ar = gf_list_get(shvccfg->param_array, i);
		count2 = gf_list_count(ar->nalus);
		for (j=0; j<count2; j++) {
			GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
//			u8 nal_type = (sl->data[0] & 0x7E) >> 1;
			u8 layer_id = ((sl->data[0] & 0x1) << 5) | (sl->data[1] >> 3);
			
			//this should not happen
			if (!layer_id) continue;

			if (!splitAll) layer_id = 1;

			if (!sti[layer_id].shvccfg) {
				sti[layer_id].shvccfg = gf_odf_hevc_cfg_new();
				sti[layer_id].shvccfg->is_shvc = 1;
				sti[layer_id].shvccfg->complete_representation = 1;
				sti[layer_id].shvccfg->num_layers = 1;
				//TODO - set scalability mask flag
			}

			s_ar = alloc_hevc_param_array(sti[layer_id].shvccfg, ar->type);
			gf_list_add(s_ar->nalus, sl);
			gf_list_rem(ar->nalus, j);
			j--;
			count2--;
		}
	}
	//remove shvc config
	e = gf_isom_shvc_config_update(file, track, 1, NULL, 0);
	if (e) goto exit;

	nal_alloc_size = 10000;
	nal_data = gf_malloc(sizeof(char) * nal_alloc_size);
	//parse all samples
	count = gf_isom_get_sample_count(file, track);
	for (i=0; i<count; i++) {
		GF_BitStream *bs;
		u32 di;
		GF_ISOSample *sample;
		u8 max_layer_id = 0;
		
		sample = gf_isom_get_sample(file, track, i+1, &di);
		
		bs = gf_bs_new(sample->data, sample->dataLength, GF_BITSTREAM_READ);
		while (gf_bs_available(bs)) {
			u8 orig_layer_id;
			u32 size = gf_bs_read_int(bs, shvccfg->nal_unit_size*8);
			u8 fzero = gf_bs_read_int(bs, 1);
			u8 nal_type = gf_bs_read_int(bs, 6);
			u8 layer_id = orig_layer_id = gf_bs_read_int(bs, 6);
			u8 temporal_id = gf_bs_read_int(bs, 3);

			if (!splitAll) layer_id = 1;

			if (max_layer_id < layer_id) 
				max_layer_id = layer_id;

			if (!sti[layer_id].bs)
				sti[layer_id].bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

			gf_bs_write_int(sti[layer_id].bs, size, shvccfg->nal_unit_size*8);
			gf_bs_write_int(sti[layer_id].bs, fzero, 1);
			gf_bs_write_int(sti[layer_id].bs, nal_type, 6);
			gf_bs_write_int(sti[layer_id].bs, orig_layer_id, 6);
			gf_bs_write_int(sti[layer_id].bs, temporal_id, 3);
			size -= 2;

			if (!sti[layer_id].temporal_id || (sti[layer_id].temporal_id < temporal_id)) {
				sti[layer_id].temporal_id = temporal_id;
			}

			if (size>nal_alloc_size) {
				nal_alloc_size = size;
				nal_data = gf_realloc(nal_data, nal_alloc_size);
			}

			gf_bs_read_data(bs, nal_data, size);
			gf_bs_write_data(sti[layer_id].bs, nal_data, size);
		}
		gf_bs_del(bs);


		gf_free(sample->data);
		sample->data = NULL;
		sample->dataLength = 0;
		//reset all samples on scalable layers
		for (j=0; j<=max_layer_id; j++) {
			if (! sti[j].bs) {
				sti[j].data_offset =  sti[j].data_size = 0;
				continue;
			}

			//clone track
			if (! sti[j].track_num) {
				u32 track_id = gf_isom_get_track_id(file, track);
				e = gf_isom_clone_track(file, track, file, 0, &sti[j].track_num);
				if (e) goto exit;

				e = gf_isom_shvc_config_update(file, sti[j].track_num, 1, sti[j].shvccfg, 0);
				if (e) goto exit;

				gf_isom_set_track_reference(file, sti[j].track_num, GF_4CC('s','b','a','s'), track_id);
				//get lower layer
				for (k=j; k>0; k--) {
					if (sti[k-1].track_num) {
						u32 track_id = gf_isom_get_track_id(file, sti[k-1].track_num);
						gf_isom_set_track_reference(file, sti[j].track_num, GF_4CC('s','c','a','l'), track_id);
						if (!use_extractors) break;
					}
				}

				//add empty sample at DTS 0 
				if (sample->DTS ) {
					u64 dts = sample->DTS;
					s32 cts = sample->CTS_Offset;
					sample->DTS = 0;
					sample->CTS_Offset = 0;
					gf_isom_add_sample(file, sti[j].track_num, 1, sample);
					sample->DTS = dts;
					sample->CTS_Offset = cts;
				}
			}

			if (j && use_extractors) {
				GF_BitStream *xbs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				//get all lower layers
				for (k=0; k<j; k++) {
					u8 trefidx;
					if (!sti[k].data_size) continue;
					//extractor is 12 bytes
					gf_bs_write_int(xbs, 2*shvccfg->nal_unit_size + 4, 8*shvccfg->nal_unit_size);
					gf_bs_write_int(xbs, 0, 1);
					gf_bs_write_int(xbs, 49, 6); //extractor
					gf_bs_write_int(xbs, k, 6);
					gf_bs_write_int(xbs, sti[k].temporal_id, 3);
					//set ref track index
					trefidx = (u8) gf_isom_has_track_reference(file, sti[j].track_num, GF_ISOM_REF_SCAL, gf_isom_get_track_id(file, sti[k].track_num) );
					gf_bs_write_int(xbs, trefidx, 8);
					// no sample offset
					gf_bs_write_int(xbs, 0, 8);
					// data offset: we start from begining of the sample data, not the extractor
					gf_bs_write_int(xbs, sti[k].data_offset, 8*shvccfg->nal_unit_size);
					gf_bs_write_int(xbs, sti[k].data_size, 8*shvccfg->nal_unit_size);
				}

				gf_bs_get_content(xbs, &sample->data, &sample->dataLength);
				gf_bs_del(xbs);

				//we wrote all our references, store offset for upper layers
				sti[j].data_offset = sample->dataLength;
				e = gf_isom_add_sample(file, sti[j].track_num, 1, sample);
				if (e) goto exit;
				gf_free(sample->data);

				//get real content, remember its size and add it to the new bs
				gf_bs_get_content(sti[j].bs, &sample->data, &sample->dataLength);
				e = gf_isom_append_sample_data(file, sti[j].track_num, sample->data, sample->dataLength);
				if (e) goto exit;


			} else {
				//get sample content
				gf_bs_get_content(sti[j].bs, &sample->data, &sample->dataLength);
				sti[j].data_offset = 0;
				sti[j].data_size = sample->dataLength;


				//add sample
				if (j) {
					e = gf_isom_add_sample(file, sti[j].track_num, 1, sample);
				} else {
					e = gf_isom_update_sample(file, sti[j].track_num, i+1, sample, 1);
				}
				if (e) goto exit;
			}
			
			gf_bs_del(sti[j].bs);
			sti[j].bs = NULL;
				

			gf_free(sample->data);
			sample->data = NULL;
			sample->dataLength = 0;
		}
		gf_isom_sample_del(&sample);

		//reset all scalable info
		for (j=0; j<=max_layer_id; j++) {
			sti[j].temporal_id = 0;
		}
	}

exit:
	//reset all scalable info
	for (j=0; j<64; j++) {
		if (sti[j].shvccfg) gf_odf_hevc_cfg_del(sti[j].shvccfg);
	}
	gf_isom_set_nalu_extract_mode(file, track, cur_extract_mode);
	if (shvccfg) gf_odf_hevc_cfg_del(shvccfg);
	if (hevccfg) gf_odf_hevc_cfg_del(hevccfg);
	if (nal_data) gf_free(nal_data);
	return e;
}

GF_EXPORT
GF_Err gf_media_change_pl(GF_ISOFile *file, u32 track, u32 profile, u32 level)
{
	u32 i, count, stype;
	GF_Err e;
	GF_AVCConfig *avcc;

	stype = gf_isom_get_media_subtype(file, track, 1);
	switch (stype) {
	case GF_ISOM_SUBTYPE_AVC_H264:
	case GF_ISOM_SUBTYPE_AVC2_H264:
	case GF_ISOM_SUBTYPE_AVC3_H264:
	case GF_ISOM_SUBTYPE_AVC4_H264:
		break;
	default:
		return GF_OK;
	}

	avcc = gf_isom_avc_config_get(file, track, 1);
	if (level) avcc->AVCLevelIndication = level;
	if (profile) avcc->AVCProfileIndication = profile;
	count = gf_list_count(avcc->sequenceParameterSets);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *slc = gf_list_get(avcc->sequenceParameterSets, i);
		if (profile) slc->data[1] = profile;
		if (level) slc->data[3] = level;
	}
	e = gf_isom_avc_config_update(file, track, 1, avcc);
	assert (e == GF_OK);
	gf_odf_avc_cfg_del(avcc);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_media_split_hevc_tiles(GF_ISOFile *file)
{
#if defined(GPAC_DISABLE_HEVC) || defined(GPAC_DISABLE_AV_PARSERS)
	return GF_NOT_SUPPORTED;
#else
	u32 i, j, cur_tile, count, stype, track, nb_tracks, di, nalu_size_length;
	s32 pps_idx=-1;
	GF_Err e;
//	HEVCSliceInfo n_state;
	HEVCState hevc;
	u32 *tiles_track;
	GF_HEVCConfig *hvcc;

	track = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		stype = gf_isom_get_media_subtype(file, i+1, 1);
		switch (stype) {
		case GF_ISOM_SUBTYPE_HVC1:
		case GF_ISOM_SUBTYPE_HEV1:

			if (track) return GF_NOT_SUPPORTED;
			track = i+1;
			break;
		default:
			break;
		}
	}
	if (!track) return GF_NOT_SUPPORTED;

	hvcc = gf_isom_hevc_config_get(file, track, 1);
	nalu_size_length = hvcc->nal_unit_size;

	count = gf_list_count(hvcc->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = gf_list_get(hvcc->param_array, i);
		if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
			GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, 0);
			if (sl)
				pps_idx = gf_media_hevc_read_pps(sl->data, sl->size, &hevc);
		}
	}
	gf_odf_hevc_cfg_del(hvcc);


	if (pps_idx==-1) return GF_BAD_PARAM;
	if (! hevc.pps[pps_idx].tiles_enabled_flag) return GF_OK;

	nb_tracks = hevc.pps[pps_idx].num_tile_columns * hevc.pps[pps_idx].num_tile_rows;
	tiles_track = gf_malloc(sizeof(u32) * nb_tracks);
	for (i=0; i<nb_tracks; i++) {
		e = gf_isom_clone_track(file, track, file, 0, &tiles_track[i] );
		if (e) goto err_exit;

		gf_isom_set_track_reference(file, track, GF_4CC('d','o','n','d') , gf_isom_get_track_id(file, tiles_track[i]) );
	}

	count = gf_isom_get_sample_count(file, track);
	for (i=0; i<count; i++) {
		u8 *data;
		u32 size;
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		GF_ISOSample *sample = gf_isom_get_sample(file, track, i+1, &di);

	
		data = sample->data;
		size = sample->dataLength;
		sample->data = NULL;
		sample->dataLength = 0;

		for (j=0; j<nb_tracks; j++) {
			e = gf_isom_add_sample(file, tiles_track[j], 1, sample);
			if (e)
				goto err_exit;
		}
		sample->data = data;
		cur_tile = 0;

		while (size) {
			u8 nal_type = 0;
			u32 nalu_size = 0;
			for (j=0; j<nalu_size_length; j++) {
				nalu_size = (nalu_size<<8) + data[j];
			}
			nal_type = (data[nalu_size_length] & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_SLICE_TRAIL_N:
			case GF_HEVC_NALU_SLICE_TRAIL_R:
			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_TSA_R:
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_STSA_R:
			case GF_HEVC_NALU_SLICE_BLA_W_LP:
			case GF_HEVC_NALU_SLICE_BLA_W_DLP:
			case GF_HEVC_NALU_SLICE_BLA_N_LP:
			case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			case GF_HEVC_NALU_SLICE_IDR_N_LP:
			case GF_HEVC_NALU_SLICE_CRA:
			case GF_HEVC_NALU_SLICE_RADL_R:
			case GF_HEVC_NALU_SLICE_RASL_R:
				//ret = hevc_parse_slice_segment(bs, hevc, &n_state);
				e = gf_isom_append_sample_data(file, tiles_track[cur_tile], data, nalu_size + nalu_size_length);
				if (e)
					goto err_exit;
				cur_tile++;
				break;
			default:
				gf_bs_write_data(bs, data, nalu_size + nalu_size_length);
				break;
			}
			data += nalu_size + nalu_size_length;
			size -= nalu_size + nalu_size_length;
		}
		gf_free(sample->data);
		gf_bs_get_content(bs, &sample->data, &sample->dataLength);
		gf_bs_del(bs);

		e = gf_isom_update_sample(file, track, i+1, sample, 1);
		if (e) goto err_exit;
		gf_isom_sample_del(&sample);
	}

err_exit:
	gf_free(tiles_track);
	return e;
#endif
}

#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

typedef struct
{
	u32 TrackID;
	u32 SampleNum, SampleCount;
	u32 FragmentLength;
	u32 OriginalTrack;
	u32 TimeScale, MediaType, DefaultDuration;
} GF_TrackFragmenter;

GF_EXPORT
GF_Err gf_media_fragment_file(GF_ISOFile *input, const char *output_file, Double max_duration_sec)
{
	u8 NbBits;
	u32 i, TrackNum, descIndex, j, count;
	u32 defaultDuration, defaultSize, defaultDescriptionIndex, defaultRandomAccess, nb_samp, nb_done;
	u8 defaultPadding;
	u16 defaultDegradationPriority;
	GF_Err e;
	const char *tag;
	u32 tag_len;
	GF_ISOFile *output;
	GF_ISOSample *sample, *next;
	GF_List *fragmenters;
	u32 MaxFragmentDuration;
	GF_TrackFragmenter *tf;

	//create output file
	output = gf_isom_open(output_file, GF_ISOM_OPEN_WRITE, NULL);
	if (!output) return gf_isom_last_error(NULL);


	nb_samp = 0;
	fragmenters = gf_list_new();

	/*FIXME - ALL THESE SHOULD GO DO A clone_movie item !!*/
	e = gf_isom_set_brand_info(output, GF_ISOM_BRAND_MP42, 1);
	if (e) goto err_exit;
	e = gf_isom_modify_alternate_brand(output, GF_ISOM_BRAND_ISOM, 1);
	if (e) goto err_exit;

	//copy movie desc
	gf_isom_clone_root_od(input, output);
	//clone copyright
	count = gf_isom_get_copyright_count(input);
	if (count) {
		const char *lang, *note;
		for (i=0; i<count; i++) {
			gf_isom_get_copyright(input, i+1, &lang, &note);
			gf_isom_set_copyright(output, (char *)lang, (char *)note);
		}
	}
	count = gf_isom_get_chapter_count(input, 0);
	if (count) {
		const char *name;
		u64 time;
		for (i=0; i<count; i++) {
			gf_isom_get_chapter(input, 0, i+1, &time, &name);
			gf_isom_add_chapter(output, 0, time, (char *)name);
		}
	}

	if (gf_isom_apple_get_tag(input, 0, &tag, &tag_len) == GF_OK) {
		for (i=GF_ISOM_ITUNE_ALBUM; i<(u32) GF_ISOM_ITUNE_WRITER; i++) {
			if (gf_isom_apple_get_tag(input, GF_ISOM_ITUNE_NAME, &tag, &tag_len)==GF_OK)
				gf_isom_apple_set_tag(output, GF_ISOM_ITUNE_NAME, tag, tag_len);
		}
	}

	MaxFragmentDuration = (u32) (max_duration_sec * 1000);
	//duplicates all tracks
	for (i=0; i<gf_isom_get_track_count(input); i++) {
		e = gf_isom_clone_track(input, i+1, output, 0, &TrackNum);
		if (e) goto err_exit;

		//if few samples don't fragment track
		count = gf_isom_get_sample_count(input, i+1);
		if (count<=2) {
			for (j=0; j<count; j++) {
				sample = gf_isom_get_sample(input, i+1, j+1, &descIndex);
				e = gf_isom_add_sample(output, TrackNum, 1, sample);
				gf_isom_sample_del(&sample);
				if (e) goto err_exit;
			}
		}
		//otherwise setup fragmented
		else {
			gf_isom_get_fragment_defaults(input, i+1,
										 &defaultDuration, &defaultSize, &defaultDescriptionIndex, &defaultRandomAccess, &defaultPadding, &defaultDegradationPriority);
			//otherwise setup fragmentation
			e = gf_isom_setup_track_fragment(output, gf_isom_get_track_id(output, TrackNum),
						defaultDescriptionIndex, defaultDuration,
						defaultSize, (u8) defaultRandomAccess,
						defaultPadding, defaultDegradationPriority);
			if (e) goto err_exit;

			GF_SAFEALLOC(tf, GF_TrackFragmenter);
			tf->TrackID = gf_isom_get_track_id(output, TrackNum);
			tf->SampleCount = gf_isom_get_sample_count(input, i+1);
			tf->OriginalTrack = i+1;
			tf->TimeScale = gf_isom_get_media_timescale(input, i+1);
			tf->MediaType = gf_isom_get_media_type(input, i+1);
			tf->DefaultDuration = defaultDuration;
			gf_list_add(fragmenters, tf);
			nb_samp += count;
		}

		if (gf_isom_is_track_in_root_od(input, i+1)) gf_isom_add_track_to_root_od(output, TrackNum);
		//copy user data ?
	}


	//flush movie
	e = gf_isom_finalize_for_fragment(output, 0);
	if (e) goto err_exit;

	nb_done = 0;

	while ( (count = gf_list_count(fragmenters)) ) {

		e = gf_isom_start_fragment(output, 1);
		if (e) goto err_exit;
		//setup some default
		for (i=0; i<count; i++) {
			tf = (GF_TrackFragmenter *)gf_list_get(fragmenters, i);
			if (tf->MediaType == GF_ISOM_MEDIA_VISUAL) {
				e = gf_isom_set_fragment_option(output, tf->TrackID, GF_ISOM_TRAF_RANDOM_ACCESS, 1);
				if (e) goto err_exit;
			}
		}
		sample = NULL;

		/*copy PSSHs from moov to the first moof*/
		if (!nb_done) {
			e = gf_isom_clone_pssh(output, input, GF_TRUE);
			if (e) goto err_exit;
		}

		//process track by track
		for (i=0; i<count; i++) {
			tf = (GF_TrackFragmenter *)gf_list_get(fragmenters, i);

			gf_isom_set_nalu_extract_mode(input, tf->OriginalTrack, GF_ISOM_NALU_EXTRACT_INSPECT);

			//ok write samples
			while (1) {
				if (!sample) {
					sample = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 1, &descIndex);
				}
				gf_isom_get_sample_padding_bits(input, tf->OriginalTrack, tf->SampleNum+1, &NbBits);

				next = gf_isom_get_sample(input, tf->OriginalTrack, tf->SampleNum + 2, &j);
				if (next) {
					defaultDuration = (u32) (next->DTS - sample->DTS);
				} else {
					defaultDuration = tf->DefaultDuration;
				}

				e = gf_isom_fragment_add_sample(output, tf->TrackID, sample, descIndex, defaultDuration, NbBits, 0, 0);
				if (e) goto err_exit;

				e = gf_isom_fragment_add_sai(output, input, tf->TrackID, tf->SampleNum + 1);
				if (e) goto err_exit;

				/*copy subsample information*/
				e = gf_isom_fragment_copy_subsample(output, tf->TrackID, input, tf->OriginalTrack, tf->SampleNum + 1);
				if (e) 
					goto err_exit;

				gf_set_progress("ISO File Fragmenting", nb_done, nb_samp);
				nb_done++;

				gf_isom_sample_del(&sample);
				sample = next;
				tf->FragmentLength += defaultDuration;
				tf->SampleNum += 1;

				//end of track fragment or track
				if ((tf->SampleNum==tf->SampleCount) || 
                    /* TODO: should probably test the time position (not only duratino) to avoid drift */
                    (tf->FragmentLength*1000 >= MaxFragmentDuration*tf->TimeScale)) {
					gf_isom_sample_del(&next);
					sample = next = NULL;
					tf->FragmentLength = 0;
					break;
				}
			}
			if (tf->SampleNum==tf->SampleCount) {
				gf_free(tf);
				gf_list_rem(fragmenters, i);
				i--;
				count --;
			}
		}
	}

err_exit:
	while (gf_list_count(fragmenters)) {
		tf = (GF_TrackFragmenter *)gf_list_get(fragmenters, 0);
		gf_free(tf);
		gf_list_rem(fragmenters, 0);
	}
	gf_list_del(fragmenters);
	if (e) gf_isom_delete(output);
	else gf_isom_close(output);
	gf_set_progress("ISO File Fragmenting", nb_samp, nb_samp);
	return e;
}


GF_EXPORT
GF_Err gf_media_split_tiles(GF_ISOFile *file)
{
#if defined(GPAC_DISABLE_HEVC) || defined(GPAC_DISABLE_AV_PARSERS)
	return GF_NOT_SUPPORTED;
#else
	u32 i, j, count, stype, track, nb_tracks, di, nalu_size_length;
	s32 pps_idx=-1;
//	GF_Err e;
//	HEVCSliceInfo n_state;
	HEVCState hevc;
	u32 *tiles_track;
	GF_HEVCConfig *hvcc;

	track = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		stype = gf_isom_get_media_subtype(file, i+1, 1);
		switch (stype) {
		case GF_ISOM_SUBTYPE_HVC1:
		case GF_ISOM_SUBTYPE_HEV1:

			if (track) return GF_NOT_SUPPORTED;
			track = i+1;
			break;
		default:
			break;
		}
	}
	if (!track) return GF_NOT_SUPPORTED;

	hvcc = gf_isom_hevc_config_get(file, track, 1);
	nalu_size_length = hvcc->nal_unit_size;

	count = gf_list_count(hvcc->param_array);
	for (i=0; i<count; i++) {
		GF_HEVCParamArray *ar = gf_list_get(hvcc->param_array, i);
		if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
			GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, 0);
			if (sl)
				pps_idx = gf_media_hevc_read_pps(sl->data, sl->size, &hevc);
		}
	}
	gf_odf_hevc_cfg_del(hvcc);


	if (pps_idx==-1) return GF_BAD_PARAM;
	if (! hevc.pps[pps_idx].tiles_enabled_flag) return GF_OK;

	nb_tracks = hevc.pps[pps_idx].num_tile_columns * hevc.pps[pps_idx].num_tile_rows;
	tiles_track = malloc(sizeof(u32) * nb_tracks);
	for (i=0; i<nb_tracks; i++) {
		gf_isom_clone_track(file, track, file, 0, &tiles_track[i] );
	}

	count = gf_isom_get_sample_count(file, track);
	for (i=0; i<nb_tracks; i++) {
		u8 *data;
		u32 size;
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		GF_ISOSample *sample = gf_isom_get_sample(file, track, i+1, &di);

	
		data = sample->data;
		size = sample->dataLength;
		sample->data = NULL;
		sample->dataLength = 0;

		for (j=0; j<nb_tracks; j++) {
			gf_isom_add_sample(file, tiles_track[i], 1, sample);
		}

		while (size) {
			u8 nal_type = 0;
			u32 nalu_size = 0;
			for (j=0; j<nalu_size_length; j++) {
				nalu_size = (nalu_size<<8) + data[j];
			}
			nal_type = (data[nalu_size_length] & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_SLICE_TRAIL_N:
			case GF_HEVC_NALU_SLICE_TRAIL_R:
			case GF_HEVC_NALU_SLICE_TSA_N:
			case GF_HEVC_NALU_SLICE_TSA_R:
			case GF_HEVC_NALU_SLICE_STSA_N:
			case GF_HEVC_NALU_SLICE_STSA_R:
			case GF_HEVC_NALU_SLICE_BLA_W_LP:
			case GF_HEVC_NALU_SLICE_BLA_W_DLP:
			case GF_HEVC_NALU_SLICE_BLA_N_LP:
			case GF_HEVC_NALU_SLICE_IDR_W_DLP:
			case GF_HEVC_NALU_SLICE_IDR_N_LP:
			case GF_HEVC_NALU_SLICE_CRA:
			case GF_HEVC_NALU_SLICE_RADL_R:
			case GF_HEVC_NALU_SLICE_RASL_R:
				//ret = hevc_parse_slice_segment(bs, hevc, &n_state);
				gf_isom_append_sample_data(file, track, data, nalu_size + nalu_size_length);
				break;
			default:
				gf_bs_write_data(bs, data, nalu_size + nalu_size_length);
				break;
			}
			data += nalu_size + nalu_size_length;
			size -= nalu_size + nalu_size_length;
		}
		gf_free(data);
		gf_bs_get_content(bs, &sample->data, &sample->dataLength);
		gf_bs_del(bs);

		gf_isom_update_sample(file, track, i+1, sample, 1);
		gf_isom_sample_del(&sample);
	}

	return GF_OK;
#endif //GPAC_DISABLE_HEVC

}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

