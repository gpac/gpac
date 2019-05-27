/*
* DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIOsmozilla.idl
*/

#ifndef __gen_nsIOsmozilla_h__
#define __gen_nsIOsmozilla_h__

#include "osmo_npapi.h"

#ifdef GECKO_XPCOM

#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    nsIOsmozilla */
#define NS_IOSMOZILLA_IID_STR "d2d536a0-b6fc-11d5-9d10-0060b0fbd80b"

#define NS_IOSMOZILLA_IID \
  {0xd2d536a0, 0xb6fc, 0x11d5, \
	{ 0x9d, 0x10, 0x00, 0x60, 0xb0, 0xfb, 0xd8, 0x0b }}

class NS_NO_VTABLE nsIOsmozilla : public nsISupports {
public:

	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOSMOZILLA_IID)

	/* void Pause (); */
	NS_IMETHOD Pause(void) = 0;

	/* void Play (); */
	NS_IMETHOD Play(void) = 0;

	/* void Stop (); */
	NS_IMETHOD Stop(void) = 0;

	/* void Update (in string type, in string commands); */
	NS_IMETHOD Update(const char *type, const char *commands) = 0;

	/* void QualitySwitch (in int switch_up); */
	NS_IMETHOD QualitySwitch(int switch_up) = 0;

	/* void SetURL (in string url); */
	NS_IMETHOD SetURL(const char *url) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIOSMOZILLA \
	NS_IMETHOD Pause(void); \
	NS_IMETHOD Play(void); \
	NS_IMETHOD Stop(void); \
	NS_IMETHOD Update(const char *type, const char *commands); \
	NS_IMETHOD QualitySwitch(int switch_up); \
	NS_IMETHOD SetURL(const char *type);

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIOSMOZILLA(_to) \
	NS_IMETHOD Pause(void) { return _to Pause(); } \
	NS_IMETHOD Play(void) { return _to Play(); } \
	NS_IMETHOD Stop(void) { return _to Stop(); } \
	NS_IMETHOD Update(const char *type, const char *commands) { return _to Update(type, commands); } \
	NS_IMETHOD QualitySwitch(int switch_up) { return _to QualitySwitch( switch_up ); } \
	NS_IMETHOD SetURL(const char *url) { return _to SetURL(url); }

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSIOSMOZILLA(_to) \
	NS_IMETHOD Pause(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Pause(); } \
	NS_IMETHOD Play(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Play(); } \
	NS_IMETHOD Stop(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Stop(); } \
	NS_IMETHOD Update(const char *type, const char *commands) { return !_to ? NS_ERROR_NULL_POINTER : _to->Update(type, commands); } \
	NS_IMETHOD QualitySwitch(int switch_up) { return !_to ? NS_ERROR_NULL_POINTER : _to->QualitySwitch(switch_up); } \
	NS_IMETHOD SetURL(const char *url) { return !_to ? NS_ERROR_NULL_POINTER : _to->Update(url); } \
 
#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsOsmozilla : public nsIOsmozilla
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIOSMOZILLA

	nsOsmozilla();
	virtual ~nsOsmozilla();
	/* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsOsmozilla, nsIOsmozilla)

nsOsmozilla::nsOsmozilla()
{
	/* member initializers and constructor code */
}

nsOsmozilla::~nsOsmozilla()
{
	/* destructor code */
}

/* void Pause (); */
NS_IMETHODIMP nsOsmozilla::Pause()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Play (); */
NS_IMETHODIMP nsOsmozilla::Play()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Stop (); */
NS_IMETHODIMP nsOsmozilla::Stop()
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Update (in string type, in string commands); */
NS_IMETHODIMP nsOsmozilla::Update(const char *type, const char *commands)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void QualitySwitch (in int switch_up); */
NS_IMETHODIMP nsOsmozilla::QualitySwitch(int switch_up)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* void SetURL (in string url); */
NS_IMETHODIMP nsOsmozilla::SetURL(const char *type)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

#endif //GECKO_XPCOM

#endif /* __gen_nsIOsmozilla_h__ */
