package org.citra.citra_android.ui.settings;

import android.text.TextUtils;

import org.citra.citra_android.R;
import org.citra.citra_android.model.settings.Setting;
import org.citra.citra_android.model.settings.SettingSection;
import org.citra.citra_android.model.settings.view.CheckBoxSetting;
import org.citra.citra_android.model.settings.view.DateTimeSetting;
import org.citra.citra_android.model.settings.view.HeaderSetting;
import org.citra.citra_android.model.settings.view.InputBindingSetting;
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
            case SettingsFile.SECTION_CONTROLS:
                addInputSettings(sl);
                break;
            case SettingsFile.SECTION_RENDERER:
                addGraphicsSettings(sl);
                break;
            case SettingsFile.SECTION_AUDIO:
                addAudioSettings(sl);
                break;
            default:
                mView.showToastMessage("Unimplemented menu", false);
                return;
        }

        mSettingsList = sl;
        mView.showSettingsList(mSettingsList);
    }

    private void addConfigSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_settings);

        sl.add(new SubmenuSetting(null, null, R.string.preferences_general, 0, SettingsFile.SECTION_CORE));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_system, 0, SettingsFile.SECTION_SYSTEM));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_controls, 0, SettingsFile.SECTION_CONTROLS));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_graphics, 0, SettingsFile.SECTION_RENDERER));
        sl.add(new SubmenuSetting(null, null, R.string.preferences_audio, 0, SettingsFile.SECTION_AUDIO));
    }

    private void addGeneralSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_general);

        Setting useCpuJit = null;
        Setting frameLimitEnable = null;
        Setting frameLimitValue = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            useCpuJit = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_CPU_JIT);
            frameLimitEnable = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_FRAME_LIMIT_ENABLED);
            frameLimitValue = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_FRAME_LIMIT);

        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new CheckBoxSetting(SettingsFile.KEY_CPU_JIT, SettingsFile.SECTION_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.cpu_jit, R.string.cpu_jit_description, true, useCpuJit, true, mView));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_FRAME_LIMIT_ENABLED, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.frame_limit_enable, R.string.frame_limit_enable_description, true, frameLimitEnable));
        sl.add(new SliderSetting(SettingsFile.KEY_FRAME_LIMIT, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.frame_limit_slider, R.string.frame_limit_slider_description, 0, 200, "%", 100, frameLimitValue));
    }

    private void addSystemSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_system);

        Setting region = null;
        Setting language = null;
        Setting systemClock = null;
        Setting dateTime = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            region = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_REGION_VALUE);
            language = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_LANGUAGE);
            systemClock = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_INIT_CLOCK);
            dateTime = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_SYSTEM).getSetting(SettingsFile.KEY_INIT_TIME);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new SingleChoiceSetting(SettingsFile.KEY_REGION_VALUE, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.emulated_region, 0, R.array.regionNames, R.array.regionValues, -1, region));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_LANGUAGE, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.emulated_language, 0, R.array.languageNames, R.array.languageValues, 1, language));
        sl.add(new SingleChoiceSetting(SettingsFile.KEY_INIT_CLOCK, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.init_clock, R.string.init_clock_description, R.array.systemClockNames, R.array.systemClockValues, 0, systemClock));
        sl.add(new DateTimeSetting(SettingsFile.KEY_INIT_TIME, SettingsFile.SECTION_SYSTEM, SettingsFile.SETTINGS_DOLPHIN, R.string.init_time, R.string.init_time_description, "2000-01-01 00:00:01", dateTime));
    }


    private void addInputSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_controls);

        Setting buttonA = null;
        Setting buttonB = null;
        Setting buttonX = null;
        Setting buttonY = null;
        Setting buttonSelect = null;
        Setting buttonStart = null;
        Setting circlepadAxisVert = null;
        Setting circlepadAxisHoriz = null;
        Setting cstickAxisVert = null;
        Setting cstickAxisHoriz = null;
        Setting dpadAxisVert = null;
        Setting dpadAxisHoriz = null;
        // Setting buttonUp = null;
        // Setting buttonDown = null;
        // Setting buttonLeft = null;
        // Setting buttonRight = null;
        Setting buttonL = null;
        Setting buttonR = null;
        Setting buttonZL = null;
        Setting buttonZR = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            buttonA = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_A);
            buttonB = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_B);
            buttonX = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_X);
            buttonY = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_Y);
            buttonSelect = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_SELECT);
            buttonStart = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_START);
            circlepadAxisVert = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_CIRCLEPAD_AXIS_VERTICAL);
            circlepadAxisHoriz = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_CIRCLEPAD_AXIS_HORIZONTAL);
            cstickAxisVert = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_CSTICK_AXIS_VERTICAL);
            cstickAxisHoriz = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_CSTICK_AXIS_HORIZONTAL);
            dpadAxisVert = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_DPAD_AXIS_VERTICAL);
            dpadAxisHoriz = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_DPAD_AXIS_HORIZONTAL);
            // buttonUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_UP);
            // buttonDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_DOWN);
            // buttonLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_LEFT);
            // buttonRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_RIGHT);
            buttonL = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_L);
            buttonR = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_R);
            buttonZL = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_ZL);
            buttonZR = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_CONTROLS).getSetting(SettingsFile.KEY_BUTTON_ZR);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_A, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_a, buttonA));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_B, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_b, buttonB));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_X, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_x, buttonX));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_Y, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_y, buttonY));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_SELECT, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_select, buttonSelect));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_START, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_start, buttonStart));

        sl.add(new HeaderSetting(null, null, R.string.controller_circlepad, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_CIRCLEPAD_AXIS_VERTICAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_vertical, circlepadAxisVert));
        sl.add(new InputBindingSetting(SettingsFile.KEY_CIRCLEPAD_AXIS_HORIZONTAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_horizontal, circlepadAxisHoriz));

        sl.add(new HeaderSetting(null, null, R.string.controller_c, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_CSTICK_AXIS_VERTICAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_vertical, cstickAxisVert));
        sl.add(new InputBindingSetting(SettingsFile.KEY_CSTICK_AXIS_HORIZONTAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_horizontal, cstickAxisHoriz));

        sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_DPAD_AXIS_VERTICAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_vertical, dpadAxisVert));
        sl.add(new InputBindingSetting(SettingsFile.KEY_DPAD_AXIS_HORIZONTAL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_axis_horizontal, dpadAxisHoriz));

        // TODO(bunnei): Figure out what to do with these. Configuring is functional, but removing for MVP because they are confusing.
        // sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_UP, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, buttonUp));
        // sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_DOWN, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, buttonDown));
        // sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_LEFT, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, buttonLeft));
        // sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_RIGHT, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, buttonRight));

        sl.add(new HeaderSetting(null, null, R.string.controller_triggers, 0));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_L, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_l, buttonL));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_R, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_r, buttonR));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_ZL, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_zl, buttonZL));
        sl.add(new InputBindingSetting(SettingsFile.KEY_BUTTON_ZR, SettingsFile.SECTION_CONTROLS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_zr, buttonZR));
    }

    private void addGraphicsSettings(ArrayList<SettingsItem> sl) {
        mView.getActivity().setTitle(R.string.preferences_graphics);

        Setting hardwareRenderer = null;
        Setting hardwareShader = null;
        Setting shadersAccurateMul = null;
        Setting resolutionFactor = null;
        Setting vsyncEnable = null;
        Setting filterMode = null;

        if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty()) {
            hardwareRenderer = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_HW_RENDERER);
            hardwareShader = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_HW_SHADER);
            shadersAccurateMul = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_SHADERS_ACCURATE_MUL);
            resolutionFactor = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_RESOLUTION_FACTOR);
            vsyncEnable = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_USE_VSYNC);
            filterMode = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_RENDERER).getSetting(SettingsFile.KEY_FILTER_MODE);
        } else {
            mView.passSettingsToActivity(mSettings);
        }

        sl.add(new CheckBoxSetting(SettingsFile.KEY_HW_RENDERER, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.hw_renderer, R.string.hw_renderer_description, true, hardwareRenderer, true, mView));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_HW_SHADER, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.hw_shaders, R.string.hw_shaders_description, true, hardwareShader, true, mView));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_SHADERS_ACCURATE_MUL, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.shaders_accurate_mul, R.string.shaders_accurate_mul_description, false, shadersAccurateMul));
        sl.add(new SliderSetting(SettingsFile.KEY_RESOLUTION_FACTOR, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.internal_resolution, R.string.internal_resolution_description, 1, 4, "x", 1, resolutionFactor));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_USE_VSYNC, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.vsync, R.string.vsync_description, true, vsyncEnable));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_FILTER_MODE, SettingsFile.SECTION_RENDERER, SettingsFile.SETTINGS_DOLPHIN, R.string.linear_filtering, R.string.linear_filtering_description, true, filterMode));
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