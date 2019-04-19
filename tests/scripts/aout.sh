#test audio modules

srcfile=$TEMP_DIR/test.mp4

raw_out_test ()
{

test_begin "audio-$1"
if [ $test_skip  = 1 ] ; then
return
fi

if [ $2 = 1 ] ; then
jdon=`pgrep jackd`
if [ -z $jdon ] ; then
#sleep before starting jackd since we just executed an audio test before, and it looks like the deamon has issues starting up
echo "starting jackd, sleeping for 5 sec" >> $LOGS
sleep 5
jackd -r -d alsa -r 44100 &
echo "jackd started, sleeping for 2 sec" >> $LOGS
sleep 2
fi
fi

do_test "$GPAC -i $srcfile aout:drv=$1 -logs=mmio@debug" "play"

if [ $2 = 1 ] && [ -z $jdon ] ; then
echo "killing jackd" >> $LOGS
killall -9 jackd
fi

#sleep 1

test_end
}

#we do a test with 0.4 seconds. using more results in higher dynamics in the signal which are not rounded the same way on all platforms
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=0.4 -new $srcfile 2> /dev/null


config_linux=`gpac -h bin 2>&1 | grep GPAC_CONFIG_LINUX`
config_osx=`gpac -h bin 2>&1 | grep GPAC_CONFIG_DARWIN`
config_win=`gpac -h bin 2>&1 | grep GPAC_CONFIG_WIN32`

#todo - we should check which modules are indeed present

#SDL is built on all platforms
raw_out_test "sdl" 0

#alsa, pulse, jack and oss on linux
if [ -n "$config_linux" ] ; then
raw_out_test "alsa" 0
raw_out_test "pulseaudio" 0

hasjd=`which jackd`

if [ -n "hasjd" ] ; then
raw_out_test "jack" 1
fi

if [ -f "/dev/dsp" ]; then
raw_out_test "oss_audio" 0
fi


fi
#end linux tests

#DSound and wav in windows
if [ -n "$config_win" ] ; then

raw_out_test "dx_hw" 0

raw_out_test "wav_audio" 0

fi
#end windows tests



