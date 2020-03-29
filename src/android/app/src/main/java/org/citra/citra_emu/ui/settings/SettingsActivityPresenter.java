package org.citra.citra_emu.ui.settings;

import android.content.IntentFilter;
import android.os.Bundle;
import android.text.TextUtils;

import org.citra.citra_emu.NativeLibrary;
import org.citra.citra_emu.model.settings.SettingSection;
import org.citra.citra_emu.utils.DirectoryInitialization;
import org.citra.citra_emu.utils.DirectoryInitialization.DirectoryInitializationState;
import org.citra.citra_emu.utils.DirectoryStateReceiver;
import org.citra.citra_emu.utils.Log;
import org.citra.citra_emu.utils.SettingsFile;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsActivityPresenter {
    private static final String KEY_SHOULD_SAVE = "should_save";

    private SettingsActivityView mView;

    private ArrayList<HashMap<String, SettingSection>> mSettings = new ArrayList<>();

    private int mStackCount;

    private boolean mShouldSave;

    private DirectoryStateReceiver directoryStateReceiver;

    private String menuTag;
    private String gameId;

    public SettingsActivityPresenter(SettingsActivityView view) {
        mView = view;
    }

    public void onCreate(Bundle savedInstanceState, String menuTag, String gameId) {
        if (savedInstanceState == null) {
            this.menuTag = menuTag;
            this.gameId = gameId;
        } else {
            mShouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE);
        }
    }

    public void onStart() {
        prepareCitraDirectoriesIfNeeded();
    }

    void loadSettingsUI() {
        if (mSettings.isEmpty()) {
            if (!TextUtils.isEmpty(gameId)) {
                mSettings.add(SettingsFile.SETTINGS_CITRA, SettingsFile.readFile("../GameSettings/" + gameId, mView));
            } else {
                mSettings.add(SettingsFile.SETTINGS_CITRA, SettingsFile.readFile(SettingsFile.FILE_NAME_CONFIG, mView));
            }
        }

        mView.showSettingsFragment(menuTag, false, gameId);
        mView.onSettingsFileLoaded(mSettings);
    }

    private void prepareCitraDirectoriesIfNeeded() {
        File configFile = new File(DirectoryInitialization.getUserDirectory() + "/config/" + SettingsFile.FILE_NAME_CONFIG + ".ini");
        if (!configFile.exists()) {

        }
        if (DirectoryInitialization.areCitraDirectoriesReady()) {
            loadSettingsUI();
        } else {
            mView.showLoading();
            IntentFilter statusIntentFilter = new IntentFilter(
                    DirectoryInitialization.BROADCAST_ACTION);

            directoryStateReceiver =
                    new DirectoryStateReceiver(directoryInitializationState ->
                    {
                        if (directoryInitializationState == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
                            mView.hideLoading();
                            loadSettingsUI();
                        } else if (directoryInitializationState == DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED) {
                            mView.showPermissionNeededHint();
                            mView.hideLoading();
                        } else if (directoryInitializationState == DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE) {
                            mView.showExternalStorageNotMountedHint();
                            mView.hideLoading();
                        }
                    });

            mView.startDirectoryInitializationService(directoryStateReceiver, statusIntentFilter);
        }
    }

    public void setSettings(ArrayList<HashMap<String, SettingSection>> settings) {
        mSettings = settings;
    }

    public HashMap<String, SettingSection> getSettings(int file) {
        return mSettings.get(file);
    }

    public void onStop(boolean finishing) {
        if (directoryStateReceiver != null) {
            mView.stopListeningToDirectoryInitializationService(directoryStateReceiver);
            directoryStateReceiver = null;
        }

        if (mSettings != null && finishing && mShouldSave) {
            if (!TextUtils.isEmpty(gameId)) {
                Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
                // Needed workaround for now due to an odd bug in how it handles saving two different settings sections to the same file. It won't save GFX settings if it follows the normal saving pattern
                if (menuTag.equals("Dolphin")) {
                    SettingsFile.saveFile("../GameSettings/" + gameId, mSettings.get(SettingsFile.SETTINGS_CITRA), mView);
                }
                mView.showToastMessage("Saved settings for " + gameId, false);
            } else {
                Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...");
                SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG, mSettings.get(SettingsFile.SETTINGS_CITRA), mView);
                mView.showToastMessage("Saved settings", false);
            }
            NativeLibrary.ReloadSettings();
        }
    }

    public void addToStack() {
        mStackCount++;
    }

    public void onBackPressed() {
        if (mStackCount > 0) {
            mView.popBackStack();
            mStackCount--;
        } else {
            mView.finish();
        }
    }

    public boolean handleOptionsItem(int itemId) {
        return false;
    }

    public void onSettingChanged() {
        mShouldSave = true;
    }

    public void saveState(Bundle outState) {
        outState.putBoolean(KEY_SHOULD_SAVE, mShouldSave);
    }
}
