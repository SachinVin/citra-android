package org.citra.citra_android.activities;

import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.support.annotation.IntDef;
import android.support.v4.app.ActivityOptionsCompat;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;
import android.util.SparseIntArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.citra.citra_android.NativeLibrary;
import org.citra.citra_android.R;
import org.citra.citra_android.fragments.EmulationFragment;
import org.citra.citra_android.fragments.MenuFragment;
import org.citra.citra_android.model.settings.view.InputBindingSetting;
import org.citra.citra_android.ui.main.MainPresenter;
import org.citra.citra_android.utils.Animations;
import org.citra.citra_android.utils.ControllerMappingHelper;

import java.lang.annotation.Retention;
import java.util.List;

import static android.view.MotionEvent.AXIS_RZ;
import static android.view.MotionEvent.AXIS_X;
import static android.view.MotionEvent.AXIS_Y;
import static android.view.MotionEvent.AXIS_Z;
import static java.lang.annotation.RetentionPolicy.SOURCE;

public final class EmulationActivity extends AppCompatActivity {
    public static final int REQUEST_CHANGE_DISC = 1;
    public static final String EXTRA_SELECTED_GAME = "SelectedGame";
    public static final String EXTRA_SELECTED_TITLE = "SelectedTitle";
    public static final String EXTRA_SCREEN_PATH = "ScreenPath";
    public static final String EXTRA_GRID_POSITION = "GridPosition";
    public static final int MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0;
    public static final int MENU_ACTION_TOGGLE_CONTROLS = 1;
    public static final int MENU_ACTION_ADJUST_SCALE = 2;
    public static final int MENU_ACTION_EXIT = 3;
    public static final int MENU_ACTION_TOGGLE_PREF_STATS = 4;
    public static final int MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE = 5;
    public static final int MENU_ACTION_SCREEN_LAYOUT_PORTRAIT = 6;
    public static final int MENU_ACTION_SCREEN_LAYOUT_SINGLE = 7;
    public static final int MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE = 8;
    public static final int MENU_ACTION_SWAP_SCREENS = 9;
    public static final int MENU_ACTION_RESET_OVERLAY = 10;
    private static final String BACKSTACK_NAME_MENU = "menu";
    private static final String BACKSTACK_NAME_SUBMENU = "submenu";
    private static SparseIntArray buttonsActionsMap = new SparseIntArray();

    static {
        buttonsActionsMap.append(R.id.menu_emulation_edit_layout,
                EmulationActivity.MENU_ACTION_EDIT_CONTROLS_PLACEMENT);
        buttonsActionsMap.append(R.id.menu_emulation_toggle_controls,
                EmulationActivity.MENU_ACTION_TOGGLE_CONTROLS);
        buttonsActionsMap
                .append(R.id.menu_emulation_adjust_scale, EmulationActivity.MENU_ACTION_ADJUST_SCALE);
        buttonsActionsMap.append(R.id.menu_emulation_toggle_perf_stats,
                EmulationActivity.MENU_ACTION_TOGGLE_PREF_STATS);
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
        buttonsActionsMap
                .append(R.id.menu_emulation_reset_overlay, EmulationActivity.MENU_ACTION_RESET_OVERLAY);
    }

    private View mDecorView;
    private ImageView mImageView;
    private EmulationFragment mEmulationFragment;
    private SharedPreferences mPreferences;
    private ControllerMappingHelper mControllerMappingHelper;
    // So that MainActivity knows which view to invalidate before the return animation.
    private int mPosition;
    private boolean mDeviceHasTouchScreen;
    private boolean mMenuVisible;
    private boolean activityRecreated;
    private String mScreenPath;
    private String mSelectedTitle;
    private String mPath;
    private Runnable afterShowingScreenshot = new Runnable() {
        @Override
        public void run() {
            setResult(mPosition);
            supportFinishAfterTransition();
        }
    };

    public static void launch(FragmentActivity activity, String path, String title,
                              String screenshotPath, int position, View sharedView) {
        Intent launcher = new Intent(activity, EmulationActivity.class);

        launcher.putExtra(EXTRA_SELECTED_GAME, path);
        launcher.putExtra(EXTRA_SELECTED_TITLE, title);
        launcher.putExtra(EXTRA_SCREEN_PATH, screenshotPath);
        launcher.putExtra(EXTRA_GRID_POSITION, position);

        ActivityOptionsCompat options = ActivityOptionsCompat.makeSceneTransitionAnimation(
                activity,
                sharedView,
                "image_game_screenshot");

        // I believe this warning is a bug. Activities are FragmentActivity from the support lib
        //noinspection RestrictedApi
        activity.startActivityForResult(launcher, MainPresenter.REQUEST_EMULATE_GAME,
                options.toBundle());
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState == null) {
            // Get params we were passed
            Intent gameToEmulate = getIntent();
            mPath = gameToEmulate.getStringExtra(EXTRA_SELECTED_GAME);
            mSelectedTitle = gameToEmulate.getStringExtra(EXTRA_SELECTED_TITLE);
            mScreenPath = gameToEmulate.getStringExtra(EXTRA_SCREEN_PATH);
            mPosition = gameToEmulate.getIntExtra(EXTRA_GRID_POSITION, -1);
            activityRecreated = false;
        } else {
            activityRecreated = true;
            restoreState(savedInstanceState);
        }

        mDeviceHasTouchScreen = getPackageManager().hasSystemFeature("android.hardware.touchscreen");
        mControllerMappingHelper = new ControllerMappingHelper();

        int themeId;
        if (mDeviceHasTouchScreen) {
            themeId = R.style.CitraEmulation;

            // Get a handle to the Window containing the UI.
            mDecorView = getWindow().getDecorView();
            mDecorView.setOnSystemUiVisibilityChangeListener(visibility ->
            {
                if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                    // Go back to immersive fullscreen mode in 3s
                    Handler handler = new Handler(getMainLooper());
                    handler.postDelayed(this::enableFullscreenImmersive, 3000 /* 3s */);
                }
            });
            // Set these options now so that the SurfaceView the game renders into is the right size.
            enableFullscreenImmersive();
        } else {
            themeId = R.style.CitraEmulationTv;
        }

        setTheme(themeId);

        setContentView(R.layout.activity_emulation);

        mImageView = findViewById(R.id.image_screenshot);

        // Find or create the EmulationFragment
        mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
                .findFragmentById(R.id.frame_emulation_fragment);
        if (mEmulationFragment == null) {
            mEmulationFragment = EmulationFragment.newInstance(mPath);
            getSupportFragmentManager().beginTransaction()
                    .add(R.id.frame_emulation_fragment, mEmulationFragment)
                    .commit();
        }

        if (savedInstanceState == null) {
            // Picasso will take a while to load these big-ass screenshots. So don't run
            // the animation until we say so.
            postponeEnterTransition();

            Picasso.get()
                    .load(mScreenPath)
                    .noFade()
                    .noPlaceholder()
                    .into(mImageView, new Callback() {
                        @Override
                        public void onSuccess() {
                            supportStartPostponedEnterTransition();
                        }
                        @Override
                        public void onError(Exception ex) {
                            // Still have to do this, or else the app will crash.
                            supportStartPostponedEnterTransition();
                        }
                    });

            Animations.fadeViewOut(mImageView)
                    .setStartDelay(2000)
                    .withEndAction(() -> mImageView.setVisibility(View.GONE));
        } else {
            mImageView.setVisibility(View.GONE);
        }

        if (mDeviceHasTouchScreen) {
            setTitle(mSelectedTitle);
        }

        mPreferences = PreferenceManager.getDefaultSharedPreferences(this);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putString(EXTRA_SELECTED_GAME, mPath);
        outState.putString(EXTRA_SELECTED_TITLE, mSelectedTitle);
        outState.putString(EXTRA_SCREEN_PATH, mScreenPath);
        outState.putInt(EXTRA_GRID_POSITION, mPosition);
        super.onSaveInstanceState(outState);
    }

    protected void restoreState(Bundle savedInstanceState) {
        mPath = savedInstanceState.getString(EXTRA_SELECTED_GAME);
        mSelectedTitle = savedInstanceState.getString(EXTRA_SELECTED_TITLE);
        mScreenPath = savedInstanceState.getString(EXTRA_SCREEN_PATH);
        mPosition = savedInstanceState.getInt(EXTRA_GRID_POSITION);
    }

    @Override
    public void onBackPressed() {
        if (!mDeviceHasTouchScreen) {
            boolean popResult = getSupportFragmentManager().popBackStackImmediate(
                    BACKSTACK_NAME_SUBMENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);
            if (!popResult) {
                toggleMenu();
            }
        } else {
            NativeLibrary.PauseEmulation();
            new AlertDialog.Builder(this)
                    .setTitle(R.string.emulation_close_game)
                    .setMessage(R.string.emulation_close_game_message)
                    .setPositiveButton(android.R.string.yes, (dialogInterface, i) ->
                    {
                        mEmulationFragment.stopEmulation();
                        exitWithAnimation();
                    })
                    .setNegativeButton(android.R.string.cancel, (dialogInterface, i) ->
                    {
                    }).setOnDismissListener(dialogInterface ->
                    {
                        NativeLibrary.UnPauseEmulation();
                    })
                    .create()
                    .show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent result) {
    }

    private void enableFullscreenImmersive() {
        // It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
        mDecorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_IMMERSIVE);
    }

    private void toggleMenu() {
        boolean result = getSupportFragmentManager().popBackStackImmediate(
                BACKSTACK_NAME_MENU, FragmentManager.POP_BACK_STACK_INCLUSIVE);
        mMenuVisible = false;

        if (!result) {
            // Removing the menu failed, so that means it wasn't visible. Add it.
            Fragment fragment = MenuFragment.newInstance(mSelectedTitle);
            getSupportFragmentManager().beginTransaction()
                    .setCustomAnimations(
                            R.animator.menu_slide_in_from_left,
                            R.animator.menu_slide_out_to_left,
                            R.animator.menu_slide_in_from_left,
                            R.animator.menu_slide_out_to_left)
                    .add(R.id.frame_menu, fragment)
                    .addToBackStack(BACKSTACK_NAME_MENU)
                    .commit();
            mMenuVisible = true;
        }
    }

    public void exitWithAnimation() {
        runOnUiThread(() ->
        {
            Picasso.get()
                    .invalidate(mScreenPath);

            Picasso.get()
                    .load(mScreenPath)
                    .noFade()
                    .noPlaceholder()
                    .into(mImageView, new Callback() {
                        @Override
                        public void onSuccess() {
                            showScreenshot();
                        }

                        @Override
                        public void onError(Exception ex) {
                            finish();
                        }
                    });
        });
    }

    private void showScreenshot() {
        Animations.fadeViewIn(mImageView)
                .withEndAction(afterShowingScreenshot);
    }

    // These must match what is defined in src/core/settings.h
    public static final int LayoutOption_Default = 0;
    public static final int LayoutOption_SingleScreen = 1;
    public static final int LayoutOption_LargeScreen = 2;
    public static final int LayoutOption_SideScreen = 3;
    public static final int LayoutOption_MobilePortrait = 4;
    public static final int LayoutOption_MobileLandscape = 5;

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_emulation, menu);


        // mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        int layoutOption = mPreferences.getInt("LandscapeScreenLayout", LayoutOption_MobileLandscape);

        int menuItemId = R.id.menu_screen_layout_landscape;
        switch (layoutOption) {
            case LayoutOption_SingleScreen:
                menuItemId = R.id.menu_screen_layout_single;
                break;
            case LayoutOption_SideScreen:
                menuItemId = R.id.menu_screen_layout_sidebyside;
                break;
            case LayoutOption_MobilePortrait:
                menuItemId = R.id.menu_screen_layout_portrait;
                break;
        }

        menu.findItem(menuItemId).setChecked(true);

        return true;
    }

    @SuppressWarnings("WrongConstant")
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int action = buttonsActionsMap.get(item.getItemId(), -1);

        switch (action) {
            // Edit the placement of the controls
            case MENU_ACTION_EDIT_CONTROLS_PLACEMENT:
                editControlsPlacement();
                break;

            // Enable/Disable specific buttons or the entire input overlay.
            case MENU_ACTION_TOGGLE_CONTROLS:
                toggleControls();
                break;

            // Adjust the scale of the overlay controls.
            case MENU_ACTION_ADJUST_SCALE:
                adjustScale();
                break;

            // Toggle the visibility of the Performance stats TextView
            case MENU_ACTION_TOGGLE_PREF_STATS:
                mEmulationFragment.togglePerfStatsVisibility();
                break;

            // Sets the screen layout to Landscape
            case MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE:
                changeScreenOrientation(LayoutOption_MobileLandscape, item);
                break;

            // Sets the screen layout to Portrait
            case MENU_ACTION_SCREEN_LAYOUT_PORTRAIT:
                changeScreenOrientation(LayoutOption_MobilePortrait, item);
                break;

            // Sets the screen layout to Single
            case MENU_ACTION_SCREEN_LAYOUT_SINGLE:
                changeScreenOrientation(LayoutOption_SingleScreen, item);
                break;

            // Sets the screen layout to Side by Side
            case MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE:
                changeScreenOrientation(LayoutOption_SideScreen, item);
                break;

            // Swap the top and bottom screen locations
            case MENU_ACTION_SWAP_SCREENS:
                NativeLibrary.SwapScreens(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);
                break;

            // Reset overlay placement
            case MENU_ACTION_RESET_OVERLAY:
                resetOverlay();
                break;

            case MENU_ACTION_EXIT:
                toggleMenu();  // Hide the menu (it will be showing since we just clicked it)
                mEmulationFragment.stopEmulation();
                exitWithAnimation();
                break;
        }

        return true;
    }

    private void changeScreenOrientation(int layoutOption, MenuItem item) {
        item.setChecked(true);

        NativeLibrary.NotifyOrientationChange(layoutOption,
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);

        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt("LandscapeScreenLayout", layoutOption);
        editor.apply();
    }

    private void editControlsPlacement() {
        if (mEmulationFragment.isConfiguringControls()) {
            mEmulationFragment.stopConfiguringControls();
        } else {
            mEmulationFragment.startConfiguringControls();
        }
    }

    // Gets button presses
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mMenuVisible) {
            return super.dispatchKeyEvent(event);
        }

        int action;
        int button = mPreferences.getInt(InputBindingSetting.getInputButtonKey(event.getKeyCode()), event.getKeyCode());

        switch (event.getAction()) {
            case KeyEvent.ACTION_DOWN:
                // Handling the case where the back button is pressed.
                if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                    onBackPressed();
                    return true;
                }

                // Normal key events.
                action = NativeLibrary.ButtonState.PRESSED;
                break;
            case KeyEvent.ACTION_UP:
                action = NativeLibrary.ButtonState.RELEASED;
                break;
            default:
                return false;
        }
        InputDevice input = event.getDevice();
        return NativeLibrary.onGamePadEvent(input.getDescriptor(), button, action);
    }

    private void toggleControls() {
        final SharedPreferences.Editor editor = mPreferences.edit();
        boolean[] enabledButtons = new boolean[14];
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.emulation_toggle_controls);

        for (int i = 0; i < enabledButtons.length; i++) {
            // Buttons that are disabled by default
            boolean defaultValue = true;
            switch (i) {
                case 6: // ZL
                case 7: // ZR
                case 12: // C-stick
                    defaultValue = false;
                    break;
            }

            enabledButtons[i] = mPreferences.getBoolean("buttonToggle" + i, defaultValue);
        }
        builder.setMultiChoiceItems(R.array.n3dsButtons, enabledButtons,
                (dialog, indexSelected, isChecked) -> editor
                        .putBoolean("buttonToggle" + indexSelected, isChecked));

        builder.setPositiveButton(android.R.string.ok, (dialogInterface, i) ->
        {
            editor.apply();

            mEmulationFragment.refreshInputOverlay();
        });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

    private void adjustScale() {
        LayoutInflater inflater = LayoutInflater.from(this);
        View view = inflater.inflate(R.layout.dialog_seekbar, null);

        final SeekBar seekbar = view.findViewById(R.id.seekbar);
        final TextView value = view.findViewById(R.id.text_value);
        final TextView units = view.findViewById(R.id.text_units);

        seekbar.setMax(150);
        seekbar.setProgress(mPreferences.getInt("controlScale", 50));
        seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) { }

            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                value.setText(String.valueOf(progress + 50));
            }

            public void onStopTrackingTouch(SeekBar seekBar) { }
        });

        value.setText(String.valueOf(seekbar.getProgress() + 50));
        units.setText("%");

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.emulation_control_scale);
        builder.setView(view);
        builder.setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> { });
        builder.setPositiveButton(android.R.string.ok, (dialogInterface, i) ->
        {
            SharedPreferences.Editor editor = mPreferences.edit();
            editor.putInt("controlScale", seekbar.getProgress());
            editor.apply();
            mEmulationFragment.refreshInputOverlay();
        });
        builder.setNeutralButton(R.string.slider_default, (dialogInterface, i) -> {
            SharedPreferences.Editor editor = mPreferences.edit();
            editor.putInt("controlScale", 50);
            editor.apply();
            mEmulationFragment.refreshInputOverlay();
        });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

    private void resetOverlay() {
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.emulation_touch_overlay_reset))
                .setPositiveButton(android.R.string.yes, (dialogInterface, i) -> mEmulationFragment.resetInputOverlay())
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> { })
                .create()
                .show();
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (mMenuVisible) {
            return false;
        }

        if (((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == 0)) {
            return super.dispatchGenericMotionEvent(event);
        }

        // Don't attempt to do anything if we are disconnecting a device.
        if (event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            return true;
        }

        InputDevice input = event.getDevice();
        List<InputDevice.MotionRange> motions = input.getMotionRanges();

        float[] axisValuesCirclePad = {0.0f, 0.0f};
        float[] axisValuesCStick = {0.0f, 0.0f};
        float[] axisValuesDPad = {0.0f, 0.0f};
        boolean isTriggerPressedL = false;
        boolean isTriggerPressedR = false;
        boolean isTriggerPressedZL = false;
        boolean isTriggerPressedZR = false;

        for (InputDevice.MotionRange range : motions) {
            int axis = range.getAxis();
            float origValue = event.getAxisValue(axis);
            float value = mControllerMappingHelper.scaleAxis(input, axis, origValue);
            int nextMapping = mPreferences.getInt(InputBindingSetting.getInputAxisButtonKey(axis), -1);
            int guestOrientation = mPreferences.getInt(InputBindingSetting.getInputAxisOrientationKey(axis), -1);

            if (nextMapping == -1 || guestOrientation == -1) {
                // Axis is unmapped
                continue;
            }

            if ((value > 0.f && value < 0.1f) || (value < 0.f && value > -0.1f)) {
                // Skip joystick wobble
                value = 0.f;
            }

            if (nextMapping == NativeLibrary.ButtonType.STICK_LEFT) {
                axisValuesCirclePad[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.STICK_C) {
                axisValuesCStick[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.DPAD) {
                axisValuesDPad[guestOrientation] = value;
            } else if (nextMapping == NativeLibrary.ButtonType.TRIGGER_L && value != 0.f) {
                isTriggerPressedL = true;
            } else if (nextMapping == NativeLibrary.ButtonType.TRIGGER_R && value != 0.f) {
                isTriggerPressedR = true;
            } else if (nextMapping == NativeLibrary.ButtonType.BUTTON_ZL && value != 0.f) {
                isTriggerPressedZL = true;
            } else if (nextMapping == NativeLibrary.ButtonType.BUTTON_ZR && value != 0.f) {
                isTriggerPressedZR = true;
            }
        }

        // Circle-Pad and C-Stick status
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), NativeLibrary.ButtonType.STICK_LEFT, axisValuesCirclePad[0], axisValuesCirclePad[1]);
        NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), NativeLibrary.ButtonType.STICK_C, axisValuesCStick[0], axisValuesCStick[1]);

        // Triggers L/R and ZL/ZR
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.TRIGGER_L, isTriggerPressedL ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.TRIGGER_R, isTriggerPressedR ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.BUTTON_ZL, isTriggerPressedZL ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);
        NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.BUTTON_ZR, isTriggerPressedZR ? NativeLibrary.ButtonState.PRESSED : NativeLibrary.ButtonState.RELEASED);

        // Work-around to allow D-pad axis to be bound to emulated buttons
        if (axisValuesDPad[0] == 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.RELEASED);
        } if (axisValuesDPad[0] < 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.PRESSED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.RELEASED);
        } if (axisValuesDPad[0] > 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_LEFT, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_RIGHT, NativeLibrary.ButtonState.PRESSED);
        }
        if (axisValuesDPad[1] == 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.RELEASED);
        } if (axisValuesDPad[1] < 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.PRESSED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.RELEASED);
        } if (axisValuesDPad[1] > 0.f) {
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_UP, NativeLibrary.ButtonState.RELEASED);
            NativeLibrary.onGamePadEvent(NativeLibrary.TouchScreenDevice, NativeLibrary.ButtonType.DPAD_DOWN, NativeLibrary.ButtonState.PRESSED);
        }

        return true;
    }

    public boolean isActivityRecreated() {
        return activityRecreated;
    }

    @Retention(SOURCE)
    @IntDef({MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
            MENU_ACTION_EXIT, MENU_ACTION_TOGGLE_PREF_STATS, MENU_ACTION_SCREEN_LAYOUT_LANDSCAPE,
            MENU_ACTION_SCREEN_LAYOUT_PORTRAIT, MENU_ACTION_SCREEN_LAYOUT_SINGLE, MENU_ACTION_SCREEN_LAYOUT_SIDEBYSIDE,
            MENU_ACTION_SWAP_SCREENS, MENU_ACTION_RESET_OVERLAY})
    public @interface MenuAction {
    }
}
