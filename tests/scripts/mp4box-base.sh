

test_begin "mp4box-base-dump"
if [ "$test_skip" = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/test.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -add $MEDIA_DIR/auxiliary_files/subtitle_fr.srt:lang=fra -new $mp4file" "create-mp4"
do_hash_test $mp4file "create-mp4"

do_test "$MP4BOX -add $mp4file -dref -new $TEMP_DIR/dref.mp4" "create-dref-mp4"
do_hash_test $TEMP_DIR/dref.mp4 "create-dref-mp4"

do_test "$MP4BOX  -raw 1 $mp4file -out $TEMP_DIR/test.tmp" "raw-264"
do_hash_test $TEMP_DIR/test.tmp "raw-264"

do_test "$MP4BOX  -raw 2 $mp4file -out $TEMP_DIR/test.tmp" "raw-aac"
do_hash_test $TEMP_DIR/test.tmp "raw-aac"

do_test "$MP4BOX  -raw 3 $mp4file -out $TEMP_DIR/test.tmp" "raw-text"
do_hash_test $TEMP_DIR/test.tmp "raw-text"

do_test "$MP4BOX  -srt 3 $mp4file -out $TEMP_DIR/test.tmp" "srt-text"
do_hash_test $TEMP_DIR/test.tmp "srt-text"

do_test "$MP4BOX  -ttxt 3 $mp4file -out $TEMP_DIR/test.tmp" "ttxt-text"
do_hash_test $TEMP_DIR/test.tmp "ttxt-text"


do_test "$MP4BOX -raws 1 $mp4file" "raw-samples"
n=`ls $TEMP_DIR/test_track* | wc -l`
n=${n#0}
if [ "$n" != 173 ] ; then
result="Wrong sample count $n (expected 173)"
fi
rm $TEMP_DIR/test_track* 2&>/dev/null

do_test "$MP4BOX -info 1 $mp4file" "InfoTk1"
do_test "$MP4BOX -info 2 $mp4file" "InfoTk2"
do_test "$MP4BOX -info 3 $mp4file" "InfoTk3"

do_test "$MP4BOX -raws 1:1 $mp4file -out $TEMP_DIR/test.tmp" "raw-sample"
do_hash_test $TEMP_DIR/test.tmp "raw-sample"

do_test "$MP4BOX -flat $mp4file" "flat-storage"
do_hash_test $mp4file "flat-storage"

do_test "$MP4BOX -brand MP4V:1 -ab iso6 -inter 250 $mp4file" "interleave-250ms"
do_hash_test $mp4file "interleave-250ms"

do_test "$MP4BOX -rb iso6 -frag 1000 $mp4file -out $TEMP_DIR/frag-1s.mp4" "frag-1s"
do_hash_test "$TEMP_DIR/frag-1s.mp4" "frag-1s"
mv "$TEMP_DIR/frag-1s.mp4" $mp4file

test_end

#commented out since not very usefull, always changing and not impacting coverage
return

test_begin "mp4box-base-help"

do_test "$MP4BOX -version" "Version" &
do_test "$MP4BOX -h" "Help" &
do_test "$MP4BOX -h general" "HelpGeneral" &
do_test "$MP4BOX -h hint" "HelpHint" &
do_test "$MP4BOX -h dash" "HelpDash" &
do_test "$MP4BOX -h import" "HelpImport" &
do_test "$MP4BOX -h encode" "HelpEncode" &
do_test "$MP4BOX -h meta" "HelpMeta" &
do_test "$MP4BOX -h extract" "HelpExtract" &
do_test "$MP4BOX -h dump" "HelpDump" &
do_test "$MP4BOX -h swf" "HelpSwf" &
do_test "$MP4BOX -h crypt" "HelpCrypt" &
do_test "$MP4BOX -h format" "HelpFormat" &
do_test "$MP4BOX -h rtp" "HelpRtp" &
do_test "$MP4BOX -h live" "HelpLive" &
do_test "$MP4BOX -h all" "HelpAll" &
do_test "$MP4BOX -nodes" "Nodes" &
do_test "$MP4BOX -node AnimationStream" "NodeAnimStream" &
do_test "$MP4BOX -xnodes" "Xnodes" &
do_test "$MP4BOX -xnode" "ElevationGrid" &
do_test "$MP4BOX -snodes" "Snodes" &
do_test "$MP4BOX -languages" "Languages" &

test_end

