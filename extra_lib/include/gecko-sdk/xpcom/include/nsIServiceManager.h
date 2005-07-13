/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM c:/builds/seamonkey/mozilla/xpcom/components/nsIServiceManager.idl
 */

#ifndef __gen_nsIServiceManager_h__
#define __gen_nsIServiceManager_h__


#ifndef __gen_nsISupports_h__
#include "nsISupports.h"
#endif

/* For IDL files that don't want to include root IDL files. */
#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

/* starting interface:    nsIServiceManager */
#define NS_ISERVICEMANAGER_IID_STR "8bb35ed9-e332-462d-9155-4a002ab5c958"

#define NS_ISERVICEMANAGER_IID \
  {0x8bb35ed9, 0xe332, 0x462d, \
    { 0x91, 0x55, 0x4a, 0x00, 0x2a, 0xb5, 0xc9, 0x58 }}

/**
 * The nsIServiceManager manager interface provides a means to obtain
 * global services in an application. The service manager depends on the 
 * repository to find and instantiate factories to obtain services.
 *
 * Users of the service manager must first obtain a pointer to the global
 * service manager by calling NS_GetServiceManager. After that, 
 * they can request specific services by calling GetService. When they are
 * finished they can NS_RELEASE() the service as usual.
 *
 * A user of a service may keep references to particular services indefinitely
 * and only must call Release when it shuts down.
 *
 * @status FROZEN
 */
class NS_NO_VTABLE nsIServiceManager : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISERVICEMANAGER_IID)

  /**
     * getServiceByContractID
     *
     * Returns the instance that implements aClass or aContractID and the
     * interface aIID.  This may result in the instance being created.
     *
     * @param aClass or aContractID : aClass or aContractID of object 
     *                                instance requested
     * @param aIID : IID of interface requested
     * @param result : resulting service 
     */
  /* void getService (in nsCIDRef aClass, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
  NS_IMETHOD GetService(const nsCID & aClass, const nsIID & aIID, void * *result) = 0;

  /* void getServiceByContractID (in string aContractID, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
  NS_IMETHOD GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result) = 0;

  /**
     * isServiceInstantiated
     *
     * isServiceInstantiated will return a true if the service has already
     * been created, otherwise false
     *
     * @param aClass or aContractID : aClass or aContractID of object 
     *                                instance requested
     * @param aIID : IID of interface requested
     * @param aIID : IID of interface requested
     */
  /* boolean isServiceInstantiated (in nsCIDRef aClass, in nsIIDRef aIID); */
  NS_IMETHOD IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval) = 0;

  /* boolean isServiceInstantiatedByContractID (in string aContractID, in nsIIDRef aIID); */
  NS_IMETHOD IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSISERVICEMANAGER \
  NS_IMETHOD GetService(const nsCID & aClass, const nsIID & aIID, void * *result); \
  NS_IMETHOD GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result); \
  NS_IMETHOD IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval); \
  NS_IMETHOD IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSISERVICEMANAGER(_to) \
  NS_IMETHOD GetService(const nsCID & aClass, const nsIID & aIID, void * *result) { return _to GetService(aClass, aIID, result); } \
  NS_IMETHOD GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result) { return _to GetServiceByContractID(aContractID, aIID, result); } \
  NS_IMETHOD IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval) { return _to IsServiceInstantiated(aClass, aIID, _retval); } \
  NS_IMETHOD IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval) { return _to IsServiceInstantiatedByContractID(aContractID, aIID, _retval); } 

/* Use this macro to declare functions that forward the behavior of this interface to another object in a safe way. */
#define NS_FORWARD_SAFE_NSISERVICEMANAGER(_to) \
  NS_IMETHOD GetService(const nsCID & aClass, const nsIID & aIID, void * *result) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetService(aClass, aIID, result); } \
  NS_IMETHOD GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result) { return !_to ? NS_ERROR_NULL_POINTER : _to->GetServiceByContractID(aContractID, aIID, result); } \
  NS_IMETHOD IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->IsServiceInstantiated(aClass, aIID, _retval); } \
  NS_IMETHOD IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval) { return !_to ? NS_ERROR_NULL_POINTER : _to->IsServiceInstantiatedByContractID(aContractID, aIID, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsServiceManager : public nsIServiceManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEMANAGER

  nsServiceManager();
  virtual ~nsServiceManager();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsServiceManager, nsIServiceManager)

nsServiceManager::nsServiceManager()
{
  /* member initializers and constructor code */
}

nsServiceManager::~nsServiceManager()
{
  /* destructor code */
}

/* void getService (in nsCIDRef aClass, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP nsServiceManager::GetService(const nsCID & aClass, const nsIID & aIID, void * *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getServiceByContractID (in string aContractID, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP nsServiceManager::GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isServiceInstantiated (in nsCIDRef aClass, in nsIIDRef aIID); */
NS_IMETHODIMP nsServiceManager::IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isServiceInstantiatedByContractID (in string aContractID, in nsIIDRef aIID); */
NS_IMETHODIMP nsServiceManager::IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

#define NS_ERROR_SERVICE_NOT_FOUND      NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XPCOM, 22)
#define NS_ERROR_SERVICE_IN_USE         NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XPCOM, 23)
// Observing xpcom startup.  If you component has not been created, it will be.
#define NS_XPCOM_STARTUP_OBSERVER_ID "xpcom-startup"
// Observing xpcom shutdown
#define NS_XPCOM_SHUTDOWN_OBSERVER_ID "xpcom-shutdown"
// Observing xpcom autoregistration.  Topics will be 'start' and 'stop'.
#define NS_XPCOM_AUTOREGISTRATION_OBSERVER_ID "xpcom-autoregistration"
#ifndef MOZILLA_STRICT_API
#include "nsXPCOM.h"
#include "nsIServiceManagerUtils.h"
#include "nsIServiceManagerObsolete.h"
#endif

#endif /* __gen_nsIServiceManager_h__ */
