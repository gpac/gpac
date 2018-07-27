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


#ifndef __OSMO4_VIEW_H__
#define __OSMO4_VIEW_H__

// INCLUDES
#include <coecntrl.h>

#if defined(__SERIES60_3X__)
#include <remconcoreapitargetobserver.h>
#include <remconcoreapitarget.h>
#include <remconinterfaceselector.h>
#endif


#include <gpac/terminal.h>
#include <gpac/thread.h>


// CLASS DECLARATION
class COsmo4AppView : public CCoeControl
#if defined(__SERIES60_3X__)
	,MRemConCoreApiTargetObserver
#endif

{
public: // New methods

	/**
	* NewL.
	* Two-phased constructor.
	* Create a COsmo4AppView object, which will draw itself to aRect.
	* @param aRect The rectangle this view will be drawn to.
	* @return a pointer to the created instance of COsmo4AppView.
	*/
	static COsmo4AppView* NewL( const TRect& aRect );

	/**
	* NewLC.
	* Two-phased constructor.
	* Create a COsmo4AppView object, which will draw itself
	* to aRect.
	* @param aRect Rectangle this view will be drawn to.
	* @return A pointer to the created instance of COsmo4AppView.
	*/
	static COsmo4AppView* NewLC( const TRect& aRect );

	/**
	* ~COsmo4AppView
	* Virtual Destructor.
	*/
	virtual ~COsmo4AppView();

public:  // Functions from base classes

	/**
	* From CCoeControl, Draw
	* Draw this COsmo4AppView to the screen.
	* @param aRect the rectangle of this view that needs updating
	*/
	void Draw( const TRect& aRect ) const;

	/**
	* From CoeControl, SizeChanged.
	* Called by framework when the view size is changed.
	*/
	virtual void SizeChanged();

#ifndef GPAC_GUI_ONLY
	GF_User *GetUser() {
		return &m_user;
	}
#else
	GF_User *GetUser() {
		return NULL;
	}
#endif
	void SetupLogs();
	void MessageBox(const char *text, const char *title);

	virtual TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

#if defined(__SERIES60_3X__)
	void MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct);
#endif

	TInt OnTick();

	void Shutdown();

	void Connect(const char *url);
	void ShowHide(Bool show);
	Bool EventProc(GF_Event *evt);

#ifndef GPAC_GUI_ONLY
	GF_Terminal *m_term;
#endif

private: // Constructors

	/**
	* ConstructL
	* 2nd phase constructor.
	* Perform the second phase construction of a
	* COsmo4AppView object.
	* @param aRect The rectangle this view will be drawn to.
	*/
	void ConstructL(const TRect& aRect);

	void DisplayRTI();

	/**
	* COsmo4AppView.
	* C++ default constructor.
	*/
	COsmo4AppView();

	CPeriodic *m_pTimer;

	RWindow m_window;
	RWsSession m_session;
#ifndef GPAC_GUI_ONLY
	GF_SystemRTInfo m_rti;
#endif

#if defined(__SERIES60_3X__)
	CRemConInterfaceSelector *selector;
	CRemConCoreApiTarget *target;
#endif


public:
	u32 last_title_update;
	FILE *m_Logs;
	Bool do_log;
	Bool show_rti;
#ifndef GPAC_GUI_ONLY
	GF_Mutex *m_mx;
	GF_User m_user;
#endif
};


#endif // __OSMO4_VIEW_H__

// End of File

