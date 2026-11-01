LOCAL_PATH := $(call my-dir)

# Discord Social SDK (download the C++/Android package from the Developer Portal)
# Expected layout:  sdk/include/discordpp.h
#                   sdk/lib/arm64-v8a/libdiscord_partner_sdk.so
include $(CLEAR_VARS)
LOCAL_MODULE := discord_partner_sdk
LOCAL_SRC_FILES := sdk/lib/$(TARGET_ARCH_ABI)/libdiscord_partner_sdk.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := HexedDiscordRPC
LOCAL_SRC_FILES := hexed_discord_rpc.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)/sdk/include $(LOCAL_PATH)/aml/include
LOCAL_SHARED_LIBRARIES := discord_partner_sdk
LOCAL_LDLIBS := -llog
LOCAL_CPPFLAGS := -std=c++20 -fexceptions
include $(BUILD_SHARED_LIBRARY)

# Application.mk:
#   APP_ABI := arm64-v8a
#   APP_PLATFORM := android-24
#   APP_STL := c++_static
