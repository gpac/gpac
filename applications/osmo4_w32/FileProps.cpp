// FileProps.cpp : implementation file
//

#include "stdafx.h"
#include "osmo4.h"
#include "FileProps.h"
#include "MainFrm.h"

/*ISO 639 languages*/
#include <gpac/iso639.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileProps dialog


CFileProps::CFileProps(CWnd* pParent /*=NULL*/)
	: CDialog(CFileProps::IDD, pParent)
{
	//{{AFX_DATA_INIT(CFileProps)
	//}}AFX_DATA_INIT
}


void CFileProps::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CFileProps)
	DDX_Control(pDX, IDC_VIEWSEL, m_ViewSel);
	DDX_Control(pDX, IDC_ODINFO, m_ODInfo);
	DDX_Control(pDX, IDC_ODTREE, m_ODTree);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CFileProps, CDialog)
	//{{AFX_MSG_MAP(CFileProps)
	ON_NOTIFY(TVN_SELCHANGED, IDC_ODTREE, OnSelchangedOdtree)
	ON_BN_CLICKED(IDC_WORLD, OnWorld)
	ON_BN_CLICKED(IDC_VIEWSG, OnViewsg)
	ON_WM_TIMER()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_NOTIFY(TCN_SELCHANGE, IDC_VIEWSEL, OnSelchangeViewsel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFileProps message handlers


#define FP_TIMER_ID	20

BOOL CFileProps::OnInitDialog() 
{
	CDialog::OnInitDialog();

	char sText[5000];
	sprintf(sText, "%s Properties", ((CMainFrame*)GetApp()->m_pMainWnd)->m_pPlayList->GetDisplayName());
	
	SetWindowText(sText);
	current_odm = NULL;

	m_ViewSel.InsertItem(0, "General");
	m_ViewSel.InsertItem(1, "Streams");
	m_ViewSel.InsertItem(2, "Playback");
	m_ViewSel.InsertItem(3, "Network");

	m_ODTree.SetIndent(0);
	RewriteODTree();
	SetTimer(FP_TIMER_ID, 500, NULL);

	return TRUE;
}


void CFileProps::WriteInlineTree(GF_ObjectManager *root_od, HTREEITEM parent)
{
	Osmo4 *gpac = GetApp();

	/*browse all ODs*/
	u32 count = gf_term_get_object_count(gpac->m_term, root_od);

	for (u32 i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_term_get_object(gpac->m_term, root_od, i);
		if (!odm) return;
		HTREEITEM item = m_ODTree.InsertItem("Object Descriptor", 0, 0, parent);
		m_ODTree.SetItemData(item, (DWORD) odm);
		/*if inline propagate*/
		switch (gf_term_object_subscene_type(gpac->m_term, odm)) {
		case 1:
			m_ODTree.SetItemText(item, "Root Scene");
			WriteInlineTree(odm, item);
			break;
		case 2:
			m_ODTree.SetItemText(item, "Inline Scene");
			WriteInlineTree(odm, item);
			break;
		case 3:
			m_ODTree.SetItemText(item, "Extern Proto Lib");
			WriteInlineTree(odm, item);
			break;
		default:
			break;
		}
	}
}

void CFileProps::RewriteODTree()
{
	Osmo4 *gpac = GetApp();
	
	m_ODTree.DeleteAllItems();

	GF_ObjectManager *root_odm = gf_term_get_root_object(gpac->m_term);
	if (!root_odm) return;

	HTREEITEM root = m_ODTree.InsertItem("Root OD", 0, 0);
	m_ODTree.SetItemData(root, (DWORD) root_odm);

	m_ODTree.SetItemText(root, "Root Scene");
	WriteInlineTree(root_odm, root);
}

void CFileProps::OnSelchangedOdtree(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	*pResult = 0;
	
	HTREEITEM item = m_ODTree.GetSelectedItem();
	GF_ObjectManager *odm = (GF_ObjectManager *) m_ODTree.GetItemData(item);
	if (!odm) return;
	
	SetInfo(odm);
}


void CFileProps::OnClose() 
{
	KillTimer(FP_TIMER_ID);
	DestroyWindow();
}

void CFileProps::OnDestroy() 
{
	CDialog::OnDestroy();
	delete this;
	((CMainFrame *)GetApp()->m_pMainWnd)->m_pProps = NULL;
}

void CFileProps::OnSelchangeViewsel(NMHDR* pNMHDR, LRESULT* pResult) 
{
	SetInfo(current_odm);	
	*pResult = 0;
}

void CFileProps::SetInfo(GF_ObjectManager *odm)
{
	current_odm = odm;
	switch (m_ViewSel.GetCurSel()) {
	case 3: SetNetworkInfo(); break;
	case 2: SetDecoderInfo(); break;
	case 1: SetStreamsInfo(); break;
	default: SetGeneralInfo(); break;
	}
}

void CFileProps::OnTimer(UINT nIDEvent) 
{
	if (nIDEvent == FP_TIMER_ID) {
		switch (m_ViewSel.GetCurSel()) {
		case 3: SetNetworkInfo(); break;
		case 2: SetDecoderInfo(); break;
		}
	}
	
	CDialog::OnTimer(nIDEvent);
}

void CFileProps::SetGeneralInfo()
{
	char info[10000];
	char buf[1000];
	ODInfo odi;
	GF_ObjectManager *odm;
	u32 h, m, s, i, j;

	Osmo4 *gpac = GetApp();
	odm = current_odm;

	strcpy(info, "");
	if (!odm || gf_term_get_object_info(gpac->m_term, odm, &odi) != GF_OK) return;

	if (!odi.od) {
		strcat(info, odi.service_url);
		m_ODInfo.SetWindowText(info);
		return;
	}
	sprintf(buf, "%sObject Descriptor ID %d\r\n", (odi.has_profiles) ? "Initial " : "", odi.od->objectDescriptorID);
	strcat(info, buf);
	if (odi.duration) {
		h = (u32) (odi.duration / 3600);
		m = (u32) (odi.duration / 60) - h*60;
		s = (u32) (odi.duration) - h*3600 - m*60;
		sprintf(buf, "Duration %02d:%02d:%02d\r\n", h, m, s);
		strcat(info, buf);
	} else {
		strcat(info, "Unknown duration\r\n");
	}
	if (odi.owns_service) {
		strcat(info, "Service Handler: ");
		strcat(info, odi.service_handler);
		strcat(info, "\r\n");
		strcat(info, "Service URL: ");
		strcat(info, odi.service_url);
		strcat(info, "\r\n");
	}
	
	if (odi.od->URLString) {
		strcat(info, "Remote OD - URL: ");
		strcat(info, odi.od->URLString);
		strcat(info, "\r\n");
	} 
	/*get OD content info*/
	if (odi.codec_name) {
		switch (odi.od_type) {
		case GF_STREAM_VISUAL:
			sprintf(buf, "Video Object: Width %d - Height %d\r\n", odi.width, odi.height);
			strcat(info, buf);
			strcat(info, "Media Codec ");
			strcat(info, odi.codec_name);
			strcat(info, "\r\n");
			if (odi.par) {
				sprintf(buf, "Pixel Aspect Ratio: %d:%d\r\n", (odi.par>>16)&0xFF, (odi.par)&0xFF);
				strcat(info, buf);
			}
			break;
		case GF_STREAM_AUDIO:
			sprintf(buf, "Audio Object: Sample Rate %d - %d channels\r\n", odi.sample_rate, odi.num_channels);
			strcat(info, buf);
			strcat(info, "Media Codec ");
			strcat(info, odi.codec_name);
			strcat(info, "\r\n");
			break;
		case GF_STREAM_PRIVATE_SCENE:
		case GF_STREAM_SCENE:
			if (odi.width && odi.height) {
				sprintf(buf, "Scene Description: Width %d - Height %d\r\n", odi.width, odi.height);
			} else {
				sprintf(buf, "Scene Description: No size specified\r\n");
			}
			strcat(info, buf);
			strcat(info, "Scene Codec ");
			strcat(info, odi.codec_name);
			strcat(info, "\r\n");
			break;
		case GF_STREAM_TEXT:
			if (odi.width && odi.height) {
				sprintf(buf, "Text Object: Width %d - Height %d\r\n", odi.width, odi.height);
			} else {
				sprintf(buf, "Text Object: No size specified\r\n");
			}
			strcat(info, buf);
			strcat(info, "Text Codec ");
			strcat(info, odi.codec_name);
			strcat(info, "\r\n");
			break;
		}
	}
	if (odi.protection) {
		strcat(info, "Encrypted Media");
		if (odi.protection==2) strcat(info, " NOT UNLOCKED");
		strcat(info, "\r\n");
	}

	if (!gf_list_count(odi.od->OCIDescriptors)) {
		m_ODInfo.SetWindowText(info);
		return;
	}

	strcat(info, "\r\nObject Content Information:\r\n");

	/*check OCI (not everything interests us) - FIXME: support for unicode*/
	for (i=0; i<gf_list_count(odi.od->OCIDescriptors); i++) {
		GF_Descriptor *desc = (GF_Descriptor *) gf_list_get(odi.od->OCIDescriptors, i);
		switch (desc->tag) {
		case GF_ODF_SEGMENT_TAG:
		{
			GF_Segment *sd = (GF_Segment *) desc;
			strcat(info, "\r\nSegment Descriptor:\r\n");
			sprintf(buf, "Name: %s - start time %g sec - duration %g sec\r\n", sd->SegmentName, sd->startTime, sd->Duration);
			strcat(info, buf);
		}
			break;
		case GF_ODF_CC_NAME_TAG:
		{
			GF_CC_Name *ccn = (GF_CC_Name *)desc;
			strcat(info, "\r\nContent Creators:\r\n");
			for (j=0; j<gf_list_count(ccn->ContentCreators); j++) {
				GF_ContentCreatorInfo *ci = (GF_ContentCreatorInfo *) gf_list_get(ccn->ContentCreators, j);
				if (!ci->isUTF8) continue;
				strcat(info, "\t");
				strcat(info, ci->contentCreatorName);
				strcat(info, "\r\n");
			}
		}
			break;

		case GF_ODF_SHORT_TEXT_TAG:
			{
				GF_ShortTextual *std = (GF_ShortTextual *)desc;
				strcat(info, "\r\n");
				strcat(info, std->eventName);
				strcat(info, ": ");
				strcat(info, std->eventText);
				strcat(info, "\r\n");
			}
			break;
		/*todo*/
		case GF_ODF_CC_DATE_TAG:
			break;
		default:
			break;
		}

	}

	m_ODInfo.SetWindowText(info);
}

void CFileProps::OnWorld() 
{
	CString wit;
	const char *str;
	GF_List *descs;
	Osmo4 *gpac = GetApp();

	descs = gf_list_new();
	str = gf_term_get_world_info(gpac->m_term, current_odm, descs);
	if (!str) {
		MessageBox("No World Info available", "Sorry!");
		return;
	}

	wit = "";
	for (u32 i=0; i<gf_list_count(descs); i++) {
		const char *d = (const char *) gf_list_get(descs, i);
		wit += d;
		wit += "\n";
	}
	MessageBox(wit, str);
	gf_list_del(descs);
}

void CFileProps::OnViewsg() 
{
	char szOutFile[GF_MAX_PATH];
	Osmo4 *gpac = GetApp();

	strcpy(szOutFile, gpac->szAppPath);
	strcat(szOutFile, "scene_dump");

	GF_Err e = gf_term_dump_scene(gpac->m_term, (char *) szOutFile, gpac->m_ViewXMTA, 0, current_odm);

	if (e) {
		MessageBox(gf_error_to_string(e), "Error while dumping");
	} else {
		ShellExecute(NULL, "open", szOutFile, NULL, NULL, SW_SHOWNORMAL);
	}
}

void CFileProps::SetDecoderInfo()
{
	ODInfo odi;
	char buf[1000], info[2000];
	u32 h, m, s;
	Osmo4 *gpac = GetApp();

	sprintf(info, "");
	m_ODInfo.SetWindowText("");

	if (!current_odm || gf_term_get_object_info(gpac->m_term, current_odm, &odi)) return;
	if (!odi.od) return;

	strcat(info, "Status: ");
	switch (odi.status) {
	case 0:
	case 1:
	case 2:
		h = (u32) (odi.current_time / 3600);
		m = (u32) (odi.current_time / 60) - h*60;
		s = (u32) (odi.current_time) - h*3600 - m*60;
		sprintf(buf, "%s\r\nObject Time: %02d:%02d:%02d\r\n", (odi.status==0) ? "Stopped" : (odi.status==1) ? "Playing" : "Paused", h, m, s);
		strcat(info, buf);
		break;
	case 3:
		strcat(info, "Not Setup");
		m_ODInfo.SetWindowText(info);
		return;
	default:
		strcat(info, "Setup Failed");
		m_ODInfo.SetWindowText(info);
		return;
	}
	/*get clock drift*/
	sprintf(buf, "Clock drift: %d ms\r\n", odi.clock_drift);
	strcat(info, buf);
	/*get buffering*/
	if (odi.buffer>=0) {
		sprintf(buf, "Buffering Time: %d ms\r\n", odi.buffer);
		strcat(info, buf);
	} else if (odi.buffer==-1) {
		strcat(info, "Not buffering\r\n");
	} else {
		strcat(info, "Not Playing\r\n");
	}
	/*get DB occupation*/
	if (odi.buffer>=0) {
		sprintf(buf, "Decoding Buffer: %d Access Units\r\n", odi.db_unit_count);
		strcat(info, buf);
	}
	/*get CB occupation*/
	if (odi.cb_max_count) {
		sprintf(buf, "Composition Memory: %d/%d Units\r\n", odi.cb_unit_count, odi.cb_max_count);
		strcat(info, buf);
	}

	Float avg_dec_time = 0;
	if (odi.nb_dec_frames) { 
		avg_dec_time = (Float) odi.total_dec_time; 
		avg_dec_time /= odi.nb_dec_frames; 
	}
	sprintf(buf, "Bitrate over last second: %d kbps\r\nMax bitrate over one second: %d kbps\r\nAverage Decoding Time %.2f ms (%d max)\r\nTotal decoded frames %d - %d dropped\r\n", 
		(u32) odi.avg_bitrate/1024, odi.max_bitrate/1024, avg_dec_time, odi.max_dec_time, odi.nb_dec_frames, odi.nb_droped);
	strcat(info, buf);

	m_ODInfo.SetWindowText(info);
}


void CFileProps::SetStreamsInfo()
{
	u32 i, count;
	char info[10000];
	char buf[1000], code[5];
	ODInfo odi;
	GF_ObjectManager *odm;
	Bool is_media;

	Osmo4 *gpac = GetApp();
	odm = current_odm;

	strcpy(info, "");
	m_ODInfo.SetWindowText("");

	if (!odm || gf_term_get_object_info(gpac->m_term, odm, &odi) != GF_OK) return;
	if (!odi.od) return;


	if (odi.has_profiles) {
		strcat(info, "MPEG-4 Profiles and Levels:\r\n");
		sprintf(buf, "\tOD Profile@Level %d\r\n", odi.OD_pl);
		strcat(info, buf);
		sprintf(buf, "\tScene Profile@Level %d\r\n", odi.scene_pl);
		strcat(info, buf);
		sprintf(buf, "\tGraphics Profile@Level %d\r\n", odi.graphics_pl);
		strcat(info, buf);
		sprintf(buf, "\tAudio Profile@Level %d\r\n", odi.audio_pl);
		strcat(info, buf);
		sprintf(buf, "\tVisual Profile@Level %d\r\n", odi.visual_pl);
		strcat(info, buf);
		sprintf(buf, "\tInline Content Profiled %s\r\n", odi.inline_pl ? "yes" : "no");
		strcat(info, buf);
		strcat(info, "\r\n");
	}
	is_media = 0;
	count = gf_list_count(odi.od->ESDescriptors);

	for (i=0; i<count; i++) {
		GF_ESD *esd = (GF_ESD *) gf_list_get(odi.od->ESDescriptors, i);

		sprintf(buf, "\t** Stream ID %d - Clock ID %d **\r\n", esd->ESID, esd->OCRESID);
		strcat(info, buf);
		if (esd->dependsOnESID) {
			sprintf(buf, "Depends on Stream ID %d for decoding\r\n", esd->dependsOnESID);
			strcat(info, buf);
		}
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
			sprintf(buf, "OD Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_OCR:
			sprintf(buf, "OCR Stream\r\n");
			strcat(info, buf);
			break;
		case GF_STREAM_SCENE:
			sprintf(buf, "Scene Description Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_PRIVATE_SCENE:
			sprintf(buf, "GPAC Private Scene Description Stream\r\n");
			strcat(info, buf);
			break;
		case GF_STREAM_VISUAL:
			is_media = 1;
			strcat(info, "Visual Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case 0x20: strcat(info, "MPEG-4\r\n"); break;
			case 0x60: strcat(info, "MPEG-2 Simple Profile\r\n"); break;
			case 0x61: strcat(info, "MPEG-2 Main Profile\r\n"); break;
			case 0x62: strcat(info, "MPEG-2 SNR Profile\r\n"); break;
			case 0x63: strcat(info, "MPEG-2 Spatial Profile\r\n"); break;
			case 0x64: strcat(info, "MPEG-2 High Profile\r\n"); break;
			case 0x65: strcat(info, "MPEG-2 422 Profile\r\n"); break;
			case 0x6A: strcat(info, "MPEG-1\r\n"); break;
			case 0x6C: strcat(info, "JPEG\r\n"); break;
			case 0x6D: strcat(info, "PNG\r\n"); break;
			case 0x80:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				sprintf(buf, "GPAC Intern (%s)\r\n", code);
				strcat(info, buf);
				break;
			default:
				sprintf(buf, "Private/Unknown (0x%x)\r\n", esd->decoderConfig->objectTypeIndication);
				strcat(info, buf);
				break;
			}
			break;

		case GF_STREAM_AUDIO:
			is_media = 1;
			strcat(info, "Audio Stream - media type: ");
			switch (esd->decoderConfig->objectTypeIndication) {
			case 0x40: strcat(info, "MPEG-4\r\n"); break;
			case 0x66: strcat(info, "MPEG-2 AAC Main Profile\r\n"); break;
			case 0x67: strcat(info, "MPEG-2 AAC LowComplexity Profile\r\n"); break;
			case 0x68: strcat(info, "MPEG-2 AAC Scalable Sampling Rate Profile\r\n"); break;
			case 0x69: strcat(info, "MPEG-2 Audio\r\n"); break;
			case 0x6B: strcat(info, "MPEG-1 Audio\r\n"); break;
			case 0xA0: strcat(info, "EVRC Audio\r\n"); break;
			case 0xA1: strcat(info, "SMV Audio\r\n"); break;
			case 0xE1: strcat(info, "QCELP Audio\r\n"); break;
			case 0x80:
				memcpy(code, esd->decoderConfig->decoderSpecificInfo->data, 4);
				code[4] = 0;
				sprintf(buf, "GPAC Intern (%s)\r\n", code);
				strcat(info, buf);
				break;
			default:
				sprintf(buf, "Private/Unknown (0x%x)\r\n", esd->decoderConfig->objectTypeIndication);
				strcat(info, buf);
				break;
			}
			break;
		case GF_STREAM_MPEG7:
			sprintf(buf, "MPEG-7 Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_IPMP:
			sprintf(buf, "IPMP Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_OCI:
			sprintf(buf, "OCI Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_MPEGJ:
			sprintf(buf, "MPEGJ Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_INTERACT:
			sprintf(buf, "User Interaction Stream - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		case GF_STREAM_TEXT:
			sprintf(buf, "3GPP/MPEG-4 Timed Text - version %d\r\n", esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		default:
			sprintf(buf, "Private/Unknown (StreamType 0x%x OTI 0x%x)\r\n", esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
			strcat(info, buf);
			break;
		}

		sprintf(buf, "Buffer Size %d\r\nAverage Bitrate %d bps\r\nMaximum Bitrate %d bps\r\n", esd->decoderConfig->bufferSizeDB, esd->decoderConfig->avgBitrate, esd->decoderConfig->maxBitrate);
		strcat(info, buf);
		if (esd->slConfig->predefined==SLPredef_SkipSL) {
			sprintf(buf, "Not using MPEG-4 Synchronization Layer\r\n");
		} else {
			sprintf(buf, "Stream Clock Resolution %d\r\n", esd->slConfig->timestampResolution);
		}
		strcat(info, buf);
		if (esd->URLString) {
			sprintf(buf, "Stream Location: %s\r\n", esd->URLString);
			strcat(info, buf);
		}

		/*check language*/
		if (esd->langDesc) {
			u32 i=0;
			char lan[4], *szLang;
			lan[0] = esd->langDesc->langCode>>16;
			lan[1] = (esd->langDesc->langCode>>8)&0xFF;
			lan[2] = (esd->langDesc->langCode)&0xFF;
			lan[3] = 0;

			if ((lan[0]=='u') && (lan[1]=='n') && (lan[2]=='d')) szLang = "Undetermined";
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
			sprintf(buf, "Stream Language: %s\r\n", szLang);
			strcat(info, buf);
		}
		strcat(info, "\r\n");
	}
	
	m_ODInfo.SetWindowText(info);
}

void CFileProps::SetNetworkInfo()
{
	char info[10000];
	char buf[10000];
	u32 id;
	NetStatCommand com;
	ODInfo odi;
	u32 d_enum, nb_streams;
	GF_Err e;
	GF_ObjectManager *odm;
	Osmo4 *gpac = GetApp();
	odm = current_odm;

	strcpy(info, "");
	m_ODInfo.SetWindowText("");

	if (!odm || gf_term_get_object_info(gpac->m_term, odm, &odi) != GF_OK) return;
	if (!odi.od) return;

	if (odi.owns_service) {
		const char *url, *path;
		u32 done, total, bps;
		strcpy(info, "Current Downloads in service:\r\n");
		d_enum = 0;
		while (gf_term_get_download_info(gpac->m_term, odm, &d_enum, &url, &path, &done, &total, &bps)) {
			if (total && done) {
				sprintf(buf, "%s %s: %d / %d bytes (%.2f %%) - %.2f kBps\r\n", url, path, done, total, (100.0f*done)/total, ((Float)bps)/1024);
			} else {
				sprintf(buf, "%s %s: %.2f kbps\r\n", url, path, ((Float)bps*8)/1024);
			}
			strcat(info, buf);
		}
		if (!d_enum) strcpy(info, "No Downloads in service\r\n");
		strcat(info, "\r\n");
	}

	d_enum = 0;
	nb_streams = 0;
	while (gf_term_get_channel_net_info(gpac->m_term, odm, &d_enum, &id, &com, &e)) {
		if (e) continue;
		if (!com.bw_down && !com.bw_up) continue;
		nb_streams ++;

		sprintf(buf, "Stream ID %d statistics:\r\n", id);
		strcat(info, buf);
		if (com.multiplex_port) {
			sprintf(buf, "\tMultiplex Port %d - multiplex ID %d\r\n", com.multiplex_port, com.port);
		} else {
			sprintf(buf, "\tPort %d\r\n", com.port);
		}
		strcat(info, buf);
		sprintf(buf, "\tPacket Loss Percentage: %.4f\r\n", com.pck_loss_percentage);
		strcat(info, buf);
		sprintf(buf, "\tDown Bandwidth: %.3f kbps\r\n", ((Float)com.bw_down) / 1024);
		strcat(info, buf);
		if (com.bw_up) {
			sprintf(buf, "\tUp Bandwidth: %d bps\r\n", com.bw_up);
			strcat(info, buf);
		}
		if (com.ctrl_port) {
			if (com.multiplex_port) {
				sprintf(buf, "\tControl Multiplex Port: %d - Control Multiplex ID %d\r\n", com.multiplex_port, com.ctrl_port);
			} else {
				sprintf(buf, "\tControl Port: %d\r\n", com.ctrl_port);
			}
			strcat(info, buf);
			sprintf(buf, "\tControl Down Bandwidth: %d bps\r\n", com.ctrl_bw_down);
			strcat(info, buf);
			sprintf(buf, "\tControl Up Bandwidth: %d bps\r\n", com.ctrl_bw_up);
			strcat(info, buf);
		}
		strcat(info, "\r\n");
	}
	if (!nb_streams) strcat(info, "No network streams in this object\r\n");

	m_ODInfo.SetWindowText(info);
}
