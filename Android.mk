LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := pomelo_static

LOCAL_MODULE_FILENAME := libpomelo

LOCAL_SRC_FILES := \
src/client.c \
src/message.c \
src/package.c \
src/pkg-handshake.c \
src/transport.c \
src/common.c \
src/msg-json.c \
src/pb-decode.c \
src/pkg-heartbeat.c \
src/listener.c \
src/msg-pb.c \
src/pb-encode.c \
src/protocol.c \
src/map.c \
src/network.c \
src/pb-util.c \
src/thread.c \





LOCAL_EXPORT_C_INCLUDES :=$(LOCAL_PATH)/include \
                          $(LOCAL_PATH)/include/pomelo-protobuf \
						  $(LOCAL_PATH)/include/pomelo-protocol



LOCAL_C_INCLUDES := $(LOCAL_PATH)/include \
                    $(LOCAL_PATH)/include/pomelo-protobuf \
					$(LOCAL_PATH)/include/pomelo-protocol \
					$(LOCAL_PATH)/include/pomelo-private

LOCAL_WHOLE_STATIC_LIBRARIES := uv_static jansson_static



include $(BUILD_STATIC_LIBRARY)

LOCAL_CFLAGS    := -D__ANDROID__ 

$(call import-module,libpomelo/deps/uv) \
$(call import-module,libpomelo/deps/jansson)