LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsg7231

LOCAL_CFLAGS += -DVERSION=\"android\"

LOCAL_SRC_FILES = src/msg7231.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include
	
include $(BUILD_STATIC_LIBRARY)


