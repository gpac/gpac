{
    "variables": {
        "PROJECT_ROOT": "<!(echo $PWD)/../../..",
    },
    "targets": [
        {
            "target_name": "gpac",
            "sources": ["gpac_wrap.cpp"],
            "include_dirs": ["<(PROJECT_ROOT)/include", "<(PROJECT_ROOT)"],
            "libraries": ["-L<(PROJECT_ROOT)/bin/gcc", "-lgpac"],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "cflags_cc": ["-std=c++17", "-DGPAC_HAVE_CONFIG_H"],
        }
    ],
}
