// Sliders.cpp : implementation file
//

#include "stdafx.h"
#include "osmo4.h"
#include "Sliders.h"
#include <gpac/options.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Sliders dialog

Sliders::Sliders(CWnd* pParent /*=NULL*/)
	: CDialog(Sliders::IDD, pParent)
{
	//{{AFX_DATA_INIT(Sliders)
	//}}AFX_DATA_INIT

	m_grabbed = GF_FALSE;
}


void Sliders::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(Sliders)
	DDX_Control(pDX, ID_AUDIO_VOL, m_AudioVol);
	DDX_Control(pDX, ID_SLIDER, m_PosSlider);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Sliders, CDialog)
	//{{AFX_MSG_MAP(Sliders)
	ON_WM_HSCROLL()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Sliders message handlers

void Sliders::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{

	Osmo4 *app = GetApp();
	if (pScrollBar->GetDlgCtrlID() == ID_SLIDER) {
		switch (nSBCode) {
		case TB_PAGEUP:
		case TB_PAGEDOWN:
		case TB_LINEUP:
		case TB_LINEDOWN:
		case TB_TOP:
		case TB_BOTTOM:
//			m_grabbed = GF_TRUE;
			break;
		case TB_THUMBPOSITION:
		case TB_THUMBTRACK:
			m_grabbed = GF_TRUE;
			break;
		case TB_ENDTRACK:
			if (m_grabbed) {
				if (!app->can_seek || !app->m_isopen) {
					m_PosSlider.SetPos(0);
				} else {
					u32 range = m_PosSlider.GetRangeMax() - m_PosSlider.GetRangeMin();
					u32 seek_to = m_PosSlider.GetPos();
					app->PlayFromTime(seek_to);
				}
				m_grabbed = GF_FALSE;
			}
			break;
		}
	}
	if (pScrollBar->GetDlgCtrlID() == ID_AUDIO_VOL) {
		u32 vol = m_AudioVol.GetPos();
		gf_term_set_option(app->m_term, GF_OPT_AUDIO_VOLUME, vol);
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void Sliders::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if (!m_PosSlider.m_hWnd) return;
	RECT rc, rc2;

	u32 tw = 40;
	//m_PosSlider.GetClientRect(&rc);
	//rc.right = rc.left + cx;
	//m_PosSlider.SetWindowPos(this, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_NOMOVE);

	m_PosSlider.GetClientRect(&rc);
	rc.right = rc.left + cx - tw;
	rc.top += 10;
	rc.bottom += 10;
	m_PosSlider.SetWindowPos(this, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_NOMOVE);

	const UINT nPixelsLength = 24;
	m_PosSlider.ModifyStyle(0,TBS_FIXEDLENGTH,FALSE);
	m_PosSlider.SendMessage(TBM_SETTHUMBLENGTH,nPixelsLength,0);

	m_AudioVol.GetClientRect(&rc2);
	rc2.top = rc2.bottom = cy/2;
	rc2.top -= cy/3;
	rc2.bottom += cy/3;
	rc2.left = rc.right;
	rc2.right = rc.right+tw;
	m_AudioVol.MoveWindow(&rc2);

}

/*we sure don't want to close this window*/
void Sliders::OnClose()
{
	u32 i = 0;
	return;
}

BOOL Sliders::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		GetApp()->m_pMainWnd->SetFocus();
		GetApp()->m_pMainWnd->PostMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
		return TRUE;
	}
	return CDialog::PreTranslateMessage(pMsg);
}


BOOL Sliders::OnInitDialog()
{
	CDialog::OnInitDialog();
	m_AudioVol.SetRange(0, 100);
	return TRUE;
}

void Sliders::SetVolume()
{
	m_AudioVol.SetPos(gf_term_get_option(GetApp()->m_term, GF_OPT_AUDIO_VOLUME));
}
