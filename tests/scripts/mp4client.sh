
single_test "$MP4CLIENT -guid -run-for 2" "mp4client-gui"

single_test "$MP4CLIENT -guid -rmt -rmt-ogl -run-for 2 -stats $MEDIA_DIR/auxiliary_files/sky.jpg" "mp4client-gui-stats"

single_test "$MP4CLIENT -gui -for-test -gui-test http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-V-BS.mpd" "mp4client-gui-dash"


test_begin "mp4client-playfrom"
if [ $test_skip = 0 ] ; then

#run import with regular MP4Box (no test, with progress) for coverage
do_test "MP4Box -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $TEMP_DIR/file.mp4" "create"

do_test "$MP4CLIENT -for-test -no-save -size 100x100 -speed 2 -rtix $TEMP_DIR/logs.txt -exit -play-from 6 $TEMP_DIR/file.mp4 -cov" "play"

fi
test_end


test_begin "mp4client-playfrom-ts"
if [ $test_skip = 0 ] ; then

do_test "$GPAC -i $MEDIA_DIR/auxiliary_files/enst_video.h264 -o $TEMP_DIR/file.ts" "create"

do_test "$MP4CLIENT -for-test -no-save -size 100x100 -speed 2 -rtix $TEMP_DIR/logs.txt -exit -play-from 6 $TEMP_DIR/file.ts -cov -blacklist=vtbdec,nvdec" "play"

fi
test_end



