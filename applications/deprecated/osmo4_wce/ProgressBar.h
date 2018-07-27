#if !defined(AFX_PROGRESSBAR_H__85FCAC2B_9C83_4C83_8E66_D712FC88B221__INCLUDED_)
#define AFX_PROGRESSBAR_H__85FCAC2B_9C83_4C83_8E66_D712FC88B221__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProgressBar.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ProgressBar dialog

class ProgressBar : public CDialog
{
// Construction
public:
	ProgressBar(CWnd* pParent = NULL);   // standard constructor

	Bool m_grabbed, m_range_invalidated;
	Double m_FPS;
	u32 m_prev_time;
	void SetPosition(u32 time_ms);

// Dialog Data
	//{{AFX_DATA(ProgressBar)
	enum { IDD = IDD_CONTROL };
	CStatic	m_Time;
	CSliderCtrl	m_Slider;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(ProgressBar)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(ProgressBar)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROGRESSBAR_H__85FCAC2B_9C83_4C83_8E66_D712FC88B221__INCLUDED_)
