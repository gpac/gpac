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

#ifndef __osmo4DOCUMENT_H__
#define __osmo4DOCUMENT_H__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class COsmo4AppUi;
class CEikApplication;


// CLASS DECLARATION

/**
* COsmo4Document application class.
* An instance of class COsmo4Document is the Document part of the
* AVKON application framework for the Osmo4 example application.
*/
class COsmo4Document : public CAknDocument
    {
    public: // Constructors and destructor

        /**
        * NewL.
        * Two-phased constructor.
        * Construct a COsmo4Document for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of COsmo4Document.
        */
        static COsmo4Document* NewL( CEikApplication& aApp );

        /**
        * NewLC.
        * Two-phased constructor.
        * Construct a COsmo4Document for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of COsmo4Document.
        */
        static COsmo4Document* NewLC( CEikApplication& aApp );

        /**
        * ~COsmo4Document
        * Virtual Destructor.
        */
        virtual ~COsmo4Document();

    public: // Functions from base classes

        /**
        * CreateAppUiL
        * From CEikDocument, CreateAppUiL.
        * Create a COsmo4AppUi object and return a pointer to it.
        * The object returned is owned by the Uikon framework.
        * @return Pointer to created instance of AppUi.
        */
        CEikAppUi* CreateAppUiL();

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * COsmo4Document.
        * C++ default constructor.
        * @param aApp Application creating this document.
        */
        COsmo4Document( CEikApplication& aApp );

    };

#endif // __osmo4DOCUMENT_H__

// End of File

