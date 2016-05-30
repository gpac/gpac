#On utilise gnustl au lieu de system pour etre compatible avec la stl c++
APP_STL := gnustl_static
#Pour le code c++
APP_CPPFLAGS := -frtti -fexceptions -std=c++11
NDK_TOOLCHAIN_VERSION := 4.9

APP_ABI := armeabi-v7a armeabi
APP_MODULES := gpacWrapper