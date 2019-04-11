/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#include <gpac/scene_manager.h>
#include <gpac/constants.h>
#include <gpac/bifs.h>
#ifndef GPAC_DISABLE_LASER
#include <gpac/laser.h>
#endif



#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_LOADER_ISOM)

static void UpdateODCommand(GF_ISOFile *mp4, GF_ODCom *com)
{
	u32 i, j;
	const char *szName;
	char szPath[2048];

	szName = gf_isom_get_filename(mp4);
	if (com->tag == GF_ODF_OD_UPDATE_TAG) {
		GF_ObjectDescriptor *od;
		GF_ODUpdate *odU = (GF_ODUpdate *)com;
		i=0;
		while ((od = (GF_ObjectDescriptor *)gf_list_enum(odU->objectDescriptors, &i))) {
			GF_ESD *esd;
			j=0;
			while ((esd = (GF_ESD *)gf_list_enum(od->ESDescriptors, &j))) {
				Bool import = 1;
				if (esd->URLString) continue;
				switch (esd->decoderConfig->streamType) {
				case GF_STREAM_OD:
					import = 0;
					break;
				case GF_STREAM_SCENE:
					if ((esd->decoderConfig->objectTypeIndication != GPAC_OTI_SCENE_AFX) &&
					        (esd->decoderConfig->objectTypeIndication != GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE)
					   ) {
						import = 0;
					}
					break;
				/*dump the OCR track duration in case the OCR is used by media controls & co*/
				case GF_STREAM_OCR:
				{
					u32 track;
					Double dur;
					GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
					gf_list_add(esd->extensionDescriptors, mi);
					track = gf_isom_get_track_by_id(mp4, esd->ESID);
					dur = (Double) (s64) gf_isom_get_track_duration(mp4, track);
					dur /= gf_isom_get_timescale(mp4);
					mi->duration = (u32) (dur * 1000);
					import = 0;
				}
				break;
				default:
					break;
				}
				if (import) {
					GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
					gf_list_add(esd->extensionDescriptors, mi);
					sprintf(szPath, "%s#%d", szName, esd->ESID);
					mi->file_name = gf_strdup(szPath);
					mi->streamFormat = gf_strdup("MP4");
				}
			}
		}
		return;
	}
	if (com->tag == GF_ODF_ESD_UPDATE_TAG) {
		GF_ESD *esd;
		GF_ESDUpdate *esdU = (GF_ESDUpdate *)com;
		i=0;
		while ((esd = (GF_ESD *)gf_list_enum(esdU->ESDescriptors, &i))) {
			Bool import = 1;
			if (esd->URLString) continue;
			switch (esd->decoderConfig->streamType) {
			case GF_STREAM_OD:
				import = 0;
				break;
			case GF_STREAM_SCENE:
				if ((esd->decoderConfig->objectTypeIndication != GPAC_OTI_SCENE_AFX) &&
				        (esd->decoderConfig->objectTypeIndication != GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE)
				   ) {
					import = 0;
				}
				break;
			/*dump the OCR track duration in case the OCR is used by media controls & co*/
			case GF_STREAM_OCR:
			{
				u32 track;
				Double dur;
				GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
				gf_list_add(esd->extensionDescriptors, mi);
				track = gf_isom_get_track_by_id(mp4, esd->ESID);
				dur = (Double) (s64) gf_isom_get_track_duration(mp4, track);
				dur /= gf_isom_get_timescale(mp4);
				mi->duration = (u32) (dur * 1000);
				import = 0;
			}
			break;
			default:
				break;
			}
			if (import) {
				GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
				gf_list_add(esd->extensionDescriptors, mi);
				sprintf(szPath, "%s#%d", szName, esd->ESID);
				mi->file_name = gf_strdup(szPath);
				mi->streamFormat = gf_strdup("MP4");
			}
		}
		return;
	}
}

static void mp4_report(GF_SceneLoader *load, GF_Err e, char *format, ...)
{
#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_PARSER, e ? GF_LOG_ERROR : GF_LOG_WARNING)) {
		char szMsg[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(szMsg, 1024, format, args);
		va_end(args);
		GF_LOG((u32) (e ? GF_LOG_ERROR : GF_LOG_WARNING), GF_LOG_PARSER, ("[MP4 Loading] %s\n", szMsg) );
	}
#endif
}

static GF_Err gf_sm_load_run_isom(GF_SceneLoader *load)
{
	GF_Err e;
	FILE *logs;
	u32 i, j, di, nbBifs, nbLaser, nb_samp, samp_done, init_offset;
	GF_StreamContext *sc;
	GF_ESD *esd;
	GF_ODCodec *od_dec;
#ifndef GPAC_DISABLE_BIFS
	GF_BifsDecoder *bifs_dec;
#endif
#ifndef GPAC_DISABLE_LASER
	GF_LASeRCodec *lsr_dec;
#endif

	if (!load || !load->isom) return GF_BAD_PARAM;

	nbBifs = nbLaser = 0;
	e = GF_OK;
#ifndef GPAC_DISABLE_BIFS
	bifs_dec = gf_bifs_decoder_new(load->scene_graph, 1);
	gf_bifs_decoder_set_extraction_path(bifs_dec, load->localPath, load->fileName);
#endif
	od_dec = gf_odf_codec_new();
	logs = NULL;
#ifndef GPAC_DISABLE_LASER
	lsr_dec = gf_laser_decoder_new(load->scene_graph);
#endif
	esd = NULL;
	/*load each stream*/
	nb_samp = 0;
	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		switch (type) {
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OD:
			nb_samp += gf_isom_get_sample_count(load->isom, i+1);
			break;
		default:
			break;
		}
	}
	samp_done = 1;
	gf_isom_text_set_streaming_mode(load->isom, 1);

	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		switch (type) {
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_OD:
			break;
		default:
			continue;
		}
		esd = gf_isom_get_esd(load->isom, i+1, 1);
		if (!esd) continue;


		if ((esd->decoderConfig->objectTypeIndication == GPAC_OTI_SCENE_AFX) ||
		        (esd->decoderConfig->objectTypeIndication == GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE)
		   ) {
			nb_samp += gf_isom_get_sample_count(load->isom, i+1);
			continue;
		}
		sc = gf_sm_stream_new(load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
		sc->streamType = esd->decoderConfig->streamType;
		sc->ESID = esd->ESID;
		sc->objectType = esd->decoderConfig->objectTypeIndication;
		sc->timeScale = gf_isom_get_media_timescale(load->isom, i+1);

		/*we still need to reconfig the BIFS*/
		if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
#ifndef GPAC_DISABLE_BIFS
			/*BIFS*/
			if (esd->decoderConfig->objectTypeIndication<=2) {
				if (!esd->dependsOnESID && nbBifs && !i)
					mp4_report(load, GF_OK, "several scene namespaces used or improper scene dependencies in file - import may be incorrect");
				if (!esd->decoderConfig->decoderSpecificInfo) {
					/* Hack for T-DMB non compliant streams */
					e = gf_bifs_decoder_configure_stream(bifs_dec, esd->ESID, NULL, 0, esd->decoderConfig->objectTypeIndication);
				} else {
					e = gf_bifs_decoder_configure_stream(bifs_dec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
				}
				if (e) goto exit;
				nbBifs++;
			}
#endif

#ifndef GPAC_DISABLE_LASER
			/*LASER*/
			if (esd->decoderConfig->objectTypeIndication==0x09) {
				if (!esd->dependsOnESID && nbBifs && !i)
					mp4_report(load, GF_OK, "several scene namespaces used or improper scene dependencies in file - import may be incorrect");
				e = gf_laser_decoder_configure_stream(lsr_dec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
				if (e) goto exit;
				nbLaser++;
			}
#endif
		}

		init_offset = 0;
		/*dump all AUs*/
		for (j=0; j<gf_isom_get_sample_count(load->isom, i+1); j++) {
			GF_AUContext *au;
			GF_ISOSample *samp = gf_isom_get_sample(load->isom, i+1, j+1, &di);
			if (!samp) {
				mp4_report(load, gf_isom_last_error(load->isom), "Unable to fetch sample %d from track ID %d - aborting track import", j+1, gf_isom_get_track_id(load->isom, i+1));
				break;
			}
			/*check if track has initial offset*/
			if (!j && gf_isom_get_edit_segment_count(load->isom, i+1)) {
				u64 EditTime, dur, mtime;
				u8 mode;
				gf_isom_get_edit_segment(load->isom, i+1, 1, &EditTime, &dur, &mtime, &mode);
				if (mode==GF_ISOM_EDIT_EMPTY) {
					init_offset = (u32) (dur * sc->timeScale / gf_isom_get_timescale(load->isom) );
				}
			}
			samp->DTS += init_offset;

			au = gf_sm_stream_au_new(sc, samp->DTS, ((Double)(s64) samp->DTS) / sc->timeScale, (samp->IsRAP==RAP) ? 1 : 0);

			if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
#ifndef GPAC_DISABLE_BIFS
				if (esd->decoderConfig->objectTypeIndication<=2)
					e = gf_bifs_decode_command_list(bifs_dec, esd->ESID, samp->data, samp->dataLength, au->commands);
#endif
#ifndef GPAC_DISABLE_LASER
				if (esd->decoderConfig->objectTypeIndication==0x09)
					e = gf_laser_decode_command_list(lsr_dec, esd->ESID, samp->data, samp->dataLength, au->commands);
#endif
			} else {
				e = gf_odf_codec_set_au(od_dec, samp->data, samp->dataLength);
				if (!e) e = gf_odf_codec_decode(od_dec);
				if (!e) {
					while (1) {
						GF_ODCom *odc = gf_odf_codec_get_com(od_dec);
						if (!odc) break;
						/*update ESDs if any*/
						UpdateODCommand(load->isom, odc);
						gf_list_add(au->commands, odc);
					}
				}
			}
			gf_isom_sample_del(&samp);
			if (e) {
				mp4_report(load, gf_isom_last_error(load->isom), "decoding sample %d from track ID %d failed", j+1, gf_isom_get_track_id(load->isom, i+1));
				goto exit;
			}

			samp_done++;
			gf_set_progress("MP4 Loading", samp_done, nb_samp);
		}
		gf_odf_desc_del((GF_Descriptor *) esd);
		esd = NULL;
	}
	gf_isom_text_set_streaming_mode(load->isom, 0);

exit:
#ifndef GPAC_DISABLE_BIFS
	gf_bifs_decoder_del(bifs_dec);
#endif
	gf_odf_codec_del(od_dec);
#ifndef GPAC_DISABLE_LASER
	gf_laser_decoder_del(lsr_dec);
#endif
	if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
	if (logs) gf_fclose(logs);
	return e;
}

static void gf_sm_load_done_isom(GF_SceneLoader *load)
{
	/*nothing to do the file is not ours*/
}

static GF_Err gf_sm_isom_suspend(GF_SceneLoader *loader, Bool suspend)
{
	return GF_OK;
}

GF_Err gf_sm_load_init_isom(GF_SceneLoader *load)
{
	u32 i;
	GF_BIFSConfig *bc;
	GF_ESD *esd;
	GF_Err e;
	char *scene_msg = "MPEG-4 BIFS Scene Parsing";
	if (!load->isom) return GF_BAD_PARAM;

	/*load IOD*/
	load->ctx->root_od = (GF_ObjectDescriptor *) gf_isom_get_root_od(load->isom);
	if (!load->ctx->root_od) {
		e = gf_isom_last_error(load->isom);
		if (e) return e;
	} else if ((load->ctx->root_od->tag != GF_ODF_OD_TAG) && (load->ctx->root_od->tag != GF_ODF_IOD_TAG)) {
		gf_odf_desc_del((GF_Descriptor *) load->ctx->root_od);
		load->ctx->root_od = NULL;
	}

	esd = NULL;

	/*get root scene stream*/
	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		if (type != GF_ISOM_MEDIA_SCENE) continue;
		if (! gf_isom_is_track_in_root_od(load->isom, i+1) ) continue;
		esd = gf_isom_get_esd(load->isom, i+1, 1);

		if (esd && esd->URLString) {
			gf_odf_desc_del((GF_Descriptor *)esd);
			esd = NULL;
			continue;
		}

		/*make sure we load the root BIFS stream first*/
		if (esd && esd->dependsOnESID && (esd->dependsOnESID!=esd->ESID) ) {
			u32 track = gf_isom_get_track_by_id(load->isom, esd->dependsOnESID);
			if (gf_isom_get_media_type(load->isom, track) != GF_ISOM_MEDIA_OD) {
				gf_odf_desc_del((GF_Descriptor *)esd);
				esd = NULL;
				continue;
			}
		}
		if (esd && esd->decoderConfig && esd->decoderConfig->objectTypeIndication==0x09) scene_msg = "MPEG-4 LASeR Scene Parsing";
		break;
	}
	if (!esd) return GF_OK;

	GF_LOG(GF_LOG_INFO, GF_LOG_PARSER, ("%s\n", scene_msg));

	/*BIFS: update size & pixel metrics info*/
	if (esd && esd->decoderConfig) {
		if (esd->decoderConfig->objectTypeIndication<=2) {
			bc = gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication);
			if (!bc->elementaryMasks && bc->pixelWidth && bc->pixelHeight) {
				load->ctx->scene_width = bc->pixelWidth;
				load->ctx->scene_height = bc->pixelHeight;
				load->ctx->is_pixel_metrics = bc->pixelMetrics;
			}
			gf_odf_desc_del((GF_Descriptor *) bc);
			/*note we don't load the first BIFS AU to avoid storing the BIFS decoder, needed to properly handle quantization*/
		}
		/*LASeR*/
		else if (esd->decoderConfig->objectTypeIndication==0x09) {
			load->ctx->is_pixel_metrics = 1;
		}
	}
	gf_odf_desc_del((GF_Descriptor *) esd);
	esd = NULL;

	load->process = gf_sm_load_run_isom;
	load->done = gf_sm_load_done_isom;
	load->suspend = gf_sm_isom_suspend;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM && GPAC_DISABLE_LOADER_ISOM*/
