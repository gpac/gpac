


test_begin "mp4box-dsap"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/base.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $mp4file" "add"
do_hash_test $mp4file "add"

dsapfile="$TEMP_DIR/dsap.xml"
do_test "$MP4BOX -dsap 1 $mp4file -out $dsapfile" "dsap"
do_hash_test $dsapfile "dsap"

dsapfile="$TEMP_DIR/dsaps.xml"
do_test "$MP4BOX -dsaps 1 $mp4file -out $dsapfile" "dsaps"
do_hash_test $dsapfile "dsaps"

dsapfile="$TEMP_DIR/dsapc.xml"
do_test "$MP4BOX -dsapc 1 $mp4file -out $dsapfile" "dsapc"
do_hash_test $dsapfile "dsapc"

dsapfile="$TEMP_DIR/dsapd.xml"
do_test "$MP4BOX -dsapd 1 $mp4file -out $dsapfile" "dsapd"
do_hash_test $dsapfile "dsapd"

dsapfile="$TEMP_DIR/dsapp.xml"
do_test "$MP4BOX -dsapp 1 $mp4file -out $dsapfile" "dsapp"
do_hash_test $dsapfile "dsapp"


test_end



