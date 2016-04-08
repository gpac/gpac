/* Array representing the different files to download per quality */
var myanmarFullVideoSegmentsLocal = 
	[ 
		{ 
		  baseURL: "dash/full/",
		  initName: "myanmar_360_60s_dashinit.mp4",
		  segmentPrefix: "myanmar_360_60s_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 17
		}
	];

var myanmarTile1VideoSegmentsLocal = 
	[ 
		{ 
		  baseURL: "dash/t1/",
		  initName: "myanmar_360_t1_dashinit.mp4",
		  segmentPrefix: "myanmar_360_t1_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t1/",
		  initName: "myanmar_720_t1_dashinit.mp4",
		  segmentPrefix: "myanmar_720_t1_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t1/",
		  initName: "myanmar_1080_t1_dashinit.mp4",
		  segmentPrefix: "myanmar_1080_t1_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		}
	];

var myanmarTile2VideoSegmentsLocal = 
	[ 
		{ 
		  baseURL: "dash/t2/",
		  initName: "myanmar_360_t2_dashinit.mp4",
		  segmentPrefix: "myanmar_360_t2_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t2/",
		  initName: "myanmar_720_t2_dashinit.mp4",
		  segmentPrefix: "myanmar_720_t2_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t2/",
		  initName: "myanmar_1080_t2_dashinit.mp4",
		  segmentPrefix: "myanmar_1080_t2_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		}
	];

var myanmarTile3VideoSegmentsLocal = 
	[ 
		{ 
		  baseURL: "dash/t3/",
		  initName: "myanmar_360_t3_dashinit.mp4",
		  segmentPrefix: "myanmar_360_t3_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t3/",
		  initName: "myanmar_720_t3_dashinit.mp4",
		  segmentPrefix: "myanmar_720_t3_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t3/",
		  initName: "myanmar_1080_t3_dashinit.mp4",
		  segmentPrefix: "myanmar_1080_t3_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		}
	];

var myanmarTile4VideoSegmentsLocal = 
	[ 
		{ 
		  baseURL: "dash/t4/",
		  initName: "myanmar_360_t4_dashinit.mp4",
		  segmentPrefix: "myanmar_360_t4_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t4/",
		  initName: "myanmar_720_t4_dashinit.mp4",
		  segmentPrefix: "myanmar_720_t4_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		},
		{ 
		  baseURL: "dash/t4/",
		  initName: "myanmar_1080_t4_dashinit.mp4",
		  segmentPrefix: "myanmar_1080_t4_dash",
		  segmentSuffix: ".m4s",
		  segStartIndex: 1,
		  segEndIndex: 39
		}
	];
	
var myanmarSpatialRepresentations = {
	global: { x: 0, y: 0, w: 1280, h: 720 },
	sets: [
		{ x:   0, y:   0, w: 640, h: 360, representations: myanmarTile1VideoSegmentsLocal },
		{ x: 640, y:   0, w: 640, h: 360, representations: myanmarTile2VideoSegmentsLocal },
		{ x:   0, y: 360, w: 640, h: 360, representations: myanmarTile3VideoSegmentsLocal },
		{ x: 640, y: 360, w: 640, h: 360, representations: myanmarTile4VideoSegmentsLocal }
	]
}

