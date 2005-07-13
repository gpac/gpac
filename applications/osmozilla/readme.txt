to regenerate plugin interface from IDL:

xpidl -m header -I path_to\gecko-sdk\xpcom\idl nsIOsmozilla.idl
xpidl -m typelib -I path_to\gecko-sdk\xpcom\idl nsIOsmozilla.idl
xpt_link nposmozilla.xpt nsIOsmozilla.xpt 
