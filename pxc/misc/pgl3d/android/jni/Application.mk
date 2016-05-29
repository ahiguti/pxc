
# Uncomment this if you're using STL in your project
# See CPLUSPLUS-SUPPORT.html in the NDK documentation for more information
# APP_STL := stlport_static 

APP_STL := gnustl_static

#APP_ABI := all
#APP_ABI := armeabi-v7a arm64-v8a
APP_ABI := armeabi-v7a

APP_CPPFLAGS += --std=c++11

ifeq ($(strip $(NDK_DEBUG)),1)
  APP_CFLAGS +=
else
  APP_CFLAGS += -O3
endif

APP_PLATFORM := android-21
