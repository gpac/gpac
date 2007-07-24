#include "osmo4.h"
#include "osmo4_ui.h"


EXPORT_C CApaApplication* NewApplication()
	{
	return new COsmo4Application;
	}


#ifdef EKA2

#include <eikstart.h>

GLDEF_C TInt E32Main() 
{
    return EikStart::RunApplication( NewApplication );
}

#else


GLDEF_C TInt E32Dll( TDllReason /*aReason*/ )
{
    return KErrNone;
}

#endif


#if defined(__SERIES60_3X__)
const TUid KUidOsmo4App = { 0xf01f9075 };
#else
const TUid KUidOsmo4App = { 0x1000AC00 };
#endif


CApaDocument* COsmo4Application::CreateDocumentL()
{
    return (static_cast<CApaDocument*> ( COsmo4Document::NewL( *this ) ) );
}

TUid COsmo4Application::AppDllUid() const
{
    return KUidOsmo4App;
}


COsmo4Document* COsmo4Document::NewL( CEikApplication& aApp )
{
    COsmo4Document* self = NewLC( aApp );
    CleanupStack::Pop( self );
    return self;
}

COsmo4Document* COsmo4Document::NewLC( CEikApplication& aApp )
{
    COsmo4Document* self =
        new ( ELeave ) COsmo4Document( aApp );

    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
}
void COsmo4Document::ConstructL()
{
}

COsmo4Document::COsmo4Document( CEikApplication& aApp )
    : CAknDocument( aApp )
{
}

COsmo4Document::~COsmo4Document()
{
}

CEikAppUi* COsmo4Document::CreateAppUiL()
{
    return ( static_cast <CEikAppUi*> ( new ( ELeave ) COsmo4AppUi ) );
}

