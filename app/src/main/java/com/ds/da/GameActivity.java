package com.ds.da;

import android.os.Build;
import android.os.Bundle;
import android.system.Os;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/** Hosts the native DSDA-Doom engine (libmain.so) on top of SDL2. */
public class GameActivity extends SDLActivity {

    public static final String EXTRA_ARGS = "args";
    public static final String EXTRA_BASE_DIR = "base_dir";
    public static final String EXTRA_WAD_DIR = "wad_dir";
    public static final String EXTRA_TOUCH_SENS = "touch_sens";
    public static final String EXTRA_TOUCH_UI = "touch_ui";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The native side reads its configuration from the environment; it has to be
        // set before SDLActivity starts the game thread.
        try {
            String base = getIntent().getStringExtra(EXTRA_BASE_DIR);
            String wads = getIntent().getStringExtra(EXTRA_WAD_DIR);
            if (base != null) {
                Os.setenv("DSDA_BASE_DIR", base, true);
            }
            if (wads != null) {
                Os.setenv("DOOMWADDIR", wads, true);
                Os.setenv("DOOMWADPATH", wads, true);
            }
            Os.setenv("DSDA_TOUCH_SENS",
                    String.valueOf(getIntent().getFloatExtra(EXTRA_TOUCH_SENS, 1.0f)), true);
            Os.setenv("DSDA_TOUCH_LAYOUT", TouchLayout.export(this), true);
            Os.setenv("DSDA_TOUCH_UI",
                    getIntent().getBooleanExtra(EXTRA_TOUCH_UI, true) ? "1" : "0", true);
        } catch (Exception ignored) {
        }

        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        installBarsGuard();
        hideSystemBars();
        if (Build.VERSION.SDK_INT >= 28) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemBars();
    }

    /** The soft keyboard (and any focus change) brings the status / navigation bars back. */
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
            getWindow().getDecorView().postDelayed(this::hideSystemBars, 300);
        }
    }

    private void installBarsGuard() {
        final View decor = getWindow().getDecorView();
        decor.setOnApplyWindowInsetsListener((v, insets) -> {
            boolean shown;
            if (Build.VERSION.SDK_INT >= 30) {
                shown = insets.isVisible(WindowInsets.Type.statusBars())
                        || insets.isVisible(WindowInsets.Type.navigationBars());
            } else {
                shown = insets.getSystemWindowInsetTop() > 0 || insets.getSystemWindowInsetRight() > 0
                        || insets.getSystemWindowInsetLeft() > 0;
            }
            if (shown) {
                decor.post(this::hideSystemBars);
            }
            return v.onApplyWindowInsets(insets);
        });
    }

    @SuppressWarnings("deprecation")
    private void hideSystemBars() {
        View decor = getWindow().getDecorView();
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String[] getArguments() {
        String[] args = getIntent().getStringArrayExtra(EXTRA_ARGS);
        return args != null ? args : new String[0];
    }
}
