#!/bin/sh

#test import of encrypted files, whether flat or fragmented. 
test_begin "encryption-import"
if [ $test_skip  = 1 ] ; then
 return
fi

mp4file="$TEMP_DIR/crypted.mp4"
mp4file_short="$TEMP_DIR/crypted_short.mp4"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_640x360_192kbps.264:dur=60 -crypt $MEDIA_DIR/encryption/cbcs.xml -new $mp4file 2> /dev/null

do_test "$MP4BOX -add $mp4file:dur=20 -new $mp4file_short" "import"
do_hash_test $mp4file_short "import"

fragfile="$TEMP_DIR/crypted_frag.mp4"
mp4file_short="$TEMP_DIR/crypted_frag_short.mp4"

$MP4BOX -frag 1000 $mp4file -out $fragfile 2> /dev/null

do_test "$MP4BOX -add $fragfile:dur=20 -new $mp4file_short" "import-frag"
do_hash_test $mp4file_short "import-frag"

test_end
