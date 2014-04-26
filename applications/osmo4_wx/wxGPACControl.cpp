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

#include "wxOsmo4.h"
#include <gpac/options.h>
#include <gpac/modules/codec.h>
#include <gpac/modules/raster2d.h>
#include <gpac/modules/font.h>
#include <gpac/modules/audio_out.h>
#include <gpac/modules/video_out.h>

#include <gpac/iso639.h>

#include "wxGPACControl.h"


#define NUM_RATES 11
static const char *BIFSRates[11] =
{
	"5.0",
	"7.5",
	"10.0",
	"12.5",
	"15.0",
	"24.0",
	"25.0",
	"30.0",
	"50.0",
	"60.0",
	"100.0"
};

void wxGPACControl::SetYUVLabel()
{
	u32 yuv_format = gf_term_get_option(m_pApp->m_term, GF_OPT_YUV_FORMAT);
	if (!yuv_format) {
		m_yuvtxt->SetLabel(wxT("(No YUV used)"));
	} else {
		char str[100];
		sprintf(str, "(%s used)", gf_4cc_to_str(yuv_format));
		m_yuvtxt->SetLabel(wxString(str, wxConvUTF8) );
	}
}

wxGPACControl::wxGPACControl(wxWindow *parent)
	: wxDialog(parent, -1, wxString(wxT("GPAC Control Panel")))
{
	const char *sOpt;
	SetSize(320, 240);
	u32 i;
	wxBoxSizer *bs;
	Centre();

	m_pApp = (wxOsmo4Frame *)parent;

	s_main = new wxBoxSizer(wxVERTICAL);

	s_header = new wxBoxSizer(wxHORIZONTAL);
	//s_header->Add(new wxStaticText(this, 0, wxT("Category"), wxDefaultPosition, wxSize(60, 20)), wxALIGN_CENTER);
	m_select = new wxComboBox(this, ID_SELECT, wxT(""), wxDefaultPosition, wxSize(120, 30), 0, NULL, wxCB_READONLY);
	s_header->Add(m_select, 2, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_header->Add( new wxButton(this, ID_APPLY, wxT("Apply"), wxDefaultPosition,
#ifdef WIN32
	                            wxSize(40, 20)
#else
	                            wxSize(40, 30)
#endif
	                           ),
	               1, wxALIGN_TOP|wxALIGN_RIGHT|wxADJUST_MINSIZE);
	s_main->Add(s_header, 0, wxEXPAND, 0);

	/*general section*/
	s_general = new wxBoxSizer(wxVERTICAL);
	m_loop = new wxCheckBox(this, 0, wxT("Loop at End"), wxPoint(10, 40), wxSize(140, 20));
	s_general->Add(m_loop);
	m_lookforsubs = new wxCheckBox(this, 0, wxT("Look for Subtitles"), wxPoint(180, 40), wxSize(140, 20));
	s_general->Add(m_lookforsubs);
	m_noconsole = new wxCheckBox(this, 0, wxT("Disable console messages"), wxPoint(10, 80), wxSize(180, 20));
	s_general->Add(m_noconsole);
	m_viewxmt = new wxCheckBox(this, 0, wxT("View graph in XMT-A format"), wxPoint(10, 120), wxSize(180, 20));
	s_general->Add(m_viewxmt);
	s_main->Add(s_general, 0, wxEXPAND, 0);

	/*MPEG-4 systems*/
	s_mpeg4 = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Prefered Stream Language")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_lang = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_lang, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_mpeg4->Add(bs, 0, wxALL|wxEXPAND, 2);

	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Decoder Threading Mode")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_thread = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_thread, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_mpeg4->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_bifsalwaysdrawn = new wxCheckBox(this, 0, wxT("Always draw late BIFS frames"));
	s_mpeg4->Add(m_bifsalwaysdrawn);
	m_singletime = new wxCheckBox(this, 0, wxT("Force Single Timeline"));
	s_mpeg4->Add(m_singletime);
	s_main->Add(s_mpeg4, 0, wxEXPAND, 0);

	/*media decoders*/
	s_mdec = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Prefered Audio Output")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_decaudio = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_decaudio, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_mdec->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Prefered Video Output")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_decvideo = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_decvideo, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_mdec->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_mdec, 0, wxEXPAND, 0);

	/*Rendering*/
	s_rend = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Target Frame Rate")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_fps = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_fps, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Anti-Aliasing")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_aa = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_aa, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Graphics Driver")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_graph = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_graph, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_draw_bounds = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(new wxStaticText(this, 0, wxT("Bounds")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	bs->Add(m_draw_bounds, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_fast = new wxCheckBox(this, 0, wxT("Fast Rendering"));
	m_force_size = new wxCheckBox(this, 0, wxT("Force Scene Size"));
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(m_fast, wxALIGN_CENTER | wxADJUST_MINSIZE);
	bs->Add(m_force_size, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_use3D = new wxCheckBox(this, 0, wxT("Use 3D Renderer"));
	s_rend->Add(m_use3D, 0, wxALL|wxEXPAND, 2);
	m_bWas3D = m_use3D->GetValue();
	s_main->Add(s_rend, 0, wxEXPAND, 0);

	/*Render 2D*/
	s_rend2d = new wxBoxSizer(wxVERTICAL);
	m_direct = new wxCheckBox(this, 0, wxT("Direct Rendering"));
	s_rend2d->Add(m_direct, 0, wxALL|wxEXPAND, 2);
	m_scalable = new wxCheckBox(this, 0, wxT("Scalable Zoom"));
	s_rend2d->Add(m_scalable, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_noyuv = new wxCheckBox(this, 0, wxT("Disable YUV hardware"));
	bs->Add(m_noyuv, wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_yuvtxt = new wxStaticText(this, 0, wxT("(No YUV used)"), wxDefaultPosition, wxSize(60, 20), wxALIGN_LEFT);
	bs->Add(m_yuvtxt, wxALIGN_CENTER|wxADJUST_MINSIZE);
	s_rend2d->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_rend2d, 0, wxEXPAND, 0);

	/*Render 3D*/
	s_rend3d = new wxBoxSizer(wxVERTICAL);
	m_raster_outlines = new wxCheckBox(this, 0, wxT("Use OpenGL Raster outlines"));
	s_rend3d->Add(m_raster_outlines, 0, wxALL|wxEXPAND, 2);
	m_polyaa = new wxCheckBox(this, 0, wxT("Enable polygon anti-aliasing"));
	s_rend3d->Add(m_polyaa, 0, wxALL|wxEXPAND, 2);
	m_nobackcull = new wxCheckBox(this, 0, wxT("Disable backface culling"));
	s_rend3d->Add(m_nobackcull, 0, wxALL|wxEXPAND, 2);

	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Wireframe mode")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_wire = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_wire, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend3d->Add(bs, 0, wxALL|wxEXPAND, 2);

	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Draw Normals")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_normals = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_normals, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rend3d->Add(bs, 0, wxALL|wxEXPAND, 2);

	m_emulpow2 = new wxCheckBox(this, 0, wxT("Emulate power-of-two textures for video"));
	s_rend3d->Add(m_emulpow2, 0, wxALL|wxEXPAND, 2);
	m_norectext = new wxCheckBox(this, 0, wxT("Disable rectangular texture extensions"));
	s_rend3d->Add(m_norectext, 0, wxALL|wxEXPAND, 2);
	m_copypixels = new wxCheckBox(this, 0, wxT("Bitmap node uses direct pixel copy"));
	s_rend3d->Add(m_copypixels, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_rend3d, 0, wxEXPAND, 0);

	/*video*/
	s_video = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Video Driver")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_video = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_video , wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_video->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_switchres = new wxCheckBox(this, 0, wxT("Change video resolution in fullscreen"));
	s_video->Add(m_switchres, 0, wxALL|wxEXPAND, 2);
	m_usehwmem = new wxCheckBox(this, 0, wxT("Use hardware memory in 2D mode"));
	s_video->Add(m_usehwmem, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_video, 0, wxEXPAND, 0);


	/*audio*/
	s_audio = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Audio Driver")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_audio = new wxComboBox(this, ID_AUDIO_DRIVER, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_audio, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_audio->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_forcecfg = new wxCheckBox(this, ID_FORCE_AUDIO, wxT("Force Audio Config"));
	m_forcecfg->SetValue(1);
	s_audio->Add(m_forcecfg, 0, wxALL|wxEXPAND, 2);


	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Number of buffers")), wxALIGN_CENTER|wxADJUST_MINSIZE);
	m_nbbuf = new wxSpinCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(20, 20), wxSP_WRAP | wxSP_ARROW_KEYS, 1, 30, 15);
	m_nbbuf->SetValue(8);
	bs->Add(m_nbbuf, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_audio->Add(bs, 0, wxALL|wxEXPAND, 2);

	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Total length in ms")), wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_buflen = new wxSpinCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(20, 20), wxSP_WRAP | wxSP_ARROW_KEYS, 1, 1000);
	m_buflen->SetValue(400);
	bs->Add(m_buflen, wxALIGN_CENTER | wxADJUST_MINSIZE|wxLEFT,10);
	s_audio->Add(bs, 0, wxALL|wxEXPAND, 2);

	m_noresync = new wxCheckBox(this, -1, wxT("Disable Resynchronization"));
	s_audio->Add(m_noresync);
	m_nomulitch = new wxCheckBox(this, -1, wxT("Disable Multichannel"));
	s_audio->Add(m_nomulitch);
#ifdef WIN32
	m_notifs = new wxCheckBox(this, -1, wxT("Disable DirectSound Notifications"));
	s_audio->Add(m_notifs);
#endif
	s_main->Add(s_audio, 0, wxEXPAND, 0);

	/*font*/
	s_font = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Font Engine")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_font = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_font, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_font->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("System Font Directory")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_fontdir = new wxButton(this, ID_FONT_DIR, wxT("..."), wxDefaultPosition, wxDefaultSize);
	bs->Add(m_fontdir, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_font->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Text Texturing Mode")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_texturemode = new wxComboBox(this, 0, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
	bs->Add(m_texturemode, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_font->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_font, 0, wxEXPAND, 0);

	/*download*/
	s_dnld = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Cache Directory")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_cachedir = new wxButton(this, ID_CACHE_DIR, wxT("..."));
	bs->Add(m_cachedir, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_dnld->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_cleancache = new wxCheckBox(this, -1, wxT("Remove temp files on exit"));
	s_dnld->Add(m_cleancache);
	m_restartcache = new wxCheckBox(this, -1, wxT("Always redownload incomplete cached files"));
	s_dnld->Add(m_restartcache);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_progressive = new wxCheckBox(this, ID_PROGRESSIVE, wxT("XML progressive load"));
	bs->Add(m_progressive, wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_sax_duration = new wxTextCtrl(this, 0, wxT(""), wxPoint(10, 120), wxSize(60, 20));
	bs->Add(m_sax_duration, wxALIGN_CENTER | wxADJUST_MINSIZE);
	bs->Add(new wxStaticText(this, 0, wxT("max load slice (ms)")), wxADJUST_MINSIZE | wxALIGN_CENTER);
	s_dnld->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_use_proxy = new wxCheckBox(this, ID_USE_PROXY, wxT("Use proxy"));
	bs->Add(m_use_proxy, wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_proxy_name = new wxTextCtrl(this, 0, wxT(""), wxPoint(10, 120), wxSize(60, 20));
	bs->Add(m_proxy_name, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_dnld->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_dnld, 0, wxEXPAND, 0);

	/*streaming*/
	s_stream = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Default RTSP port")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_port = new wxComboBox(this, ID_RTSP_PORT, wxT(""), wxPoint(160, 40), wxSize(140, 30), 0, NULL, wxCB_READONLY);
	bs->Add(m_port, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_stream->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_rtsp = new wxCheckBox(this, ID_RTP_OVER_RTSP, wxT("RTP over RTSP"), wxPoint(10, 80), wxSize(140, 20));
	m_reorder = new wxCheckBox(this, -1, wxT("use RTP reordering"), wxPoint(160, 80), wxSize(130, 20));
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(m_rtsp, wxALIGN_CENTER | wxADJUST_MINSIZE);
	bs->Add(m_reorder, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_stream->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_timeout = new wxTextCtrl(this, 0, wxT(""), wxPoint(10, 120), wxSize(60, 20));
	bs->Add(new wxStaticText(this, 0, wxT("Control Timeout (ms): ")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	bs->Add(m_timeout, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_stream->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_buffer = new wxTextCtrl(this, 0, wxT(""), wxPoint(10, 150), wxSize(60, 20));
	bs->Add(new wxStaticText(this, 0, wxT("Media Buffering (ms): ")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	bs->Add(m_buffer, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_stream->Add(bs, 0, wxALL|wxEXPAND, 2);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_dorebuffer = new wxCheckBox(this, ID_RTSP_REBUFFER, wxT("Rebuffer if below"));
	bs->Add(m_dorebuffer, wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_rebuffer = new wxTextCtrl(this, 0, wxT(""), wxPoint(200, 180), wxSize(60, 20));
	bs->Add(m_rebuffer, wxALIGN_CENTER | wxADJUST_MINSIZE);
	m_rebuffer->Disable();
	s_stream->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_stream, 0, wxEXPAND, 0);

	/*streaming cache*/
	s_rec = new wxBoxSizer(wxVERTICAL);
	bs = new wxBoxSizer(wxHORIZONTAL);
	bs->Add(new wxStaticText(this, 0, wxT("Record To: ")), wxALIGN_CENTER | wxADJUST_MINSIZE | wxALIGN_RIGHT);
	m_recdir = new wxButton(this, ID_RECORD_DIR, wxT("..."));
	bs->Add(m_recdir, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rec->Add(bs, 0, wxALL|wxEXPAND, 2);
	m_overwrite = new wxCheckBox(this, -1, wxT("Overwrite existing files"));
	s_rec->Add(m_overwrite);
	bs = new wxBoxSizer(wxHORIZONTAL);
	m_usename = new wxCheckBox(this, ID_USE_FILENAME, wxT("Use filename"));
	m_recfile = new wxTextCtrl(this, 0, wxT(""));
	bs->Add(m_usename, wxALIGN_CENTER | wxADJUST_MINSIZE);
	bs->Add(m_recfile, wxALIGN_CENTER | wxADJUST_MINSIZE);
	s_rec->Add(bs, 0, wxALL|wxEXPAND, 2);
	s_main->Add(s_rec, 0, wxEXPAND, 0);

	/*load options*/
	GF_Config *cfg = m_pApp->m_user.config;
	/*general*/
	sOpt = gf_cfg_get_key(cfg, "General", "Loop");
	m_loop->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "General", "LookForSubtitles");
	m_lookforsubs->SetValue((sOpt && !stricmp(sOpt, "no")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "General", "ConsoleOff");
	m_noconsole->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "General", "ViewXMT");
	m_viewxmt->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	/*systems config*/
	sOpt = gf_cfg_get_key(cfg, "Systems", "Language3CC");
	if (!sOpt) sOpt = "eng";
	u32 select = 0;
	i=0;
	while (GF_ISO639_Lang[i]) {
		/*only use common languages (having both 2- and 3-char code names)*/
		if (GF_ISO639_Lang[i+2][0]) {
			m_lang->Append(wxString(GF_ISO639_Lang[i], wxConvUTF8) );
			if (sOpt && !stricmp(sOpt, GF_ISO639_Lang[i+1])) select = m_lang->GetCount() - 1;
		}
		i+=3;
	}
	m_lang->SetSelection(select);
	sOpt = gf_cfg_get_key(cfg, "Systems", "ThreadingPolicy");
	select = 0;
	m_thread->Append(wxT("Single Thread"));
	m_thread->Append(wxT("Mutli Thread"));
	if (sOpt && !stricmp(sOpt, "Multi")) select = 1;
	m_thread->Append(wxT("Free"));
	if (sOpt && !stricmp(sOpt, "Free")) select = 2;
	m_thread->SetSelection(select);
	sOpt = gf_cfg_get_key(cfg, "Systems", "ForceSingleClock");
	m_singletime->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Systems", "AlwaysDrawBIFS");
	m_bifsalwaysdrawn->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);


	/*audio dec enum*/
	sOpt = gf_cfg_get_key(cfg, "Systems", "DefAudioDec");
	u32 count = gf_modules_get_count(m_pApp->m_user.modules);
	GF_BaseDecoder *ifc_d;
	select = 0;
	s32 to_sel = 0;
	for (i=0; i<count; i++) {
		ifc_d = (GF_BaseDecoder *) gf_modules_load_interface(m_pApp->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE);
		if (!ifc_d) continue;
		if (ifc_d->CanHandleStream(ifc_d, GF_STREAM_AUDIO, NULL, 0)) {
			if (sOpt && !stricmp(ifc_d->module_name, sOpt)) select = to_sel;
			m_decaudio->Append(wxString(ifc_d->module_name, wxConvUTF8) );
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *) ifc_d);
	}
	m_decaudio->SetSelection(select);

	/*video dec enum*/
	sOpt = gf_cfg_get_key(cfg, "Systems", "DefVideoDec");
	select = to_sel = 0;
	for (i=0; i<count; i++) {
		ifc_d = (GF_BaseDecoder *) gf_modules_load_interface(m_pApp->m_user.modules, i, GF_MEDIA_DECODER_INTERFACE);
		if (!ifc_d) continue;
		if (ifc_d->CanHandleStream(ifc_d, GF_STREAM_VISUAL, NULL, 0)) {
			if (sOpt && !stricmp(ifc_d->module_name, sOpt)) select = to_sel;
			m_decvideo->Append(wxString(ifc_d->module_name, wxConvUTF8) );
			to_sel++;
		}
		gf_modules_close_interface((GF_BaseInterface *) ifc_d);
	}
	m_decvideo->SetSelection(select);

	/*rendering FIXME*/
	m_bWas3D = 0;
	m_use3D->SetValue(m_bWas3D ? 1 : 0);

	sOpt = gf_cfg_get_key(cfg, "Compositor", "ForceSceneSize");
	m_force_size->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(cfg, "Compositor", "FrameRate");
	if (!sOpt) sOpt = "30.0";
	select = 0;
	for (i = 0; i<NUM_RATES; i++) {
		m_fps->Append(wxString(BIFSRates[i], wxConvUTF8) );
		if (sOpt && !stricmp(sOpt, BIFSRates[i]) ) select = i;
	}
	m_fps->SetSelection(select);

	sOpt = gf_cfg_get_key(cfg, "Compositor", "HighSpeed");
	m_fast->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	sOpt = gf_cfg_get_key(cfg, "Compositor", "AntiAlias");
	m_aa->Append(wxT("None"));
	m_aa->Append(wxT("Text only"));
	m_aa->Append(wxT("Complete"));
	select = 2;
	if (sOpt && !stricmp(sOpt, "Text")) select = 1;
	else if (sOpt && !stricmp(sOpt, "None")) select = 0;
	m_aa->SetSelection(select);

	sOpt = gf_cfg_get_key(cfg, "Compositor", "BoundingVolume");
	m_draw_bounds->Append(wxT("None"));
	m_draw_bounds->Append(wxT("Box/Rect"));
	m_draw_bounds->Append(wxT("AABB Tree"));
	select = 0;
	if (sOpt && !stricmp(sOpt, "Box")) select = 1;
	else if (sOpt && !stricmp(sOpt, "AABB")) select = 2;
	m_draw_bounds->SetSelection(select);

	/*render2d*/
	sOpt = gf_cfg_get_key(cfg, "Compositor", "DirectDraw");
	m_direct->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "ScalableZoom");
	m_scalable->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "DisableYUV");
	m_noyuv->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	SetYUVLabel();

	/*graphics driver enum*/
	sOpt = gf_cfg_get_key(cfg, "Compositor", "Raster2D");
	GF_BaseInterface *ifce;
	select = to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(m_pApp->m_user.modules, i, GF_RASTER_2D_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_graph->Append(wxString(((GF_BaseInterface *)ifce)->module_name, wxConvUTF8) );
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_graph->SetSelection(select);

	/*render3d*/
	sOpt = gf_cfg_get_key(cfg, "Compositor", "RasterOutlines");
	m_raster_outlines->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "EmulatePOW2");
	m_emulpow2->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "PolygonAA");
	m_polyaa->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "BackFaceCulling");
	m_nobackcull->SetValue((sOpt && !stricmp(sOpt, "Off")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "Wireframe");
	sOpt = gf_cfg_get_key(cfg, "Compositor", "BitmapCopyPixels");
	m_copypixels->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Compositor", "DisableRectExt");
	m_norectext->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	m_wire->Append(wxT("No Wireframe"));
	m_wire->Append(wxT("Wireframe Only"));
	m_wire->Append(wxT("Solid and Wireframe"));
	sOpt = gf_cfg_get_key(cfg, "Compositor", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) m_wire->SetSelection(1);
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) m_wire->SetSelection(2);
	else m_wire->SetSelection(0);
	m_normals->Append(wxT("Never"));
	m_normals->Append(wxT("Per Face"));
	m_normals->Append(wxT("Per Vertex"));
	sOpt = gf_cfg_get_key(cfg, "Compositor", "DrawNormals");
	if (sOpt && !stricmp(sOpt, "PerFace")) m_normals->SetSelection(1);
	else if (sOpt && !stricmp(sOpt, "PerVertex")) m_normals->SetSelection(2);
	else m_normals->SetSelection(0);

	/*video*/
	sOpt = gf_cfg_get_key(cfg, "Video", "SwitchResolution");
	m_switchres->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Video", "UseHardwareMemory");
	m_usehwmem->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Video", "DriverName");
	select = to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(m_pApp->m_user.modules, i, GF_VIDEO_OUTPUT_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_video->Append(wxString(((GF_BaseInterface *)ifce)->module_name, wxConvUTF8) );
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_video->SetSelection(select);

	/*audio*/
	sOpt = gf_cfg_get_key(cfg, "Audio", "ForceConfig");
	m_forcecfg->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Audio", "NumBuffers");
	m_nbbuf->SetValue( sOpt ? wxString(sOpt, wxConvUTF8) : wxT("2"));
	sOpt = gf_cfg_get_key(cfg, "Audio", "TotalDuration");
	m_buflen->SetValue( sOpt ? wxString(sOpt, wxConvUTF8) : wxT("120"));
	wxCommandEvent event;
	ForceAudio(event);
	sOpt = gf_cfg_get_key(cfg, "Audio", "NoResync");
	m_noresync->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Audio", "DisableMultiChannel");
	m_nomulitch->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	/*driver enum*/
	sOpt = gf_cfg_get_key(cfg, "Audio", "DriverName");
	select = to_sel = 0;
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(m_pApp->m_user.modules, i, GF_AUDIO_OUTPUT_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_audio->Append(wxString(((GF_BaseInterface *)ifce)->module_name, wxConvUTF8) );
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_audio->SetSelection(select);
#ifdef WIN32
	sOpt = gf_cfg_get_key(cfg, "Audio", "DisableNotification");
	m_notifs->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	wxCommandEvent audevt;
	OnSetAudioDriver(audevt);
#endif

	/*font*/
	sOpt = gf_cfg_get_key(cfg, "FontEngine", "FontReader");
	to_sel = select = 0;
	for (i=0; i<count; i++) {
		ifce = gf_modules_load_interface(m_pApp->m_user.modules, i, GF_FONT_READER_INTERFACE);
		if (!ifce) continue;
		if (sOpt && !stricmp(((GF_BaseInterface *)ifce)->module_name, sOpt)) select = to_sel;
		m_font->Append(wxString(((GF_BaseInterface *)ifce)->module_name, wxConvUTF8) );
		gf_modules_close_interface(ifce);
		to_sel++;
	}
	m_font->SetSelection(select);
	sOpt = gf_cfg_get_key(cfg, "FontEngine", "FontDirectory");
	if (sOpt) m_fontdir->SetLabel(wxString(sOpt, wxConvUTF8) );
	sOpt = gf_cfg_get_key(cfg, "Compositor", "TextureTextMode");
	m_texturemode->Append(wxT("Default"));
	m_texturemode->Append(wxT("Never"));
	m_texturemode->Append(wxT("Always"));
	if (sOpt && !stricmp(sOpt, "Always")) m_texturemode->SetSelection(2);
	else if (sOpt && !stricmp(sOpt, "3D")) m_texturemode->SetSelection(1);
	else m_texturemode->SetSelection(0);

	/*downloader*/
	sOpt = gf_cfg_get_key(cfg, "General", "CacheDirectory");
	if (sOpt) m_cachedir->SetLabel(wxString(sOpt, wxConvUTF8) );
	sOpt = gf_cfg_get_key(cfg, "Downloader", "CleanCache");
	m_cleancache->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "Downloader", "RestartFiles");
	m_restartcache->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "SAXLoader", "Progressive");
	m_progressive->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	sOpt = gf_cfg_get_key(cfg, "SAXLoader", "MaxDuration");
	m_sax_duration->SetValue(sOpt ? wxString(sOpt, wxConvUTF8) : wxT("30"));
	if (! m_progressive->GetValue()) m_sax_duration->Enable(0);

	sOpt = gf_cfg_get_key(cfg, "HTTPProxy", "Enabled");
	m_use_proxy->SetValue( (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	char szProxy[GF_MAX_PATH];
	strcpy(szProxy, "");
	sOpt = gf_cfg_get_key(cfg, "HTTPProxy", "Name");
	if (sOpt) {
		strcat(szProxy, sOpt);
		sOpt = gf_cfg_get_key(cfg, "HTTPProxy", "Port");
		if (sOpt) {
			strcat(szProxy, ":");
			strcat(szProxy, sOpt);
		}
	} else {
		m_use_proxy->SetValue(0);
	}
	m_proxy_name->SetValue( wxString((char *)szProxy, wxConvUTF8) );
	if (! m_use_proxy->GetValue()) m_proxy_name->Enable(0);

	/*streaming*/
	m_port->Append(wxT("554 (RTSP standard)"));
	m_port->Append(wxT("7070 (RTSP ext)"));
	m_port->Append(wxT("80 (RTSP / HTTP tunnel)"));
	m_port->Append(wxT("8080 (RTSP / HTTP tunnel)"));
	sOpt = gf_cfg_get_key(cfg, "Streaming", "DefaultPort");
	u32 port = 554;
	Bool force_rtsp = 0;
	if (sOpt) port = atoi(sOpt);
	switch (port) {
	case 8080:
		m_port->SetSelection(3);
		force_rtsp = 1;
		break;
	case 80:
		m_port->SetSelection(2);
		force_rtsp = 1;
		break;
	case 7070:
		m_port->SetSelection(1);
		break;
	default:
		m_port->SetSelection(0);
		break;
	}

	Bool use_rtsp = 0;
	sOpt = gf_cfg_get_key(cfg, "Streaming", "RTPoverRTSP");
	if (sOpt && !stricmp(sOpt, "yes")) use_rtsp = 1;

	if (force_rtsp) {
		m_rtsp->SetValue(1);
		m_rtsp->Enable(0);
		m_reorder->SetValue(0);
		m_reorder->Enable(0);
	} else {
		m_rtsp->SetValue(use_rtsp ? 1 : 0);
		m_rtsp->Enable(1);
		m_reorder->Enable(1);
		sOpt = gf_cfg_get_key(cfg, "Streaming", "ReorderSize");
		m_reorder->SetValue( (sOpt && !stricmp(sOpt, "0")) ? 1 : 0);
	}
	sOpt = gf_cfg_get_key(cfg, "Streaming", "RTSPTimeout");
	m_timeout->SetValue(sOpt ? wxString(sOpt, wxConvUTF8) : wxT("30000"));
	sOpt = gf_cfg_get_key(cfg, "Network", "BufferLength");
	m_buffer->SetValue(sOpt ? wxString(sOpt, wxConvUTF8) : wxT("3000"));
	sOpt = gf_cfg_get_key(cfg, "Network", "RebufferLength");
	u32 buf_len = 0;
	if (sOpt) buf_len = atoi(sOpt);
	if (buf_len) {
		m_dorebuffer->SetValue(1);
		m_rebuffer->SetValue(wxString(sOpt, wxConvUTF8));
		m_rebuffer->Enable(1);
	} else {
		m_dorebuffer->SetValue(0);
		m_rebuffer->SetValue(wxT("0"));
		m_rebuffer->Enable(0);
	}

	RTPoverRTSP(event);

	sOpt = gf_cfg_get_key(cfg, "StreamingCache", "RecordDirectory");
	if (!sOpt) sOpt = gf_cfg_get_key(cfg, "General", "CacheDirectory");
	if (sOpt) m_recdir->SetLabel(wxString(sOpt, wxConvUTF8));
	sOpt = gf_cfg_get_key(cfg, "StreamingCache", "KeepExistingFiles");
	m_overwrite->SetValue((sOpt && !stricmp(sOpt, "yes")) ? 0 : 1);

	sOpt = gf_cfg_get_key(cfg, "StreamingCache", "BaseFileName");
	if (sOpt) {
		m_usename->SetValue(1);
		m_recfile->Enable(1);
		m_recfile->SetValue(wxString(sOpt, wxConvUTF8));
	} else {
		m_usename->SetValue(0);
		m_recfile->Enable(0);
		m_recfile->SetValue(wxT("uses service URL"));
	}

	m_select->Append(wxT("General"));
	m_select->Append(wxT("MPEG-4 Systems"));
	m_select->Append(wxT("Media Decoders"));
	m_select->Append(wxT("Compositor"));
	m_select->Append(wxT("Renderer 2D"));
	m_select->Append(wxT("Renderer 3D"));
	m_select->Append(wxT("Video Output"));
	m_select->Append(wxT("Audio Output"));
	m_select->Append(wxT("Text Engine"));
	m_select->Append(wxT("File Download"));
	m_select->Append(wxT("Real-Time Streaming"));
	m_select->Append(wxT("Streaming Cache"));

	sOpt = gf_cfg_get_key(cfg, "General", "ConfigPanel");
	m_sel = sOpt ? atoi(sOpt) : 0;
	if (m_sel>11) m_sel=11;
	m_select->SetSelection(m_sel);

	DoSelect();
}

BEGIN_EVENT_TABLE(wxGPACControl, wxDialog)
	EVT_BUTTON(ID_APPLY, wxGPACControl::Apply)
	EVT_COMBOBOX(ID_SELECT, wxGPACControl::OnSetSelection)
	EVT_CHECKBOX(ID_FORCE_AUDIO, wxGPACControl::ForceAudio)
	EVT_COMBOBOX(ID_AUDIO_DRIVER, wxGPACControl::OnSetAudioDriver)
	EVT_BUTTON(ID_FONT_DIR, wxGPACControl::FontDir)
	EVT_BUTTON(ID_CACHE_DIR, wxGPACControl::CacheDir)
	EVT_CHECKBOX(ID_PROGRESSIVE, wxGPACControl::OnProgressive)
	EVT_CHECKBOX(ID_USE_PROXY, wxGPACControl::OnUseProxy)
	EVT_CHECKBOX(ID_RTP_OVER_RTSP, wxGPACControl::RTPoverRTSP)
	EVT_CHECKBOX(ID_RTSP_REBUFFER, wxGPACControl::Rebuffer)
	EVT_COMBOBOX(ID_RTSP_PORT, wxGPACControl::OnSetRTSPPort)
	EVT_CHECKBOX(ID_USE_FILENAME, wxGPACControl::OnUseFileName)
	EVT_BUTTON(ID_RECORD_DIR, wxGPACControl::OnRecDir)
END_EVENT_TABLE()


wxGPACControl::~wxGPACControl()
{
	char str[20];
	sprintf(str, "%d", m_sel);
	gf_cfg_set_key(m_pApp->m_user.config, "General", "ConfigPanel", str);
}


void wxGPACControl::DoSelect()
{

	/*hide everything*/
	s_main->Show(s_general, false);
	s_main->Show(s_mpeg4, false);
	s_main->Show(s_mdec, false);
	s_main->Show(s_rend, false);
	s_main->Show(s_rend2d, false);
	s_main->Show(s_rend3d, false);
	s_main->Show(s_video, false);
	s_main->Show(s_audio, false);
	s_main->Show(s_font, false);
	s_main->Show(s_dnld, false);
	s_main->Show(s_stream, false);
	s_main->Show(s_rec, false);
	switch (m_sel) {
	case 0:
		s_main->Show(s_general, true);
		break;
	case 1:
		s_main->Show(s_mpeg4, true);
		break;
	case 2:
		s_main->Show(s_mdec, true);
		break;
	case 3:
		s_main->Show(s_rend, true);
		break;
	case 4:
		s_main->Show(s_rend2d, true);
		break;
	case 5:
		s_main->Show(s_rend3d, true);
		break;
	case 6:
		s_main->Show(s_video, true);
		break;
	case 7:
		s_main->Show(s_audio, true);
		break;
	case 8:
		s_main->Show(s_font, true);
		break;
	case 9:
		s_main->Show(s_dnld, true);
		break;
	case 10:
		s_main->Show(s_stream, true);
		break;
	case 11:
		s_main->Show(s_rec, true);
		break;
	}
	SetSizer(s_main);
	s_main->Fit(this);
	//s_main->Layout();
	return;

}

void wxGPACControl::OnSetSelection(wxCommandEvent &WXUNUSED(event))
{
	m_sel = m_select->GetSelection();
	DoSelect();
}

void wxGPACControl::FontDir(wxCommandEvent &WXUNUSED(event))
{
	wxDirDialog dlg(this);
	dlg.SetPath(m_fontdir->GetLabel());
	if (dlg.ShowModal() == wxID_OK) {
		m_fontdir->SetLabel(dlg.GetPath());
	}
}
void wxGPACControl::CacheDir(wxCommandEvent &WXUNUSED(event))
{
	wxDirDialog dlg(this);
	dlg.SetPath(m_cachedir->GetLabel());
	if (dlg.ShowModal() == wxID_OK) {
		m_cachedir->SetLabel(dlg.GetPath());
	}
}

void wxGPACControl::OnProgressive(wxCommandEvent &WXUNUSED(event))
{
	m_sax_duration->Enable(m_progressive->GetValue() ? 1 : 0);
}

void wxGPACControl::OnUseProxy(wxCommandEvent &WXUNUSED(event))
{
	m_proxy_name->Enable(m_use_proxy->GetValue() ? 1 : 0);
}

void wxGPACControl::RTPoverRTSP(wxCommandEvent &WXUNUSED(event))
{
	m_reorder->Enable(m_rtsp->GetValue() ? 0 : 1);
}

void wxGPACControl::Rebuffer(wxCommandEvent &WXUNUSED(event))
{
	if (m_dorebuffer->GetValue()) {
		m_rebuffer->Enable();
	} else {
		m_rebuffer->Disable();
	}
}

void wxGPACControl::OnSetRTSPPort(wxCommandEvent &WXUNUSED(event))
{
	if (m_port->GetSelection() > 1) {
		m_rtsp->Enable(0);
		m_reorder->Enable(0);
	} else {
		m_rtsp->Enable(1);
		m_reorder->Enable(1);
	}
}

void wxGPACControl::OnRecDir(wxCommandEvent &WXUNUSED(event))
{
	wxDirDialog dlg(this);
	dlg.SetPath(m_recdir->GetLabel());
	if (dlg.ShowModal() == wxID_OK) {
		m_recdir->SetLabel(dlg.GetPath());
	}
}

void wxGPACControl::OnUseFileName(wxCommandEvent &WXUNUSED(event))
{
	if (m_usename->GetValue()) {
		m_recfile->Enable();
		m_recfile->SetValue(wxT("record"));
	} else {
		m_recfile->Disable();
		m_recfile->SetValue(wxT("uses service URL"));
	}
}

void wxGPACControl::ForceAudio(wxCommandEvent &WXUNUSED(event))
{
	if (m_forcecfg->GetValue()) {
		m_nbbuf->Enable();
		m_buflen->Enable();
	} else {
		m_nbbuf->Disable();
		m_buflen->Disable();
	}
}

void wxGPACControl::OnSetAudioDriver(wxCommandEvent &WXUNUSED(event))
{
#ifdef WIN32
	if (strstr(m_audio->GetStringSelection().mb_str(wxConvUTF8), "DirectSound")) {
		m_notifs->Enable(1);
	} else {
		m_notifs->Enable(0);
	}
#endif
}



void wxGPACControl::Apply(wxCommandEvent &WXUNUSED(event))
{
	/*save options*/
	GF_Config *cfg = m_pApp->m_user.config;

	m_pApp->m_loop = m_loop->GetValue() ? 1 : 0;
	gf_cfg_set_key(cfg, "General", "Loop", m_loop->GetValue() ? "yes" : "no");
	m_pApp->m_lookforsubs = m_lookforsubs->GetValue() ? 1 : 0;
	gf_cfg_set_key(cfg, "General", "LookForSubtitles",  m_lookforsubs->GetValue() ? "yes" : "no");
	m_pApp->m_console_off = m_noconsole->GetValue() ? 1 : 0;
	gf_cfg_set_key(cfg, "General", "ConsoleOff", m_noconsole->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "General", "ViewXMT", m_viewxmt->GetValue() ? "yes" : "no");

	s32 sel = m_lang->GetSelection();
	u32 i=0;
	while (GF_ISO639_Lang[i]) {
		/*only use common languages (having both 2- and 3-char code names)*/
		if (GF_ISO639_Lang[i+2][0]) {
			if (!sel) break;
			sel--;
		}
		i+=3;
	}
	gf_cfg_set_key(cfg, "Systems", "LanguageName", GF_ISO639_Lang[i]);
	gf_cfg_set_key(cfg, "Systems", "Language3CC", GF_ISO639_Lang[i+1]);
	gf_cfg_set_key(cfg, "Systems", "Language2CC", GF_ISO639_Lang[i+2]);


	sel = m_thread->GetSelection();
	gf_cfg_set_key(cfg, "Systems", "ThreadingPolicy", (sel==0) ? "Single" : ( (sel==1) ? "Multi" : "Free"));
	gf_cfg_set_key(cfg, "Systems", "ForceSingleClock", m_singletime->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Systems", "AlwaysDrawBIFS", m_bifsalwaysdrawn->GetValue() ? "yes" : "no");

	gf_cfg_set_key(cfg, "Systems", "DefAudioDec", m_decaudio->GetStringSelection().mb_str(wxConvUTF8));
	gf_cfg_set_key(cfg, "Systems", "DefVideoDec", m_decvideo->GetStringSelection().mb_str(wxConvUTF8));


	gf_cfg_set_key(cfg, "Compositor", "HighSpeed", m_fast->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "ForceSceneSize", m_force_size->GetValue() ? "yes" : "no");

	gf_cfg_set_key(cfg, "Compositor", "FrameRate", BIFSRates[m_fps->GetSelection()]);
	sel = m_aa->GetSelection();
	gf_cfg_set_key(cfg, "Compositor", "AntiAlias", (sel==0) ? "None" : ( (sel==1) ? "Text" : "All"));
	sel = m_draw_bounds->GetSelection();
	gf_cfg_set_key(cfg, "Compositor", "BoundingVolume", (sel==2) ? "AABB" : (sel==1) ? "Box" : "None");

	Bool is_3D = m_use3D->GetValue() ? 1 : 0;
	if (m_bWas3D != is_3D) {
		/*FIXME*/
	}
	gf_cfg_set_key(cfg, "Compositor", "Raster2D", m_graph->GetStringSelection().mb_str(wxConvUTF8));

	gf_cfg_set_key(cfg, "Compositor", "DirectDraw", m_direct->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", m_scalable->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "DisableYUV", m_noyuv->GetValue() ? "yes" : "no");

	gf_cfg_set_key(cfg, "Compositor", "RasterOutlines", m_raster_outlines->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "EmulatePOW2", m_emulpow2->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "PolygonAA", m_polyaa->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "DisableRectExt", m_norectext->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "BitmapCopyPixels", m_copypixels->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Compositor", "BackFaceCulling", m_nobackcull->GetValue() ? "Off" : "On");

	sel = m_wire->GetSelection();
	gf_cfg_set_key(cfg, "Compositor", "Wireframe", (sel==2) ? "WireOnSolid" : ( (sel==1) ? "WireOnly" : "WireNone" ) );
	sel = m_normals->GetSelection();
	gf_cfg_set_key(cfg, "Compositor", "DrawNormals", (sel==2) ? "PerVertex" : ( (sel==1) ? "PerFace" : "Never" ) );

	gf_cfg_set_key(cfg, "Video", "SwitchResolution", m_switchres->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Video", "UseHardwareMemory", m_usehwmem->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Video", "DriverName", m_video->GetStringSelection().mb_str(wxConvUTF8));


	gf_cfg_set_key(cfg, "Audio", "ForceConfig", m_forcecfg->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Audio", "NoResync", m_noresync->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Audio", "DisableMultiChannel", m_nomulitch->GetValue() ? "yes" : "no");

	gf_cfg_set_key(cfg, "Audio", "NumBuffers", wxString::Format(wxT("%d"), m_nbbuf->GetValue()).mb_str(wxConvUTF8) );
	gf_cfg_set_key(cfg, "Audio", "TotalDuration", wxString::Format(wxT("%d"), m_buflen->GetValue()).mb_str(wxConvUTF8) );
	gf_cfg_set_key(cfg, "Audio", "DriverName", m_audio->GetStringSelection().mb_str(wxConvUTF8));
#ifdef WIN32
	if (m_notifs->IsEnabled())
		gf_cfg_set_key(cfg, "Audio", "DisableNotification", m_notifs->GetValue() ? "yes" : "no");
#endif

	gf_cfg_set_key(cfg, "FontEngine", "FontReader", m_font->GetStringSelection().mb_str(wxConvUTF8));
	gf_cfg_set_key(cfg, "FontEngine", "FontDirectory", m_fontdir->GetLabel().mb_str(wxConvUTF8));
	switch (m_texturemode->GetSelection()) {
	case 2:
		gf_cfg_set_key(cfg, "Compositor", "TextureTextMode", "Always");
		break;
	case 1:
		gf_cfg_set_key(cfg, "Compositor", "TextureTextMode", "Never");
		break;
	default:
		gf_cfg_set_key(cfg, "Compositor", "TextureTextMode", "Default");
		break;
	}

	gf_cfg_set_key(cfg, "Downloader", "CleanCache", m_cleancache->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "Downloader", "RestartFiles", m_restartcache->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "SAXLoader", "Progressive", m_progressive->GetValue() ? "yes" : "no");
	gf_cfg_set_key(cfg, "SAXLoader", "MaxDuration", m_sax_duration->GetLabel().mb_str(wxConvUTF8));
	gf_cfg_set_key(cfg, "General", "CacheDirectory", m_cachedir->GetLabel().mb_str(wxConvUTF8));


	Bool force_rtsp = 0;
	switch (m_port->GetSelection()) {
	case 3:
		gf_cfg_set_key(cfg, "Streaming", "DefaultPort", "8080");
		force_rtsp = 1;
		break;
	case 2:
		gf_cfg_set_key(cfg, "Streaming", "DefaultPort", "80");
		force_rtsp = 1;
		break;
	case 1:
		gf_cfg_set_key(cfg, "Streaming", "DefaultPort", "7070");
		break;
	default:
		gf_cfg_set_key(cfg, "Streaming", "DefaultPort", "554");
		break;
	}

	if (force_rtsp) {
		gf_cfg_set_key(cfg, "Streaming", "RTPoverRTSP", "yes");
	} else {
		gf_cfg_set_key(cfg, "Streaming", "RTPoverRTSP", m_rtsp->GetValue() ? "yes" : "no");
		if (!m_rtsp->GetValue()) gf_cfg_set_key(cfg, "Streaming", "ReorderSize", m_dorebuffer->GetValue() ? "30" : "0");
	}

	gf_cfg_set_key(cfg, "Streaming", "RTSPTimeout", m_timeout->GetValue().mb_str(wxConvUTF8));
	gf_cfg_set_key(cfg, "Network", "BufferLength", m_buffer->GetValue().mb_str(wxConvUTF8));
	if (m_dorebuffer->GetValue()) {
		gf_cfg_set_key(cfg, "Network", "RebufferLength", m_rebuffer->GetValue().mb_str(wxConvUTF8));
	} else {
		gf_cfg_set_key(cfg, "Network", "RebufferLength", "0");
	}

	gf_cfg_set_key(cfg, "StreamingCache", "KeepExistingFiles", m_overwrite->GetValue() ? "no" : "yes");
	if (m_usename->GetValue()) {
		gf_cfg_set_key(cfg, "StreamingCache", "BaseFileName", m_recfile->GetValue().mb_str(wxConvUTF8));
	} else {
		gf_cfg_set_key(cfg, "StreamingCache", "BaseFileName", NULL);
	}
	gf_cfg_set_key(cfg, "StreamingCache", "RecordDirectory", m_recdir->GetLabel().mb_str(wxConvUTF8));


	gf_term_set_option(m_pApp->m_term, GF_OPT_RELOAD_CONFIG, 1);
}

