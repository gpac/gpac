/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Osmo4 wxWidgets GUI
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
 *
 */

#include "fileprops.h"
#include "wxOsmo4.h"
#include "Playlist.h"
#include <wx/filename.h>
#include <gpac/modules/codec.h>
#include <gpac/modules/service.h>
#include <gpac/constants.h>
/*ISO 639 languages*/
#include <gpac/iso639.h>


wxFileProps::wxFileProps(wxWindow *parent)
	: wxDialog(parent, -1, wxString(_T("File Properties")))
{

	m_pApp = (wxOsmo4Frame *)parent;
	SetSize(540, 260);
	assert(m_pApp->m_pPlayList);

	m_pTreeView = new wxTreeCtrl(this, ID_TREE_VIEW, wxPoint(4, 2), wxSize(200, 180), wxTR_DEFAULT_STYLE | wxSUNKEN_BORDER);

	new wxStaticText(this, 0, _T("Information"), wxPoint(210, 2), wxSize(60, 20));
	m_pViewSel = new wxComboBox(this, ID_VIEW_SEL, _T(""), wxPoint(280, 2), wxSize(120, 24), 0, NULL, wxCB_READONLY);
	m_pViewSel->Append(wxT("General"));
	m_pViewSel->Append(wxT("Streams"));
	m_pViewSel->Append(wxT("Playback"));
	m_pViewSel->Append(wxT("Network"));
	m_pViewSel->SetSelection(0);

	m_pViewInfo = new wxTextCtrl(this, -1, wxT(""), wxPoint(210, 30), wxSize(320, 200), wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL | wxSUNKEN_BORDER);

#ifdef WIN32
	m_pViewInfo->SetBackgroundColour(wxColour(wxT("LIGHT GREY")));
#endif

	m_pViewWI = new wxButton(this, ID_VIEW_WI, wxT("View World Info"), wxPoint(4, 174), wxSize(200, 40));
	m_pViewSG = new wxButton(this, ID_VIEW_SG, wxT("View Scene Graph"), wxPoint(4, 220), wxSize(200, 40));


	wxString str = m_pApp->m_pPlayList->GetDisplayName();
	str += wxT(" Properties");
	SetTitle(str);

	m_pTimer = new wxTimer();
	m_pTimer->SetOwner(this, ID_OD_TIMER);
	m_pTimer->Start(500, 0);
	RewriteODTree();

}

wxFileProps::~wxFileProps()
{
	m_pTimer->Stop();
	delete m_pTimer;
}


BEGIN_EVENT_TABLE(wxFileProps, wxDialog)
	EVT_TREE_ITEM_ACTIVATED(ID_TREE_VIEW, wxFileProps::OnSetSelection)
	EVT_TREE_SEL_CHANGED(ID_TREE_VIEW, wxFileProps::OnSetSelection)
	EVT_TREE_ITEM_EXPANDED(ID_TREE_VIEW, wxFileProps::OnSetSelection)
	EVT_TREE_ITEM_COLLAPSED(ID_TREE_VIEW, wxFileProps::OnSetSelection)
	EVT_TIMER(ID_OD_TIMER, wxFileProps::OnTimer)
	EVT_BUTTON(ID_VIEW_SG, wxFileProps::OnViewSG)
	EVT_BUTTON(ID_VIEW_WI, wxFileProps::OnViewWorld)
	EVT_COMBOBOX(ID_VIEW_SEL, wxFileProps::OnSelectInfo)
END_EVENT_TABLE()

void wxFileProps::RewriteODTree()
{
	GF_ObjectManager *root_odm = gf_term_get_root_object(m_pApp->m_term);
	if (!root_odm) return;

	m_pTreeView->DeleteAllItems();
	ODTreeData *root = new ODTreeData(root_odm);
	m_pTreeView->AddRoot(wxT("Root OD"), -1, -1, root);
	wxTreeItemId rootId = m_pTreeView->GetRootItem();

	WriteInlineTree(root);
	SetInfo(root_odm);
}

void wxFileProps::WriteInlineTree(ODTreeData *root)
{
	/*browse all ODs*/
	u32 count = gf_term_get_object_count(m_pApp->m_term, root->m_pODMan);

	for (u32 i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_term_get_object(m_pApp->m_term, root->m_pODMan, i);
		if (!odm) return;
		ODTreeData *odd = new ODTreeData(odm);
		m_pTreeView->AppendItem(root->GetId(), wxT("Object Descriptor"), -1, -1, odd);

		/*if inline propagate*/
		switch (gf_term_object_subscene_type(m_pApp->m_term, odm)) {
		case 1:
			m_pTreeView->SetItemText(odd->GetId(), wxT("Root Scene"));
			WriteInlineTree(odd);
			break;
		case 2:
			m_pTreeView->SetItemText(odd->GetId(), wxT("Inline Scene"));
			WriteInlineTree(odd);
			break;
		case 3:
			m_pTreeView->SetItemText(odd->GetId(), wxT("Extern Proto Lib"));
			break;
		default:
			break;
		}
	}
}

void wxFileProps::OnSetSelection(wxTreeEvent& event)
{
	ODTreeData *odd = (ODTreeData *) m_pTreeView->GetItemData(event.GetItem());
	SetInfo(odd->m_pODMan);
}

void wxFileProps::SetInfo(GF_ObjectManager *odm)
{
	m_current_odm = odm;

	switch (m_pViewSel->GetSelection()) {
	case 3:
		SetNetworkInfo();
		break;
	case 2:
		SetDecoderInfo();
		break;
	case 1:
		SetStreamsInfo();
		break;
	default:
		SetGeneralInfo();
		break;
	}
}

void wxFileProps::OnTimer(wxTimerEvent& WXUNUSED(event))
{
	switch (m_pViewSel->GetSelection()) {
	case 2:
		SetDecoderInfo();
		break;
	}
}
void wxFileProps::OnSelectInfo(wxCommandEvent & WXUNUSED(event) )
{
	SetInfo(m_current_odm);
}

void wxFileProps::SetGeneralInfo()
{
	wxString info;
	GF_MediaInfo odi;
	u32 h, m, s;
	u32 i, j;

	info = wxT("");
	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);

	if (!m_current_odm || gf_term_get_object_info(m_pApp->m_term, m_current_odm, &odi) != GF_OK) return;

	if (odi.has_profiles) info += wxT("Initial ");
	info += wxString::Format(wxT("Object Descriptor ID %d\n"), odi.od->objectDescriptorID);
	if (odi.duration) {
		h = (u32) (odi.duration / 3600);
		m = (u32) (odi.duration / 60) - h*60;
		s = (u32) (odi.duration) - h*3600 - m*60;
		info += wxString::Format(wxT("Duration %02d:%02d:%02d\n"), h, m, s);
	} else {
		info += wxT("Unknown duration\n");
	}

	if (odi.owns_service) {
		info += wxT("Service Handler: ") + wxString(odi.service_handler, wxConvUTF8) + wxT("\n");
		info += wxT("Service URL: ") + wxString(odi.service_url, wxConvUTF8) + wxT("\n");
	}

	if (odi.od->URLString) {
		info += wxT("Remote OD - URL: ") + wxString(odi.od->URLString, wxConvUTF8) + wxT("\n");
	}

	if (odi.codec_name) {
		switch (odi.od_type) {
		case GF_STREAM_VISUAL:
			info += wxString::Format(wxT("Video Object: Width %d - Height %d\n"), odi.width, odi.height);
			info += wxT("Media Codec ") + wxString(odi.codec_name, wxConvUTF8) + wxT("\n");
			break;
		case GF_STREAM_AUDIO:
			info += wxString::Format(wxT("Audio Object: Sample Rate %d - %d channels\n"), odi.sample_rate, odi.num_channels);
			info += wxT("Media Codec ") + wxString(odi.codec_name, wxConvUTF8) + wxT("\n");
			break;
		case GF_STREAM_PRIVATE_SCENE:
		case GF_STREAM_SCENE:
			if (odi.width && odi.height) {
				info += wxString::Format(wxT("Scene Description: Width %d - Height %d\n"), odi.width, odi.height);
			} else {
				info += wxT("Scene Description: No size specified\n");
			}
			info += wxT("Scene Codec ") + wxString(odi.codec_name, wxConvUTF8) + wxT("\n");
			break;
		case GF_STREAM_TEXT:
			if (odi.width && odi.height) {
				info += wxString::Format(wxT("Text Object: Width %d - Height %d\n"), odi.width, odi.height);
			} else {
				info += wxString::Format(wxT("Text Object: No size specified\n"));
			}
			info += wxT("Text Codec ") + wxString(odi.codec_name, wxConvUTF8) + wxT("\n");
			break;
		}
	}
	if (odi.protection==2) info += wxT("Encrypted Media NOT UNLOCKED");
	else if (odi.protection==1) info += wxT("Encrypted Media");

	if (!gf_list_count(odi.od->OCIDescriptors)) {
		m_pViewInfo->Clear();
		m_pViewInfo->AppendText(info);
		return;
	}

	info += wxT("\nObject Content Information:\n");

	/*check OCI (not everything interests us) - FIXME: support for unicode*/
	for (i=0; i<gf_list_count(odi.od->OCIDescriptors); i++) {
		GF_Descriptor *desc = (GF_Descriptor *) gf_list_get(odi.od->OCIDescriptors, i);
		switch (desc->tag) {
		case GF_ODF_SEGMENT_TAG:
		{
			GF_Segment *sd = (GF_Segment *) desc;
			info += wxT("\nSegment Descriptor:\nName: ") + wxString((char *) sd->SegmentName, wxConvUTF8);
			info += wxString::Format(wxT(" - start time %g sec - duration %g sec\n"), sd->startTime, sd->Duration);
		}
		break;
		case GF_ODF_CC_NAME_TAG:
		{
			GF_CC_Name *ccn = (GF_CC_Name *)desc;
			info += wxT("\nContent Creators:\n");
			for (j=0; j<gf_list_count(ccn->ContentCreators); j++) {
				GF_ContentCreatorInfo *ci = (GF_ContentCreatorInfo *) gf_list_get(ccn->ContentCreators, j);
				if (!ci->isUTF8) continue;
				info += wxT("\t") + wxString(ci->contentCreatorName, wxConvUTF8) + wxT("\n");
			}
		}
		break;

		case GF_ODF_SHORT_TEXT_TAG:
		{
			GF_ShortTextual *std = (GF_ShortTextual *)desc;
			info += wxT("\n") + wxString(std->eventName, wxConvUTF8) + wxT(": ") + wxString(std->eventText, wxConvUTF8) + wxT("\n");
		}
		break;
		/*todo*/
		case GF_ODF_CC_DATE_TAG:
			break;
		default:
			break;
		}

	}

	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);
}

void wxFileProps::SetStreamsInfo()
{
	u32 i, count;
	wxString info;
	GF_MediaInfo odi;
	char code[5];

	info = wxT("");
	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);

	if (!m_current_odm || gf_term_get_object_info(m_pApp->m_term, m_current_odm, &odi) != GF_OK) return;

	if (odi.has_profiles) {
		info += wxString::Format(wxT("\tOD Profile@Level %d\n"), odi.OD_pl);
		info += wxString::Format(wxT("\tScene Profile@Level %d\n"), odi.scene_pl);
		info += wxString::Format(wxT("\tGraphics Profile@Level %d\n"), odi.graphics_pl);
		info += wxString::Format(wxT("\tAudio Profile@Level %d\n"), odi.audio_pl);
		info += wxString::Format(wxT("\tVisual Profile@Level %d\n"), odi.scene_pl);
		if (odi.inline_pl) info += wxT("\tInline Content use same profiles\n");
		info += wxT("\n");
	}

	count = gf_list_count(odi.od->ESDescriptors);

	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *) gf_list_get(odi.od->ESDescriptors, i);

		info += wxString::Format(wxT("Stream ID %d - Clock ID %d\n"), esd->ESID, esd->OCRESID);
		if (esd->dependsOnESID) {
			info += wxString::Format(wxT("\tDepends on Stream ID %d for decoding\n"), esd->dependsOnESID);
		}
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
			info += wxString::Format(wxT("\tOD Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_OCR:
			info += wxT("\tObject Clock Reference Stream\n");
			break;
		case GF_STREAM_SCENE:
			info += wxString::Format(wxT("\tScene Description Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_PRIVATE_SCENE:
			info += wxString::Format(wxT("\tGPAC Private Scene Description Stream\n"));
			break;
		case GF_STREAM_VISUAL:
			info += wxT("\tVisual Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_VIDEO_MPEG4_PART2:
				info += wxT("MPEG-4\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
				info += wxT("MPEG-2 Simple Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_MAIN:
				info += wxT("MPEG-2 Main Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_SNR:
				info += wxT("MPEG-2 SNR Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
				info += wxT("MPEG-2 Spatial Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_HIGH:
				info += wxT("MPEG-2 High Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG2_422:
				info += wxT("MPEG-2 422 Profile\n");
				break;
			case GPAC_OTI_VIDEO_MPEG1:
				info += wxT("MPEG-1\n");
				break;
			case GPAC_OTI_IMAGE_JPEG:
				info += wxT("JPEG\n");
				break;
			case GPAC_OTI_IMAGE_PNG:
				info += wxT("PNG\n");
				break;
			case GPAC_OTI_IMAGE_JPEG_2000:
				info += wxT("JPEG2000\n");
				break;
			case 0x80:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				info += wxT("GPAC Intern (") + wxString(code, wxConvUTF8) + wxT(")\n");
				break;
			default:
				info += wxString::Format(wxT("Private/Unknown Type (0x%x)\n"), esd->decoderConfig->objectTypeIndication);
				break;
			}
			break;

		case GF_STREAM_AUDIO:
			info += wxT("\tAudio Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_AUDIO_AAC_MPEG4:
				info += wxT("MPEG-4\n");
				break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
				info += wxT("MPEG-2 AAC Main Profile\n");
				break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
				info += wxT("MPEG-2 AAC LowComplexity Profile\n");
				break;
			case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
				info += wxT("MPEG-2 AAC Scalable Sampling Rate Profile\n");
				break;
			case GPAC_OTI_AUDIO_MPEG2_PART3:
				info += wxT("MPEG-2 Audio\n");
				break;
			case GPAC_OTI_AUDIO_MPEG1:
				info += wxT("MPEG-1 Audio\n");
				break;
			case 0xA0:
				info += wxT("EVRC Audio\n");
				break;
			case 0xA1:
				info += wxT("SMV Audio\n");
				break;
			case 0xE1:
				info += wxT("QCELP Audio\n");
				break;
			case 0x80:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				info += wxT("GPAC Intern (") + wxString(code, wxConvUTF8) + wxT(")\n");
				break;
			default:
				info += wxString::Format(wxT("Private/Unknown Type (0x%x)\n"), esd->decoderConfig->objectTypeIndication);
				break;
			}
			break;
		case GF_STREAM_MPEG7:
			info += wxString::Format(wxT("\tMPEG-7 Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_IPMP:
			info += wxString::Format(wxT("\tIPMP Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_OCI:
			info += wxString::Format(wxT("\tOCI Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_MPEGJ:
			info += wxString::Format(wxT("\tMPEGJ Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		case GF_STREAM_INTERACT:
			info += wxString::Format(wxT("\tUser Interaction Stream - version %d\n"), esd->decoderConfig->objectTypeIndication);
			break;
		default:
			info += wxT("Private/Unknown\n");
			break;
		}

		info += wxString::Format(wxT("\tBuffer Size %d\n\tAverage Bitrate %d bps\n\tMaximum Bitrate %d bps\n"), esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate, esd->decoderConfig->maxBitrate);
		if (esd->slConfig->predefined==SLPredef_SkipSL) {
			info += wxString::Format(wxT("\tNot using MPEG-4 Synchronization Layer\n"));
		} else {
			info += wxString::Format(wxT("\tStream Clock Resolution %d\n"), esd->slConfig->timestampResolution);
		}
		if (esd->URLString)
			info += wxT("\tStream Location: ") + wxString(esd->URLString, wxConvUTF8) + wxT("\n");

		/*check language*/
		if (esd->langDesc) {
			u32 i=0;
			char lan[4], *szLang;
			lan[0] = esd->langDesc->langCode>>16;
			lan[1] = (esd->langDesc->langCode>>8)&0xFF;
			lan[2] = (esd->langDesc->langCode)&0xFF;
			lan[3] = 0;

			if ((lan[0]=='u') && (lan[1]=='n') && (lan[2]=='d')) szLang = (char*) "Undetermined";
			else {
				szLang = lan;
				while (GF_ISO639_Lang[i]) {
					if (GF_ISO639_Lang[i+2][0] && strstr(GF_ISO639_Lang[i+1], lan)) {
						szLang = (char*) GF_ISO639_Lang[i];
						break;
					}
					i+=3;
				}
			}
			info += wxString::Format(wxT("\tStream Language: %s\n"), szLang);
		}

	}
	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);
}


void wxFileProps::SetDecoderInfo()
{
	GF_MediaInfo odi;
	wxString info;
	u32 h, m, s;

	if (!m_current_odm || gf_term_get_object_info(m_pApp->m_term, m_current_odm, &odi)) {
		m_pViewInfo->Clear();
		m_pViewInfo->AppendText(info);
		return;
	}

	info = wxT("Status: ");
	switch (odi.status) {
	case 0:
	case 1:
	case 2:
		h = (u32) (odi.current_time / 3600);
		m = (u32) (odi.current_time / 60) - h*60;
		s = (u32) (odi.current_time) - h*3600 - m*60;
		if (odi.status==0) info += wxT("Stopped");
		else if (odi.status==1) info += wxT("Playing");
		else info += wxT("Paused");
		info += wxString::Format(wxT("\nObject Time: %02d:%02d:%02d\n"), h, m, s);
		break;
	case 3:
		info += wxT("Not Setup\n");
		m_pViewInfo->Clear();
		m_pViewInfo->AppendText(info);
		return;
	default:
		info += wxT("Setup Failed\n");
		m_pViewInfo->Clear();
		m_pViewInfo->AppendText(info);
		return;
	}
	/*get clock drift*/
	info += wxString::Format(wxT("Clock drift: %d ms\n"), odi.clock_drift);
	/*get buffering*/
	if (odi.buffer>=0) info += wxString::Format(wxT("Buffering Time: %d ms\n"), odi.buffer);
	else if (odi.buffer==-1) info += wxT("Not buffering\n");
	else info += wxT("Not Playing\n");

	/*get DB occupation*/
	if (odi.buffer>=0) info += wxString::Format(wxT("Decoding Buffer: %d Access Units\n"), odi.db_unit_count);
	/*get CB occupation*/
	if (odi.cb_max_count)
		info += wxString::Format(wxT("Composition Memory: %d/%d Units\n"), odi.cb_unit_count, odi.cb_max_count);

	Float avg_dec_time = 0;
	if (odi.nb_dec_frames) {
		avg_dec_time = (Float) odi.total_dec_time;
		avg_dec_time /= odi.nb_dec_frames;
	}
	info += wxString::Format(wxT("Average Bitrate %d kbps (%d max)\nAverage Decoding Time %.2f ms (%d max)\nTotal decoded frames %d - %d dropped\n"),
	                         (u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time, odi.nb_dec_frames, odi.nb_droped);

	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);
}

void wxFileProps::SetNetworkInfo()
{
	wxString info;
	u32 id;
	NetStatCommand com;
	GF_MediaInfo odi;
	u32 d_enum;
	GF_Err e;

	info = wxT("");
	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(wxT(""));

	if (!m_current_odm || gf_term_get_object_info(m_pApp->m_term, m_current_odm, &odi) != GF_OK) return;

	if (odi.owns_service) {
		const char *url, *path;
		u32 done, total, bps;
		info = wxT("Current Downloads in service:\n");
		d_enum = 0;
		while (gf_term_get_download_info(m_pApp->m_term, m_current_odm, &d_enum, &url, &path, &done, &total, &bps)) {
			info += wxString(url, wxConvUTF8);
			if (total) {
				info += wxString::Format(wxT(": %d / %d bytes (%.2f %%) - %.2f kBps\n"), done, total, (100.0*done)/total, ((Double)bps)/1024);
			} else {
				info += wxString::Format(wxT(": %.2f kBps\n"), ((Double)bps)/1024);
			}
		}
		if (!d_enum) info = wxT("No Downloads in service\n");
		info += wxT("\n");
	}

	d_enum = 0;
	while (gf_term_get_channel_net_info(m_pApp->m_term, m_current_odm, &d_enum, &id, &com, &e)) {
		if (e) continue;
		if (!com.bw_down && !com.bw_up) continue;

		info += wxString::Format(wxT("Stream ID %d statistics:\n"), id);
		if (com.multiplex_port) {
			info += wxString::Format(wxT("\tMultiplex Port %d - multiplex ID %d\n"), com.multiplex_port, com.port);
		} else {
			info += wxString::Format(wxT("\tPort %d\n"), com.port);
		}
		info += wxString::Format(wxT("\tPacket Loss Percentage: %.4f\n"), com.pck_loss_percentage);
		info += wxString::Format(wxT("\tDown Bandwidth: %.3f bps\n"), ((Float)com.bw_down)/1024);
		if (com.bw_up) info += wxString::Format(wxT("\tUp Bandwidth: %d bps\n"), com.bw_up);
		if (com.ctrl_port) {
			if (com.multiplex_port) {
				info += wxString::Format(wxT("\tControl Multiplex Port: %d - Control Multiplex ID %d\n"), com.multiplex_port, com.ctrl_port);
			} else {
				info += wxString::Format(wxT("\tControl Port: %d\n"), com.ctrl_port);
			}
			info += wxString::Format(wxT("\tControl Down Bandwidth: %d bps\n"), com.ctrl_bw_down);
			info += wxString::Format(wxT("\tControl Up Bandwidth: %d bps\n"), com.ctrl_bw_up);
		}
		info += wxT("\n");
	}
	m_pViewInfo->Clear();
	m_pViewInfo->AppendText(info);
}


void wxFileProps::OnViewWorld(wxCommandEvent &WXUNUSED(event))
{
	wxString wit;
	const char *str;
	GF_List *descs;
	descs = gf_list_new();
	str = gf_term_get_world_info(m_pApp->m_term, m_current_odm, descs);

	if (!str) {
		wxMessageDialog(this, wxT("No World Info available"), wxT("Sorry!"), wxOK).ShowModal();
		return;
	}

	wit = wxT("");
	for (u32 i=0; gf_list_count(descs); i++) {
		const char *d = (const char *) gf_list_get(descs, i);
		wit += wxString(d, wxConvUTF8);
		wit += wxT("\n");
	}
	wxMessageDialog(this, wit, wxString(str, wxConvUTF8), wxOK).ShowModal();
	gf_list_del(descs);
}

void wxFileProps::OnViewSG(wxCommandEvent &WXUNUSED(event))
{
	const char *sOpt;
	Bool dump_xmt;
	wxFileName out_file;
	char szOutFile[GF_MAX_PATH];
	wxString fname;

	sOpt = gf_cfg_get_key(m_pApp->m_user.config, "General", "CacheDirectory");
	out_file.AssignDir(wxString(sOpt, wxConvUTF8) );

	sOpt = gf_cfg_get_key(m_pApp->m_user.config, "General", "ViewXMT");
	out_file.SetFullName(wxT("scene_dump"));
	if (sOpt && !stricmp(sOpt, "yes")) {
		dump_xmt = 1;
	} else {
		dump_xmt = 0;
	}
	strcpy(szOutFile, out_file.GetFullName().mb_str(wxConvUTF8));

	GF_Err e = gf_term_dump_scene(m_pApp->m_term, szOutFile, NULL, dump_xmt, 0, m_current_odm);
	if (e) {
		wxMessageDialog dlg(this, wxString(gf_error_to_string(e), wxConvUTF8), wxT("Error while dumping"), wxOK);
		dlg.ShowModal();
	} else {
		wxString cmd = get_pref_browser(m_pApp->m_user.config);
		cmd += wxT(" ");
		cmd += wxString(szOutFile, wxConvUTF8);
		wxExecute(cmd);
	}
}
