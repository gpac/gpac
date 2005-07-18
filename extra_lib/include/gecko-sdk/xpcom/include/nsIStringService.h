/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIStringService.idl
 */

#ifndef __gen_nsIStringService_h__
#define __gen_nsIStringService_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    nsIStringService */
#define NS_ISTRINGSERVICE_IID_STR "1e66d70c-d649-441b-ac90-bf6429ead7da"

#define NS_ISTRINGSERVICE_IID \
  {0x1e66d70c, 0xd649, 0x441b, \
    { 0xac, 0x90, 0xbf, 0x64, 0x29, 0xea, 0xd7, 0xda }}

/**
 * nsIStringService - A factory for nsAString and nsACString objects.
 *
 * UNDER_REVIEW
 */
class NS_NO_VTABLE nsIStringService : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTRINGSERVICE_IID)

  /**
   * Creates an nsAString from a |PRUnichar| buffer and length.
   *
   * Must be destroyed by deleteAString();
   * 
   * @param aString : A unicode string assigned to the new nsAString
   * @param aLength : The length of the string to be assigned to the new 
   *                  nsAString
   */
  /* nsAString createAString (in wstring aString, in long aLength); */
  NS_IMETHOD CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval) = 0;

  /**
   * Creates an nsAString from a |char| buffer and lenth.
   * 
   * Must be destroyed by deleteACString();
   *
   * @param aString : A string assigned to the new nsACString
   * @param aLength : The length of the string to be assigned to the new 
   *                  nsACString
   */
  /* nsACString createACString (in string aString, in long aLength); */
  NS_IMETHOD CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval) = 0;

  /**
   * Frees memory associated with the |nsAString|.  After calling this method,
   * the |aString| is no longer valid.
   *
   * @param aString : The |nsAString| object.
   */
  /* void deleteAString (in nsAString aString); */
  NS_IMETHOD DeleteAString(nsAString * aString) = 0;

  /**
   * Frees memory associated with the |nsACString|.  After calling this method,
   * the |aString| is no longer valid.
   *
   * @param aString : The |nsACString| object.
   */
  /* void deleteACString (in nsACString aString); */
  NS_IMETHOD DeleteACString(nsACString * aString) = 0;

  /**
   * Returns a new |PRUnichar| buffer containing the bytes of |aString|.  This new 
   * buffer may contain embedded null characters.  The length of this new buffer
   * is given by |aString.Length()|.
   *
   * Allocates and returns a new |char| buffer which you must free with 
   * |nsMemory::Free|.
   *
   * @param aString : The |nsAString| object.
   * @return a new |PRUnichar| buffer you must free with |nsMemory::Free|.
   */
  /* wstring getWString (in AString aString); */
  NS_IMETHOD GetWString(const nsAString & aString, PRUnichar **_retval) = 0;

  /**
   * Returns a new |char| buffer containing the bytes of |aString|.  This new 
   * buffer may contain embedded null characters.  The length of this new buffer
   * is given by |aString.Length()|.
   *
   * Allocates and returns a new |char| buffer which you must free with 
   * |nsMemory::Free|.
   *
   * @param aString : The |nsACString| object.
   * @return a new |char| buffer you must free with |nsMemory::Free|.
   */
  /* string getString (in ACString aString); */
  NS_IMETHOD GetString(const nsACString & aString, char **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSISTRINGSERVICE \
  NS_IMETHOD CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval); \
  NS_IMETHOD CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval); \
  NS_IMETHOD DeleteAString(nsAString * aString); \
  NS_IMETHOD DeleteACString(nsACString * aString); \
  NS_IMETHOD GetWString(const nsAString & aString, PRUnichar **_retval); \
  NS_IMETHOD GetString(const nsACString & aString, char **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSISTRINGSERVICE(_to) \
  NS_IMETHOD CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval) { return _to CreateAString(aString, aLength, _retval); } \
  NS_IMETHOD CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval) { return _to CreateACString(aString, aLength, _retval); } \
  NS_IMETHOD DeleteAString(nsAString * aString) { return _to DeleteAString(aString); } \
  NS_IMETHOD DeleteACString(nsACString * aString) { return _to DeleteACString(aString); } \
  NS_IMETHOD GetWString(const nsAString & aString, PRUnichar **_retval) { return _to GetWString(aString, _retval); } \
  NS_IMETHOD GetString(const nsACString & aString, char **_retval) { return _to GetString(aString, _retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSISTRINGSERVICE(_to) \
  NS_IMETHOD CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->CreateAString(aString, aLength, _retval); } \
  NS_IMETHOD CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->CreateACString(aString, aLength, _retval); } \
  NS_IMETHOD DeleteAString(nsAString * aString) { return !_to ? NS_ERROR_NULL_POINTER : _to->DeleteAString(aString); } \
  NS_IMETHOD DeleteACString(nsACString * aString) { return !_to ? NS_ERROR_NULL_POINTER : _to->DeleteACString(aString); } \
  NS_IMETHOD GetWString(const nsAString & aString, PRUnichar **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetWString(aString, _retval); } \
  NS_IMETHOD GetString(const nsACString & aString, char **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetString(aString, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsStringService : public nsIStringService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGSERVICE

  nsStringService();
  virtual ~nsStringService();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsStringService, nsIStringService)

nsStringService::nsStringService()
{
  /* member initializers and constructor code */
}

nsStringService::~nsStringService()
{
  /* destructor code */
}

/* nsAString createAString (in wstring aString, in long aLength); */
NS_IMETHODIMP nsStringService::CreateAString(const PRUnichar *aString, PRInt32 aLength, nsAString * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsACString createACString (in string aString, in long aLength); */
NS_IMETHODIMP nsStringService::CreateACString(const char *aString, PRInt32 aLength, nsACString * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deleteAString (in nsAString aString); */
NS_IMETHODIMP nsStringService::DeleteAString(nsAString * aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void deleteACString (in nsACString aString); */
NS_IMETHODIMP nsStringService::DeleteACString(nsACString * aString)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* wstring getWString (in AString aString); */
NS_IMETHODIMP nsStringService::GetWString(const nsAString & aString, PRUnichar **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string getString (in ACString aString); */
NS_IMETHODIMP nsStringService::GetString(const nsACString & aString, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIStringService_h__ */
