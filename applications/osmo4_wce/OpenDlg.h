#if !defined(AFX_OPENDLG_H__DD903B38_EA45_4251_A8C9_4E4B08BECCCC__INCLUDED_)
#define AFX_OPENDLG_H__DD903B38_EA45_4251_A8C9_4E4B08BECCCC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// OpenDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// OpenDlg dialog

class OpenDlg : public CDialog
{
// Construction
public:
	OpenDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(OpenDlg)
	enum { IDD = IDD_OPENFILE };
	CComboBox	m_URLs;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(OpenDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(OpenDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPENDLG_H__DD903B38_EA45_4251_A8C9_4E4B08BECCCC__INCLUDED_)
