/* this ALWAYS GENERATED file contains the proxy stub code */


/* File created by MIDL compiler version 5.01.0164 */
/* at Mon Jul 17 15:58:48 2006
 */
/* Compiler settings for D:\CVS\gpac\applications\GPAX\GPAX.idl:
    Oicf (OptLev=i2), W1, Zp8, env=Win32, ms_ext, c_ext
    error checks: allocation ref bounds_check enum stub_data
*/
//@@MIDL_FILE_HEADING(  )

#define USE_STUBLESS_PROXY


/* verify that the <rpcproxy.h> version is high enough to compile this file*/
#ifndef __REDQ_RPCPROXY_H_VERSION__
#define __REQUIRED_RPCPROXY_H_VERSION__ 440
#endif


#include "rpcproxy.h"
#ifndef __RPCPROXY_H_VERSION__
#error this stub requires an updated version of <rpcproxy.h>
#endif // __RPCPROXY_H_VERSION__


#include "GPAX.h"

#define TYPE_FORMAT_STRING_SIZE   59
#define PROC_FORMAT_STRING_SIZE   213

typedef struct _MIDL_TYPE_FORMAT_STRING
{
	short          Pad;
	unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
} MIDL_TYPE_FORMAT_STRING;

typedef struct _MIDL_PROC_FORMAT_STRING
{
	short          Pad;
	unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
} MIDL_PROC_FORMAT_STRING;


extern const MIDL_TYPE_FORMAT_STRING __MIDL_TypeFormatString;
extern const MIDL_PROC_FORMAT_STRING __MIDL_ProcFormatString;


/* Standard interface: __MIDL_itf_GPAX_0000, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}} */


/* Object interface: IUnknown, ver. 0.0,
   GUID={0x00000000,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IDispatch, ver. 0.0,
   GUID={0x00020400,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}} */


/* Object interface: IGPAX, ver. 0.0,
   GUID={0xE2A9A937,0xBB35,0x47E0,{0x89,0x42,0x96,0x48,0x06,0x29,0x9A,0xB4}} */


extern const MIDL_STUB_DESC Object_StubDesc;


extern const MIDL_SERVER_INFO IGPAX_ServerInfo;

#pragma code_seg(".orpc")
extern const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[1];

static const MIDL_STUB_DESC Object_StubDesc =
{
	0,
	NdrOleAllocate,
	NdrOleFree,
	0,
	0,
	0,
	0,
	0,
	__MIDL_TypeFormatString.Format,
	1, /* -error bounds_check flag */
	0x20000, /* Ndr library version */
	0,
	0x50100a4, /* MIDL Version 5.1.164 */
	0,
	UserMarshalRoutines,
	0,  /* notify & notify_flag routine table */
	1,  /* Flags */
	0,  /* Reserved3 */
	0,  /* Reserved4 */
	0   /* Reserved5 */
};

static const unsigned short IGPAX_FormatStringOffsetTable[] =
{
	(unsigned short) -1,
	(unsigned short) -1,
	(unsigned short) -1,
	(unsigned short) -1,
	0,
	22,
	44,
	66,
	100,
	128,
	156,
	184
};

static const MIDL_SERVER_INFO IGPAX_ServerInfo =
{
	&Object_StubDesc,
	0,
	__MIDL_ProcFormatString.Format,
	&IGPAX_FormatStringOffsetTable[-3],
	0,
	0,
	0,
	0
};

static const MIDL_STUBLESS_PROXY_INFO IGPAX_ProxyInfo =
{
	&Object_StubDesc,
	__MIDL_ProcFormatString.Format,
	&IGPAX_FormatStringOffsetTable[-3],
	0,
	0,
	0
};

CINTERFACE_PROXY_VTABLE(15) _IGPAXProxyVtbl =
{
	&IGPAX_ProxyInfo,
	&IID_IGPAX,
	IUnknown_QueryInterface_Proxy,
	IUnknown_AddRef_Proxy,
	IUnknown_Release_Proxy ,
	0 /* (void *)-1 /* IDispatch::GetTypeInfoCount */ ,
	0 /* (void *)-1 /* IDispatch::GetTypeInfo */ ,
	0 /* (void *)-1 /* IDispatch::GetIDsOfNames */ ,
	0 /* IDispatch_Invoke_Proxy */ ,
	(void *)-1 /* IGPAX::Play */ ,
	(void *)-1 /* IGPAX::Pause */ ,
	(void *)-1 /* IGPAX::Stop */ ,
	(void *)-1 /* IGPAX::Update */ ,
	(void *)-1 /* IGPAX::QualitySwitch */ ,
	(void *)-1 /* IGPAX::get_URL */ ,
	(void *)-1 /* IGPAX::put_URL */ ,
	(void *)-1 /* IGPAX::get_AutoStart */ ,
	(void *)-1 /* IGPAX::put_AutoStart */
};


static const PRPC_STUB_FUNCTION IGPAX_table[] =
{
	STUB_FORWARDING_FUNCTION,
	STUB_FORWARDING_FUNCTION,
	STUB_FORWARDING_FUNCTION,
	STUB_FORWARDING_FUNCTION,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2,
	NdrStubCall2
};

CInterfaceStubVtbl _IGPAXStubVtbl =
{
	&IID_IGPAX,
	&IGPAX_ServerInfo,
	15,
	&IGPAX_table[-3],
	CStdStubBuffer_DELEGATING_METHODS
};

#pragma data_seg(".rdata")

static const USER_MARSHAL_ROUTINE_QUADRUPLE UserMarshalRoutines[1] =
{

	{
		BSTR_UserSize
		,BSTR_UserMarshal
		,BSTR_UserUnmarshal
		,BSTR_UserFree
	}

};


#if !defined(__RPC_WIN32__)
#error  Invalid build platform for this stub.
#endif

#if !(TARGET_IS_NT40_OR_LATER)
#error You need a Windows NT 4.0 or later to run this stub because it uses these features:
#error   -Oif or -Oicf, [wire_marshal] or [user_marshal] attribute, more than 32 methods in the interface.
#error However, your C/C++ compilation flags indicate you intend to run this app on earlier systems.
#error This app will die there with the RPC_X_WRONG_STUB_VERSION error.
#endif


static const MIDL_PROC_FORMAT_STRING __MIDL_ProcFormatString =
{
	0,
	{

		/* Procedure Play */

		0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
		/*  6 */	NdrFcShort( 0x7 ),	/* 7 */
#ifndef _ALPHA_
		/*  8 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 10 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 12 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 14 */	0x4,		/* Oi2 Flags:  has return, */
		0x1,		/* 1 */

		/* Return value */

		/* 16 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 18 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 20 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure Pause */

		/* 22 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 24 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 28 */	NdrFcShort( 0x8 ),	/* 8 */
#ifndef _ALPHA_
		/* 30 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 32 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 34 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 36 */	0x4,		/* Oi2 Flags:  has return, */
		0x1,		/* 1 */

		/* Return value */

		/* 38 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 40 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 42 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure Stop */

		/* 44 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 46 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 50 */	NdrFcShort( 0x9 ),	/* 9 */
#ifndef _ALPHA_
		/* 52 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 54 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 56 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 58 */	0x4,		/* Oi2 Flags:  has return, */
		0x1,		/* 1 */

		/* Return value */

		/* 60 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 62 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 64 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure Update */

		/* 66 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 68 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 72 */	NdrFcShort( 0xa ),	/* 10 */
#ifndef _ALPHA_
		/* 74 */	NdrFcShort( 0x10 ),	/* x86, MIPS, PPC Stack size/offset = 16 */
#else
		NdrFcShort( 0x20 ),	/* Alpha Stack size/offset = 32 */
#endif
		/* 76 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 78 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 80 */	0x6,		/* Oi2 Flags:  clt must size, has return, */
		0x3,		/* 3 */

		/* Parameter mtype */

		/* 82 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
#ifndef _ALPHA_
		/* 84 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 86 */	NdrFcShort( 0x1a ),	/* Type Offset=26 */

		/* Parameter updates */

		/* 88 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
#ifndef _ALPHA_
		/* 90 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 92 */	NdrFcShort( 0x1a ),	/* Type Offset=26 */

		/* Return value */

		/* 94 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 96 */	NdrFcShort( 0xc ),	/* x86, MIPS, PPC Stack size/offset = 12 */
#else
		NdrFcShort( 0x18 ),	/* Alpha Stack size/offset = 24 */
#endif
		/* 98 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure get_URL */

		/* 100 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 102 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 106 */	NdrFcShort( 0xb ),	/* 11 */
#ifndef _ALPHA_
		/* 108 */	NdrFcShort( 0xc ),	/* x86, MIPS, PPC Stack size/offset = 12 */
#else
		NdrFcShort( 0x18 ),	/* Alpha Stack size/offset = 24 */
#endif
		/* 110 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 112 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 114 */	0x5,		/* Oi2 Flags:  srv must size, has return, */
		0x2,		/* 2 */

		/* Parameter mrl */

		/* 116 */	NdrFcShort( 0x2113 ),	/* Flags:  must size, must free, out, simple ref, srv alloc size=8 */
#ifndef _ALPHA_
		/* 118 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 120 */	NdrFcShort( 0x2c ),	/* Type Offset=44 */

		/* Return value */

		/* 122 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 124 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 126 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure put_URL */

		/* 128 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 130 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 134 */	NdrFcShort( 0xc ),	/* 12 */
#ifndef _ALPHA_
		/* 136 */	NdrFcShort( 0xc ),	/* x86, MIPS, PPC Stack size/offset = 12 */
#else
		NdrFcShort( 0x18 ),	/* Alpha Stack size/offset = 24 */
#endif
		/* 138 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 140 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 142 */	0x6,		/* Oi2 Flags:  clt must size, has return, */
		0x2,		/* 2 */

		/* Parameter mrl */

		/* 144 */	NdrFcShort( 0x8b ),	/* Flags:  must size, must free, in, by val, */
#ifndef _ALPHA_
		/* 146 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 148 */	NdrFcShort( 0x1a ),	/* Type Offset=26 */

		/* Return value */

		/* 150 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 152 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 154 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure get_AutoStart */

		/* 156 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 158 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 162 */	NdrFcShort( 0xd ),	/* 13 */
#ifndef _ALPHA_
		/* 164 */	NdrFcShort( 0xc ),	/* x86, MIPS, PPC Stack size/offset = 12 */
#else
		NdrFcShort( 0x18 ),	/* Alpha Stack size/offset = 24 */
#endif
		/* 166 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 168 */	NdrFcShort( 0xe ),	/* 14 */
		/* 170 */	0x4,		/* Oi2 Flags:  has return, */
		0x2,		/* 2 */

		/* Parameter autoplay */

		/* 172 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
#ifndef _ALPHA_
		/* 174 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 176 */	0x6,		/* FC_SHORT */
		0x0,		/* 0 */

		/* Return value */

		/* 178 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 180 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 182 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		/* Procedure put_AutoStart */

		/* 184 */	0x33,		/* FC_AUTO_HANDLE */
		0x6c,		/* Old Flags:  object, Oi2 */
		/* 186 */	NdrFcLong( 0x0 ),	/* 0 */
		/* 190 */	NdrFcShort( 0xe ),	/* 14 */
#ifndef _ALPHA_
		/* 192 */	NdrFcShort( 0xc ),	/* x86, MIPS, PPC Stack size/offset = 12 */
#else
		NdrFcShort( 0x18 ),	/* Alpha Stack size/offset = 24 */
#endif
		/* 194 */	NdrFcShort( 0x6 ),	/* 6 */
		/* 196 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 198 */	0x4,		/* Oi2 Flags:  has return, */
		0x2,		/* 2 */

		/* Parameter autoplay */

		/* 200 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
#ifndef _ALPHA_
		/* 202 */	NdrFcShort( 0x4 ),	/* x86, MIPS, PPC Stack size/offset = 4 */
#else
		NdrFcShort( 0x8 ),	/* Alpha Stack size/offset = 8 */
#endif
		/* 204 */	0x6,		/* FC_SHORT */
		0x0,		/* 0 */

		/* Return value */

		/* 206 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
#ifndef _ALPHA_
		/* 208 */	NdrFcShort( 0x8 ),	/* x86, MIPS, PPC Stack size/offset = 8 */
#else
		NdrFcShort( 0x10 ),	/* Alpha Stack size/offset = 16 */
#endif
		/* 210 */	0x8,		/* FC_LONG */
		0x0,		/* 0 */

		0x0
	}
};

static const MIDL_TYPE_FORMAT_STRING __MIDL_TypeFormatString =
{
	0,
	{
		NdrFcShort( 0x0 ),	/* 0 */
		/*  2 */
		0x12, 0x0,	/* FC_UP */
		/*  4 */	NdrFcShort( 0xc ),	/* Offset= 12 (16) */
		/*  6 */
		0x1b,		/* FC_CARRAY */
		0x1,		/* 1 */
		/*  8 */	NdrFcShort( 0x2 ),	/* 2 */
		/* 10 */	0x9,		/* Corr desc: FC_ULONG */
		0x0,		/*  */
		/* 12 */	NdrFcShort( 0xfffc ),	/* -4 */
		/* 14 */	0x6,		/* FC_SHORT */
		0x5b,		/* FC_END */
		/* 16 */
		0x17,		/* FC_CSTRUCT */
		0x3,		/* 3 */
		/* 18 */	NdrFcShort( 0x8 ),	/* 8 */
		/* 20 */	NdrFcShort( 0xfffffff2 ),	/* Offset= -14 (6) */
		/* 22 */	0x8,		/* FC_LONG */
		0x8,		/* FC_LONG */
		/* 24 */	0x5c,		/* FC_PAD */
		0x5b,		/* FC_END */
		/* 26 */	0xb4,		/* FC_USER_MARSHAL */
		0x83,		/* 131 */
		/* 28 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 30 */	NdrFcShort( 0x4 ),	/* 4 */
		/* 32 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 34 */	NdrFcShort( 0xffffffe0 ),	/* Offset= -32 (2) */
		/* 36 */
		0x11, 0x4,	/* FC_RP [alloced_on_stack] */
		/* 38 */	NdrFcShort( 0x6 ),	/* Offset= 6 (44) */
		/* 40 */
		0x13, 0x0,	/* FC_OP */
		/* 42 */	NdrFcShort( 0xffffffe6 ),	/* Offset= -26 (16) */
		/* 44 */	0xb4,		/* FC_USER_MARSHAL */
		0x83,		/* 131 */
		/* 46 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 48 */	NdrFcShort( 0x4 ),	/* 4 */
		/* 50 */	NdrFcShort( 0x0 ),	/* 0 */
		/* 52 */	NdrFcShort( 0xfffffff4 ),	/* Offset= -12 (40) */
		/* 54 */
		0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
		/* 56 */	0x6,		/* FC_SHORT */
		0x5c,		/* FC_PAD */

		0x0
	}
};

const CInterfaceProxyVtbl * _GPAX_ProxyVtblList[] =
{
	( CInterfaceProxyVtbl *) &_IGPAXProxyVtbl,
	0
};

const CInterfaceStubVtbl * _GPAX_StubVtblList[] =
{
	( CInterfaceStubVtbl *) &_IGPAXStubVtbl,
	0
};

PCInterfaceName const _GPAX_InterfaceNamesList[] =
{
	"IGPAX",
	0
};

const IID *  _GPAX_BaseIIDList[] =
{
	&IID_IDispatch,
	0
};


#define _GPAX_CHECK_IID(n)	IID_GENERIC_CHECK_IID( _GPAX, pIID, n)

int __stdcall _GPAX_IID_Lookup( const IID * pIID, int * pIndex )
{

	if(!_GPAX_CHECK_IID(0))
	{
		*pIndex = 0;
		return 1;
	}

	return 0;
}

const ExtendedProxyFileInfo GPAX_ProxyFileInfo =
{
	(PCInterfaceProxyVtblList *) & _GPAX_ProxyVtblList,
	(PCInterfaceStubVtblList *) & _GPAX_StubVtblList,
	(const PCInterfaceName * ) & _GPAX_InterfaceNamesList,
	(const IID ** ) & _GPAX_BaseIIDList,
	& _GPAX_IID_Lookup,
	1,
	2,
	0, /* table of [async_uuid] interfaces */
	0, /* Filler1 */
	0, /* Filler2 */
	0  /* Filler3 */
};
