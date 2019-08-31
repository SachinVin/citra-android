package org.citra.citra_android.utils;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import org.citra.citra_android.CitraApplication;

public class EmulationMenuSettings {
    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(CitraApplication.getAppContext());

    // These must match what is defined in src/core/settings.h
    public static final int LayoutOption_Default = 0;
    public static final int LayoutOption_SingleScreen = 1;
    public static final int LayoutOption_LargeScreen = 2;
    public static final int LayoutOption_SideScreen = 3;
    public static final int LayoutOption_MobilePortrait = 4;
    public static final int LayoutOption_MobileLandscape = 5;

    public static int getLandscapeScreenLayout() {
        return mPreferences.getInt("EmulationMenuSettings_LandscapeScreenLayout", LayoutOption_MobileLandscape);
    }

    public static void setLandscapeScreenLayout(int value) {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt("EmulationMenuSettings_LandscapeScreenLayout", value);
        editor.apply();
    }

    public static boolean getShowFps() {
        return mPreferences.getBoolean("EmulationMenuSettings_ShowFps", false);
    }

    public static void setShowFps(boolean value) {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean("EmulationMenuSettings_ShowFps", value);
        editor.apply();
    }

    public static boolean getSwapScreens() {
        return mPreferences.getBoolean("EmulationMenuSettings_SwapScreens", false);
    }

    public static void setSwapScreens(boolean value) {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean("EmulationMenuSettings_SwapScreens", value);
        editor.apply();
    }

    public static boolean getShowOverlay() {
        return mPreferences.getBoolean("EmulationMenuSettings_ShowOverylay", true);
    }

    public static void setShowOverlay(boolean value) {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean("EmulationMenuSettings_ShowOverylay", value);
        editor.apply();
    }
}
