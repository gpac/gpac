// ProgressBar.cpp : implementation file
//

#include "stdafx.h"
#include "Osmo4.h"
#include "ProgressBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ProgressBar dialog


ProgressBar::ProgressBar(CWnd* pParent /*=NULL*/)
	: CDialog(ProgressBar::IDD, pParent)
{
	//{{AFX_DATA_INIT(ProgressBar)
	//}}AFX_DATA_INIT

	m_grabbed = 0;
	m_range_invalidated = 0;
}


void ProgressBar::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ProgressBar)
	DDX_Control(pDX, IDC_TIME, m_Time);
	DDX_Control(pDX, IDC_SLIDER, m_Slider);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ProgressBar, CDialog)
	//{{AFX_MSG_MAP(ProgressBar)
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ProgressBar message handlers
#define TEXT_RIGHT_PAD	2
void ProgressBar::OnSize(UINT nType, int cx, int cy)
{
	u32 tw;
	CDialog::OnSize(nType, cx, cy);

	if (!m_Slider.m_hWnd) return;

	RECT rc;
	m_Time.GetClientRect(&rc);
	tw = rc.right-rc.left;
	rc.left = cx - tw - TEXT_RIGHT_PAD;
	rc.right = cx - TEXT_RIGHT_PAD;
	m_Time.MoveWindow(&rc);
	m_Slider.GetClientRect(&rc);
	rc.left = 0;
	rc.right = cx - tw - TEXT_RIGHT_PAD;
	m_Slider.MoveWindow(&rc);
}

void ProgressBar::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	COsmo4 *app = GetApp();

	if (pScrollBar->GetDlgCtrlID() == IDC_SLIDER) {
		if (!app->m_can_seek) return;
		switch (nSBCode) {
		case TB_LINEUP:
		case TB_LINEDOWN:
		case TB_PAGEUP:
		case TB_PAGEDOWN:
		case TB_THUMBPOSITION:
		case TB_THUMBTRACK:
		case TB_TOP:
		case TB_BOTTOM:
			m_grabbed = 1;
			break;
		case TB_ENDTRACK:
			if (!app->m_open) {
				SetPosition(0);
			} else {
				u32 seek_to = m_Slider.GetPos();
				gf_term_play_from_time(app->m_term, seek_to, 0);
			}
			m_grabbed = 0;
			return;
		}
	}
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void ProgressBar::SetPosition(u32 now)
{
	TCHAR swText[20];
	u32 nb_s, nb_m;
	COsmo4 *app = GetApp();

	if (m_range_invalidated) {
		if (app->m_can_seek) {
			m_Slider.SetRangeMin(0);
			m_Slider.SetRangeMax(app->m_duration);
			m_Slider.ShowWindow(SW_SHOWNORMAL);
			m_Slider.EnableWindow(TRUE);
		} else {
			m_Slider.ShowWindow(SW_SHOWNORMAL);
			m_Slider.EnableWindow(FALSE);

		}
		m_range_invalidated = 0;
	}
	if (now==m_prev_time) return;

	if (now<m_prev_time) m_prev_time = 0;

	if (!m_prev_time || (m_prev_time + 500 <= now)) {
		m_FPS = gf_term_get_framerate(app->m_term, 0);
		m_prev_time = now;
	}
	nb_s = now/1000;
	nb_m = nb_s/60;
	nb_s -= nb_m*60;
	wsprintf(swText, _T("%02d:%02d FPS %02.2f"), nb_m, nb_s, m_FPS);
	m_Time.SetWindowText(swText);

	if (!m_grabbed) m_Slider.SetPos(now);
}
