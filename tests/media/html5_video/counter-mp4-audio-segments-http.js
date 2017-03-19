/* Object representing the different files to download per quality */
var counterAudioHTTPSegments = 
	[ 
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-main-multi/",
		  initName: "mp4-main-multi-mpd-A-BS_init.mp4",
		  segmentPrefix: "mp4-main-multi-aaclc_low-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 64
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/DASH_CONFORMANCE/TelecomParisTech/mp4-main-multi/",
		  initName: "mp4-main-multi-mpd-A-BS_init.mp4",
		  segmentPrefix: "mp4-main-multi-aaclc_high-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 64
		}
	];
