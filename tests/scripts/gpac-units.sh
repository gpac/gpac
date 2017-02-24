#runs test in no extra thread mode, single-thread lock mode (debug), and lock/lockfree mode for 1, 2 and 4 extra threads
sched_test()
{
name="$2-single"
cmd="$GPAC -ltf -stats $1"
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


single_test "$GPAC -list" "gpac-filter-unit-list"
single_test "$GPAC -info UTFilter" "gpac-filter-unit-info"

single_test "$GPAC -ltf UTSOurce:opt_dump UTFilter:opt_dump UTSink:opt_dump" "gpac-filter-dump_props"

sched_test "UTSource UTSink" "gpac-filter-1source-1sink-shared"

sched_test "UTSource:max_pck=1000:max_out=5 UTSink" "gpac-filter-source-sink-shared-pending"

sched_test "UTSource:max_pck=1000:alloc UTSink" "gpac-filter-1source-1sink-alloc"

#tee test, 1 src->2 sinks
sched_test "UTSource:max_pck=1000 UTSink UTSink" "gpac-filter-1source-2sink-shared"
sched_test "UTSource:max_pck=1000:max_out=5 UTSink UTSink" "gpac-filter-1source-2sink-shared-pending"
sched_test "UTSource:max_pck=1000:alloc UTSink UTSink" "gpac-filter-1source-2sink-alloc"


#source->filter->sink test
sched_test "UTSource:max_pck=1000 UTFilter:FID=1:fwd=shared UTSink:SID=1" "gpac-filter-1source-1filter-1sink-shared"
sched_test "UTSource:max_pck=1000 UTFilter:FID=1:fwd=copy UTSink:SID=1" "gpac-filter-1source-1filter-1sink-copy"
sched_test "UTSource:max_pck=1000 UTFilter:FID=1:fwd=ref UTSink:SID=1" "gpac-filter-1source-1filter-1sink-ref"
sched_test "UTSource:max_pck=1000 UTFilter:FID=1:fwd=mix UTSink:SID=1" "gpac-filter-1source-1filter-1sink-mix"

sched_test "UTSource:FID=1:max_pck=1000 UTFilter:FID=2:fwd=mix UTSink:SID=1,2" "gpac-filter-1source-1filter-1sinkdouble"

sched_test "UTSource:max_pck=4:nb_pids=2 UTSink" "gpac-filter-1source-1sink-2pid"

sched_test "UTSource:FID=1:max_pck=4:nb_pids=2 UTSink:SID=1#PID1: UTSink:SID=1#PID2" "gpac-filter-1source-2sink-2pid"


#framing tests
single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=default UTSink:SID=2:framing=default" "gpac-filter-framing-no-agg"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=default UTSink:SID=2" "gpac-filter-framing-agg"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=nostart UTSink:SID=2" "gpac-filter-framing-agg-nostart"

single_test "$GPAC -ltf -stats UTSource:FID=1:max_pck=2 UTFilter:FID=2:fwd=mix:framing=noend UTSink:SID=2" "gpac-filter-framing-agg-noend"

#test argument update
sched_test "UTSource:max_pck=1000:update=1,fwd,copy UTFilter:FID=1:fwd=shared UTSink:SID=1" "gpac-filter-1source-1filter-1sink-update"

