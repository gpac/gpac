/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIScriptableInputStream.idl
 */

#ifndef __gen_nsIScriptableInputStream_h__
#define __gen_nsIScriptableInputStream_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif
class nsIInputStream; /* forward declaration */


/* starting interface:    nsIScriptableInputStream */
#define NS_ISCRIPTABLEINPUTSTREAM_IID_STR "a2a32f90-9b90-11d3-a189-0050041caf44"

#define NS_ISCRIPTABLEINPUTSTREAM_IID \
  {0xa2a32f90, 0x9b90, 0x11d3, \
    { 0xa1, 0x89, 0x00, 0x50, 0x04, 0x1c, 0xaf, 0x44 }}

/**
 * nsIScriptableInputStream provides scriptable access to the nsIInputStream.  
 *
 * @status FROZEN
 */
class NS_NO_VTABLE nsIScriptableInputStream : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTABLEINPUTSTREAM_IID)

  /** 
     * Closes the stream. 
     */
  /* void close (); */
  NS_IMETHOD Close(void) = 0;

  /** Wrap the given nsIInputStream with this nsIScriptableInputStream. 
     *  @param aInputStream [in] parameter providing the stream to wrap 
     */
  /* void init (in nsIInputStream aInputStream); */
  NS_IMETHOD Init(nsIInputStream *aInputStream) = 0;

  /** Return the number of bytes currently available in the stream 
     *  @param _retval [out] parameter to hold the number of bytes 
     *         if an error occurs, the parameter will be undefined 
     *  @return error status 
     */
  /* unsigned long available (); */
  NS_IMETHOD Available(PRUint32 *_retval) = 0;

  /** Read data from the stream. 
     *  @param aCount [in] the maximum number of bytes to read 
     *  @param _retval [out] the data
     */
  /* string read (in unsigned long aCount); */
  NS_IMETHOD Read(PRUint32 aCount, char **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSISCRIPTABLEINPUTSTREAM \
  NS_IMETHOD Close(void); \
  NS_IMETHOD Init(nsIInputStream *aInputStream); \
  NS_IMETHOD Available(PRUint32 *_retval); \
  NS_IMETHOD Read(PRUint32 aCount, char **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSISCRIPTABLEINPUTSTREAM(_to) \
  NS_IMETHOD Close(void) { return _to Close(); } \
  NS_IMETHOD Init(nsIInputStream *aInputStream) { return _to Init(aInputStream); } \
  NS_IMETHOD Available(PRUint32 *_retval) { return _to Available(_retval); } \
  NS_IMETHOD Read(PRUint32 aCount, char **_retval) { return _to Read(aCount, _retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSISCRIPTABLEINPUTSTREAM(_to) \
  NS_IMETHOD Close(void) { return !_to ? NS_ERROR_NULL_POINTER : _to->Close(); } \
  NS_IMETHOD Init(nsIInputStream *aInputStream) { return !_to ? NS_ERROR_NULL_POINTER : _to->Init(aInputStream); } \
  NS_IMETHOD Available(PRUint32 *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Available(_retval); } \
  NS_IMETHOD Read(PRUint32 aCount, char **_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->Read(aCount, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsScriptableInputStream : public nsIScriptableInputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTABLEINPUTSTREAM

  nsScriptableInputStream();
  virtual ~nsScriptableInputStream();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsScriptableInputStream, nsIScriptableInputStream)

nsScriptableInputStream::nsScriptableInputStream()
{
  /* member initializers and constructor code */
}

nsScriptableInputStream::~nsScriptableInputStream()
{
  /* destructor code */
}

/* void close (); */
NS_IMETHODIMP nsScriptableInputStream::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void init (in nsIInputStream aInputStream); */
NS_IMETHODIMP nsScriptableInputStream::Init(nsIInputStream *aInputStream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* unsigned long available (); */
NS_IMETHODIMP nsScriptableInputStream::Available(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string read (in unsigned long aCount); */
NS_IMETHODIMP nsScriptableInputStream::Read(PRUint32 aCount, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIScriptableInputStream_h__ */
