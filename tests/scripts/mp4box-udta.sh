

test_begin "mp4box-udta"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/udtamoov.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/logo.png -udta ID=0:type=GPAC:src=$MEDIA_DIR/auxiliary_files/subtitle.srt $mp4file" "udta-moov"
do_hash_test $mp4file "udta-moov"

mp4file="$TEMP_DIR/udtatrack.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/logo.png -udta ID=1:type=GPAC:src=base64,dGVzdA== $mp4file" "udta-track"
do_hash_test $mp4file "udta-track"

test_end


