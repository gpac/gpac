#if !defined(AFX_OPENURL_H__ADB51A74_305E_4183_8D44_03EEB83D2BFA__INCLUDED_)
#define AFX_OPENURL_H__ADB51A74_305E_4183_8D44_03EEB83D2BFA__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// OpenUrl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COpenUrl dialog

class COpenUrl : public CDialog
{
// Construction
public:
	COpenUrl(CWnd* pParent = NULL);   // standard constructor
	CString m_url;

// Dialog Data
	//{{AFX_DATA(COpenUrl)
	enum { IDD = IDD_OPENFILE };
	CComboBox	m_URLs;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(COpenUrl)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(COpenUrl)
	afx_msg void OnBrowse();
	afx_msg void OnButgo();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPENURL_H__ADB51A74_305E_4183_8D44_03EEB83D2BFA__INCLUDED_)
