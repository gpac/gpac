#!/bin/bash
#
# calls ./configure and prints out build dependency for platform
#
# Shell-script based on code suplied in [1]
#     [1] Guide du nouveau responsable Debian. Copyright © 1998-2002 Josip Rodin

strace -f -o /tmp/log ./configure
echo ""
echo ""
echo "GPAC Build dependency for your platform"
       for x in `dpkg -S $(grep open /tmp/log|\
                           perl -pe 's!.* open\(\"([^\"]*).*!$1!' |\
                           grep "^/"| sort | uniq|\
                           grep -v "^\(/tmp\|/dev\|/proc\)" ) 2>/dev/null|\
                           cut -f1 -d":"| sort | uniq`; \
             do \
               echo -n "$x (>=" `dpkg -s $x|grep ^Version|cut -f2 -d":"` "), "; \
             done
echo ""
echo ""
