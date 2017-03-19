/* Array representing the different files to download per quality */
var redBullVideoHTTPSegments = 
	[ 
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/100kbps/",
		  initName: "redbull_240p_100kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_240p_100kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/150kbps/",
		  initName: "redbull_240p_150kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_240p_150kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/200kbps/",
		  initName: "redbull_360p_200kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_360p_200kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/250kbps/",
		  initName: "redbull_360p_250kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_360p_250kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/300kbps/",
		  initName: "redbull_360p_300kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_360p_300kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/400kbps/",
		  initName: "redbull_360p_400kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_360p_400kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/500kbps/",
		  initName: "redbull_10sec_set2_init.mp4",
		  segmentPrefix: "redbull_480p_500kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/700kbps/",
		  initName: "redbull_10sec_set2_init.mp4",
		  segmentPrefix: "redbull_480p_700kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/900kbps/",
		  initName: "redbull_10sec_set2_init.mp4",
		  segmentPrefix: "redbull_480p_900kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/1200kbps/",
		  initName: "redbull_10sec_set2_init.mp4",
		  segmentPrefix: "redbull_480p_1200kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/1500kbps/",
		  initName: "redbull_720p_1500kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_720p_1500kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/2000kbps/",
		  initName: "redbull_720p_2000kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_720p_2000kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/2500kbps/",
		  initName: "redbull_720p_2500kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_720p_2500kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/3000kbps/",
		  initName: "redbull_1080p_3000kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_1080p_3000kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/4000kbps/",
		  initName: "redbull_1080p_4000kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_1080p_4000kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/5000kbps/",
		  initName: "redbull_1080p_5000kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_1080p_5000kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		},
		{ 
		  baseURL: "http://download.tsi.telecom-paristech.fr/gpac/dataset/dash/mmsys13/video/redbull_10sec/6000kbps/",
		  initName: "redbull_1080p_6000kbps_10sec_segmentinit.mp4",
		  segmentPrefix: "redbull_1080p_6000kbps_10sec_segment",
		  segmentSuffix: ".m4s",
		  segStartIndex: 2,
		  segEndIndex: 525
		}
	];
