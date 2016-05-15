LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pxsrc

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../pgl3d-extlib \
	$(LOCAL_PATH)/../../pgl3d-extlib/SDL2/include \
	$(LOCAL_PATH)/../../pgl3d-extlib/SDL2_ttf \
	$(LOCAL_PATH)/../../pgl3d-extlib/SDL2_image \
	$(LOCAL_PATH)/../../pgl3d-extlib/bullet/src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := gen/demoapp.px.cc

LOCAL_LDLIBS := -lGLESv3 -llog
LOCAL_CPP_FEATURES += rtti exceptions

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf bullet

include $(BUILD_SHARED_LIBRARY)
