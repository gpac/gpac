#!/bin/sh

#test import of encrypted files, whether flat or fragmented. 
test_begin "encryption-import"
if [ $test_skip  = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/crypted.mp4"
mp4file_short="$TEMP_DIR/crypted_short.mp4"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_GDR_1280x720_576kbps.264:dur=60 -crypt $MEDIA_DIR/encryption/cbcs.xml -new $mp4file 2> /dev/null

do_test "$MP4BOX -add $mp4file:dur=20 -new $mp4file_short" "import"
do_hash_test $mp4file_short "import"

fragfile="$TEMP_DIR/crypted_frag.mp4"
mp4file_short="$TEMP_DIR/crypted_frag_short.mp4"

$MP4BOX -frag 1000 $mp4file -out $fragfile 2> /dev/null

do_test "$MP4BOX -add $fragfile:dur=20 -new $mp4file_short" "import-frag"
do_hash_test $mp4file_short "import-frag"

#test decryption module properly loads in dynamic resolution by dumping to YUV
#we don't do a hash on the result since decoders may give slightly different results on platforms
do_test "$GPAC -i $mp4file_short -o $TEMP_DIR/dump.yuv" "decrypt-decoder"
#but we hash the inspection of the dump
myinspect=$TEMP_DIR/inspect_multi.txt
do_test "$GPAC -i $TEMP_DIR/dump.yuv:size=1280x720 inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect-decrypt"
do_play_test "play" "$TEMP_DIR/dump.yuv:size=1280x720"

test_end
