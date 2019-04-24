#test video modules

srcfile=$TEMP_DIR/test.mp4

vout_test ()
{

test_begin "vout-$1"
if [ $test_skip  = 1 ] ; then
return
fi

do_test "$GPAC -i $srcfile vout:drv=$1" "3D"

do_test "$GPAC -i $srcfile vout:drv=$1:fullscreen" "3Dfullscreen"

do_test "$GPAC -i $srcfile vout:drv=$1:blit" "2D"

do_test "$GPAC -i $srcfile vout:drv=$1:soft" "softblt"

test_end

}

#we do a test with 0.4 seconds. using more results in higher dynamics in the signal which are not rounded the same way on all platforms
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264:dur=0.4 -new $srcfile 2> /dev/null


config_linux=`gpac -h bin 2>&1 | grep GPAC_CONFIG_LINUX`
config_osx=`gpac -h bin 2>&1 | grep GPAC_CONFIG_DARWIN`
config_win=`gpac -h bin 2>&1 | grep GPAC_CONFIG_WIN32`

#todo - we should check which modules are indeed present

#SDL is built on all platforms
vout_test "sdl"

#X11 on linux
if [ -n "$config_linux" ] ; then
vout_test "x11_out"
fi
#end linux tests

#DirectX in windows
if [ -n "$config_win" ] ; then

vout_test "dx_hw"

fi
#end windows tests



