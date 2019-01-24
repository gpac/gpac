#test raw video

DST="127.0.0.1"
IFCE="127.0.0.1"

raw_audio_test ()
{

test_begin "raw-aud-$1"
if [ $test_skip  = 1 ] ; then
return
fi

rawfile=$TEMP_DIR/raw.$1
do_test "$GPAC -i $mp4file -o $rawfile" "dump"
do_hash_test "$rawfile" "dump"


myinspect="inspect:fmt=@pn@-@cts@-@bo@@lf@"
insfile=$TEMP_DIR/dump.txt
do_test "$GPAC -i $rawfile:samplerate=48000:channels=2 $myinspect:log=$insfile" "inspect"
do_hash_test "$insfile" "inspect"

#playback test at 10x speed - this exercices the reverse conversion to pcm 16 usually used by the sound card
do_test "$GPAC -i $rawfile:samplerate=48000:channels=2 aout:speed=10:vol=0 -graph" "play"

#only do the reverse tests for pcm (same for the other formats)
if [ $1  = "pcm" ] ; then

myinspect="inspect:speed=-1:fmt=@pn@-@cts@-@bo@@lf@"
insfile=$TEMP_DIR/dumpns.txt
do_test "$GPAC -i $rawfile:samplerate=48000:channels=2 $myinspect:log=$insfile" "inspect_reverseplay"
do_hash_test "$insfile" "inspect_reverseplay"

do_test "$GPAC -i $rawfile:samplerate=48000:channels=2 aout:speed=-10:vol=0" "reverse_play"

fi

test_end
}

mp4file="$TEMP_DIR/aud.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_audio.aac:dur=1 -new $mp4file 2> /dev/null

#complete lists of audio formats extensions in gpac
afstr="pc8 pcm s24 s32 flt dbl pc8p pcmp s24p s32p fltp dblp"

for i in $afstr ; do
	raw_audio_test $i
done

