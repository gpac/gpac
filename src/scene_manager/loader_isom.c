/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


#ifndef GPAC_READ_ONLY

static void UpdateODCommand(GF_ISOFile *mp4, GF_ODCom *com)
{
	u32 i, j;
	const char *szName;
	char szPath[2048];

	szName = gf_isom_get_filename(mp4);
	if (com->tag == GF_ODF_OD_UPDATE_TAG) {
		GF_ODUpdate *odU = (GF_ODUpdate *)com;
		for (i=0; i<gf_list_count(odU->objectDescriptors); i++) {
			GF_ObjectDescriptor *od = gf_list_get(odU->objectDescriptors, i);
			for (j=0; j<gf_list_count(od->ESDescriptors); j++) {
				GF_ESD *esd = gf_list_get(od->ESDescriptors, j);
				if (esd->URLString) continue;
				switch (esd->decoderConfig->streamType) {
				case GF_STREAM_OD:
				case GF_STREAM_SCENE:
				case GF_STREAM_OCR:
					break;
				default:
				{
					GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
					gf_list_add(esd->extensionDescriptors, mi);
					sprintf(szPath, "%s#%d", szName, esd->ESID);
					mi->file_name = strdup(szPath);
					mi->streamFormat = strdup("MP4");
				}
					break;
				}
			}
		}
		return;
	}
	if (com->tag == GF_ODF_ESD_UPDATE_TAG) {
		GF_ESDUpdate *esdU = (GF_ESDUpdate *)com;
		for (i=0; i<gf_list_count(esdU->ESDescriptors); i++) {
			GF_ESD *esd = gf_list_get(esdU->ESDescriptors, i);
			if (esd->URLString) continue;
			switch (esd->decoderConfig->streamType) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
			case GF_STREAM_OCR:
				break;
			default:
			{
				GF_MuxInfo *mi = (GF_MuxInfo *) gf_odf_desc_new(GF_ODF_MUXINFO_TAG);
				gf_list_add(esd->extensionDescriptors, mi);
				sprintf(szPath, "%s#%d", szName, esd->ESID);
				mi->file_name = strdup(szPath);
				mi->streamFormat = strdup("MP4");
			}
				break;
			}
		}
		return;
	}
}

static void mp4_report(GF_SceneLoader *load, GF_Err e, char *format, ...)
{
	va_list args;
	va_start(args, format);
	if (load->OnMessage) {
		char szMsg[1024];
		vsprintf(szMsg, format, args);
		load->OnMessage(load->cbk, szMsg, e);
	} else {
		if (e) fprintf(stdout, "%s: ", gf_error_to_string(e));
		vfprintf(stdout, format, args);
		fprintf(stdout, "\n");
	}
	va_end(args);
}

GF_Err gf_sm_load_run_MP4(GF_SceneLoader *load)
{
	u32 i, j, di, nbBifs, nb_samp, samp_done;
	GF_StreamContext *sc, *base_bifs;
	GF_ESD *esd;
	GF_ODCodec *oddec;
	GF_BifsDecoder *bdec;
	GF_Err e;

	if (!load || !load->isom) return GF_BAD_PARAM;
	base_bifs = gf_list_get(load->ctx->streams, 0);
	/*no scene info*/
	if (!base_bifs) return GF_OK;
	nbBifs = 1;
	e = GF_OK;
	bdec = gf_bifs_decoder_new(load->scene_graph, 1);
	oddec = gf_odf_codec_new();
	esd = NULL;
	/*load each stream*/
	nb_samp = 0;
	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		switch (type) {
		case GF_ISOM_MEDIA_BIFS:
		case GF_ISOM_MEDIA_OD:
			nb_samp += gf_isom_get_sample_count(load->isom, i+1);
			break;
		default:
			break;
		}
	}
	samp_done = 1;

	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		switch (type) {
		case GF_ISOM_MEDIA_BIFS:
			break;
		case GF_ISOM_MEDIA_OD:
			break;
		default:
			continue;
		}
		esd = gf_isom_get_esd(load->isom, i+1, 1);
		if (!esd) continue;

		if (base_bifs->ESID!=esd->ESID) {
			sc = gf_sm_stream_new(load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
			sc->streamType = esd->decoderConfig->streamType;
			sc->ESID = esd->ESID;
			sc->objectType = esd->decoderConfig->objectTypeIndication;
			sc->timeScale = gf_isom_get_media_timescale(load->isom, i+1);
			j=0;
		} else {
			j=1;
			sc = base_bifs;
		}
		/*we still need to reconfig the BIFS*/
		if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
			if (!esd->dependsOnESID && nbBifs && !j) 
				mp4_report(load, GF_OK, "Warning: several BIFS namespaces used or improper BIFS dependencies in file - import may be incorrect");
			e = gf_bifs_decoder_configure_stream(bdec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
			if (e) goto exit;
			nbBifs++;
		}

		/*dump all AUs*/
		for (; j<gf_isom_get_sample_count(load->isom, i+1); j++) {
			GF_AUContext *au;
			GF_ISOSample *samp = gf_isom_get_sample(load->isom, i+1, j+1, &di);
			if (!samp) {
				mp4_report(load, gf_isom_last_error(load->isom), "Unable to fetch sample %d from track ID %d - aborting track import", j+1, gf_isom_get_track_id(load->isom, i+1));
				break;
//				e = gf_isom_last_error(load->isom);
//				goto exit;
			}
			au = gf_sm_stream_au_new(sc, samp->DTS, ((Double)samp->DTS) / sc->timeScale, samp->IsRAP);

			if (esd->decoderConfig->streamType==GF_STREAM_SCENE) {
				e = gf_bifs_decode_command_list(bdec, esd->ESID, samp->data, samp->dataLength, au->commands);
			} else {
				e = gf_odf_codec_set_au(oddec, samp->data, samp->dataLength);
				if (!e) e = gf_odf_codec_decode(oddec);
				if (!e) {
					while (1) {
						GF_ODCom *odc = gf_odf_codec_get_com(oddec);
						if (!odc) break;
						/*update ESDs if any*/
						UpdateODCommand(load->isom, odc);
						gf_list_add(au->commands, odc);
					}
				}
			}
			gf_isom_sample_del(&samp);
			if (e) goto exit;

			samp_done++;
			if (load->OnProgress) load->OnProgress(load->cbk, samp_done, nb_samp);
		}
		gf_odf_desc_del((GF_Descriptor *) esd);
		esd = NULL;
	}

exit:
	gf_bifs_decoder_del(bdec);
	gf_odf_codec_del(oddec);
	if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
	return e;
}

GF_Err gf_sm_load_init_MP4(GF_SceneLoader *load)
{
	u32 i, track, di, timeScale;
	GF_StreamContext *sc;
	GF_ESD *esd;
	GF_AUContext *au;
	GF_ISOSample *samp;
	GF_BifsDecoder *bdec;
	GF_Err e;
	if (!load->isom) return GF_BAD_PARAM;

	/*load IOD*/
	load->ctx->root_od = (GF_ObjectDescriptor *) gf_isom_get_root_od(load->isom);
	if (!load->ctx->root_od) {
		e = gf_isom_last_error(load->isom);
		if (e) return e;
	} else if ((load->ctx->root_od->tag != GF_ODF_OD_TAG) && (load->ctx->root_od->tag != GF_ODF_IOD_TAG)) {
		gf_odf_desc_del((GF_Descriptor *) load->ctx->root_od);
	}

	e = GF_OK;
	if (load->OnMessage) load->OnMessage(load->cbk, "MPEG-4 (MP4) Scene Parsing", GF_OK);
	else fprintf(stdout, "MPEG-4 (MP4) Scene Parsing\n");
	
	esd = NULL;

	/*get root BIFS stream*/
	for (i=0; i<gf_isom_get_track_count(load->isom); i++) {
		u32 type = gf_isom_get_media_type(load->isom, i+1);
		if (type != GF_ISOM_MEDIA_BIFS) continue;
		if (! gf_isom_is_track_in_root_od(load->isom, i+1) ) continue;
		esd = gf_isom_get_esd(load->isom, i+1, 1);

		if (esd && esd->URLString) {
			gf_odf_desc_del((GF_Descriptor *)esd);
			continue;
		}

		/*make sure we load the root BIFS stream first*/
		if (esd && esd->dependsOnESID) {
			u32 track = gf_isom_get_track_by_id(load->isom, esd->dependsOnESID);
			if (gf_isom_get_media_type(load->isom, track) != GF_ISOM_MEDIA_OD) {
				gf_odf_desc_del((GF_Descriptor *)esd);
				continue;
			}
		}
		break;
	}
	if (!esd) return GF_OK;

	bdec = gf_bifs_decoder_new(load->scene_graph, 1);
	track = i+1;

	sc = gf_sm_stream_new(load->ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
	sc->streamType = esd->decoderConfig->streamType;
	sc->ESID = esd->ESID;
	sc->objectType = esd->decoderConfig->objectTypeIndication;
	sc->timeScale = gf_isom_get_media_timescale(load->isom, track);
	e = gf_bifs_decoder_configure_stream(bdec, esd->ESID, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, esd->decoderConfig->objectTypeIndication);
	if (e) goto exit;
	timeScale = gf_isom_get_media_timescale(load->isom, track);
	/*get first AU*/
	samp = gf_isom_get_sample(load->isom, track, 1, &di);
	if (!samp) {
		e = gf_isom_last_error(load->isom);
		goto exit;
	}
	au = gf_sm_stream_au_new(sc, samp->DTS, ((Double)samp->DTS) / timeScale, samp->IsRAP);
	e = gf_bifs_decode_command_list(bdec, esd->ESID, samp->data, samp->dataLength, au->commands);
	if (!e) {
		for (i=0; i<gf_list_count(au->commands); i++) {
			GF_Command *com = gf_list_get(au->commands, i);
			if (com->tag == GF_SG_SCENE_REPLACE) {
				GF_BIFSConfig bc;
				/*also update size & pixel metrics info*/
				gf_odf_get_bifs_config(esd->decoderConfig->decoderSpecificInfo, esd->decoderConfig->objectTypeIndication, &bc);
				if (bc.isCommandStream && bc.pixelWidth && bc.pixelHeight) {
					load->ctx->scene_width = bc.pixelWidth;
					load->ctx->scene_height = bc.pixelHeight;
					load->ctx->is_pixel_metrics = bc.pixelMetrics;
				}
			}
		}
	}
	gf_isom_sample_del(&samp);
	gf_odf_desc_del((GF_Descriptor *) esd);
	esd = NULL;

exit:
	gf_bifs_decoder_del(bdec);
	if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
	return e;
}

void gf_sm_load_done_MP4(GF_SceneLoader *load)
{
	/*nothing to do the file is not ours*/
}

#endif
