package org.citra.citra_emu.features.settings.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

/**
 * FrameLayout subclass with few Properties added to simplify animations.
 */
public final class SettingsFrameLayout extends FrameLayout {

    public SettingsFrameLayout(Context context) {
        super(context);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public SettingsFrameLayout(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }
}
