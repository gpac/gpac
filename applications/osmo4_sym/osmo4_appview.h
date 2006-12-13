/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2006-200X
 *				Authors: Jean Le Feuvre 
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


#ifndef __osmo4APPVIEW_H__
#define __osmo4APPVIEW_H__

// INCLUDES
#include <coecntrl.h>
#include <remconcoreapitargetobserver.h>
#include <remconcoreapitarget.h>
#include <remconinterfaceselector.h>

#include <gpac/terminal.h>
#include <gpac/thread.h>


// CLASS DECLARATION
class COsmo4AppView : public CCoeControl,MRemConCoreApiTargetObserver
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

		GF_User *GetUser() { return &m_user; }
		void SetupLogs();
		void MessageBox(const char *text, const char *title);

		virtual TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
		void MrccatoCommand(TRemConCoreApiOperationId aOperationId, TRemConCoreApiButtonAction aButtonAct);

		void ReloadTerminal();


		TInt OnTick();

		void Shutdown();
	
		void Connect(const char *url);
		void ShowHide(Bool show);
		Bool EventProc(GF_Event *evt);

		GF_Terminal *m_term;

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
		GF_SystemRTInfo m_rti;
		CRemConInterfaceSelector *selector;
		CRemConCoreApiTarget *target;

public:
		u32 last_title_update;
		Bool do_log;
		Bool show_rti;
		GF_Mutex *m_mx;
		GF_User m_user;
    };


#endif // __osmo4APPVIEW_H__

// End of File

