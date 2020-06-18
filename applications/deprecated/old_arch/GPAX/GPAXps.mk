
GPAXps.dll: dlldata.obj GPAX_p.obj GPAX_i.obj
	link /dll /out:GPAXps.dll /def:GPAXps.def /entry:DllMain dlldata.obj GPAX_p.obj GPAX_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del GPAXps.dll
	@del GPAXps.lib
	@del GPAXps.exp
	@del dlldata.obj
	@del GPAX_p.obj
	@del GPAX_i.obj
