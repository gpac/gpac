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

#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/spinbutt.h>
#include <wx/spinctrl.h>

enum
{
	ID_SELECT = 1000,
	ID_APPLY,

	ID_MAKE_DEF,
	ID_FORCE_AUDIO,
	ID_AUDIO_DRIVER,
	ID_FONT_DIR,
	ID_CACHE_DIR,
	ID_PROGRESSIVE,
	ID_RTSP_PORT,
	ID_RTP_OVER_RTSP,
	ID_RTSP_REBUFFER,
	ID_RECORD_DIR,
	ID_USE_FILENAME,
	ID_USE_PROXY,
};

class wxOsmo4Frame;
class wxGPACControl : public wxDialog
{
public:
	wxGPACControl(wxWindow *parent);
	virtual ~wxGPACControl();

private:
	DECLARE_EVENT_TABLE()

	wxOsmo4Frame *m_pApp;

	wxComboBox *m_select;
	Bool m_bWas3D;

	void Apply(wxCommandEvent &event);
	void OnSetSelection(wxCommandEvent &event);
	void ForceAudio(wxCommandEvent &event);
	void OnSetAudioDriver(wxCommandEvent &event);
	void FontDir(wxCommandEvent &event);
	void CacheDir(wxCommandEvent &event);
	void OnProgressive(wxCommandEvent &event);
	void OnUseProxy(wxCommandEvent &event);
	void RTPoverRTSP(wxCommandEvent &event);
	void Rebuffer(wxCommandEvent &event);
	void OnSetRTSPPort(wxCommandEvent &event);
	void OnUseFileName(wxCommandEvent &event);
	void OnRecDir(wxCommandEvent &event);
	void DoSelect();
	s32 m_sel;
	void SetYUVLabel();

	wxBoxSizer *s_header, *s_main, *s_general, *s_mpeg4, *s_mdec, *s_rend, *s_rend2d, *s_rend3d, *s_audio, *s_video, *s_font, *s_dnld, *s_stream, *s_rec;

	/*general section*/
	wxCheckBox *m_loop, *m_lookforsubs, *m_noconsole, *m_viewxmt;
	/*MPEG-4 systems*/
	wxCheckBox *m_bifsalwaysdrawn, *m_singletime;
	wxComboBox *m_lang, *m_thread;
	/*media decoders*/
	wxComboBox *m_decaudio, *m_decvideo;
	/*Rendering*/
	wxComboBox *m_fps, *m_aa, *m_draw_bounds;
	wxCheckBox *m_use3D, *m_fast, *m_force_size;
	/*Renderer 2D*/
	wxComboBox *m_graph;
	wxCheckBox *m_noyuv, *m_direct, *m_scalable;
	wxStaticText *m_yuvtxt;
	/*Renderer 3D*/
	wxCheckBox *m_raster_outlines, *m_polyaa, *m_nobackcull, *m_emulpow2, *m_norectext, *m_copypixels;
	wxComboBox *m_wire, *m_normals;
	/*video*/
	wxComboBox *m_video;
	wxCheckBox *m_switchres, *m_usehwmem;
	/*audio*/
	wxSpinCtrl *m_nbbuf, *m_buflen;
	wxComboBox *m_audio;
	wxCheckBox *m_forcecfg, *m_noresync, *m_nomulitch;
#ifdef WIN32
	wxCheckBox *m_notifs;
#endif
	/*font*/
	wxComboBox *m_font;
	wxButton *m_fontdir;
	wxComboBox *m_texturemode;
	/*file download*/
	wxButton *m_cachedir;
	wxCheckBox *m_cleancache, *m_restartcache, *m_progressive, *m_use_proxy;
	wxTextCtrl *m_sax_duration, *m_proxy_name;
	/*streaming*/
	wxComboBox *m_port;
	wxCheckBox *m_rtsp, *m_reorder, *m_dorebuffer;
	wxTextCtrl *m_timeout, *m_buffer, *m_rebuffer;
	/*file recorder*/
	wxButton *m_recdir;
	wxCheckBox *m_overwrite, *m_usename;
	wxTextCtrl *m_recfile;
};

#endif

