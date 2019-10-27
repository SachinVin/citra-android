package org.citra.citra_android.ui.settings.viewholder;

import android.content.res.Resources;
import android.view.View;
import android.widget.TextView;

import org.citra.citra_android.R;
import org.citra.citra_android.model.settings.view.SettingsItem;
import org.citra.citra_android.model.settings.view.SingleChoiceSetting;
import org.citra.citra_android.ui.settings.SettingsAdapter;

public final class SingleChoiceViewHolder extends SettingViewHolder {
    private SettingsItem mItem;

    private TextView mTextSettingName;
    private TextView mTextSettingDescription;

    public SingleChoiceViewHolder(View itemView, SettingsAdapter adapter) {
        super(itemView, adapter);
    }

    @Override
    protected void findViews(View root) {
        mTextSettingName = (TextView) root.findViewById(R.id.text_setting_name);
        mTextSettingDescription = (TextView) root.findViewById(R.id.text_setting_description);
    }

    @Override
    public void bind(SettingsItem item) {
        mItem = item;

        mTextSettingName.setText(item.getNameId());
        mTextSettingDescription.setVisibility(View.VISIBLE);
        if (item.getDescriptionId() > 0) {
            mTextSettingDescription.setText(item.getDescriptionId());
        } else if (item instanceof SingleChoiceSetting) {
            SingleChoiceSetting setting = (SingleChoiceSetting) item;
            int selected = setting.getSelectedValue();
            Resources resMgr = mTextSettingDescription.getContext().getResources();
            String[] choices = resMgr.getStringArray(setting.getChoicesId());
            int[] values = resMgr.getIntArray(setting.getValuesId());
            for (int i = 0; i < values.length; ++i) {
                if (values[i] == selected) {
                    mTextSettingDescription.setText(choices[i]);
                }
            }
        } else {
            mTextSettingDescription.setVisibility(View.GONE);
        }
    }

    @Override
    public void onClick(View clicked) {
        int position = getAdapterPosition();
        if (mItem instanceof SingleChoiceSetting) {
            getAdapter().onSingleChoiceClick((SingleChoiceSetting) mItem, position);
        }
    }
}
