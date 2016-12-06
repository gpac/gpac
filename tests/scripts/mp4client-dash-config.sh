URL="http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-1s/mp4-live-1s-mpd-AV-BS.mpd"
DEFAULT_OPT="-run-for 10 -opt DASH:AutoSwitchCount=0 -opt DASH:BufferingMode=minBuffer -opt DASH:NetworkAdaptation=buffer -opt DASH:StartRepresentation=minBandwidth -opt DASH:SpeedAdaptation=no -opt DASH:SwitchProbeCount=1 -opt DASH:AgressiveSwitching=no"

do_test "$MP4CLIENT $URL $DEFAULT_OPT" "mp4client-dash-default"

#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:AutoSwitchCount=0" "mp4client-dash-autoswitch-0"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:AutoSwitchCount=1" "mp4client-dash-autoswitch-1"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:AutoSwitchCount=5" "mp4client-dash-autoswitch-5"

do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:BufferingMode=segments" "mp4client-dash-bufferingmode-segments"
#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:BufferingMode=minBuffer" "mp4client-dash-bufferingmode-minBuffer"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:BufferingMode=none" "mp4client-dash-bufferingmode-none"

do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:NetworkAdaptation=disabled" "mp4client-dash-networkadaptation-disabled"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:NetworkAdaptation=bandwidth" "mp4client-dash-networkadaptation-bandwidth"
#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:NetworkAdaptation=buffer" "mp4client-dash-networkadaptation-buffer"

#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:StartRepresentation=minBandwidth" "mp4client-dash-startrepresentation-minbandwidth"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:StartRepresentation=maxBandwidth" "mp4client-dash-startrepresentation-maxbandwidth"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:StartRepresentation=minQuality" "mp4client-dash-startrepresentation-minquality"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:StartRepresentation=maxQuality" "mp4client-dash-startrepresentation-maxquality"

do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:SpeedAdaptation=yes" "mp4client-dash-speedadaptation-yes"
#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:SpeedAdaptation=no" "mp4client-dash-speedadaptation-no"

do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:SwitchProbeCount=0" "mp4client-dash-switchprobecount-0"
#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:SwitchProbeCount=1" "mp4client-dash-switchprobecount-1"
do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:SwitchProbeCount=5" "mp4client-dash-switchprobecount-5"

do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:AgressiveSwitching=yes" "mp4client-dash-agressiveswitching-yes"
#do_test "$MP4CLIENT $URL $DEFAULT_OPT -opt DASH:AgressiveSwitching=no" "mp4client-dash-agressiveswitching-no"
