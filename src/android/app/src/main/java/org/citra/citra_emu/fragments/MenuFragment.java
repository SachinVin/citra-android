package org.citra.citra_emu.fragments;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.citra.citra_emu.R;
import org.citra.citra_emu.activities.EmulationActivity;

public final class MenuFragment extends Fragment implements View.OnClickListener {
    private static final String KEY_TITLE = "title";
    private static SparseIntArray buttonsActionsMap = new SparseIntArray();

    static {
        buttonsActionsMap.append(R.id.menu_emulation_show_fps,
                EmulationActivity.MENU_ACTION_SHOW_FPS);
        buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
        buttonsActionsMap.append(R.id.menu_screen_layout_landscape,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE);
        buttonsActionsMap.append(R.id.menu_screen_layout_portrait,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_PORTRAIT);
        buttonsActionsMap.append(R.id.menu_screen_layout_single,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_SINGLE);
        buttonsActionsMap.append(R.id.menu_screen_layout_sidebyside,
                EmulationActivity.MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE);
        buttonsActionsMap.append(R.id.menu_emulation_swap_screens,
                EmulationActivity.MENU_ACTION_SWAP_SCREENS);
        buttonsActionsMap.append(R.id.menu_emulation_show_overlay,
                EmulationActivity.MENU_ACTION_SHOW_OVERLAY);
    }

    public static MenuFragment newInstance(String title) {
        MenuFragment fragment = new MenuFragment();

        Bundle arguments = new Bundle();
        arguments.putSerializable(KEY_TITLE, title);
        fragment.setArguments(arguments);

        return fragment;
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View rootView = inflater.inflate(R.layout.fragment_ingame_menu, container, false);

        LinearLayout options = rootView.findViewById(R.id.layout_options);
        for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++) {
            Button button = (Button) options.getChildAt(childIndex);

            button.setOnClickListener(this);
        }

        TextView titleText = rootView.findViewById(R.id.text_game_title);
        String title = getArguments().getString(KEY_TITLE);
        if (title != null) {
            titleText.setText(title);
        }

        return rootView;
    }

    @SuppressWarnings("WrongConstant")
    @Override
    public void onClick(View button) {
    }
}
