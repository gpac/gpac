#if !defined(AFX_ADDRESSBAR_H__B0764C99_5CC2_4412_8B1F_22E71BAD70F0__INCLUDED_)
#define AFX_ADDRESSBAR_H__B0764C99_5CC2_4412_8B1F_22E71BAD70F0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AddressBar.h : header file
//


/////////////////////////////////////////////////////////////////////////////
// AddressBar dialog

class CInitDialogBar : public CDialogBar
{
	DECLARE_DYNAMIC(CInitDialogBar)

// Construction
public:
	CInitDialogBar(); 
	virtual ~CInitDialogBar(); 

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CInitDialogBar)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

public:
	virtual BOOL Create(CWnd * pParentWnd, UINT nIDTemplate, UINT nStyle, UINT nID);
	virtual BOOL Create(CWnd * pParentWnd, LPCTSTR lpszTemplateName, UINT nStyle, UINT nID);

protected:
	virtual BOOL OnInitDialog();

protected:

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// AddressBar dialog

class AddressBar : public CInitDialogBar
{

	DECLARE_DYNAMIC(AddressBar)

	// Construction
public:
	AddressBar(); 

// Dialog Data
	//{{AFX_DATA(AddressBar)
	enum { IDD = IDD_NAVBAR };
	CStatic	m_Title;
	CComboBox	m_Address;
	//}}AFX_DATA

	void ReloadURLs();
	void SelectionReady();


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(AddressBar)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	//virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(AddressBar)
	virtual BOOL OnInitDialog();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnClose();
	afx_msg void OnSelendOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADDRESSBAR_H__B0764C99_5CC2_4412_8B1F_22E71BAD70F0__INCLUDED_)
