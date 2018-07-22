LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
SDL_TTF_PATH := ../SDL2_ttf
SDL_MIXER_PATH := ../SDL2_mixer

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
                    $(LOCAL_PATH)/$(SDL_TTF_PATH)/include \
                    $(LOCAL_PATH)/$(SDL_MIXER_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES := arkanoid.cpp

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_ttf SDL2_mixer

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

LOCAL_CPPFLAGS := -std=c++14 -frtti

include $(BUILD_SHARED_LIBRARY)
