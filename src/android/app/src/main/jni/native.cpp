// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iostream>
#include <regex>
#include <thread>

#include <android/native_window_jni.h>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/scope_acquire_context.h"
#include "core/hle/service/am/am.h"
#include "core/settings.h"
#include "jni/applets/swkbd.h"
#include "jni/button_manager.h"
#include "jni/config.h"
#include "jni/emu_window/emu_window.h"
#include "jni/game_info.h"
#include "jni/id_cache.h"
#include "jni/native.h"
#include "jni/ndk_motion.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"

namespace {

ANativeWindow* s_surf;

std::unique_ptr<EmuWindow_Android> window;

std::atomic<bool> is_running{false};
std::atomic<bool> pause_emulation{false};

std::mutex paused_mutex;
std::mutex running_mutex;
std::condition_variable running_cv;

} // Anonymous namespace

static std::string GetJString(JNIEnv* env, jstring jstr) {
    if (!jstr) {
        return {};
    }

    const char* s = env->GetStringUTFChars(jstr, nullptr);
    std::string result = s;
    env->ReleaseStringUTFChars(jstr, s);
    return result;
}

static bool DisplayAlertMessage(const char* caption, const char* text, bool yes_no) {
    JNIEnv* env = IDCache::GetEnvForThread();

    // Execute the Java method.
    jboolean result = env->CallStaticBooleanMethod(
        IDCache::GetNativeLibraryClass(), IDCache::GetDisplayAlertMsg(), env->NewStringUTF(caption),
        env->NewStringUTF(text), yes_no ? JNI_TRUE : JNI_FALSE);

    return result != JNI_FALSE;
}

static std::string DisplayAlertPrompt(const char* caption, const char* text, int buttonConfig) {
    JNIEnv* env = IDCache::GetEnvForThread();

    jstring value = reinterpret_cast<jstring>(env->CallStaticObjectMethod(
        IDCache::GetNativeLibraryClass(), IDCache::GetDisplayAlertPrompt(),
        env->NewStringUTF(caption), env->NewStringUTF(text), buttonConfig));

    return GetJString(env, value);
}

static int AlertPromptButton() {
    JNIEnv* env = IDCache::GetEnvForThread();

    // Execute the Java method.
    return static_cast<int>(env->CallStaticIntMethod(IDCache::GetNativeLibraryClass(),
                                                     IDCache::GetAlertPromptButton()));
}

static Core::System::ResultStatus RunCitra(const std::string& filepath) {
    // Citra core only supports a single running instance
    std::lock_guard<std::mutex> lock(running_mutex);

    LOG_INFO(Frontend, "Citra is Starting");

    MicroProfileOnThreadCreate("EmuThread");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return Core::System::ResultStatus::ErrorLoader;
    }

    window = std::make_unique<EmuWindow_Android>(s_surf);

    Core::System& system{Core::System::GetInstance()};

    // Forces a config reload on game boot, if the user changed settings in the UI
    Config{};
    Settings::Apply();

    // Register frontend applets
    Frontend::RegisterDefaultApplets();
    system.RegisterSoftwareKeyboard(std::make_shared<SoftwareKeyboard::AndroidKeyboard>());

    InputManager::Init();

    window->MakeCurrent();
    const Core::System::ResultStatus load_result{system.Load(*window, filepath)};
    if (load_result != Core::System::ResultStatus::Success) {
        return load_result;
    }

    auto& telemetry_session = Core::System::GetInstance().TelemetrySession();
    telemetry_session.AddField(Telemetry::FieldType::App, "Frontend", "SDL");

    is_running = true;
    pause_emulation = false;

    window->StartPresenting();
    while (is_running) {
        if (!pause_emulation) {
            system.RunLoop();
        } else {
            // Ensure no audio bleeds out while game is paused
            const float volume = Settings::values.volume;
            SCOPE_EXIT({ Settings::values.volume = volume; });
            Settings::values.volume = 0;

            std::unique_lock<std::mutex> pause_lock(paused_mutex);
            running_cv.wait(pause_lock, [] { return !pause_emulation || !is_running; });
        }
    }
    window->StopPresenting();

    system.Shutdown();
    window.reset();
    InputManager::Shutdown();
    MicroProfileShutdown();

    return Core::System::ResultStatus::Success;
}

extern "C" {

void Java_org_citra_citra_1emu_NativeLibrary_SurfaceChanged(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz,
                                                            jobject surf) {
    s_surf = ANativeWindow_fromSurface(env, surf);

    if (window) {
        window->OnSurfaceChanged(s_surf);
    }

    LOG_INFO(Frontend, "Surface changed");
}

void Java_org_citra_citra_1emu_NativeLibrary_SurfaceDestroyed(JNIEnv* env,
                                                              [[maybe_unused]] [
                                                                  [maybe_unused]] jclass clazz) {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
    if (window) {
        window->OnSurfaceChanged(s_surf);
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_NotifyOrientationChange(JNIEnv* env,
                                                                     [[maybe_unused]] jclass clazz,
                                                                     jint layout_option,
                                                                     jint rotation) {
    Settings::values.layout_option = static_cast<Settings::LayoutOption>(layout_option);
    VideoCore::g_renderer->UpdateCurrentFramebufferLayout(!(rotation % 2));
    InputManager::screen_rotation = rotation;
}

void Java_org_citra_citra_1emu_NativeLibrary_SwapScreens(JNIEnv* env, [[maybe_unused]] jclass clazz,
                                                         jboolean swap_screens, jint rotation) {
    Settings::values.swap_screen = swap_screens;
    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout(!(rotation % 2));
    }
    InputManager::screen_rotation = rotation;
}

void Java_org_citra_citra_1emu_NativeLibrary_SetUserDirectory(JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz,
                                                              jstring j_directory) {
    FileUtil::SetCurrentDir(GetJString(env, j_directory));
}

void Java_org_citra_citra_1emu_NativeLibrary_UnPauseEmulation(JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz) {
    pause_emulation = false;
    running_cv.notify_all();
}

void Java_org_citra_citra_1emu_NativeLibrary_PauseEmulation(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    pause_emulation = true;
}

void Java_org_citra_citra_1emu_NativeLibrary_StopEmulation(JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz) {
    is_running = false;
    pause_emulation = false;
    running_cv.notify_all();
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_IsRunning(JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz) {
    return static_cast<jboolean>(is_running);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadEvent(JNIEnv* env,
                                                                [[maybe_unused]] jclass clazz,
                                                                jstring j_device, jint j_button,
                                                                jint action) {
    bool consumed{};
    if (action) {
        consumed = InputManager::ButtonHandler()->PressKey(j_button);
    } else {
        consumed = InputManager::ButtonHandler()->ReleaseKey(j_button);
    }

    return static_cast<jboolean>(consumed);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadMoveEvent(JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    jstring j_device, jint axis,
                                                                    jfloat x, jfloat y) {
    // Clamp joystick movement to supported minimum and maximum
    // Citra uses an inverted y axis sent by the frontend
    x = std::clamp(x, -1.f, 1.f);
    y = std::clamp(-y, -1.f, 1.f);
    return static_cast<jboolean>(InputManager::AnalogHandler()->MoveJoystick(axis, x, y));
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadAxisEvent(JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    jstring j_device, jint axis_id,
                                                                    jfloat axis_val) {
    return static_cast<jboolean>(
        InputManager::ButtonHandler()->AnalogButtonEvent(axis_id, axis_val));
}

void Java_org_citra_citra_1emu_NativeLibrary_onTouchEvent(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz, jfloat x,
                                                          jfloat y, jboolean pressed) {
    window->OnTouchEvent((int)x, (int)y, (bool)pressed);
}

void Java_org_citra_citra_1emu_NativeLibrary_onTouchMoved(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz, jfloat x,
                                                          jfloat y) {
    window->OnTouchMoved((int)x, (int)y);
}

jintArray Java_org_citra_citra_1emu_NativeLibrary_GetIcon(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jstring j_file) {
    std::string filepath = GetJString(env, j_file);

    std::vector<u16> icon_data = GameInfo::GetIcon(filepath);
    if (icon_data.size() == 0) {
        return 0;
    }

    jintArray icon = env->NewIntArray(static_cast<jsize>(icon_data.size()));
    env->SetIntArrayRegion(icon, 0, env->GetArrayLength(icon),
                           reinterpret_cast<jint*>(icon_data.data()));

    return icon;
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetTitle(JNIEnv* env, [[maybe_unused]] jclass clazz,
                                                         jstring j_filename) {
    std::string filepath = GetJString(env, j_filename);

    char16_t* Title = GameInfo::GetTitle(filepath);

    if (!Title) {
        return env->NewStringUTF("");
    }

    return env->NewStringUTF(Common::UTF16ToUTF8(Title).data());
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetDescription(JNIEnv* env,
                                                               [[maybe_unused]] jclass clazz,
                                                               jstring j_filename) {
    return j_filename;
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetGameId(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jstring j_filename) {
    return j_filename;
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetCompany(JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz,
                                                           jstring j_filename) {
    std::string filepath = GetJString(env, j_filename);

    char16_t* publisher = GameInfo::GetPublisher(filepath);

    if (!publisher) {
        return nullptr;
    }

    return env->NewStringUTF(Common::UTF16ToUTF8(publisher).data());
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetGitRevision(JNIEnv* env,
                                                               [[maybe_unused]] jclass clazz) {
    return nullptr;
}

void Java_org_citra_citra_1emu_NativeLibrary_CreateConfigFile(JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz) {
    Config{};
}

jint Java_org_citra_citra_1emu_NativeLibrary_DefaultCPUCore(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    return 0;
}

void Java_org_citra_citra_1emu_NativeLibrary_Run__Ljava_lang_String_2Ljava_lang_String_2Z(
    JNIEnv* env, [[maybe_unused]] jclass clazz, jstring j_file, jstring j_savestate,
    jboolean j_delete_savestate) {}

void Java_org_citra_citra_1emu_NativeLibrary_ReloadSettings(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    Config{};
    Settings::Apply();
}

jstring Java_org_citra_citra_1emu_NativeLibrary_GetUserSetting(JNIEnv* env,
                                                               [[maybe_unused]] jclass clazz,
                                                               jstring j_game_id, jstring j_section,
                                                               jstring j_key) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);

    // TODO

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());

    return env->NewStringUTF("");
}

void Java_org_citra_citra_1emu_NativeLibrary_SetUserSetting(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz,
                                                            jstring j_game_id, jstring j_section,
                                                            jstring j_key, jstring j_value) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);
    std::string_view value = env->GetStringUTFChars(j_value, 0);

    // TODO

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());
    env->ReleaseStringUTFChars(j_value, value.data());
}

void Java_org_citra_citra_1emu_NativeLibrary_InitGameIni(JNIEnv* env, [[maybe_unused]] jclass clazz,
                                                         jstring j_game_id) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);

    // TODO

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
}

jdoubleArray Java_org_citra_citra_1emu_NativeLibrary_GetPerfStats(JNIEnv* env,
                                                                  [[maybe_unused]] jclass clazz) {
    auto& core = Core::System::GetInstance();
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (core.IsPoweredOn()) {
        auto results = core.GetAndResetPerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

void Java_org_citra_citra_1emu_utils_DirectoryInitialization_SetSysDirectory(
    JNIEnv* env, [[maybe_unused]] jclass clazz, jstring j_path) {
    std::string_view path = env->GetStringUTFChars(j_path, 0);

    env->ReleaseStringUTFChars(j_path, path.data());
}

void Java_org_citra_citra_1emu_NativeLibrary_Run__Ljava_lang_String_2(JNIEnv* env,
                                                                      [[maybe_unused]] jclass clazz,
                                                                      jstring j_path) {
    const std::string path = GetJString(env, j_path);

    if (is_running) {
        is_running = false;
        running_cv.notify_all();
    }

    const Core::System::ResultStatus result{RunCitra(path)};
    if (result != Core::System::ResultStatus::Success) {
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetExitEmulationActivity(), static_cast<int>(result));
    }
}

jobjectArray Java_org_citra_citra_1emu_NativeLibrary_GetTextureFilterNames(JNIEnv* env,
                                                                           jclass clazz) {
    auto names = OpenGL::TextureFilterer::GetFilterNames();
    jobjectArray ret = (jobjectArray)env->NewObjectArray(static_cast<jsize>(names.size()),
                                                         env->FindClass("java/lang/String"),
                                                         env->NewStringUTF(""));
    for (jsize i = 0; i < names.size(); ++i)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(names[i].data()));
    return ret;
}

} // extern "C"
