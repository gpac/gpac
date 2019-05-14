

test_begin "mp4box-dashrip"
if [ "$test_skip" = 1 ] ; then
return
fi

do_test "$MP4BOX -mpd-rip -mpd-rip http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live/ttml.mpd -out $TEMP_DIR/" "rip"

odir=$TEMP_DIR/download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live

do_hash_test $odir/ttml.mpd "rip-mpd"
do_hash_test $odir/counter30s_dash3.m4s "rip-media"
do_hash_test $odir/ttml_dash3.m4s "rip-ttml"

test_end

