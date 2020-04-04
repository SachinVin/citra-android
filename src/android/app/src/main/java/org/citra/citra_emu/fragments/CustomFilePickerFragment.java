package org.citra.citra_emu.fragments;

import android.net.Uri;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.Toolbar;
import androidx.core.content.FileProvider;

import com.nononsenseapps.filepicker.FilePickerFragment;

import org.citra.citra_emu.R;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class CustomFilePickerFragment extends FilePickerFragment {
    private int mTitle;
    private static final Set<String> extensions = new HashSet<>(Arrays.asList("elf", "axf", "cci", "3ds", "cxi", "app", "3dsx", "cia"));

    @NonNull
    @Override
    public Uri toUri(@NonNull final File file) {
        return FileProvider
                .getUriForFile(getContext(),
                        getContext().getApplicationContext().getPackageName() + ".filesprovider",
                        file);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        if (mode == MODE_DIR) {
            TextView ok = getActivity().findViewById(R.id.nnf_button_ok);
            ok.setText(R.string.select_dir);

            TextView cancel = getActivity().findViewById(R.id.nnf_button_cancel);
            cancel.setVisibility(View.GONE);
        }
    }

    @Override
    protected View inflateRootView(LayoutInflater inflater, ViewGroup container) {
        View view = super.inflateRootView(inflater, container);
        if (mTitle != 0) {
            Toolbar toolbar = view.findViewById(com.nononsenseapps.filepicker.R.id.nnf_picker_toolbar);
            ViewGroup parent = (ViewGroup) toolbar.getParent();
            int index = parent.indexOfChild(toolbar);
            View newToolbar = inflater.inflate(R.layout.filepicker_toolbar, toolbar, false);
            TextView title = newToolbar.findViewById(R.id.filepicker_title);
            title.setText(mTitle);
            parent.removeView(toolbar);
            parent.addView(newToolbar, index);
        }
        return view;
    }

    public void setTitle(int title) {
        mTitle = title;
    }


    @Override
    protected boolean isItemVisible(@NonNull final File file) {
        // Some users jump to the conclusion that Dolphin isn't able to detect their
        // files if the files don't show up in the file picker when mode == MODE_DIR.
        // To avoid this, show files even when the user needs to select a directory.
        return (showHiddenItems || !file.isHidden()) &&
                (file.isDirectory() ||
                        extensions.contains(fileExtension(file.getName()).toLowerCase()));
    }

    @Override
    public boolean isCheckable(@NonNull final File file) {
        // We need to make a small correction to the isCheckable logic due to
        // overriding isItemVisible to show files when mode == MODE_DIR.
        // AbstractFilePickerFragment always treats files as checkable when
        // allowExistingFile == true, but we don't want files to be checkable when mode == MODE_DIR.
        return super.isCheckable(file) && !(mode == MODE_DIR && file.isFile());
    }

    private static String fileExtension(@NonNull String filename) {
        int i = filename.lastIndexOf('.');
        return i < 0 ? "" : filename.substring(i + 1);
    }
}
