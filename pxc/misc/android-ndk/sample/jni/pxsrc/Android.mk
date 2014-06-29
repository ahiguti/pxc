LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pxsrc

SDL_PATH := ../SDL2

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include ../SDL2/include ../SDL2_image/ ../SDL2_ttf/

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/gen/*.cc))

LOCAL_LDLIBS := -lGLESv2 -llog
LOCAL_CPP_FEATURES += rtti exceptions

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf

include $(BUILD_SHARED_LIBRARY)
