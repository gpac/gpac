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

// Rewrite the good dependencies when an OD AU is extracted from the file
GF_Err Media_RewriteODFrame(GF_MediaBox *mdia, GF_ISOSample *sample)
{
	GF_Err e;
	GF_ODCodec *ODdecode;
	GF_ODCodec *ODencode;
	GF_ODCom *com;

	//the commands we proceed
	GF_ESDUpdate *esdU, *esdU2;
	GF_ESDRemove *esdR, *esdR2;
	GF_ODUpdate *odU, *odU2;

	//the desc they contain
	GF_ObjectDescriptor *od;
	GF_IsomObjectDescriptor *isom_od;
	GF_ESD *esd;
	GF_ES_ID_Ref *ref;
	GF_Descriptor *desc;
	GF_TrackReferenceTypeBox *mpod;
	u32 i, j, skipped;

	if (!mdia || !sample || !sample->data || !sample->dataLength) return GF_BAD_PARAM;

	mpod = NULL;
	e = Track_FindRef(mdia->mediaTrack, GF_ISOM_BOX_TYPE_MPOD, &mpod);
	if (e) return e;
	//no references, nothing to do...
	if (!mpod) return GF_OK;

	ODdecode = gf_odf_codec_new();
	if (!ODdecode) return GF_OUT_OF_MEM;
	ODencode = gf_odf_codec_new();
	if (!ODencode) {
		gf_odf_codec_del(ODdecode);
		return GF_OUT_OF_MEM;
	}
	e = gf_odf_codec_set_au(ODdecode, sample->data, sample->dataLength);
	if (e) goto err_exit;
	e = gf_odf_codec_decode(ODdecode);
	if (e) goto err_exit;

	while (1) {
		com = gf_odf_codec_get_com(ODdecode);
		if (!com) break;

		//we only need to rewrite commands with ESDs inside: ESDUpdate and ODUpdate
		switch (com->tag) {
		case GF_ODF_OD_UPDATE_TAG:
			odU = (GF_ODUpdate *) com;
			odU2 = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);

			i=0;
			while ((desc = (GF_Descriptor*)gf_list_enum(odU->objectDescriptors, &i))) {
				switch (desc->tag) {
				case GF_ODF_OD_TAG:
				case GF_ODF_ISOM_OD_TAG:
				//IOD can be used in OD streams
				case GF_ODF_ISOM_IOD_TAG:
					break;
				default:
					return GF_ISOM_INVALID_FILE;
				}
				e = gf_odf_desc_copy(desc, (GF_Descriptor **)&isom_od);
				if (e) goto err_exit;

				//create our OD...
				if (desc->tag == GF_ODF_ISOM_IOD_TAG) {
					od = (GF_ObjectDescriptor *) gf_malloc(sizeof(GF_InitialObjectDescriptor));
				} else {
					od = (GF_ObjectDescriptor *) gf_malloc(sizeof(GF_ObjectDescriptor));
				}
				if (!od) {
					e = GF_OUT_OF_MEM;
					goto err_exit;
				}
				od->ESDescriptors = gf_list_new();
				//and duplicate...
				od->objectDescriptorID = isom_od->objectDescriptorID;
				od->tag = GF_ODF_OD_TAG;
				od->URLString = isom_od->URLString;
				isom_od->URLString = NULL;
				od->extensionDescriptors = isom_od->extensionDescriptors;
				isom_od->extensionDescriptors = NULL;
				od->IPMP_Descriptors = isom_od->IPMP_Descriptors;
				isom_od->IPMP_Descriptors = NULL;
				od->OCIDescriptors = isom_od->OCIDescriptors;
				isom_od->OCIDescriptors = NULL;

				//init as IOD
				if (isom_od->tag == GF_ODF_ISOM_IOD_TAG) {
					((GF_InitialObjectDescriptor *)od)->audio_profileAndLevel = ((GF_IsomInitialObjectDescriptor *)isom_od)->audio_profileAndLevel;
					((GF_InitialObjectDescriptor *)od)->inlineProfileFlag = ((GF_IsomInitialObjectDescriptor *)isom_od)->inlineProfileFlag;
					((GF_InitialObjectDescriptor *)od)->graphics_profileAndLevel = ((GF_IsomInitialObjectDescriptor *)isom_od)->graphics_profileAndLevel;
					((GF_InitialObjectDescriptor *)od)->OD_profileAndLevel = ((GF_IsomInitialObjectDescriptor *)isom_od)->OD_profileAndLevel;
					((GF_InitialObjectDescriptor *)od)->scene_profileAndLevel = ((GF_IsomInitialObjectDescriptor *)isom_od)->scene_profileAndLevel;
					((GF_InitialObjectDescriptor *)od)->visual_profileAndLevel = ((GF_IsomInitialObjectDescriptor *)isom_od)->visual_profileAndLevel;
					((GF_InitialObjectDescriptor *)od)->IPMPToolList = ((GF_IsomInitialObjectDescriptor *)isom_od)->IPMPToolList;
					((GF_IsomInitialObjectDescriptor *)isom_od)->IPMPToolList = NULL;
				}

				//then rewrite the ESDesc
				j=0;
				while ((ref = (GF_ES_ID_Ref*)gf_list_enum(isom_od->ES_ID_RefDescriptors, &j))) {
					//if the ref index is not valid, skip this desc...
					if (!mpod->trackIDs || gf_isom_get_track_from_id(mdia->mediaTrack->moov, mpod->trackIDs[ref->trackRef - 1]) == NULL) continue;
					//OK, get the esd
					e = GetESDForTime(mdia->mediaTrack->moov, mpod->trackIDs[ref->trackRef - 1], sample->DTS, &esd);
					if (!e) e = gf_odf_desc_add_desc((GF_Descriptor *) od, (GF_Descriptor *) esd);
					if (e) {
						gf_odf_desc_del((GF_Descriptor *)od);
						gf_odf_com_del((GF_ODCom **)&odU2);
						gf_odf_desc_del((GF_Descriptor *)isom_od);
						gf_odf_com_del((GF_ODCom **)&odU);
						goto err_exit;
					}

				}
				//delete our desc
				gf_odf_desc_del((GF_Descriptor *)isom_od);
				gf_list_add(odU2->objectDescriptors, od);
			}
			//clean a bit
			gf_odf_com_del((GF_ODCom **)&odU);
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)odU2);
			break;

		case GF_ODF_ESD_UPDATE_TAG:
			esdU = (GF_ESDUpdate *) com;
			esdU2 = (GF_ESDUpdate *) gf_odf_com_new(GF_ODF_ESD_UPDATE_TAG);
			esdU2->ODID = esdU->ODID;
			i=0;
			while ((ref = (GF_ES_ID_Ref*)gf_list_enum(esdU->ESDescriptors, &i))) {
				//if the ref index is not valid, skip this desc...
				if (gf_isom_get_track_from_id(mdia->mediaTrack->moov, mpod->trackIDs[ref->trackRef - 1]) == NULL) continue;
				//OK, get the esd
				e = GetESDForTime(mdia->mediaTrack->moov, mpod->trackIDs[ref->trackRef - 1], sample->DTS, &esd);
				if (e) goto err_exit;
				gf_list_add(esdU2->ESDescriptors, esd);
			}
			gf_odf_com_del((GF_ODCom **)&esdU);
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)esdU2);
			break;

		//brand new case: the ESRemove follows the same principle according to the spec...
		case GF_ODF_ESD_REMOVE_REF_TAG:
			//both commands have the same structure, only the tags change
			esdR = (GF_ESDRemove *) com;
			esdR2 = (GF_ESDRemove *) gf_odf_com_new(GF_ODF_ESD_REMOVE_TAG);
			esdR2->ODID = esdR->ODID;
			esdR2->NbESDs = esdR->NbESDs;
			//alloc our stuff
			esdR2->ES_ID = (unsigned short*)gf_malloc(sizeof(u32) * esdR->NbESDs);
			if (!esdR2->ES_ID) {
				e = GF_OUT_OF_MEM;
				goto err_exit;
			}
			skipped = 0;
			//get the ES_ID in the mpod indicated in the ES_ID[]
			for (i = 0; i < esdR->NbESDs; i++) {
				//if the ref index is not valid, remove this desc...
				if (gf_isom_get_track_from_id(mdia->mediaTrack->moov, mpod->trackIDs[esdR->ES_ID[i] - 1]) == NULL) {
					skipped ++;
				} else {
					//the command in the file has the ref index of the trackID in the mpod
					esdR2->ES_ID[i - skipped] = mpod->trackIDs[esdR->ES_ID[i] - 1];
				}
			}
			//gf_realloc...
			if (skipped && (skipped != esdR2->NbESDs) ) {
				esdR2->NbESDs -= skipped;
				esdR2->ES_ID = (unsigned short*)gf_realloc(esdR2->ES_ID, sizeof(u32) * esdR2->NbESDs);
			}
			gf_odf_com_del((GF_ODCom **)&esdR);
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)esdR2);
			break;

		default:
			e = gf_odf_codec_add_com(ODencode, com);
			if (e) goto err_exit;
		}
	}
	//encode our new AU
	e = gf_odf_codec_encode(ODencode, 1);
	if (e) goto err_exit;

	//and set the buffer in the sample
	gf_free(sample->data);
	sample->data = NULL;
	sample->dataLength = 0;
	e = gf_odf_codec_get_au(ODencode, &sample->data, &sample->dataLength);

err_exit:
	gf_odf_codec_del(ODdecode);
	gf_odf_codec_del(ODencode);
	return e;
}


// Update the dependencies when an OD AU is inserted in the file
GF_Err Media_ParseODFrame(GF_MediaBox *mdia, const GF_ISOSample *sample, GF_ISOSample **od_samp)
{
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *mpod;
	GF_Err e;
	GF_ODCom *com;
	GF_ODCodec *ODencode;
	GF_ODCodec *ODdecode;
	u32 i, j;
	//the commands we proceed
	GF_ESDUpdate *esdU, *esdU2;
	GF_ESDRemove *esdR, *esdR2;
	GF_ODUpdate *odU, *odU2;

	//the desc they contain
	GF_ObjectDescriptor *od;
	GF_IsomObjectDescriptor *isom_od;
	GF_ESD *esd;
	GF_ES_ID_Ref *ref;
	GF_Descriptor *desc;

	*od_samp = NULL;
	if (!mdia || !sample || !sample->data || !sample->dataLength) return GF_BAD_PARAM;

	//First find the references, and create them if none
	tref = mdia->mediaTrack->References;
	if (!tref) {
		tref = (GF_TrackReferenceBox *) gf_isom_box_new_parent(&mdia->mediaTrack->child_boxes, GF_ISOM_BOX_TYPE_TREF);
		e = trak_on_child_box((GF_Box*)mdia->mediaTrack, (GF_Box *) tref);
		if (e) return e;
	}
	//then find the OD reference, and create it if none
	e = Track_FindRef(mdia->mediaTrack, GF_ISOM_BOX_TYPE_MPOD, &mpod);
	if (e) return e;
	if (!mpod) {
		mpod = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
		mpod->reference_type = GF_ISOM_BOX_TYPE_MPOD;
	}

	//OK, create our codecs
	ODencode = gf_odf_codec_new();
	if (!ODencode) return GF_OUT_OF_MEM;
	ODdecode = gf_odf_codec_new();
	if (!ODdecode) return GF_OUT_OF_MEM;

	e = gf_odf_codec_set_au(ODdecode, sample->data, sample->dataLength);
	if (e) goto err_exit;
	e = gf_odf_codec_decode(ODdecode);
	if (e) goto err_exit;

	while (1) {
		com = gf_odf_codec_get_com(ODdecode);
		if (!com) break;

		//check our commands
		switch (com->tag) {
		//Rewrite OD Update
		case GF_ODF_OD_UPDATE_TAG:
			//duplicate our command
			odU = (GF_ODUpdate *) com;
			odU2 = (GF_ODUpdate *) gf_odf_com_new(GF_ODF_OD_UPDATE_TAG);

			i=0;
			while ((desc = (GF_Descriptor*)gf_list_enum(odU->objectDescriptors, &i))) {
				//both OD and IODs are accepted
				switch (desc->tag) {
				case GF_ODF_OD_TAG:
				case GF_ODF_IOD_TAG:
					break;
				default:
					e = GF_ODF_INVALID_DESCRIPTOR;
					goto err_exit;
				}
				//get the esd
				e = gf_odf_desc_copy(desc, (GF_Descriptor **)&od);
				if (e) goto err_exit;
				if (desc->tag == GF_ODF_OD_TAG) {
					isom_od = (GF_IsomObjectDescriptor *) gf_malloc(sizeof(GF_IsomObjectDescriptor));
					isom_od->tag = GF_ODF_ISOM_OD_TAG;
				} else {
					isom_od = (GF_IsomObjectDescriptor *) gf_malloc(sizeof(GF_IsomInitialObjectDescriptor));
					isom_od->tag = GF_ODF_ISOM_IOD_TAG;
					//copy PL
					((GF_IsomInitialObjectDescriptor *)isom_od)->inlineProfileFlag = ((GF_InitialObjectDescriptor *)od)->inlineProfileFlag;
					((GF_IsomInitialObjectDescriptor *)isom_od)->graphics_profileAndLevel = ((GF_InitialObjectDescriptor *)od)->graphics_profileAndLevel;
					((GF_IsomInitialObjectDescriptor *)isom_od)->audio_profileAndLevel = ((GF_InitialObjectDescriptor *)od)->audio_profileAndLevel;
					((GF_IsomInitialObjectDescriptor *)isom_od)->OD_profileAndLevel = ((GF_InitialObjectDescriptor *)od)->OD_profileAndLevel;
					((GF_IsomInitialObjectDescriptor *)isom_od)->scene_profileAndLevel = ((GF_InitialObjectDescriptor *)od)->scene_profileAndLevel;
					((GF_IsomInitialObjectDescriptor *)isom_od)->visual_profileAndLevel = ((GF_InitialObjectDescriptor *)od)->visual_profileAndLevel;
					((GF_IsomInitialObjectDescriptor *)isom_od)->IPMPToolList = ((GF_InitialObjectDescriptor *)od)->IPMPToolList;
					((GF_InitialObjectDescriptor *)od)->IPMPToolList = NULL;
				}
				//in OD stream only ref desc are accepted
				isom_od->ES_ID_RefDescriptors = gf_list_new();
				isom_od->ES_ID_IncDescriptors = NULL;

				//TO DO: check that a given sampleDescription exists
				isom_od->extensionDescriptors = od->extensionDescriptors;
				od->extensionDescriptors = NULL;
				isom_od->IPMP_Descriptors = od->IPMP_Descriptors;
				od->IPMP_Descriptors = NULL;
				isom_od->OCIDescriptors = od->OCIDescriptors;
				od->OCIDescriptors = NULL;
				isom_od->URLString = od->URLString;
				od->URLString = NULL;
				isom_od->objectDescriptorID = od->objectDescriptorID;

				j=0;
				while ((esd = (GF_ESD*)gf_list_enum(od->ESDescriptors, &j))) {
					ref = (GF_ES_ID_Ref *) gf_odf_desc_new(GF_ODF_ESD_REF_TAG);
					if (!esd->ESID) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOM] Missing ESID on ESD, cannot add track reference in OD frame"));
						e = GF_BAD_PARAM;
						goto err_exit;
					}
					//1 to 1 mapping trackID and ESID. Add this track to MPOD
					//if track does not exist, this will be remove while reading the OD stream
					e = reftype_AddRefTrack(mpod, esd->ESID, &ref->trackRef);
					if (e) goto err_exit;
					e = gf_odf_desc_add_desc((GF_Descriptor *)isom_od, (GF_Descriptor *)ref);
					if (e) goto err_exit;
				}
				//delete our desc
				gf_odf_desc_del((GF_Descriptor *)od);
				//and add the new one to our command
				gf_list_add(odU2->objectDescriptors, isom_od);
			}
			//delete the command
			gf_odf_com_del((GF_ODCom **)&odU);
			//and add the new one to the codec
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)odU2);
			break;

		//Rewrite ESD Update
		case GF_ODF_ESD_UPDATE_TAG:
			esdU = (GF_ESDUpdate *) com;
			esdU2 = (GF_ESDUpdate *) gf_odf_com_new(GF_ODF_ESD_UPDATE_TAG);
			esdU2->ODID = esdU->ODID;
			i=0;
			while ((esd = (GF_ESD*)gf_list_enum(esdU->ESDescriptors, &i))) {
				ref = (GF_ES_ID_Ref *) gf_odf_desc_new(GF_ODF_ESD_REF_TAG);
				//1 to 1 mapping trackID and ESID
				e = reftype_AddRefTrack(mpod, esd->ESID, &ref->trackRef);
				if (e) goto err_exit;
				e = gf_list_add(esdU2->ESDescriptors, ref);
				if (e) goto err_exit;
			}
			gf_odf_com_del((GF_ODCom **)&esdU);
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)esdU2);
			break;

		//Brand new case: the ESRemove has to be rewritten too according to the specs...
		case GF_ODF_ESD_REMOVE_TAG:
			esdR = (GF_ESDRemove *) com;
			esdR2 = (GF_ESDRemove *) gf_odf_com_new(GF_ODF_ESD_REMOVE_TAG);
			//change the tag for the file format
			esdR2->tag = GF_ODF_ESD_REMOVE_REF_TAG;
			esdR2->ODID = esdR->ODID;
			esdR2->NbESDs = esdR->NbESDs;
			if (esdR->NbESDs) {
				//alloc our stuff
				esdR2->ES_ID = (unsigned short*)gf_malloc(sizeof(u32) * esdR->NbESDs);
				if (!esdR2->ES_ID) {
					e = GF_OUT_OF_MEM;
					goto err_exit;
				}
				for (i = 0; i < esdR->NbESDs; i++) {
					//1 to 1 mapping trackID and ESID
					e = reftype_AddRefTrack(mpod, esdR->ES_ID[i], &esdR2->ES_ID[i]);
					if (e) goto err_exit;
				}
			}
			gf_odf_com_del(&com);
			gf_odf_codec_add_com(ODencode, (GF_ODCom *)esdR2);
			break;

		//Add the command as is
		default:
			e = gf_odf_codec_add_com(ODencode, com);
			if (e) goto err_exit;
		}
	}

	//encode our new AU
	e = gf_odf_codec_encode(ODencode, 1);
	if (e) goto err_exit;

	//and set the buffer in the sample
	*od_samp = gf_isom_sample_new();
	(*od_samp)->CTS_Offset = sample->CTS_Offset;
	(*od_samp)->DTS = sample->DTS;
	(*od_samp)->IsRAP = sample->IsRAP;
	e = gf_odf_codec_get_au(ODencode, & (*od_samp)->data, & (*od_samp)->dataLength);
	if (e) {
		gf_isom_sample_del(od_samp);
		*od_samp = NULL;
	}

err_exit:

	gf_odf_codec_del(ODencode);
	gf_odf_codec_del(ODdecode);
	return e;
}



// Rewrite the good dependencies when an OD AU is extracted from the file
static u32 Media_FindOD_ID(GF_MediaBox *mdia, GF_ISOSample *sample, u32 track_id)
{
	GF_Err e;
	GF_ODCodec *ODdecode;
	GF_ODCom *com;
	u32 the_od_id;
	GF_ODUpdate *odU;
	GF_ESD *esd;
	GF_Descriptor *desc;
	GF_TrackReferenceTypeBox *mpod;
	u32 i, j;

	if (!mdia || !sample || !sample->data || !sample->dataLength) return 0;

	mpod = NULL;
	e = Track_FindRef(mdia->mediaTrack, GF_ISOM_BOX_TYPE_MPOD, &mpod);
	if (e) return 0;
	//no references, nothing to do...
	if (!mpod) return 0;

	the_od_id = 0;

	ODdecode = gf_odf_codec_new();
	if (!ODdecode) return 0;
	e = gf_odf_codec_set_au(ODdecode, sample->data, sample->dataLength);
	if (e) goto err_exit;
	e = gf_odf_codec_decode(ODdecode);
	if (e) goto err_exit;

	while (1) {
		GF_List *esd_list = NULL;
		com = gf_odf_codec_get_com(ODdecode);
		if (!com) break;
		if (com->tag != GF_ODF_OD_UPDATE_TAG) continue;
		odU = (GF_ODUpdate *) com;

		i=0;
		while ((desc = (GF_Descriptor*)gf_list_enum(odU->objectDescriptors, &i))) {
			switch (desc->tag) {
			case GF_ODF_OD_TAG:
			case GF_ODF_IOD_TAG:
				esd_list = ((GF_ObjectDescriptor *)desc)->ESDescriptors;
				break;
			default:
				continue;
			}
			j=0;
			while ((esd = (GF_ESD*)gf_list_enum( esd_list, &j))) {
				if (esd->ESID==track_id) {
					the_od_id = ((GF_IsomObjectDescriptor*)desc)->objectDescriptorID;
					break;
				}
			}
			if (the_od_id) break;
		}
		gf_odf_com_del((GF_ODCom **)&odU);
		if (the_od_id) break;
	}

err_exit:
	gf_odf_codec_del(ODdecode);
	if (e) return 0;
	return the_od_id;
}


GF_EXPORT
u32 gf_isom_find_od_for_track(GF_ISOFile *file, u32 track)
{
	u32 i, j, di, the_od_id;
	GF_TrackBox *od_tk;
	GF_TrackBox *tk = gf_isom_get_track_from_file(file, track);
	if (!tk) return 0;

	i=0;
	while ( (od_tk = (GF_TrackBox*)gf_list_enum(file->moov->trackList, &i))) {
		if (od_tk->Media->handler->handlerType != GF_ISOM_MEDIA_OD) continue;

		for (j=0; j<od_tk->Media->information->sampleTable->SampleSize->sampleCount; j++) {
			GF_ISOSample *samp = gf_isom_get_sample(file, i, j+1, &di);
			the_od_id = Media_FindOD_ID(od_tk->Media, samp, tk->Header->trackID);
			gf_isom_sample_del(&samp);
			if (the_od_id) return the_od_id;
		}
	}
	return 0;
}

#endif /*GPAC_DISABLE_ISOM*/
