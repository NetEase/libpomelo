LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jansson_static

LOCAL_MODULE_FILENAME := libjansson

LOCAL_SRC_FILES := \
src/dump.c \
src/load.c \
src/strbuffer.c \
src/error.c \
src/value.c \
src/hashtable.c \
src/memory.c \
src/strconv.c \
src/pack_unpack.c \
src/utf.c \




LOCAL_EXPORT_C_INCLUDES :=$(LOCAL_PATH)/src



LOCAL_C_INCLUDES := $(LOCAL_PATH) \
                    $(LOCAL_PATH)/src


include $(BUILD_STATIC_LIBRARY)


