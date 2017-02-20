#runs test in no extra thread mode, single-thread lock mode (debug), and lock/lockfree mode for 1, 2 and 4 extra threads
sched_test()
{
name="$2-single"
cmd="$GPAC -stats $1"
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

single_test "$GPAC UTFilter:opt_dump UTFilter:mode=filter:opt_dump UTFilter:mode=sink:opt_dump" "gpac-filter-dump_props"

sched_test "UTFilter UTFilter:mode=sink" "gpac-filter-1source-1sink-shared"

sched_test "UTFilter:mode=source:max_pck=1000:max_out=5 UTFilter:mode=sink" "gpac-filter-source-sink-shared-pending"

sched_test "UTFilter:mode=source:max_pck=1000:alloc UTFilter:mode=sink" "gpac-filter-1source-1sink-alloc"

#tee test, 1 src->2 sinks
sched_test "UTFilter:mode=source:max_pck=1000 UTFilter:mode=sink UTFilter:mode=sink" "gpac-filter-1source-2sink-shared"
sched_test "UTFilter:mode=source:max_pck=1000:max_out=5 UTFilter:mode=sink UTFilter:mode=sink" "gpac-filter-1source-2sink-shared-pending"
sched_test "UTFilter:mode=source:max_pck=1000:alloc UTFilter:mode=sink UTFilter:mode=sink" "gpac-filter-1source-2sink-alloc"


#source->filter->sink test
sched_test "UTFilter:mode=source:max_pck=1000 UTFilter:FID=1:mode=filter:fwd=shared UTFilter:SID=1:mode=sink" "gpac-filter-1source-1filter-1sink-shared"
sched_test "UTFilter:mode=source:max_pck=1000 UTFilter:FID=1:mode=filter:fwd=copy UTFilter:SID=1:mode=sink" "gpac-filter-1source-1filter-1sink-copy"
sched_test "UTFilter:mode=source:max_pck=1000 UTFilter:FID=1:mode=filter:fwd=ref UTFilter:SID=1:mode=sink" "gpac-filter-1source-1filter-1sink-ref"
sched_test "UTFilter:mode=source:max_pck=1000 UTFilter:FID=1:mode=filter:fwd=mix UTFilter:SID=1:mode=sink" "gpac-filter-1source-1filter-1sink-mix"

sched_test "UTFilter:FID=1:mode=source:max_pck=1000 UTFilter:FID=2:mode=filter:fwd=mix UTFilter:SID=1,2:mode=sink" "gpac-filter-1source-1filter-1sinkdouble"

sched_test "UTFilter:mode=source:max_pck=4:nb_pids=2 UTFilter:mode=sink" "gpac-filter-1source-1sink-2pid"

sched_test "UTFilter:FID=1:mode=source:max_pck=4:nb_pids=2 UTFilter:SID=1#PID1:mode=sink UTFilter:SID=1#PID2:mode=sink" "gpac-filter-1source-2sink-2pid"


#framing tests
single_test "$GPAC -stats UTFilter:FID=1:mode=source:max_pck=2 UTFilter:FID=2:mode=filter:fwd=mix:framing=default UTFilter:SID=2:mode=sink:framing=default" "gpac-filter-framing-no-agg"

single_test "$GPAC -stats UTFilter:FID=1:mode=source:max_pck=2 UTFilter:FID=2:mode=filter:fwd=mix:framing=default UTFilter:SID=2:mode=sink" "gpac-filter-framing-agg"

single_test "$GPAC -stats UTFilter:FID=1:mode=source:max_pck=2 UTFilter:FID=2:mode=filter:fwd=mix:framing=nostart UTFilter:SID=2:mode=sink" "gpac-filter-framing-agg-nostart"

single_test "$GPAC -stats UTFilter:FID=1:mode=source:max_pck=2 UTFilter:FID=2:mode=filter:fwd=mix:framing=noend UTFilter:SID=2:mode=sink" "gpac-filter-framing-agg-noend"

#test argument update
sched_test "UTFilter:mode=source:max_pck=1000:update=1,fwd,copy UTFilter:FID=1:mode=filter:fwd=shared UTFilter:SID=1:mode=sink" "gpac-filter-1source-1filter-1sink-update"

