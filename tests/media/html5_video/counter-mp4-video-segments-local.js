/* Array representing the different files to download per quality */
var counterVideoLocalSegments = 
	[ 
		{ 
		  baseURL: "media/mp4-main-multi/",
		  initName: "mp4-main-multi-mpd-V-BS_init.mp4",
		  segmentPrefix: "mp4-main-multi-h264bl_low-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 60
		},
		{ 
		  baseURL: "media/mp4-main-multi/",
		  initName: "mp4-main-multi-mpd-V-BS_init.mp4",
		  segmentPrefix: "mp4-main-multi-h264bl_mid-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 60
		},
		{ 
		  baseURL: "media/mp4-main-multi/",
		  initName: "mp4-main-multi-mpd-V-BS_init.mp4",
		  segmentPrefix: "mp4-main-multi-h264bl_hd-",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 60
		}
	];
