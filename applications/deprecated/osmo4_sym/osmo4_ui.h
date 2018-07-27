/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2006-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Symbian GUI player
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
 */

#ifndef __OSMO4_UI_H__
#define __OSMO4_UI_H__

// INCLUDES
#include <aknappui.h>


// FORWARD DECLARATIONS
class COsmo4AppView;
class CPlaylist;


// osmo4 enumerate command codes
enum TOsmo4Ids
{
	/*Playlist commands*/
	EOsmo4PlayListView = 0x6001,
	EOsmo4PlayListAdd,
	EOsmo4PlayListRem,
	EOsmo4PlayListMode,
	EOsmo4PlayListMoveUp,
	EOsmo4PlayListMoveDown,
	EOsmo4PlayListClear,
	EOsmo4PlayListAllFiles,
	/*file commands*/
	EOsmo4OpenURL,
	EOsmo4OpenRecentFirst,
	EOsmo4OpenRecentLast = EOsmo4OpenRecentFirst + 10,
	/*view commands*/
	EOsmo4Fullscreen,
	EOsmo4ViewMaxSize,
	EOsmo4AROriginal,
	EOsmo4ARFillScreen,
	EOsmo4AR4_3,
	EOsmo4AR16_9,
	EOsmo4NavReset,
	EOsmo4NavNone,
	EOsmo4NavSlide,
	EOsmo4NavWalk,
	EOsmo4NavFly,
	EOsmo4NavExamine,
	EOsmo4NavHeadlight,
	EOsmo4NavGravity,
	EOsmo4CollideNone,
	EOsmo4CollideSimple,
	EOsmo4CollideDisp,
	EOsmo4ViewRTI,

	/*option commands*/
	EOsmo4OptEnableLogs,
	EOsmo4OptOpenGL,
	EOsmo4OptDirectDraw,
	EOsmo4OptXMLProgressive,


};


/** osmo4 application panic codes */
enum TOsmo4Panics
{
	EOsmo4Ui = 1
	           // add further panics here
};

inline void Panic(TOsmo4Panics aReason)
{
	_LIT(applicationName,"Osmo4");
	User::Panic(applicationName, aReason);
}


#define MAX_KEY_CAP	10
typedef struct
{
	TInt32 key_cap;
	TInt32 key_cap_ud;
} KeyCapInfo;

// CLASS DECLARATION
/**
* COsmo4AppUi application UI class.
* Interacts with the user through the UI and request message processing
* from the handler class
*/
class COsmo4AppUi : public CAknAppUi
{
public: // Constructors and destructor

	/**
	* ConstructL.
	* 2nd phase constructor.
	*/
	void ConstructL();

	/**
	* COsmo4AppUi.
	* C++ default constructor. This needs to be public due to
	* the way the framework constructs the AppUi
	*/
	COsmo4AppUi();

	/**
	* ~COsmo4AppUi.
	* Virtual Destructor.
	*/
	virtual ~COsmo4AppUi();

private:  // Functions from base classes

	/**
	* From CEikAppUi, HandleCommandL.
	* Takes care of command handling.
	* @param aCommand Command to be handled.
	*/
	void HandleCommandL( TInt aCommand );


	/**
	*  HandleStatusPaneSizeChange.
	*  Called by the framework when the application status pane
	*  size is changed.
	*/

	void HandleStatusPaneSizeChange();
	virtual void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane);

public:
	void PlayURL(const char *);
	void SetTitle(const char *title, TBool store_it = ETrue);
	void SetInfo(const char *);

private:
	void TogglePlaylist();
	void SetTitleInfo(const char *);
	void HandleForegroundEventL(TBool aForeground);
	void CaptureKeys(int do_capture);
	void CaptureKey(TInt32 code, TInt32 scancode);

private:
	COsmo4AppView* iAppView;
	CPlaylist *iPlaylist;
	int view_was_max;
	int smenu_id;
	char *m_title;
	/*current view mode*/
	int view_mode;

	KeyCapInfo keys[MAX_KEY_CAP];
	int nb_keys;
};

#endif // __OSMO4_UI_H__

// End of File

