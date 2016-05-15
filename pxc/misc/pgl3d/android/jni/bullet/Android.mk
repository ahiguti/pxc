LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := bullet

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../pgl3d-extlib/bullet/src

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_BASE := $(LOCAL_PATH)/../../pgl3d-extlib/bullet/src
LOCAL_FILE_LIST := \
  $(wildcard $(LOCAL_SRC_BASE)/LinearMath/*.cpp) \
  $(wildcard $(LOCAL_SRC_BASE)/BulletCollision/*/*.cpp) \
  $(wildcard $(LOCAL_SRC_BASE)/BulletDynamics/*/*.cpp) \
  $(wildcard $(LOCAL_SRC_BASE)/BulletSoftBody/*.cpp)

LOCAL_SRC_FILES := $(LOCAL_FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS :=
LOCAL_CPP_FEATURES += rtti
# LOCAL_CPP_FEATURES += rtti exceptions

LOCAL_SHARED_LIBRARIES :=

include $(BUILD_SHARED_LIBRARY)
