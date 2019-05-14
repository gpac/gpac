

test_begin "mp4box-dashrip"
if [ "$test_skip" != 1 ] ; then

do_test "$MP4BOX -mpd-rip -mpd-rip http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live/ttml.mpd -out $TEMP_DIR/" "rip"

odir=$TEMP_DIR/download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live

do_hash_test $odir/ttml.mpd "rip-mpd"
do_hash_test $odir/counter30s_dash3.m4s "rip-media"
do_hash_test $odir/ttml_dash3.m4s "rip-ttml"

fi

test_end


test_begin "mp4box-wget"
if [ "$test_skip" != 1 ] ; then

do_test "$MP4BOX -wget http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live/ttml.mpd $TEMP_DIR/test.xml" "wget"
do_hash_test $TEMP_DIR/test.xml "wget"

do_test "$MP4BOX -wget https://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/ttml-live/ttml.mpd $TEMP_DIR/test2.xml" "wget-https"
do_hash_test $TEMP_DIR/test2.xml "wget-https"

fi

test_end

