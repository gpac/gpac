This is the documentation for creating a new test in GPAC test suite.

# Running the test suite

To run the entire test suite, simply run `make_tests.sh` or use the Makefile task `make test_suite`.

Running the test suite or a given test generates:
- result/all_logs.txt: logs of all tests that were executed 
- result/all_results.xml: GNU time/gtime statistics and execution status of all tests that were executed

The different options of make_tests.sh can are given by the command:
```
make_tests.sh -h
```
In particular, tests are cached, so that only failed tests are re-launched when running the test suite. 

# Writing a test
GPAC test suite is composed of scripts written in the Bash language. Tests are placed in gpac/tests/scripts/. Each .sh file in that folder will be executed when running the entire test suite or may be run individually. To run a particular script, run `make_tests.sh ascript.sh`.

Media files used in a test can be anywhere (local file, http URL). The environmnent variable `$MEDIA_DIR` can be used to access to data located in gpac/tests/media. 

Scripts should use the `$TEMP_DIR` environment variable to get a directory where to place the file they generate. This directtory is cleaned after each test.

A simple GPAC test can be:
```
#!/bin/sh
MP4Box -add $MEDIA_DIR\foo.bar $TEMP_DIR\foo.mp4
```
or 
```
#!/bin/sh
MP4Client $MEDIA_DIR\foo.mp4
```

In order to provide test caching and produce the unified test report for all the tests, with common timing, logging, etc, `make_tests.sh` defines several functions that should be used in a test.

Each test has its own log file `$LOGS` in which you can write (a lot is already in there, such as test name/data and all stderr).

## Simple Testing
A simple test whose result will be integrated in the test suite report is run using the single_test function.

### single_test 
Two arguments:
- `CMD_LINE`: Executes the given `CMD_LINE`. The command line muste be passed between quotes. 
- `TESTNAME`: specifies the test name

Each call generates:
- a log file called `TESTNAME-single-logs.txt` containg test name, command line and all stderr of the test. You should not use file logging of GPAC ( `-lf` or `-logs` options) in your command line.
- a file called `TESTNAME-single-passed.xml` containg the statistics of the test, as retrieved by GNU time/gtime.
`TESTNAME` shall be unique in the test suite. In case of doubts, run the test suite script with `-check-name`.

A simple test using `single_test` looks like:
```
#!/bin/sh
single_test 'MP4Box -add $MEDIA_DIR\foo.bar $TEMP_DIR\foo.mp4' test01
```
or 
```
#!/bin/sh
single_test 'MP4Client $MEDIA_DIR\foo.mp4' test02
```

### single_playback_test 
Two arguments: 
- `MP4CLIENT_ARGS`: specifies the arguments to be passed to MP4Client
- `TESTNAME`: specifies the test name for logging, as used in `single_test`
Performs a single playback test using MP4Client with the arguments given in `MP4CLIENT_ARGS`. The result of the playback is captured as a raw audio/video AVI. If ffmpeg is present on the system, the AVI is converted to mp4 in `results/video/$TESTNAME-play.mp4`.
See `do_playback_test` for more info.

## Aggregating tests
When the results of several tests need to be aggregated, instead of using multiple `single_test` calls, sub-tests can be run within a test using the `do_test` function. 
This is usefull when severa tests require the generation of the same input file, for example.

A typical test with two subtests used in a batch will look like:
```
mytest()
{
test_begin $1
do_test CMD_LINE1 "Name1"
do_test CMD_LINE1 "Name2"
test_end
}

for i in * ; do
  mytest $i
done
```

### test_begin 
Argument:
- `TESTNAME`: specifies the test name for logging, as used in `single_test`

### test_end
No argument.

Triggers the end of the test and writes all logs and statistics.
The function evaluates the variable `$result`. If not empty, the test is considered failed, otherwise (default) the test has passed. 
All results of subtests are automatically appended to the `$result` variable in the `end_test` function.
WARNING: this variable can not be set in a subshell (eg in `some_function SOME_PARAM &` called during a test), it must be set in the shell calling `test_begin`.

### do_test
Two arguments:
- `CMD_LINE`: the command line to execute
- `SUBTEST_NAME`: the subtest name as it appears in the logs and in the stats

Performs a subtest in the current test. If needed, the return value is available in `$ret`. 
Function does nothing when the test is skipped (see below) or a previous error occured in the parent test `TESTNAME`.

### do_playback_test 
Two arguments:
- `MP4CLIENT_ARGS`: specifies the arguments to be passed to MP4Client
- `SUBTEST_NAME`: the subtest name as it appears in the logs and in the stats
 `do_playback_test` tests the playback of MP4CLIENT_ARGS by extracting it to a raw AVI and converting it to MP4. The subtest name for logs is `SUBTEST`.
Function does nothing when the test is skipped (see below) or a previous error occured in the parent test `TESTNAME`.

The generated videos are
-  `external_videos_refs/$TESTNAME-$SUBTEST-ref.mp4` when generating the reference hashes (see below). These videos are currently not used by test suite, only used for visual checking of the reference.
-  `results/videos/$TESTNAME-$SUBTEST-test.mp4` when testing the suite. These videos are currently not used by the test suite, only used for debugging a test.

This function uses overridable variables:
- `$dump_size`: by default "200x200" but can be overriden by your test
- `$dump_dur`: by default "10" seconds but can be overriden by your test

## Test failure/success
A test is considered successful if the execution returned 0.
Some tests may return a non-zero value and still be considered successful (e.g. negative tests). In that case, errors to be ignored are placed in a file in the gpac/rules directory. If any of the line in this file is found, the test is considered successful. 
The error file is named:
- `TESTNAME-stderr.txt` for tests run with the function `single_test`
- `$TESTNAME-$SUBTEST-stderr.txt` for subtests run with `do_test` `CMD_LINE``SUBTEST`

## Customized test 
To customize a test, a file named `$TESTNAME.sh` can be placed in the gpac/rules directory. It is called before the actual test is run and allows custom variables to be set or overriding a given variable in a batch of tests. Directories variables shall not be modified. Other variables of the test suite documented here are reset at the begining of each test.  

## Testing events and interactivity
The functions `single_playback_test` and `single_playback_test`  may use recorded event traces to test interactivity; the recorded trace file is:
- $RULES_DIR/${basename $FILE.*}-$SUBTEST-ui.xml

An event trace can be recorded to $SOMEFILE using `MP4Client -opt Validator:Mode=Play -opt Validator:Trace=$SOMEFILE $ARGS`

This event trace can be using `MP4Client -opt Validator:Mode=Record -opt Validator:Trace=$SOMEFILE $ARGS`

## Parallel tests with subscripts

Subtests may be run in subscripts, for example:
```
test_begin TESTNAME
 do_test CMD_LINE1 "Name1" &
 do_test CMD_LINE1 "Name2" &
test_end
```

Tests may also be run in subscript, for example:
```
function my_test {
  test_begin $2
  do_test $1 $2
  test_end
}

my_test CMD_LINE1 "Name1" &
my_test CMD_LINE2 "Name2" &
```

## Testing and caching results
In order to avoid running all tests again whenever one test fails, the GPAC testing environment caches previous test results. If an XML file corresponding to the test or subtest exists in the results folder, the test or subtest is not run. If you want to ignore previous results, use `make_tests.sh -clean` before running the test suite. If you only want to invalidate tests of a given script, use  `make_tests.sh -clean script.sh` 
In case a test requires generating files before testing , it is recommended to check the variable `test_skip`to check if the tests is being skipped because cached. A typical function looks like:

A typical test with several subtests and hash testing looks like:
```
test_begin TESTNAME
if [ $test_skip = 1 ] ; then
 return
fi
 
#create some file or costly operation
MP4Box -add 4GB.src -new TESTFILE
do_test CMD_LINE1_USING_TESTFILE "Name1"
do_test CMD_LINE2 "Name2"

test_end
```
In this example, failure to checktest $test_skip will make the script import 4GB.src even though all `do_test` calls will be skipped.


## Testing regressions with hashes

Tests may perform all the required checks to detect a regression, such as checking generated file info, counting importing samples, etc ... 
To simplify test writing, the GPAC testing environment provide simple hashing (SHA-1) functions that you can perform on generated files such as MP4 or TS files, logs, XML dumps, ... Using hashes avoid storing such files while allowing generic regression detection. Because of this functionnality, the test suite has a specific mode for generating the hashes, `./make_tests -hash`. Hashes are typically generated only once, upon initial test creation.
The test suite script will warn you when reference hashes are missing.
- To clean hashes for a single test, use `./make_tests -clean-hash mytest.sh`
- To generate hashes for a single test, use `./make_tests -hash mytest.sh`

The following functions are available for hash testing.

### do_hash_test 
Two arguments
- `FILE`
- `HASHNAME`
When the test suite is run in hash generation mode, this generates the reference `hash_refs/$TESTNAME-$HASHNAME.hash`. Otherwise, creates a SHA-1 of FILE and compares it with the reference `$TESTNAME-$HASHNAME.hash`. 
Function does nothing when the test is skipped (see below) or a previous error occured in the parent test `TESTNAME`.

WARNING: `HASHNAME` shall be unique for a given test, otherwise hashes will be overwritten.

### do_compare_file_hashes 
Two arguments
- `FILE1`
- `FILE2`
Compares hashes of `FILE1` and `FILE2`. returns 0 if OK or error code otherwise.
Function does nothing when the test is skipped (see below) or a previous error occured in the parent test `TESTNAME`.

The playback functions `single_playback_test` and `do_playback_test` use hashes to check video and audio renderers. 
- For  `single_playback_test`, the generated hashes are `$TESTNAME-avirawvideo.hash`and `$TESTNAME-avirawaudio.hash`. 
- For `do_playback_test`, the generated hashes are `$TESTNAME-$SUBTEST-avirawvideo.hash` and `$TESTNAME-$SUBTEST-avirawaudio.hash`.


A typical test with several subtests and hash testing looks like:
```
test_begin TESTNAME
if [ $test_skip = 1 ] ; then
 return
fi
 
#run some test creating a file FILE1
do_test CMD_LINE1_CREATING_FILE1 "Name1"
#perform hash on FILE1 - the hash name may be the same as the subtest names
do_hash_test FILE1 "Name1"

#play the file - hashes wil be generated/checked 
do_playback_test FILE1 "Name1"


test_end
```
