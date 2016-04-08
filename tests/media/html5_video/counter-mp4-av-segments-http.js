/* Object representing the different files to download per quality */
var counterAudioVideoHTTPSegments = 
	[ 
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-mux/",
		  initName: "mp4-live-mux-mpd-AV-BS_init.mp4",
		  segmentPrefix: "mp4-live-mux-h264bl_low-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 60
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-live-mux/",
		  initName: "mp4-live-mux-mpd-AV-BS_init.mp4",
		  segmentPrefix: "mp4-live-mux-h264bl_hd-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 60
		}
	];
