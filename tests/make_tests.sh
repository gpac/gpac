#!/bin/bash

#for user doc, check scripts/00-template


base_args=""

GNU_TIME=time
GNU_DATE=date
#GNU_SED=sed
DIFF=diff
GCOV=gcov
FFMPEG=ffmpeg

EXTERNAL_MEDIA_AVAILABLE=1

main_dir=`pwd`

platform=`uname -s`
if [ $platform = "Darwin" ] ; then
GNU_TIME=gtime
GNU_DATE=gdate
fi

generate_hash=0
play_all=0
do_ui=0

DEF_DUMP_DUR=10
DEF_DUMP_SIZE="200x200"

#remote location of resource files: all media files, hash files and generated videos
REFERENCE_DIR="http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/resources/"
#dir where all external media are stored
EXTERNAL_MEDIA_DIR="$main_dir/external"
#dir where all hashes are stored
HASH_DIR="$main_dir/hash_refs"
#dir where all specific test rules (override of defaults, positive tests, ...) are stored
RULES_DIR="$main_dir/rules"
#dir where all referenced videos are stored
SCRIPTS_DIR="$main_dir/scripts"
#dir where all referenced videos are stored
VIDEO_DIR_REF="$main_dir/external_videos_refs"

#dir where all local media data (ie from git repo) is stored
MEDIA_DIR="$main_dir/media"
#local dir where all data will be generated (except hashes and referenced videos)
LOCAL_OUT_DIR="$main_dir/results"

#dir where all test videos are generated
VIDEO_DIR="$LOCAL_OUT_DIR/videos"
#dir where all logs are generated
LOGS_DIR="$LOCAL_OUT_DIR/logs"
#temp dir for any test
TEMP_DIR="$LOCAL_OUT_DIR/temp"

ALL_REPORTS="$LOCAL_OUT_DIR/all_results.xml"
ALL_LOGS="$LOCAL_OUT_DIR/all_logs.txt"

rm -f "$TEMP_DIR/err_exit" 2> /dev/null
rm -f "$LOGS_DIR/*.sh" 2> /dev/null

if [ ! -e $LOCAL_OUT_DIR ] ; then
mkdir $LOCAL_OUT_DIR
fi

if [ ! -e $HASH_DIR ] ; then
mkdir $HASH_DIR
fi

if [ ! -e $VIDEO_DIR ] ; then
mkdir $VIDEO_DIR
fi

if [ ! -e $VIDEO_DIR_REF ] ; then
mkdir $VIDEO_DIR_REF
fi


if [ ! -e $LOGS_DIR ] ; then
mkdir $LOGS_DIR
fi

if [ ! -e $RULES_DIR ] ; then
mkdir $RULES_DIR
fi

if [ ! -e $TEMP_DIR ] ; then
mkdir $TEMP_DIR
fi


print_usage ()
{
echo "GPAC Test Suite Usage: use either one of this command or no command at all"
echo "*** Test suite validation options"
echo "  -clean [ARG]:          removes all removes all results (logs, stat cache and video). If ARG is specified, only removes for test names ARG*."
echo "  -play-all:             force playback of BT and XMT files for BIFS (by default only MP4)."
echo "  -force:                force recomputing all tests."
echo "  -no-hash:              runs test suite without hash checking."
echo ""
echo "*** Test suite generation options"
echo "  -clean-hash [ARG]:     removes all generated hash, logs, stat cache and videos. If ARG is specified, only removes for test names ARG*."
echo "  -hash:                 regenerate tests with missing hash files."
echo "  -uirec [FILE]:         generates UI record traces. If FILE is set, records for the given file."
echo "  -uiplay [FILE]:        replays all recorded UI traces. If FILE is set, replays the given file."
echo ""
echo "*** General options"
echo "  -strict:               stops at the first error"
echo "  -keep-avi:             keeps raw AVI files (warning this can be pretty big)"
echo "  -sync:                 syncs all remote resources and results with local base (warning this can be long)"
echo "  -check-names:          check name of each test is unique"
echo "  -h:                    print this help"
}


#performs mirroring of media and references hash & videos
sync_resources ()
{
 echo "Mirroring $REFERENCE_DIR to $EXTERNAL_MEDIA_DIR"
 cd $EXTERNAL_MEDIA_DIR
 wget -m -nH --no-parent --cut-dirs=3 --reject *.gif $REFERENCE_DIR
 cd $main_dir
}

url_arg=""
do_clean=0
keep_avi=0
do_clean_hash=0
check_names=0
disable_hash=0
strict_mode=0

#Parse arguments
for i in $* ; do
 if [ "$i" = "-hash" ] ; then
  generate_hash=1
 elif [ "$i" = "-play-all" ] ; then
  play_all=1
 elif [ "$i" = "-clean" ] ; then
  do_clean=1
 elif [ "$i" = "-clean-hash" ] ; then
  do_clean_hash=1
 elif [ "$i" = "-uirec" ] ; then
  do_ui=1
 elif [ "$i" = "-uiplay" ] ; then
  do_ui=2
 elif [ "$i" = "-keep-avi" ] ; then
  keep_avi=1
 elif [ "$i" = "-no-hash" ] ; then
  disable_hash=1
 elif [ "$i" = "-strict" ] ; then
  strict_mode=1
 elif [ "$i" = "-sync" ] ; then
  sync_resources
  exit
 elif [ "$i" = "-check-names" ] ; then
  check_names=1
 elif [ "$i" = "-h" ] ; then
  print_usage
  exit
 elif [ ${i:0:1} = "-" ] ; then
  echo "Unknown Option \"$i\" - check usage (-h)"
  exit
 else
  if [ -n "$url_arg" ] ; then
   echo "More than one input secified - check usage (-h)"
   exit
  fi
  url_arg=$i
 fi
done

if [ $check_names != 0 ] ; then
 do_clean_hash=0
 do_clean=0
 do_ui=0
fi

#Clean all hashes and reference videos
if [ $do_clean_hash != 0 ] ; then

 read -p "This will remove all referenced videos and hashes. Are you sure (y/n)?" choice
 if [ $choice != "y" ] ; then
  echo "Canceled"
  exit
 fi

 if [ -n "$url_arg" ] ; then
  echo "Deleting SHA-1 Hash for $url_arg"
  rm -rf $HASH_DIR/$url_arg* 2> /dev/null
  rm -rf $VIDEO_DIR_REF/$url_arg* 2> /dev/null
 else
  echo "Deleting SHA-1 Hashes"
  rm -rf $HASH_DIR/* 2> /dev/null
  rm -rf $VIDEO_DIR_REF/* 2> /dev/null
 fi
 #force cleaning as well
 do_clean=1
fi

#Clean all cached results and generated videos
if [ $do_clean != 0 ] ; then

 if [ -n "$url_arg" ] ; then
  echo "Deleting $url_arg cache (logs, stats and videos) "
  rm -rf $LOGS_DIR/$url_arg* > /dev/null
  rm -rf $VIDEO_DIR/$url_arg* 2> /dev/null
 else
  echo "Deleting cache (logs, stats and videos)"
  rm -rf $LOGS_DIR/* > /dev/null
  rm -rf $VIDEO_DIR/* 2> /dev/null
 fi
 rm -f $ALL_REPORTS > /dev/null
 rm -f $ALL_LOGS > /dev/null
 rm -rf $TEMP_DIR/* 2> /dev/null
 exit
fi


if [ ! "$(ls -A $HASH_DIR)" ]; then
disable_hash=1
echo "Hashes not found - skippping them"
fi

if [ ! -e $EXTERNAL_MEDIA_DIR ] ; then
EXTERNAL_MEDIA_AVAILABLE=0
elif [ ! -e $EXTERNAL_MEDIA_DIR/counter ] ; then
EXTERNAL_MEDIA_AVAILABLE=0
fi

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
echo ""
echo "** External media dir unavailable - you may sync it using -sync **"
echo ""
fi

#test for GNU time
res=`$GNU_TIME --version 2> /dev/null`
res=$?
if [ $res != 0 ] ; then
echo "GNU time not found (ret $res) - exiting"
exit 1
fi

#test for GNU date
res=`$GNU_DATE 2> /dev/null`
res=$?
if [ $res != 0 ] ; then
echo "GNU date not found (ret $res) - exiting"
exit 1
fi

#test for ffmpeg - if not present, disable video storing
do_store_video=1

`$FFMPEG -version > /dev/null 2>&1 `
if [ $? != 0 ] ; then
echo "ffmpeg not found - disabling playback video storage"
do_store_video=0
fi


#check MP4Box, MP4Client and MP42TS (use default args, not custum ones because of -mem-track)
`MP4Box -h 2> /dev/null`
if [ $? != 0 ] ; then
echo "MP4Box not found (ret $?) - exiting"
exit 1
fi

`MP4Client -run-for 0 2> /dev/null`
res=$?
if [ $res != 0 ] ; then
echo "MP4Client not found (ret $res) - exiting"
exit 1
fi

`MP42TS -h 2> /dev/null`
if [ $? != 0 ] ; then
echo "MP42TS not found (ret $?) - exiting"
exit 1
fi

#check mem tracking is supported
res=`MP4Box -mem-track -h 2>&1 | grep "WARNING"`
if [ -n "$res" ]; then
  echo "GPAC not compiled with memory tracking"
else
 echo "Enabling memory-tracking"
 base_args="$base_args -mem-track"
fi


#reassign our default programs
MP4BOX="MP4Box -noprog -for-test $base_args"
MP4CLIENT="MP4Client -noprog -strict-error $base_args"
MP42TS="MP42TS $base_args"
DASHCAST="DashCast $base_args"

$MP4BOX -version 2> $TEMP_DIR/version.txt
VERSION="`head -1 $TEMP_DIR/version.txt | cut -d ' ' -f 5-` "
rm $TEMP_DIR/version.txt

#reset all the possible return values
reset_stat ()
{
 EXECUTION_STATUS="N/A"
 RETURN_VALUE="N/A"
 MEM_TOTAL_AVG="N/A"
 MEM_RESIDENT_AVG="N/A"
 MEM_RESIDENT_MAX="N/A"
 CPU_PERCENT="N/A"
 CPU_ELAPSED_TIME="N/A"
 CPU_USER_TIME="N/A"
 CPU_KERNEL_TIME="N/A"
 PAGE_FAULTS="N/A"
 FILE_INPUTS="N/A"
 SOCKET_MSG_REC="N/A"
 SOCKET_MSG_SENT="N/A"
}

#begin a test with name $1 and using hashes called $1-$2 ... $1-$N
test_begin ()
{

 result=""
 TEST_NAME=$1

 if [ $check_names != 0 ] ; then
  report="$TEMP_DIR/$TEST_NAME.test"
  if [ -f $report ] ; then
   echo "Test name $TEST_NAME already exists - please fix"
   rm -rf $TEMP_DIR/* 2> /dev/null
   exit
  fi
  echo "" > $report
  test_skip=1
  return
 fi

 report="$TEMP_DIR/$TEST_NAME-temp.txt"
 LOGS="$LOGS_DIR/$TEST_NAME-logs.txt-new"
 final_report="$LOGS_DIR/$TEST_NAME-passed.xml"

 #reset defaults
 dump_dur=$DEF_DUMP_DUR
 dump_size=$DEF_DUMP_SIZE


 hash_skipable=0
 test_skip=0

 test_args=("$@")
 test_nb_args=$#
 skip_play_hash=0

 rules_sh=$RULES_DIR/$TEST_NAME.sh
 if [ -f $rules_sh ] ; then
  source $rules_sh
 fi

 #we are generating - check all hash are present. If so, skip test
 if [ $generate_hash != 0 ] ; then
  hash_skipable=1
  for ((i=1; i < test_nb_args; i++)) {
   hash_found=0

   if [ $skip_play_hash = 0 ] ; then
    hash_file=$HASH_DIR/$TEST_NAME-${test_args[$i]}-avirawvideo.hash
    if [ -f $hash_file ] ; then
	 hash_found=1
    fi

    hash_file=$HASH_DIR/$TEST_NAME-${test_args[$i]}-avirawaudio.hash
    if [ -f $hash_file ] ; then
     hash_found=1
    fi
   else
    if [ ${test_args[$i]} = "play" ] ; then
     hash_found=1
    fi
   fi

   hash_file=$HASH_DIR/$TEST_NAME-${test_args[$i]}.hash
   if [ -f $hash_file ] ; then
    hash_found=1
   fi

   if [ $hash_found != 1 ] ; then
 	hash_skipable=0
	break
   fi

  }

 if [ $disable_hash = 1 ] ; then
  hash_skipable=1
 fi

  if [ $hash_skipable = 1 ] ; then
   test_skip=1
  fi

 #we are not generating, skip only if final report is present
 elif [ -f "$final_report" ] ; then
  test_skip=1
 fi

 if [ $test_skip = 1 ] ; then
  test_stats="$LOGS_DIR/$TEST_NAME-stats.sh"
  echo "TEST_SKIP=1" > $test_stats
 else
  echo "*** $TEST_NAME logs (GPAC version $VERSION) - test date $(date '+%d/%m/%Y %H:%M:%S') ***" > $LOGS
  echo "" >> $LOGS
 fi
}

#ends test - gather all logs/stats produced and generate report
test_end ()
{
 if [ $test_skip = 1 ] ; then
  return
 fi

 #wait for all sub-tests to complete (some may use subshells)
 wait

 test_stats="$LOGS_DIR/$TEST_NAME-stats.sh"
 echo "TEST_SKIP=0" > $test_stats
 stat_xml_temp="$TEMP_DIR/$TEST_NAME-statstemp.xml"
 echo "" > $stat_xml_temp

 test_fail=0
 test_leak=0
 test_exec_na=0
 nb_subtests=0
 nb_test_hash=0
 nb_hash_fail=0
 nb_hash_missing=0

 if [ "$result" != "" ] ; then
  test_fail=1
 fi

 #gather all stats per subtests
 for i in $TEMP_DIR/$TEST_NAME-stats-*.sh ; do
  reset_stat
  RETURN_VALUE=0
  SUBTEST_NAME=""
  COMMAND_LINE=""

  nb_subtests=$((nb_subtests + 1))

  source $i

  echo "  <stat subtest=\"$SUBTEST_NAME\" execution_status=\"$EXECUTION_STATUS\" return_status=\"$RETURN_STATUS\" mem_total_avg=\"$MEM_TOTAL_AVG\" mem_resident_avg=\"$MEM_RESIDENT_AVG\" mem_resident_max=\"$MEM_RESIDENT_MAX\" cpu_percent=\"$CPU_PERCENT\" cpu_elapsed_time=\"$CPU_ELAPSED_TIME\" cpu_user_time=\"$CPU_USER_TIME\" cpu_kernel_time=\"$CPU_KERNEL_TIME\" page_faults=\"$PAGE_FAULTS\" file_inputs=\"$FILE_INPUTS\" socket_msg_rec=\"$SOCKET_MSG_REC\" socket_msg_sent=\"$SOCKET_MSG_SENT\" return_value=\"$RETURN_VALUE\">" >> $stat_xml_temp

  echo "   <command_line>$COMMAND_LINE</command_line>" >> $stat_xml_temp
  echo "  </stat>" >> $stat_xml_temp

  if [ $RETURN_VALUE -eq 1 ] ; then
   result="$SUBTEST_NAME:Fail $result"
   test_fail=$((test_fail + 1))
  elif [ $RETURN_VALUE -eq 2 ] ; then
   result="$SUBTEST_NAME:MemLeak $result"
   test_leak=$((test_leak + 1))
  elif [ $RETURN_VALUE != 0 ] ; then
   result="$SUBTEST_NAME:UnknownFailure($RETURN_VALUE) $result"
   test_exec_na=$((test_exec_na + 1))
  fi
 done
 rm -f $TEMP_DIR/$TEST_NAME-stat-*.sh > /dev/null

 #gather all hashes for this test
 for i in $TEMP_DIR/$TEST_NAME-stathash-*.sh ; do
  if [ -f $i ] ; then
   HASH_TEST=""
   HASH_NOT_FOUND=0
   HASH_FAIL=0

   source $i
   nb_test_hash=$((nb_test_hash + 1))
   if [ $HASH_NOT_FOUND -eq 1 ] ; then
    result="$HASH_TEST:HashNotFound $result"
    nb_hash_missing=$((nb_hash_missing + 1))
   elif [ $HASH_FAIL -eq 1 ] ; then
    result="$HASH_TEST:HashFail $result"
    nb_hash_fail=$((nb_hash_fail + 1))
   fi
  fi
 done
 rm -f $TEMP_DIR/$TEST_NAME-stathash-*.sh > /dev/null

 if [ "$result" = "" ] ; then
  result="OK"
 fi

 echo " <test name=\"$TEST_NAME\" result=\"$result\" date=\"$(date '+%d/%m/%Y %H:%M:%S')\">" > $report
 cat $stat_xml_temp >> $report
 rm -f $stat_xml_temp > /dev/null
 echo " </test>" >> $report

 echo "TEST_FAIL=$test_fail" >> $test_stats
 echo "TEST_LEAK=$test_leak" >> $test_stats
 echo "TEST_EXEC_NA=$test_exec_na" >> $test_stats
 echo "NB_HASH_TEST=$nb_test_hash" >> $test_stats
 echo "NB_HASH_MISSING=$nb_hash_missing" >> $test_stats
 echo "NB_HASH_FAIL=$nb_hash_fail" >> $test_stats

 # list all logs files
 for i in $LOGS_DIR/$TEST_NAME-logs-*.txt; do
  cat $i >> $LOGS
 done
 rm -f $LOGS_DIR/$TEST_NAME-logs-*.txt > /dev/null

 echo "NB_SUBTESTS=$nb_subtests" >> $test_stats

 if [ "$result" == "OK" ] ; then
  mv $report "$LOGS_DIR/$TEST_NAME-passed-new.xml"
 else
  mv $report "$LOGS_DIR/$TEST_NAME-failed.xml"
 fi

 echo "$TEST_NAME: $result"

 if [ $strict_mode = 1 ] ; then
  if [ -f "$TEMP_DIR/err_exit" ] ; then
   cat $LOGS >> $ALL_LOGS
   cat "$LOGS_DIR/$TEST_NAME-failed.xml" >> $ALL_REPORTS
   exit
  fi
 fi
}

mark_errexit ()
{
 if [ $strict_mode = 1 ] ; then
  echo "" > "$TEMP_DIR/err_exit"
 fi
}

#@do_test execute the command line given $1 using GNU time and store stats with return value, command line ($1) and subtest name ($2)
ret=0
do_test ()
{
 if [ $strict_mode = 1 ] ; then
  if [ -f "$TEMP_DIR/err_exit" ] ; then
   return
  fi
 fi

 if [ $test_skip = 1 ] ; then
  return
 fi


log_subtest="$LOGS_DIR/$TEST_NAME-logs-$2.txt"
stat_subtest="$TEMP_DIR/$TEST_NAME-stats-$2.sh"
echo "SUBTEST_NAME=$2" > $stat_subtest

echo "" > $log_subtest
echo "*** Subtest \"$2\": executing \"$1\" ***" >> $log_subtest

$GNU_TIME -o $stat_subtest -f ' EXECUTION_STATUS="OK"\n RETURN_STATUS=%x\n MEM_TOTAL_AVG=%K\n MEM_RESIDENT_AVG=%t\n MEM_RESIDENT_MAX=%M\n CPU_PERCENT=%P\n CPU_ELAPSED_TIME=%E\n CPU_USER_TIME=%U\n CPU_KERNEL_TIME=%S\n PAGE_FAULTS=%F\n FILE_INPUTS=%I\n SOCKET_MSG_REC=%r\n SOCKET_MSG_SENT=%s' $1 >> $log_subtest 2>&1
rv=$?

if [ $rv -gt 2 ] ; then
 echo " Return Value $rv - re-executing without GNU TIME" >> $log_subtest
 $1 >> $log_subtest 2>&1
 rv=$?
fi

#regular error, check if this is a negative test.
if [ $rv -eq 1 ] ; then
 negative_test_stderr=$RULES_DIR/$TEST_NAME-$2-stderr.txt
 if [ -f $negative_test_stderr ] ; then
  #look for all lines in -stderr file, if one found consider this a success
  while read line ; do
   res_err=`grep -w "$line" $log_subtest`
   if [ -n "$res_err" ]; then
    echo "Negative test detected, reverting to success (found \"$res_err\" in stderr)" >> $log_subtest
    rv=0
    echo "" > $stat_subtest
    break
   fi
  done < $negative_test_stderr
 fi
fi

#override generated stats if error, since gtime may put undesired lines in output file which would break sourcing
if [ $rv != 0 ] ; then
echo "SUBTEST_NAME=$2" > $stat_subtest
mark_errexit
fi

echo "RETURN_VALUE=$rv" >> $stat_subtest
echo "COMMAND_LINE=\"$1\"" >> $stat_subtest

echo "" >> $log_subtest
ret=$rv
}
#end do_test

#@do_playback_test: checks for user input record if any, then launch MP4Client with $1 with dump_dur and dump_size video sec AVI recording, then checks audio and video hash of the dump and convert the video to MP4 when generating the hash. The results are logged as with do_test

do_playback_test ()
{
 if [ $strict_mode = 1 ] ; then
  if [ -f "$TEMP_DIR/err_exit" ] ; then
   return
  fi
 fi

 if [ $test_skip  = 1 ] ; then
  return 0
 fi

 AVI_DUMP="$TEMP_DIR/$TEST_NAME-$2-dump"

 args="$MP4CLIENT -avi 0-$dump_dur -out $AVI_DUMP -size $dump_size $1"

 ui_rec=$RULES_DIR/$TEST_NAME-$2-ui.xml

 if [ -f $ui_rec ] ; then
  args="$args -opt Validator:Mode=Play -opt Validator:Trace=$ui_rec"
 else
  args="$args -opt Validator:Mode=Disable"
 fi
 do_test "$args" $2

 #don't try hash if error
 if [ $ret != 0 ] ; then
  return
 fi

 if [ $skip_play_hash = 0 ] ; then
  #since AVI dump in MP4Client is based on real-time grab of multithreaded audio and video render
  #we may have interleaving differences in the resulting AVI :(
  #we generate a hash for both audio and video since we don't have a fix yet
  #furthermore this will allow figuring out if the error is in the video or the audio renderer
  $MP4BOX -aviraw video "$AVI_DUMP.avi" -out "$AVI_DUMP.video" > /dev/null 2>&1
  do_hash_test "$AVI_DUMP.video" "$2-avirawvideo"
  rm "$AVI_DUMP.video" 2> /dev/null

  $MP4BOX -aviraw audio "$AVI_DUMP.avi" -out "$AVI_DUMP.audio" > /dev/null 2>&1
  do_hash_test "$AVI_DUMP.audio" "$2-avirawaudio"
  rm "$AVI_DUMP.audio" 2> /dev/null
 fi

 if [ $do_store_video != 0 ] ; then
  if [ $generate_hash != 0 ] ; then
   ffmpeg_encode "$AVI_DUMP.avi" "$VIDEO_DIR_REF/$TEST_NAME-$2-ref.mp4"
  else
   ffmpeg_encode "$AVI_DUMP.avi" "$VIDEO_DIR/$TEST_NAME-$2-test.mp4"
  fi
 fi

if [ $keep_avi != 0 ] ; then
 if [ $generate_hash != 0 ] ; then
   mv "$AVI_DUMP.avi" "$VIDEO_DIR_REF/$TEST_NAME-$2-raw-ref.avi"
  else
   mv "$AVI_DUMP.avi" "$VIDEO_DIR/$TEST_NAME-$2-raw-test.avi"
  fi
else
  rm "$AVI_DUMP.avi" 2> /dev/null
fi

}
#end do_playback_test

#@do_hash_test: generates a hash for $1 file , compare it to HASH_DIR/$TEST_NAME$2.hash
do_hash_test ()
{
 if [ $strict_mode = 1 ] ; then
  if [ -f "$TEMP_DIR/err_exit" ] ; then
   return
  fi
 fi

 if [ $test_skip  = 1 ] ; then
  return
 fi

 if [ $disable_hash = 1 ] ; then
  return
 fi

 STATHASH_SH="$TEMP_DIR/$TEST_NAME-stathash-$2.sh"

 test_hash="$TEMP_DIR/$TEST_NAME-$2-test.hash"
 ref_hash="$HASH_DIR/$TEST_NAME-$2.hash"

 echo "HASH_TEST=$2" > $STATHASH_SH

 echo "Computing $1  ($2) hash: " >> $log_subtest
 $MP4BOX -hash -std $1 > $test_hash 2>> $log_subtest
 if [ $generate_hash = 0 ] ; then
  if [ ! -f $ref_hash ] ; then
   echo "HASH_NOT_FOUND=1" >> $STATHASH_SH
   return
  fi

  echo "HASH_NOT_FOUND=0" >> $STATHASH_SH

  $DIFF $test_hash $ref_hash > /dev/null
  rv=$?

  if [ $rv != 0 ] ; then
   fhash=`hexdump -ve '1/1 "%.2X"' $ref_hash`
   echo "Hash fail, ref hash $ref_hash was $fhash"  >> $log_subtest
   echo "HASH_FAIL=1" >> $STATHASH_SH
  else
   echo "Hash OK for $1"  >> $log_subtest
   echo "HASH_FAIL=0" >> $STATHASH_SH
  fi
  rm $test_hash

 else
  mv $test_hash $ref_hash
 fi
}
#end do_hash_test

#compare hashes of $1 and $2, return 0 if OK, error otherwise
do_compare_file_hashes ()
{
test_hash_first="$TEMP_DIR/$TEST_NAME-$(basename $1).hash"
test_hash_second="$TEMP_DIR/$TEST_NAME-$(basename $2).hash"

$MP4BOX -hash -std $1 > $test_hash_first 2> /dev/null
$MP4BOX -hash -std $1 > $test_hash_second 2> /dev/null
$DIFF $test_hash_first $test_hash_first > /dev/null

rv=$?
if [ $rv != 0 ] ; then
echo "Hash fail between $1 and $2"  >> $log_subtest
else
echo "Same Hash for $1 and $2"  >> $log_subtest
fi

rm $test_hash_first
rm $test_hash_second

return $rv

}
#end do_compare_file_hashes


#@ffmpeg_encode: encode source file $1 to $2 using default ffmpeg settings
ffmpeg_encode ()
{
 #run ffmpeg in force overwrite mode
 $FFMPEG -y -i $1 -pix_fmt yuv420p -strict -2 $2 2> /dev/null
}
#end


load_ui_rules ()
{
rules_sh=$RULES_DIR/$1.sh
if [ -f $rules_sh ] ; then
 source $rules_sh
fi

}
#@do_ui_tests: if $do_ui is 1 records user input on $1 playback (10 sec) and stores in $RULES_DIR/$1-ui.xml. If $do_ui is 2, plays back the recorded stream
do_ui_tests ()
{
 if [ $1 == *"-game-"* ] ; then
  return
 fi

 src=$1
 mp4file=${src%.*}'.mp4'
 test_name=$(basename $1)
 ui_stream=$RULES_DIR/${test_name%.*}-ui.xml

 dump_dur=$DEF_DUMP_DUR
 dump_size=$DEF_DUMP_SIZE
 load_ui_rules $test_name

 if [ $do_ui = 1 ] ; then
  if [ -f $ui_stream ]; then
   return
  fi
  echo "Recording user input for $src into $ui_stream" >> $ALL_LOGS
  $MP4BOX -mp4 $src -out $mp4file >> $ALL_LOGS
  $MP4CLIENT -run-for $dump_dur -size $dump_size $mp4file -no-save -opt Validator:Mode=Record -opt Validator:Trace=$ui_stream >> $ALL_LOGS
  rm $mp4file
 else
  if [ ! -f $dst ]; then
   return
  fi
  echo "Playing recorded user input for $src" >> $ALL_LOGS
  $MP4BOX -mp4 $src -out $mp4file >> $ALL_LOGS
  $MP4CLIENT -run-for $dump_dur -size $dump_size $mp4file -no-save -opt Validator:Mode=Play -opt Validator:Trace=$ui_stream >> $ALL_LOGS
  rm $mp4file
 fi
}
#end do_ui_tests


#record all user inputs and exit
if [ $do_ui != 0 ] ; then

 if [ -n "$url_arg" ] ; then
  test_name=$(basename $url_arg)
  ui_stream=$RULES_DIR/${test_name%.*}-ui.xml
  dump_dur=$DEF_DUMP_DUR
  dump_size=$DEF_DUMP_SIZE
  load_ui_rules $test_name
  if [ $do_ui = 1 ] ; then
   echo "*** Recording user input for $url_arg into $ui_stream ***"
   echo ""
echo "$MP4CLIENT -run-for $dump_dur -size $dump_size $url_arg -no-save -opt Validator:Mode=Record -opt Validator:Trace=$ui_stream "
$MP4CLIENT -run-for $dump_dur -size $dump_size $url_arg -no-save -opt Validator:Mode=Record -opt Validator:Trace=$ui_stream >> $ALL_LOGS
  else
   echo "*** Playing back $url_arg with user input $ui_stream ***"
   echo ""
echo "$MP4CLIENT -run-for $dump_dur -size $dump_size $url_arg -no-save -opt Validator:Mode=Play -opt Validator:Trace=$ui_stream"
   $MP4CLIENT -run-for $dump_dur -size $dump_size $url_arg -no-save -opt Validator:Mode=Play -opt Validator:Trace=$ui_stream >> $ALL_LOGS
  fi
  exit
 fi

 echo "User Inputs Tests - GPAC version $VERSION" > $ALL_LOGS

 #check bifs files
 for i in bifs/*.bt ; do
  has_sensor=`grep Sensor $i | grep -v TimeSensor | grep -v MediaSensor`
  if [ "$has_sensor" != "" ]; then
   do_ui_tests $i
  fi
 done

 #done
 exit
fi

#start of our tests
start=`$GNU_DATE +%s%N`

if [ $generate_hash = 1 ] ; then
 echo "SHA-1 Hash Generation enabled"
fi

echo "Logs for GPAC test suite - execution date $(date '+%d/%m/%Y %H:%M:%S')" > $ALL_LOGS

echo '<?xml version="1.0" encoding="UTF-8"?>' > $ALL_REPORTS
echo "<GPACTestSuite version=\"$VERSION\" platform=\"$platform\" date=\"$(date '+%d/%m/%Y %H:%M:%S')\">" >> $ALL_REPORTS

#run our tests
if [ -n "$url_arg" ] ; then
 source $url_arg
else
 for i in $SCRIPTS_DIR/*.sh ; do
  source $i
 done
fi
#wait for all tests to be done, since some tests may use subshells
wait

rm -rf $TEMP_DIR/* 2> /dev/null

if [ $check_names != 0 ] ; then
 exit
fi

#count all tests using generated -stats.sh
TESTS_SKIP=0
TESTS_TOTAL=0
TESTS_DONE=0
TESTS_PASSED=0
TESTS_FAIL=0
TESTS_EXEC_NA=0
TESTS_LEAK=0
TESTS_SUBTESTS=0
TESTS_HASH=0
TESTS_HASH_FAIL=0

for i in $LOGS_DIR/*-stats.sh ; do
if [ -f $i ] ; then

#reset stats
TEST_SKIP=0
TEST_FAIL=0
TEST_LEAK=0
TEST_EXEC_NA=0
NB_HASH_TEST=0
NB_HASH_FAIL=0
NB_SUBTESTS=0

#load stats
source $i

TESTS_TOTAL=$((TESTS_TOTAL + 1))
if [ $TEST_SKIP = 0 ] ; then
 TESTS_DONE=$((TESTS_DONE + 1))
 if [ $TEST_FAIL = 0 ] ; then
  TESTS_PASSED=$((TESTS_PASSED + 1))
 fi
fi

TESTS_SKIP=$((TESTS_SKIP + $TEST_SKIP))
TESTS_FAIL=$((TESTS_FAIL + $TEST_FAIL))
TESTS_EXEC_NA=$((TESTS_EXEC_NA + $TEST_EXEC_NA))
TESTS_LEAK=$((TESTS_LEAK + $TEST_LEAK))
TESTS_SUBTESTS=$((TESTS_SUBTESTS + $NB_SUBTESTS))
TESTS_HASH=$((TESTS_HASH + $NB_HASH_TEST))
TESTS_HASH_FAIL=$((TESTS_HASH_FAIL + $NB_HASH_FAIL))

fi

done

rm -f $LOGS_DIR/*-stats.sh > /dev/null

echo "<TestSuiteResults NumTests=\"$TESTS_TOTAL\" TestsPassed=\"$TESTS_PASSED\" TestsFailed=\"$TESTS_FAIL\" TestsLeaked=\"$TESTS_LEAK\" TestsUnknown=\"$TESTS_EXEC_NA\" />" >> $ALL_REPORTS

#gather all failed reports first
for i in $LOGS_DIR/*-failed.xml; do
 if [ -f $i ] ; then
  cat $i >> $ALL_REPORTS
  echo "" >> $ALL_REPORTS
  rm $i
 fi
done

#gather all new reports
for i in $LOGS_DIR/*-passed-new.xml; do
 if [ -f $i ] ; then
  cat $i >> $ALL_REPORTS
  echo "" >> $ALL_REPORTS
  #move new report to final name
  n=${i%"-new.xml"}
  n="$n.xml"
  mv "$i" "$n"
 fi
done

echo '</GPACTestSuite>' >> $ALL_REPORTS

#cat all logs
for i in $LOGS_DIR/*-logs.txt-new; do
 if [ -f $i ] ; then
  cat $i >> $ALL_LOGS
  echo "" >> $ALL_LOGS
  #move new report to final name
  n=${i%".txt-new"}
  n="$n.txt"
  mv "$i" "$n"
 fi
done

if [ $TESTS_TOTAL = 0 ] ; then
echo "No tests exectuted"
else

pc=$((100*TESTS_SKIP/TESTS_TOTAL))
echo "Number of Tests OK cached $TESTS_SKIP ($pc %)"
pc=$((100*TESTS_DONE/TESTS_TOTAL))
echo "Number of Tests Run $TESTS_DONE ($pc %)"


if [ $TESTS_DONE != 0 ] ; then
 pc=$((100*TESTS_PASSED/TESTS_DONE))
 echo "Tests passed $TESTS_PASSED ($pc %) - $TESTS_SUBTESTS sub-tests"
 pc=$((100*TESTS_FAIL/TESTS_DONE))
 echo "Tests failed $TESTS_FAIL ($pc %)"
 pc=$((100*TESTS_LEAK/TESTS_DONE))
 echo "Tests Leaked $TESTS_LEAK ($pc %)"
 pc=$((100*TESTS_EXEC_NA/TESTS_DONE))
 echo "Tests Unknown $TESTS_EXEC_NA ($pc %)"

 if [ $TESTS_HASH != 0 ] ; then
  pc=$((100*TESTS_HASH_FAIL/TESTS_HASH))
  echo "Tests HASH total $TESTS_HASH - fail $TESTS_HASH_FAIL ($pc %)"
 fi
fi

fi

end=`$GNU_DATE +%s%N`
runtime=$((end-start))
runtime=$(($runtime / 1000000))
echo "Generation done in $runtime milliseconds"



