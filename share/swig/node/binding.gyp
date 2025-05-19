{
    "variables": {
        "module_name": "gpac",
        "GPAC_INCLUDE_DIR": "<!(pkg-config --variable=includedir gpac)",
        "GPAC_LIB_DIR": "<!(pkg-config --variable=libdir gpac)",
    },
    "targets": [
        {
            "target_name": "<(module_name)",
            "sources": ["gpac_wrap.cpp"],
            "include_dirs": ["<(GPAC_INCLUDE_DIR)"],
            "libraries": ["-L<(GPAC_LIB_DIR)", "-lgpac"],
            "dependencies": [
                "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except",
            ],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "cflags_cc": ["-std=c++17"],
            "conditions": [
                [
                    'OS=="mac"',
                    {
                        "xcode_settings": {
                            "OTHER_CFLAGS!": ["-fno-exceptions"],
                            "OTHER_CFLAGS": ["-std=c++17"],
                            "cflags+": ["-fvisibility=hidden"],
                            "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
                        }
                    },
                ]
            ],
        }
    ],
}
