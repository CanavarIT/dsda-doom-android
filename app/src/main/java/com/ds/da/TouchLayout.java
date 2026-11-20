package com.ds.da;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Saved on-screen button layout. Positions are stored as fractions of the screen and the
 * size as a multiplier of the default, so the layout survives resolution / rotation changes.
 * Buttons that were never touched have no entry and use the built-in defaults (same numbers
 * as dsda_android.c, in units of 1% of the screen height).
 */
final class TouchLayout {
    static final String PREFS = "touch_layout";

    static final String[] IDS = {"FIRE", "USE", "NEXT", "PREV", "MENU", "MAP", "STICK"};
    static final String[] NAMES = {"Fire", "Use", "Next weapon", "Previous weapon",
            "Menu", "Automap", "Move stick"};

    private TouchLayout() {
    }

    /** Default {x, y, radius} in pixels for a w x h screen. */
    static float[] defaults(int i, float w, float h) {
        float u = h / 100f;
        switch (IDS[i]) {
            case "FIRE":  return new float[] {w - 17 * u, h - 22 * u, 9.5f * u};
            case "USE":   return new float[] {w - 35 * u, h - 13 * u, 6.0f * u};
            case "NEXT":  return new float[] {w - 11 * u, h - 44 * u, 4.6f * u};
            case "PREV":  return new float[] {w - 27 * u, h - 40 * u, 4.6f * u};
            case "MENU":  return new float[] {w - 6.5f * u, 6.5f * u, 4.4f * u};
            case "MAP":   return new float[] {w - 18 * u, 6.5f * u, 4.4f * u};
            default:      return new float[] {17 * u, h - 23 * u, 13 * u};   // STICK
        }
    }

    static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    /** {nx, ny, scale} or null when the button uses the default. */
    static float[] load(Context c, int i) {
        SharedPreferences p = prefs(c);
        String k = IDS[i];
        if (!p.contains(k + "_x")) {
            return null;
        }
        return new float[] {p.getFloat(k + "_x", 0), p.getFloat(k + "_y", 0),
                p.getFloat(k + "_s", 1)};
    }

    static void save(Context c, int i, float nx, float ny, float scale) {
        prefs(c).edit().putFloat(IDS[i] + "_x", nx).putFloat(IDS[i] + "_y", ny)
                .putFloat(IDS[i] + "_s", scale).apply();
    }

    static void reset(Context c) {
        prefs(c).edit().clear().apply();
    }

    /** Value for the DSDA_TOUCH_LAYOUT environment variable read by the native side. */
    static String export(Context c) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < IDS.length; i++) {
            float[] v = load(c, i);
            if (v == null) {
                continue;
            }
            if (sb.length() > 0) {
                sb.append(';');
            }
            sb.append(IDS[i]).append('=')
                    .append(String.format(java.util.Locale.US, "%.4f,%.4f,%.3f", v[0], v[1], v[2]));
        }
        return sb.toString();
    }
}
