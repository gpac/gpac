
test_sar()
{

name=$(basename $1)
name=${name%.*}
test_begin "mp4box-sar-$name"

if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/setpar.mp4"

do_test "$MP4BOX -add $1 -par 1=16:9 -new $mp4file" "create"
do_hash_test $mp4file "create"

mp4file="$TEMP_DIR/rewritepar.mp4"
do_test "$MP4BOX -add $1 -par 1=w16:9 -new $mp4file" "rewrite"
do_hash_test $mp4file "rewrite"

mp4file2="$TEMP_DIR/remove.mp4"
do_test "$MP4BOX -add $mp4file -par 1=none -new $mp4file2" "remove"
do_hash_test $mp4file2 "remove"

mp4file="$TEMP_DIR/auto.mp4"
do_test "$MP4BOX -add $mp4file2 -par 1=auto -new $mp4file" "auto"
do_hash_test $mp4file "auto"

mp4file="$TEMP_DIR/set-pl.mp4"
do_test "$MP4BOX -add $1:profile=1:level:1 -new $mp4file" "set-pl"
do_hash_test $mp4file "set-pl"

test_end
}

test_sar $MEDIA_DIR/auxiliary_files/enst_video.h264

test_sar $MEDIA_DIR/auxiliary_files/count_video.cmp

test_sar $MEDIA_DIR/auxiliary_files/counter.hvc


