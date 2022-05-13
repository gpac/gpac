APP_ABI := x86 armeabi-v7a
APP_STL := c++_static
APP_CPPFLAGS := -frtti -fexceptions -Wno-parentheses -Wno-shift-negative-value -Wno-pointer-sign
APP_CFLAGS := -Wno-parentheses -Wno-shift-negative-value -Wno-pointer-sign

#pass these through ndk-build
#LOCAL_CFLAGS := -O3
#APP_OPTIM := release
#LOCAL_CFLAGS := -g
#APP_OPTIM := debug

APP_MODULES := gpac gm_ft_font libjavaenv gm_droid_out gm_droid_audio gpacWrapper
