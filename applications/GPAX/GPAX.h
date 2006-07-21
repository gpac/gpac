
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 5.03.0286 */
/* at Thu Jul 20 19:14:15 2006
 */
/* Compiler settings for \CVS\gpac\applications\GPAX\GPAX.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32 (32b run), ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __GPAX_h__
#define __GPAX_h__

/* Forward Declarations */ 

#ifndef __IGPAX_FWD_DEFINED__
#define __IGPAX_FWD_DEFINED__
typedef interface IGPAX IGPAX;
#endif 	/* __IGPAX_FWD_DEFINED__ */


#ifndef __IGPAXEvents_FWD_DEFINED__
#define __IGPAXEvents_FWD_DEFINED__
typedef interface IGPAXEvents IGPAXEvents;
#endif 	/* __IGPAXEvents_FWD_DEFINED__ */


#ifndef __GPAX_FWD_DEFINED__
#define __GPAX_FWD_DEFINED__

#ifdef __cplusplus
typedef class GPAX GPAX;
#else
typedef struct GPAX GPAX;
#endif /* __cplusplus */

#endif 	/* __GPAX_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t);
void __RPC_USER MIDL_user_free( void __RPC_FAR * ); 


#ifndef __GPAXLib_LIBRARY_DEFINED__
#define __GPAXLib_LIBRARY_DEFINED__

/* library GPAXLib */
/* [helpstring][version][uuid] */ 



#define	DISPID_SRC	( 100 )

#define	DISPID_AutoStart	( 101 )

#define	DISPID_PlayEvent	( 100 )

#define	DISPID_PauseEvent	( 101 )

#define	DISPID_StopEvent	( 102 )


EXTERN_C const IID LIBID_GPAXLib;

#ifndef __IGPAX_INTERFACE_DEFINED__
#define __IGPAX_INTERFACE_DEFINED__

/* interface IGPAX */
/* [object][oleautomation][hidden][dual][helpstring][uuid] */ 


EXTERN_C const IID IID_IGPAX;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("E2A9A937-BB35-47E0-8942-964806299AB4")
    IGPAX : public IDispatch
    {
    public:
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Play( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Pause( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Stop( void) = 0;
        
        virtual /* [helpstring] */ HRESULT STDMETHODCALLTYPE Update( 
            /* [in] */ BSTR mtype,
            /* [in] */ BSTR updates) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_src( 
            /* [retval][out] */ BSTR __RPC_FAR *url) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_src( 
            /* [in] */ BSTR url) = 0;
        
        virtual /* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE get_AutoStart( 
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *autoplay) = 0;
        
        virtual /* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE put_AutoStart( 
            /* [in] */ VARIANT_BOOL autoplay) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IGPAXVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IGPAX __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IGPAX __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IGPAX __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Play )( 
            IGPAX __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Pause )( 
            IGPAX __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Stop )( 
            IGPAX __RPC_FAR * This);
        
        /* [helpstring] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Update )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ BSTR mtype,
            /* [in] */ BSTR updates);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_src )( 
            IGPAX __RPC_FAR * This,
            /* [retval][out] */ BSTR __RPC_FAR *url);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_src )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ BSTR url);
        
        /* [helpstring][propget][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *get_AutoStart )( 
            IGPAX __RPC_FAR * This,
            /* [retval][out] */ VARIANT_BOOL __RPC_FAR *autoplay);
        
        /* [helpstring][propput][id] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *put_AutoStart )( 
            IGPAX __RPC_FAR * This,
            /* [in] */ VARIANT_BOOL autoplay);
        
        END_INTERFACE
    } IGPAXVtbl;

    interface IGPAX
    {
        CONST_VTBL struct IGPAXVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGPAX_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IGPAX_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IGPAX_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IGPAX_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IGPAX_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IGPAX_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IGPAX_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)


#define IGPAX_Play(This)	\
    (This)->lpVtbl -> Play(This)

#define IGPAX_Pause(This)	\
    (This)->lpVtbl -> Pause(This)

#define IGPAX_Stop(This)	\
    (This)->lpVtbl -> Stop(This)

#define IGPAX_Update(This,mtype,updates)	\
    (This)->lpVtbl -> Update(This,mtype,updates)

#define IGPAX_get_src(This,url)	\
    (This)->lpVtbl -> get_src(This,url)

#define IGPAX_put_src(This,url)	\
    (This)->lpVtbl -> put_src(This,url)

#define IGPAX_get_AutoStart(This,autoplay)	\
    (This)->lpVtbl -> get_AutoStart(This,autoplay)

#define IGPAX_put_AutoStart(This,autoplay)	\
    (This)->lpVtbl -> put_AutoStart(This,autoplay)

#endif /* COBJMACROS */


#endif 	/* C style interface */



/* [helpstring] */ HRESULT STDMETHODCALLTYPE IGPAX_Play_Proxy( 
    IGPAX __RPC_FAR * This);


void __RPC_STUB IGPAX_Play_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IGPAX_Pause_Proxy( 
    IGPAX __RPC_FAR * This);


void __RPC_STUB IGPAX_Pause_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IGPAX_Stop_Proxy( 
    IGPAX __RPC_FAR * This);


void __RPC_STUB IGPAX_Stop_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring] */ HRESULT STDMETHODCALLTYPE IGPAX_Update_Proxy( 
    IGPAX __RPC_FAR * This,
    /* [in] */ BSTR mtype,
    /* [in] */ BSTR updates);


void __RPC_STUB IGPAX_Update_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IGPAX_get_src_Proxy( 
    IGPAX __RPC_FAR * This,
    /* [retval][out] */ BSTR __RPC_FAR *url);


void __RPC_STUB IGPAX_get_src_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IGPAX_put_src_Proxy( 
    IGPAX __RPC_FAR * This,
    /* [in] */ BSTR url);


void __RPC_STUB IGPAX_put_src_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propget][id] */ HRESULT STDMETHODCALLTYPE IGPAX_get_AutoStart_Proxy( 
    IGPAX __RPC_FAR * This,
    /* [retval][out] */ VARIANT_BOOL __RPC_FAR *autoplay);


void __RPC_STUB IGPAX_get_AutoStart_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);


/* [helpstring][propput][id] */ HRESULT STDMETHODCALLTYPE IGPAX_put_AutoStart_Proxy( 
    IGPAX __RPC_FAR * This,
    /* [in] */ VARIANT_BOOL autoplay);


void __RPC_STUB IGPAX_put_AutoStart_Stub(
    IRpcStubBuffer *This,
    IRpcChannelBuffer *_pRpcChannelBuffer,
    PRPC_MESSAGE _pRpcMessage,
    DWORD *_pdwStubPhase);



#endif 	/* __IGPAX_INTERFACE_DEFINED__ */


#ifndef __IGPAXEvents_DISPINTERFACE_DEFINED__
#define __IGPAXEvents_DISPINTERFACE_DEFINED__

/* dispinterface IGPAXEvents */
/* [helpstring][uuid] */ 


EXTERN_C const IID DIID_IGPAXEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)

    MIDL_INTERFACE("1FDA32FC-4C9A-461F-B33B-0715B0343006")
    IGPAXEvents : public IDispatch
    {
    };
    
#else 	/* C style interface */

    typedef struct IGPAXEventsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *QueryInterface )( 
            IGPAXEvents __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ void __RPC_FAR *__RPC_FAR *ppvObject);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *AddRef )( 
            IGPAXEvents __RPC_FAR * This);
        
        ULONG ( STDMETHODCALLTYPE __RPC_FAR *Release )( 
            IGPAXEvents __RPC_FAR * This);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfoCount )( 
            IGPAXEvents __RPC_FAR * This,
            /* [out] */ UINT __RPC_FAR *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetTypeInfo )( 
            IGPAXEvents __RPC_FAR * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE __RPC_FAR *GetIDsOfNames )( 
            IGPAXEvents __RPC_FAR * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
            /* [in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE __RPC_FAR *Invoke )( 
            IGPAXEvents __RPC_FAR * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
            /* [out] */ VARIANT __RPC_FAR *pVarResult,
            /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
            /* [out] */ UINT __RPC_FAR *puArgErr);
        
        END_INTERFACE
    } IGPAXEventsVtbl;

    interface IGPAXEvents
    {
        CONST_VTBL struct IGPAXEventsVtbl __RPC_FAR *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IGPAXEvents_QueryInterface(This,riid,ppvObject)	\
    (This)->lpVtbl -> QueryInterface(This,riid,ppvObject)

#define IGPAXEvents_AddRef(This)	\
    (This)->lpVtbl -> AddRef(This)

#define IGPAXEvents_Release(This)	\
    (This)->lpVtbl -> Release(This)


#define IGPAXEvents_GetTypeInfoCount(This,pctinfo)	\
    (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo)

#define IGPAXEvents_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo)

#define IGPAXEvents_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)

#define IGPAXEvents_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)

#endif /* COBJMACROS */


#endif 	/* C style interface */


#endif 	/* __IGPAXEvents_DISPINTERFACE_DEFINED__ */


EXTERN_C const CLSID CLSID_GPAX;

#ifdef __cplusplus

class DECLSPEC_UUID("181D18E6-4DC1-4B55-B72E-BE2A10064995")
GPAX;
#endif
#endif /* __GPAXLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


