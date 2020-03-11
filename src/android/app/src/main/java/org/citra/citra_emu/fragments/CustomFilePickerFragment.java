package org.citra.citra_emu.fragments;

import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.v4.content.FileProvider;
import android.support.v7.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.nononsenseapps.filepicker.FilePickerFragment;

import org.citra.citra_emu.R;

import java.io.File;

public class CustomFilePickerFragment extends FilePickerFragment {
    private int mTitle;

    @NonNull
    @Override
    public Uri toUri(@NonNull final File file) {
        return FileProvider
                .getUriForFile(getContext(),
                        getContext().getApplicationContext().getPackageName() + ".filesprovider",
                        file);
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
}
