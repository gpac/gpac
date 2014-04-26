#if !defined(AFX_PLAYLIST_H__EA74376A_83DF_435E_8484_A15BF5B77A32__INCLUDED_)
#define AFX_PLAYLIST_H__EA74376A_83DF_435E_8484_A15BF5B77A32__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Playlist.h : header file
//


class PLEntry
{
public:
	PLEntry(CString url, char *path = NULL);
	~PLEntry();

	char *m_url;
	char *m_disp_name;
	u32 m_duration;

	Bool m_bIsSelected;
	Bool m_bIsDead;
	Bool m_bIsPlaying;
};


/////////////////////////////////////////////////////////////////////////////
// Playlist dialog

class Playlist : public CDialog
{
// Construction
public:
	Playlist();
	virtual ~Playlist();

	virtual BOOL Create() {
		/*use desktop window to enable playlist behind player*/
		return CDialog::Create(IDD_PLAYLIST, GetDesktopWindow());
	}

	CToolBar    m_toolBar;
	GF_List *m_entries;

	void Clear();
	void ClearButPlaying();
	void RefreshList();
	void AddDir(Bool do_recurse);
	void Truncate();
	void SetDead();
	void SetDuration(u32 duration);

	void Play();
	void PlayNext();
	void PlayPrev();
	Bool HasValidEntries();
	CString GetDisplayName();
	CString GetURL();

	void OpenPlayList(CString fileName);

	void QueueURL(CString filename);
	s32 m_cur_entry;

// Dialog Data
	//{{AFX_DATA(Playlist)
	enum { IDD = IDD_PLAYLIST};
	CListCtrl	m_FileList;
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(Playlist)
protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//}}AFX_VIRTUAL

// Implementation
protected:
	s32 m_all_dead_entries;
	void UpdateEntry(u32 idx);
	void RefreshCurrent();
	void Sort(u32 type);
	void Save(char *szPath, Bool save_m3u);

	// Generated message map functions
	//{{AFX_MSG(Playlist)
	virtual BOOL OnInitDialog() ;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnPlAddFile();
	afx_msg void OnPlRemFile();
	afx_msg void OnSelUp();
	afx_msg void OnSelDown();
	afx_msg void OnPlSave();
	afx_msg void OnClose();
	afx_msg void OnPlRemDead();
	afx_msg void OnPlRemAll();
	afx_msg void OnPlAddDir();
	afx_msg void OnPlAddDirRec();
	afx_msg void OnPlAddUrl();
	afx_msg void OnPlOpen();
	afx_msg void OnReverseSelection();
	afx_msg void OnReverseList();
	afx_msg void OnRandomize();
	afx_msg void OnSortTitle();
	afx_msg void OnSortFile();
	afx_msg void OnSortDuration();
	afx_msg void OnPlPlay();
	afx_msg void OnRclickFilelist(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkFilelist(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PLAYLIST_H__EA74376A_83DF_435E_8484_A15BF5B77A32__INCLUDED_)
