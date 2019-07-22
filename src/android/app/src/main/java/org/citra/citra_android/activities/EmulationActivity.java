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
import android.widget.Toast;

import com.squareup.picasso.Callback;
import com.squareup.picasso.Picasso;

import org.citra.citra_android.NativeLibrary;
import org.citra.citra_android.R;
import org.citra.citra_android.fragments.EmulationFragment;
import org.citra.citra_android.fragments.MenuFragment;
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
    public static final int MENU_ACTION_SWITCH_SCREEN_LAYOUT = 5;
    public static final int MENU_ACTION_SWAP_SCREENS = 6;
    public static final int MENU_ACTION_RESET_OVERLAY = 7;
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
        buttonsActionsMap.append(R.id.menu_emulation_switch_screen_layout,
                EmulationActivity.MENU_ACTION_SWITCH_SCREEN_LAYOUT);
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
    private boolean mBackPressedOnce;
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

            Picasso.with(this)
                    .load(mScreenPath)
                    .noFade()
                    .noPlaceholder()
                    .into(mImageView, new Callback() {
                        @Override
                        public void onSuccess() {
                            supportStartPostponedEnterTransition();
                        }

                        @Override
                        public void onError() {
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
            if (mBackPressedOnce) {
                mEmulationFragment.stopEmulation();
                exitWithAnimation();
            } else {
                mBackPressedOnce = true;
                Toast.makeText(this, "Press back again to exit", Toast.LENGTH_SHORT).show();
            }

            Handler mHandler = new Handler();
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    mBackPressedOnce = false;
                    mHandler.removeCallbacks(this);
                }
            }, 2000);
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
            Picasso.with(EmulationActivity.this)
                    .invalidate(mScreenPath);

            Picasso.with(EmulationActivity.this)
                    .load(mScreenPath)
                    .noFade()
                    .noPlaceholder()
                    .into(mImageView, new Callback() {
                        @Override
                        public void onSuccess() {
                            showScreenshot();
                        }

                        @Override
                        public void onError() {
                            finish();
                        }
                    });
        });
    }

    private void showScreenshot() {
        Animations.fadeViewIn(mImageView)
                .withEndAction(afterShowingScreenshot);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_emulation, menu);
        return true;
    }

    @SuppressWarnings("WrongConstant")
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int action = buttonsActionsMap.get(item.getItemId(), -1);
        if (action >= 0) {
            handleMenuAction(action);
        }
        return true;
    }

    public void handleMenuAction(@MenuAction int menuAction) {
        switch (menuAction) {
            // Edit the placement of the controls
            case MENU_ACTION_EDIT_CONTROLS_PLACEMENT:
                editControlsPlacement();
                break;

            // Enable/Disable specific buttons or the entire input overlay.
            case MENU_ACTION_TOGGLE_CONTROLS:
                toggleControls();
                return;

            // Adjust the scale of the overlay controls.
            case MENU_ACTION_ADJUST_SCALE:
                adjustScale();
                return;

            // Toggle the visibility of the Performance stats TextView
            case MENU_ACTION_TOGGLE_PREF_STATS:
                mEmulationFragment.togglePerfStatsVisibility();
                return;

            // Switch the layout of the screens
            case MENU_ACTION_SWITCH_SCREEN_LAYOUT:
                NativeLibrary.SwitchScreenLayout(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);
                return;

            // Swap the top and bottom screen locations
            case MENU_ACTION_SWAP_SCREENS:
                NativeLibrary.SwapScreens(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT);
                return;

            // Reset overlay placement
            case MENU_ACTION_RESET_OVERLAY:
                resetOverlay();
                break;

            case MENU_ACTION_EXIT:
                toggleMenu();  // Hide the menu (it will be showing since we just clicked it)
                mEmulationFragment.stopEmulation();
                exitWithAnimation();
                return;
        }
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
        return NativeLibrary.onGamePadEvent(input.getDescriptor(), event.getKeyCode(), action);
    }

    private void toggleControls() {
        final SharedPreferences.Editor editor = mPreferences.edit();
        boolean[] enabledButtons = new boolean[14];
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.emulation_toggle_controls);

        for (int i = 0; i < enabledButtons.length; i++) {
            enabledButtons[i] = mPreferences.getBoolean("buttonToggle" + i, true);
        }
        builder.setMultiChoiceItems(R.array.n3dsButtons, enabledButtons,
                (dialog, indexSelected, isChecked) -> editor
                        .putBoolean("buttonToggle" + indexSelected, isChecked));


        builder.setNeutralButton(getString(R.string.emulation_toggle_all),
                (dialogInterface, i) -> mEmulationFragment.toggleInputOverlayVisibility());
        builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
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
            public void onStartTrackingTouch(SeekBar seekBar) {
                // Do nothing
            }

            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                value.setText(String.valueOf(progress + 50));
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
                // Do nothing
            }
        });

        value.setText(String.valueOf(seekbar.getProgress() + 50));
        units.setText("%");

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.emulation_control_scale);
        builder.setView(view);
        builder.setPositiveButton(getString(R.string.ok), (dialogInterface, i) ->
        {
            SharedPreferences.Editor editor = mPreferences.edit();
            editor.putInt("controlScale", seekbar.getProgress());
            editor.apply();

            mEmulationFragment.refreshInputOverlay();
        });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

    private void resetOverlay() {
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.emulation_touch_overlay_reset))
                .setPositiveButton(R.string.yes, (dialogInterface, i) ->
                {
                    mEmulationFragment.resetInputOverlay();
                })
                .setNegativeButton(R.string.cancel, (dialogInterface, i) ->
                {
                })
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
        if (event.getActionMasked() == MotionEvent.ACTION_CANCEL)
            return true;

        InputDevice input = event.getDevice();
        List<InputDevice.MotionRange> motions = input.getMotionRanges();

        float[] axisValues = {0.0f, 0.0f};
        for (InputDevice.MotionRange range : motions) {
            boolean consumed = false;
            int axis = range.getAxis();
            float origValue = event.getAxisValue(axis);
            float value = mControllerMappingHelper.scaleAxis(input, axis, origValue);

            if (axis == AXIS_X || axis == AXIS_Z) {
                axisValues[0] = value;
            } else if (axis == AXIS_Y || axis == AXIS_RZ) {
                axisValues[1] = value;
            }

            // If the input is still in the "flat" area, that means it's really zero.
            // This is used to compensate for imprecision in joysticks.
            if (Math.abs(axisValues[0]) > range.getFlat() || Math.abs(axisValues[1]) > range.getFlat()) {
                consumed = NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, axisValues[0], axisValues[1]);
            } else {
                consumed = NativeLibrary.onGamePadMoveEvent(input.getDescriptor(), axis, 0.0f, 0.0f);
            }

            return NativeLibrary.onGamePadAxisEvent(input.getDescriptor(), axis, value) || consumed;
        }

        return false;
    }

    public boolean isActivityRecreated() {
        return activityRecreated;
    }

    @Retention(SOURCE)
    @IntDef({MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
            MENU_ACTION_EXIT, MENU_ACTION_TOGGLE_PREF_STATS, MENU_ACTION_SWITCH_SCREEN_LAYOUT,
            MENU_ACTION_SWAP_SCREENS, MENU_ACTION_RESET_OVERLAY})
    public @interface MenuAction {
    }
}
