// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <jni.h>

namespace IDCache {

JNIEnv* GetEnvForThread();
jclass GetNativeLibraryClass();
jmethodID GetDisplayAlertMsg();
jmethodID GetIsPortraitMode();
jmethodID GetLandscapeScreenLayout();

} // namespace IDCache
