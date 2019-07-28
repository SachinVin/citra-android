package org.citra.citra_android.activities;

import android.content.Intent;
import android.os.Environment;
import android.support.annotation.Nullable;

import com.nononsenseapps.filepicker.AbstractFilePickerFragment;
import com.nononsenseapps.filepicker.FilePickerActivity;

import org.citra.citra_android.fragments.CustomFilePickerFragment;

import java.io.File;

public class CustomFilePickerActivity extends FilePickerActivity {
    public static final String EXTRA_TITLE = "filepicker.intent.TITLE";

    @Override
    protected AbstractFilePickerFragment<File> getFragment(
            @Nullable final String startPath, final int mode, final boolean allowMultiple,
            final boolean allowCreateDir, final boolean allowExistingFile,
            final boolean singleClick) {
        CustomFilePickerFragment fragment = new CustomFilePickerFragment();
        // startPath is allowed to be null. In that case, default folder should be SD-card and not "/"
        fragment.setArgs(
                startPath != null ? startPath : Environment.getExternalStorageDirectory().getPath(),
                mode, allowMultiple, allowCreateDir, allowExistingFile, singleClick);

        Intent intent = getIntent();
        String title = intent == null ? null : intent.getStringExtra(EXTRA_TITLE);
        fragment.setTitle(title);
        return fragment;
    }
}
