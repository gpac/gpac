/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __OSMO4_H__
#define __OSMO4_H__

// INCLUDES
#include <aknapp.h>
#include <akndoc.h>

class COsmo4Application : public CAknApplication
{
public:
	TUid AppDllUid() const;

protected:
	CApaDocument* CreateDocumentL();
};


class COsmo4Document : public CAknDocument
{
public:
	static COsmo4Document* NewL( CEikApplication& aApp );
	static COsmo4Document* NewLC( CEikApplication& aApp );
	virtual ~COsmo4Document();

public:
	CEikAppUi* CreateAppUiL();

private:
	void ConstructL();
	COsmo4Document( CEikApplication& aApp );

};

#endif // __OSMO4_H__

