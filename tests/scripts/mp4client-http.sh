
#progressive bifs in http
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/bifs/bifs-2D-background-background2D-bind.bt" "mp4client-http-bt"

#progressive bifs in https
single_playback_test "-guid https://raw.githubusercontent.com/gpac/gpac/master/tests/media/bifs/bifs-2D-background-background2D-image.bt" "mp4client-https-bt"

#progressive jpg (same code for jpg & png)
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/sky.jpg" "mp4client-http-jpg"

#progressive mp3
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/count_french.mp3" "mp4client-http-mp3"

#progressive aac
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/auxiliary_files/enst_audio.aac" "mp4client-http-aac"

#progressive ac3
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/resources/media/counter/counter-30s_audio.ac3" "mp4client-http-ac3"

#progressive amr
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/resources/media/import/bear_audio.amr" "mp4client-http-amr"

#progressive bifs with commands in http
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/bifs/bifs-command-animated-osmo4logo.bt" "mp4client-http-bt-commands"

#progressive MP4 with commands in http
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/bifs/bifs-command-animated-osmo4logo.mp4" "mp4client-http-mp4-commands"


#progressive MP4 AV in http
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/uhd/mux_sources/hevcds_720p60_3M.mp4" "mp4client-http-mp4av"


#re-run progressive bifs in http to test the cache
single_playback_test "-guid http://download.tsi.telecom-paristech.fr/gpac/gpac_test_suite/regression_tests/bifs/bifs-2D-background-background2D-bind.bt" "mp4client-http-bt-cached"
