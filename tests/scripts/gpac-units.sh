#runs test in no extra thread mode, single-thread lock mode (debug), and lock/lockfree mode for 1, 2 and 4 extra threads
sched_test()
{
name="$2-single"
cmd="$GPAC -logs=strict -ltf -stats $1"
single_test "$cmd" "$name"
single_test "$cmd -sched=direct" "$name-directsched"
single_test "$cmd -sched=flock" "$name-locksched"

name="$2-1th"
single_test "$cmd -threads=1" "$name"
single_test "$cmd -threads=1 -sched=lock" "$name-locksched"

name="$2-2th"
single_test "$cmd -threads=2" "$name"
single_test "$cmd -threads=2 -sched=lock" "$name-locksched"

name="$2-4th"
single_test "$cmd -threads=4" "$name"
single_test "$cmd -threads=4 -sched=lock" "$name-locksched"

}


#coverage
$GPAC 2> /dev/null
single_test "$GPAC -h" "gpac-h"
single_test "$GPAC -hh" "gpac-hh"
single_test "$GPAC -h bin" "gpac-h-bin"
single_test "$GPAC -h log" "gpac-h-logs"
single_test "$GPAC -h doc" "gpac-h-doc"
single_test "$GPAC -h props" "gpac-h-props"
single_test "$GPAC -h codecs" "gpac-h-codecs"
single_test "$GPAC -h alias" "gpac-h-alias"
single_test "$GPAC -ha alias" "gpac-ha-alias"
single_test "$GPAC -h links" "gpac-h-links"
single_test "$GPAC -h links mp4mx" "gpac-h-links-single"
single_test "$GPAC -h modules" "gpac-h-modules"
single_test "$GPAC -h filters" "gpac-h-filters"
single_test 'gpac -h filters:*' "gpac-h-filters-all"
single_test 'gpac -h *:*' "gpac-h-all"
single_test 'gpac -h ffdec:*' "gpac-h-subfilters-all"
single_test "$GPAC -ltf -h UTFilter" "gpac-unit-info"
single_test "$GPAC -ha mp4mx" "gpac-mp4mx-ha"
single_test "$GPAC -hx mp4mx" "gpac-mp4mx-hx"
single_test "$GPAC -hh mp4mx" "gpac-mp4mx-hh"
single_test "$GPAC -hh core" "gpac-hh-core"
single_test "$GPAC -mkl=test.unk" "gpac-lang-file"
rm -f test.unk 2> /dev/null

single_test "$GPAC -seps=123456 -p=myprofile -wc -we -wf -wfx -no-save" "gpac-filter-profile-full"
single_test 'gpac -p=0 -hh mp4mx' "gpac-null-profile"

test_begin "gpac-link-dir"
if [ $test_skip != 1 ] ; then
do_test "$GPAC  -k -i $MEDIA_DIR/auxiliary_files/logo.jpg @0 inspect:log=$TEMP_DIR/insp.txt @1 -o $TEMP_DIR/test.jpg -stats -graph -runfor=500" "gpac-exec"
do_hash_test $TEMP_DIR/test.jpg  "gpac-exec"
do_hash_test $TEMP_DIR/insp.txt  "gpac-inspect"
fi
test_end

test_begin "gpac-io-syntax"
if [ $test_skip != 1 ] ; then
do_test "$GPAC -lc -logs=all@debug src=$MEDIA_DIR/auxiliary_files/logo.jpg dst=$TEMP_DIR/test.jpg" "io-syntax1"
do_hash_test $TEMP_DIR/test.jpg  "io-syntax1"

do_test "$GPAC -src $MEDIA_DIR/auxiliary_files/logo.jpg -dst $TEMP_DIR/test.jpg" "io-syntax2"
do_hash_test $TEMP_DIR/test.jpg  "io-syntax2"
fi
test_end

test_begin "gpac-alias"
if [ $test_skip != 1 ] ; then
#test expand
$GPAC -alias='test src=@{+1:N} inspect'":log=$TEMP_DIR/logs.txt" -aliasdoc='test some doc' 2> /dev/null
do_test "$GPAC test $MEDIA_DIR/auxiliary_files/logo.jpg $MEDIA_DIR/auxiliary_files/logo.png" "gpac-alias-expand"
do_hash_test $TEMP_DIR/logs.txt  "inspect-res"

#test regular
$GPAC -alias='test src=@{1} inspect:log=src=@{N-1}' -aliasdoc='test some doc' 2> /dev/null
do_test "$GPAC test $MEDIA_DIR/auxiliary_files/logo.jpg -k $TEMP_DIR/logs.txt -loop=2" "gpac-alias-nargs"
do_hash_test $TEMP_DIR/logs.txt  "inspect-res2"

#test list
$GPAC -alias='test flist:srcs=@{-:N-1} inspect:log=src=@{N}' -aliasdoc='test some doc' 2> /dev/null
do_test "$GPAC -threads=-1 test $MEDIA_DIR/auxiliary_files/logo.jpg $MEDIA_DIR/auxiliary_files/logo.png $TEMP_DIR/logs.txt" "gpac-alias-list"
do_hash_test $TEMP_DIR/logs.txt  "inspect-res3"

fi
test_end

test_begin "gpac-remotery"
if [ $test_skip != 1 ] ; then
do_test "$GPAC src=$MEDIA_DIR/auxiliary_files/enst_audio.aac inspect -logs=filter@info -rmt -rmt-log" "remotery"
fi
test_end

test_begin "gpac-units"
if [ $test_skip != 1 ] ; then
do_test "$GPAC -lu -logs=app@info:filter@debug -unit-tests -mem-track-stack" "units"
fi
test_end

test_begin "gpac-units-nomt"
if [ $test_skip != 1 ] ; then
do_test "gpac -for-test -unit-tests" "units-nomt"
fi
test_end

test_begin "gpac-setupfail"
if [ $test_skip != 1 ] ; then
do_test "$GPAC fin:src=blob inspect" "setupfail"
fi
test_end

test_begin "gpac-genmd"
if [ $test_skip != 1 ] ; then
do_test "$GPAC -genmd" "gpac-genmd"
do_test "$MP4BOX -genmd" "mp4box-genmd"
do_test "$MP4CLIENT -genmd" "mp4client-genmd"
for i in *.md ; do
case i in
README* )
 continue;;
esac;
rm $i
done

fi
test_end

test_begin "gpac-genman"
if [ $test_skip != 1 ] ; then
do_test "$GPAC -genman" "gpac-genman"
do_test "$MP4BOX -genman" "mp4box-genman"
do_test "$MP4CLIENT -genman" "mp4client-genman"
rm *.1
fi
test_end

test_begin "gpac-reporting"
if [ $test_skip != 1 ] ; then
do_test "$GPAC -i media/auxiliary_files/enst_video.h264 inspect -r -logs=filter@debug" "reports"
fi
test_end

test_begin "gpac-uncache"
if [ test_skip != 1 ] ; then

do_test "$GPAC -logs=filter@debug:ncl -i http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/logo.jpg inspect" "http-get"
do_test "$GPAC -uncache" "uncache"

fi
test_end


single_test "$GPAC -ltf UTSource:cov UTFilter:cov UTSink:cov" "gpac-filter-dump_props"

sched_test "UTSource UTSink" "gpac-filter-1source-1sink-shared"

sched_test "UTSource:max_pck=100:max_out=5 UTSink" "gpac-filter-source-sink-shared-pending"

sched_test "UTSource:max_pck=100:alloc UTSink" "gpac-filter-1source-1sink-alloc"

#tee test, 1 src->2 sinks
sched_test "UTSource:max_pck=100 UTSink UTSink" "gpac-filter-1source-2sink-shared"
sched_test "UTSource:max_pck=100:max_out=5 UTSink UTSink" "gpac-filter-1source-2sink-shared-pending"
sched_test "UTSource:max_pck=100:alloc UTSink UTSink" "gpac-filter-1source-2sink-alloc"


#source->filter->sink test
sched_test "UTSource:max_pck=100 UTFilter:FID=1:fwd=shared UTSink:SID=1" "gpac-filter-1source-1filter-1sink-shared"
sched_test "UTSource:max_pck=100 UTFilter:FID=1:fwd=copy UTSink:SID=1" "gpac-filter-1source-1filter-1sink-copy"
sched_test "UTSource:max_pck=100 UTFilter:FID=1:fwd=ref UTSink:SID=1" "gpac-filter-1source-1filter-1sink-ref"
sched_test "UTSource:max_pck=100 UTFilter:FID=1:fwd=mix UTSink:SID=1" "gpac-filter-1source-1filter-1sink-mix"
#same as above with non blocking endabled
sched_test "-no-block UTSource:max_pck=100 UTFilter:FID=1:fwd=mix UTSink:SID=1" "gpac-filter-1source-1filter-1sink-mix-nb"

sched_test "UTSource:FID=1:max_pck=100 UTFilter:FID=2:fwd=mix UTSink:SID=1,2" "gpac-filter-1source-1filter-1sinkdouble"

sched_test "UTSource:max_pck=4:nb_pids=2 UTSink" "gpac-filter-1source-1sink-2pid"

sched_test "UTSource:FID=1:max_pck=4:nb_pids=2 UTSink:SID=1#PID=1: UTSink:SID=1#PID=2" "gpac-filter-1source-2sink-2pid"


#framing tests
single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=default UTSink:SID=2:framing=default" "gpac-filter-framing-no-agg"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=default UTSink:SID=2" "gpac-filter-framing-agg"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=nostart UTSink:SID=2" "gpac-filter-framing-agg-nostart"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=noend UTSink:SID=2" "gpac-filter-framing-agg-noend"

#test argument update
sched_test "UTSource:max_pck=100:update=1,fwd,copy UTFilter:FID=1:fwd=shared UTSink:SID=1" "gpac-filter-1source-1filter-1sink-update"

