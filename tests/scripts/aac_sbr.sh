

aac_test()
{

test_begin "aac-$2"

if [ $test_skip  = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/$2.mp4"
mpdfile="$TEMP_DIR/$2.mpd"

do_test "$MP4BOX -add $EXTERNAL_MEDIA_DIR/misc/sbrps_fhg.aac:$1 -new $mp4file" "import"
do_hash_test $mp4file "import"

do_test "$MP4BOX -dash 1000 $mp4file -out $mpdfile" "dash"
do_hash_test $mpdfile "mpd"

test_end

}


aac_test "sbr" "sbr"

aac_test "sbrx" "sbrx"

aac_test "ps" "ps"

aac_test "psx" "psx"

aac_test "sbr:ps" "sbr+ps"

aac_test "sbr:psx" "sbr+psx"

aac_test "sbrx:ps" "sbrx+ps"

aac_test "sbrx:psx" "sbrx+psx"
