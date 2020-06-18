to regenerate plugin interface from IDL:

xpidl -m header -I path_to\gecko-sdk\xpcom\idl nsIOsmozilla.idl
xpidl -m typelib -I path_to\gecko-sdk\xpcom\idl nsIOsmozilla.idl
xpt_link nposmozilla.xpt nsIOsmozilla.xpt 

This MUST be done for win32 and linux OSs independently, an .xpt file generated on one OS is not compatible with another OS...
Please keep the w32 file "nsIOsmozilla.xpt_w32" and the linux one "nsIOsmozilla.xpt_linux" to bear with makefiles...