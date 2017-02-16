
single_test "$GPAC -list" "gpac-filter-unit-list"
single_test "$GPAC -info UTFilter" "gpac-filter-unit-info"


single_test "$GPAC UTFilter:mode=source:max_pck=1000 UTFilter:mode=sink" "gpac-filter-source-sink-shared-no-thread"
single_test "$GPAC -threads=1 UTFilter:mode=source:max_pck=1000 UTFilter:mode=sink" "gpac-filter-source-sink-shared-1-thread"
single_test "$GPAC -threads=1 -sched=lock UTFilter:mode=source:max_pck=1000 UTFilter:mode=sink" "gpac-filter-source-sink-shared-1-thread-locksched"
single_test "$GPAC -sched=flock UTFilter:mode=source:max_pck=1000 UTFilter:mode=sink" "gpac-filter-source-sink-shared-no-thread-locksched"

