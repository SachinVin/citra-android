package org.citra.citra_android.ui.settings;

import android.text.TextUtils;

import org.citra.citra_android.R;
import org.citra.citra_android.model.settings.Setting;
import org.citra.citra_android.model.settings.SettingSection;
import org.citra.citra_android.model.settings.view.CheckBoxSetting;
import org.citra.citra_android.model.settings.view.DateTimeSetting;
import org.citra.citra_android.model.settings.view.SettingsItem;
import org.citra.citra_android.model.settings.view.SingleChoiceSetting;
import org.citra.citra_android.model.settings.view.SliderSetting;
import org.citra.citra_android.model.settings.view.SubmenuSetting;
import org.citra.citra_android.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsFragmentPresenter {
    private SettingsFragmentView mView;

    private String mMenuTag;
    private String mGameID;

    private ArrayList<HashMap<String, SettingSection>> mSettings;
    private ArrayList<SettingsItem> mSettingsList;

    public SettingsFragmentPresenter(SettingsFragmentView view) {
        mView = view;
    }

    public void onCreate(String menuTag, String gameId) {
        mGameID = gameId;
        mMenuTag = menuTag;

    }

    public void onViewCreated(ArrayList<HashMap<String, SettingSection>> settings) {
        setSettings(settings);
    }

    /**
     * If the screen is rotated, the Activity will forget the settings map. This fragment
     * won't, though; so rather than have the Activity reload from disk, have the fragment pass
     * the settings map back to the Activity.
     */
    public void onAttach() {
        if (mSettings != null) {
            mView.passSettingsToActivity(mSettings);
        }
    }

    public void putSetting(Setting setting) {
        mSettings.get(setting.getFile()).get(setting.getSection()).putSetting(setting);
    }

    public void loadDefaultSettings() {
        loadSettingsList();
    }

    public void setSettings(ArrayList<HashMap<String, SettingSection>> settings) {
        if (mSettingsList == null && settings != null) {
            mSettings = settings;

            loadSettingsList();
        } else {
            mView.getActivity().setTitle(R.string.preferences_settings);
            mView.showSettingsList(mSettingsList);
        }
    }

    private void loadSettingsList() {
        if (!TextUtils.isEmpty(mGameID)) {
            mView.getActivity().setTitle("Game Settings: " + mGameID);
        }
        ArrayList<SettingsItem> sl = new ArrayList<>();

        switch (mMenuTag) {
            case SettingsFile.FILE_NAME_CONFIG:
                addConfigSettings(sl);
                break;
            case SettingsFile.SECTION_CORE:
                addGeneralSettings(sl);
                break;
            case SettingsFile.SECTION_SYSTEM:
                addSystemSettings(sl);
                break;
            case SettingsFile.SECTION_RENDERER:
                addGraphicsSettings(sl);
                break;
            case SettingsFile.SECTION_AUDIO:
                addAudioSettings(sl);
                break;
            default:
                mView.showToastMessage("Unimplemented menu");
                return;
        }

        mSettingsList = sl;
        mView.showSettingsList(mSettingsList);
    }

    private void addConfigSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_settings);

        sl.add(new SubmenuSetting(null, null, R.string.preferences_general, 0, SettingsFile.SECTION_CORE));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_system, 0, SettingsFile.SECTION_SYSTEM));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_graphics, 0, SettingsFile.SECTION_RENDERER));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_audio, 0, SettingsFile.SECTION_AUDIO));
    }

    private void addGeneralSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_general);

        Setting useCpuJit = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            useCpuJit = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_CPU_JIT);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new CheckBoxSetting(SettingsFile.KEY_CPU_JIT, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.cpu_jit, 0, true, useCpuJit));
    }

    private void addSystemSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_system);

        Setting region = null;
        Setting systemClock = null;
        Setting dateTime = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            region = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_REGION_VALUE);
            systemClock = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_INIT_CLOCK);
            dateTime = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_INIT_TIME);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new SingleChoiceSetting(SettingsFile.KEY_REGION_VALUE, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.region, 0, R.array.regionNames, R.array.regionValues, -1, region));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_INIT_CLOCK, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.init_clock, R.string.init_clock_descrip, R.array.systemClockNames, R.array.systemClockValues, 0, systemClock));
        sl.add(new DateTimeSetting(SettingsFile.KEY_INIT_TIME, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.init_time, R.string.init_time_descrip, "2000-01-01 00:00:01", dateTime));
    }

    private void addGraphicsSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_graphics);

        Setting hardwareRenderer = null;
        Setting hardwareShader = null;
        Setting shadersAccurateMul = null;
        Setting shadersAccurateGs = null;
        Setting resolutionFactor = null;
        Setting vsyncEnable = null;
        Setting frameLimitEnable = null;
        Setting frameLimitValue = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            hardwareRenderer = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_HW_RENDERER);
            hardwareShader = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_HW_SHADER);
            shadersAccurateMul = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_SHADERS_ACCURATE_MUL);
            shadersAccurateGs = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_SHADERS_ACCURATE_GS);
            resolutionFactor = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_RESOLUTION_FACTOR);
            vsyncEnable = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_USE_VSYNC);
            frameLimitEnable = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_FRAME_LIMIT_ENABLED);
            frameLimitValue = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_FRAME_LIMIT);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        if (mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new CheckBoxSetting(SettingsFile.KEY_HW_RENDERER, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.hw_renderer, 0, true, hardwareRenderer));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_HW_SHADER, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.hw_shaders, R.string.hw_shaders_descrip, true, hardwareShader));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_SHADERS_ACCURATE_MUL, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.shaders_accurate_mul, 0, false, shadersAccurateMul));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_SHADERS_ACCURATE_GS, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.shaders_accurate_gs, 0, false, shadersAccurateGs));
        sl.add(new SliderSetting(SettingsFile.KEY_RESOLUTION_FACTOR, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.internal_resolution, R.string.internal_resolution_descrip, 10, "x", 0, resolutionFactor));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_USE_VSYNC, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.vsync, 0, false, vsyncEnable));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_FRAME_LIMIT_ENABLED, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.frame_limit_enable, R.string.frame_limit_enable_description, false, frameLimitEnable));
        sl.add(new SliderSetting(SettingsFile.KEY_FRAME_LIMIT, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.frame_limit_slider, R.string.frame_limit_slider_description, 500, "%", 100, frameLimitValue));
    }

    private void addAudioSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_audio);

        Setting audioStretch = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            audioStretch = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_AUDIO).getSetting(SettingsFile.KEY_ENABLE_AUDIO_STRETCHING);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new CheckBoxSetting(SettingsFile.KEY_ENABLE_AUDIO_STRETCHING, SettingsFile.SECTION_AUDIO, SettingsFile.SETTINGS_DOLPHIN, R.string.audio_stretch, R.string.audio_stretch_description, false, audioStretch));
    }
}