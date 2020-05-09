// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_paths.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "core/settings.h"
#include "jni/applets/mii_selector.h"
#include "jni/applets/swkbd.h"
#include "jni/camera/still_image_camera.h"
#include "jni/id_cache.h"

#include <jni.h>

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

static JavaVM* s_java_vm;

static jclass s_native_library_class;
static jmethodID s_display_alert_msg;
static jmethodID s_display_alert_prompt;
static jmethodID s_alert_prompt_button;
static jmethodID s_is_portrait_mode;
static jmethodID s_landscape_screen_layout;
static jmethodID s_exit_emulation_activity;
static jmethodID s_request_camera_permission;
static jmethodID s_request_mic_permission;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jmethodID GetDisplayAlertMsg() {
    return s_display_alert_msg;
}

jmethodID GetDisplayAlertPrompt() {
    return s_display_alert_prompt;
}

jmethodID GetAlertPromptButton() {
    return s_alert_prompt_button;
}

jmethodID GetIsPortraitMode() {
    return s_is_portrait_mode;
}

jmethodID GetLandscapeScreenLayout() {
    return s_landscape_screen_layout;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetRequestCameraPermission() {
    return s_request_camera_permission;
}

jmethodID GetRequestMicPermission() {
    return s_request_mic_permission;
}

} // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize Logger
    Log::Filter log_filter;
    log_filter.ParseFilterString(Settings::values.log_filter);
    Log::SetGlobalFilter(log_filter);
    Log::AddBackend(std::make_unique<Log::LogcatBackend>());
    FileUtil::CreateFullPath(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
    Log::AddBackend(std::make_unique<Log::FileBackend>(
        FileUtil::GetUserPath(FileUtil::UserPath::LogDir) + LOG_FILE));
    LOG_INFO(Frontend, "Logging backend initialised");

    // Initialize Java methods
    const jclass native_library_class = env->FindClass("org/citra/citra_emu/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_display_alert_msg = env->GetStaticMethodID(s_native_library_class, "displayAlertMsg",
                                                 "(Ljava/lang/String;Ljava/lang/String;Z)Z");
    s_display_alert_prompt =
        env->GetStaticMethodID(s_native_library_class, "displayAlertPrompt",
                               "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;");
    s_alert_prompt_button =
        env->GetStaticMethodID(s_native_library_class, "alertPromptButton", "()I");
    s_is_portrait_mode = env->GetStaticMethodID(s_native_library_class, "isPortraitMode", "()Z");
    s_landscape_screen_layout =
        env->GetStaticMethodID(s_native_library_class, "landscapeScreenLayout", "()I");
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_request_camera_permission =
        env->GetStaticMethodID(s_native_library_class, "RequestCameraPermission", "()Z");
    s_request_mic_permission =
        env->GetStaticMethodID(s_native_library_class, "RequestMicPermission", "()Z");

    MiiSelector::InitJNI(env);
    SoftwareKeyboard::InitJNI(env);
    Camera::StillImage::InitJNI(env);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    env->DeleteGlobalRef(s_native_library_class);
    MiiSelector::CleanupJNI(env);
    SoftwareKeyboard::CleanupJNI(env);
    Camera::StillImage::CleanupJNI(env);
}

#ifdef __cplusplus
}
#endif
