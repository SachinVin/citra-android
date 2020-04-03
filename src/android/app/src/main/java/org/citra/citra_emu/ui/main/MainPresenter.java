package org.citra.citra_emu.ui.main;


import org.citra.citra_emu.BuildConfig;
import org.citra.citra_emu.CitraApplication;
import org.citra.citra_emu.R;
import org.citra.citra_emu.model.GameDatabase;
import org.citra.citra_emu.utils.AddDirectoryHelper;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

public final class MainPresenter {
    public static final int REQUEST_ADD_DIRECTORY = 1;

    private final MainView mView;
    private String mDirToAdd;

    public MainPresenter(MainView view) {
        mView = view;
    }

    public void onCreate() {
        String versionName = BuildConfig.VERSION_NAME;
        mView.setVersionString(versionName);
        refeshGameList();
    }

    public boolean handleOptionSelection(int itemId) {
        switch (itemId) {
            case R.id.menu_settings_core:
                mView.launchSettingsActivity(SettingsFile.FILE_NAME_CONFIG);
                return true;

            case R.id.button_add_directory:
                mView.launchFileListActivity();
                return true;
        }

        return false;
    }

    public void addDirIfNeeded(AddDirectoryHelper helper) {
        if (mDirToAdd != null) {
            helper.addDirectory(mDirToAdd, mView::refresh);

            mDirToAdd = null;
        }
    }

    public void onDirectorySelected(String dir) {
        mDirToAdd = dir;
    }

    private void refeshGameList() {
        GameDatabase databaseHelper = CitraApplication.databaseHelper;
        databaseHelper.scanLibrary(databaseHelper.getWritableDatabase());
        mView.refresh();
    }
}
