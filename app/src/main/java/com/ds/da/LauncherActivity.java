package com.ds.da;

import android.Manifest;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.switchmaterial.SwitchMaterial;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Small launcher: picks IWAD / PWADs / skill / compat level, makes sure the
 * "DSDA DEMO" folder exists, and starts the game (optionally recording a demo).
 */
public class LauncherActivity extends AppCompatActivity {

    private static final String FOLDER = "DSDA DEMO";
    private static final int REQ_STORAGE = 77;

    private static final String[] SKILLS = {
            "1 · I'm Too Young to Die", "2 · Hey, Not Too Rough", "3 · Hurt Me Plenty",
            "4 · Ultra-Violence", "5 · Nightmare!"};
    private static final String[] COMPLEVEL_LABELS = {
            "2 · Doom v1.9", "3 · Ultimate Doom", "4 · Final Doom", "9 · Boom",
            "11 · MBF", "21 · MBF21"};
    private static final int[] COMPLEVELS = {2, 3, 4, 9, 11, 21};
    private static final String[] RES_LABELS = {"360p (fastest)", "480p", "600p", "720p", "1080p (slow)"};
    private static final int[] RES_HEIGHTS = {360, 480, 600, 720, 1080};

    private final List<WadInfo> iwads = new ArrayList<>();
    private final List<WadInfo> pwads = new ArrayList<>();
    private boolean[] pwadChecked = new boolean[0];

    private File baseDir;
    private File wadDir;
    private File dataDir;
    private boolean sharedStorage;

    private TextView storageStatus;
    private TextView pwadSummary;
    private MaterialButton grantButton;
    private Spinner iwadSpinner, skillSpinner, compSpinner, resSpinner;
    private EditText warpEdit, extraEdit;
    private SwitchMaterial recordSwitch, touchUiSwitch;
    private SeekBar sensBar;
    private TextView sensLabel;
    private boolean compTouched;

    /** "Edit Touch" returns RESULT_OK when Launch was pressed: start the game at once. */
    private final androidx.activity.result.ActivityResultLauncher<Intent> touchEditor =
            registerForActivityResult(
                    new androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult(),
                    r -> {
                        if (r.getResultCode() == RESULT_OK) {
                            startGame(false, null);
                        }
                    });

    // ---- saved launcher settings ----
    private static final String PREFS = "launcher";
    private SharedPreferences prefs;
    private boolean pwadsRestored;
    private String wantedIwad;                 // IWAD to select once the list is known
    private final java.util.Set<String> savedPwads = new java.util.HashSet<>();

    private static class WadInfo {
        File file;
        String name;
        boolean iwad;
        boolean doom2Style; // MAPxx instead of ExMy
    }

    // ------------------------------------------------------------------ life cycle

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        buildUi();
        loadSettings();
    }

    @Override
    protected void onPause() {
        saveSettings();
        super.onPause();
    }

    @Override
    protected void onStop() {
        saveSettings();
        super.onStop();
    }

    /** Writes every launcher option so that nothing is reset after leaving the game. */
    private void saveSettings() {
        if (prefs == null || skillSpinner == null) {
            return;
        }
        SharedPreferences.Editor e = prefs.edit();
        WadInfo w = selectedIwad();
        if (w != null) {
            e.putString("iwad", w.name);
        } else if (wantedIwad != null) {
            e.putString("iwad", wantedIwad);
        }
        e.putInt("skill", skillSpinner.getSelectedItemPosition());
        e.putString("warp", warpEdit.getText().toString());
        e.putBoolean("record", recordSwitch.isChecked());
        e.putInt("comp", compSpinner.getSelectedItemPosition());
        e.putBoolean("compTouched", compTouched);
        e.putInt("res", resSpinner.getSelectedItemPosition());
        e.putInt("sens", sensBar.getProgress());
        e.putBoolean("touchUi", touchUiSwitch.isChecked());
        e.putString("extra", extraEdit.getText().toString());
        java.util.Set<String> names = new java.util.HashSet<>();
        if (pwadsRestored) {
            for (int i = 0; i < pwadChecked.length && i < pwads.size(); i++) {
                if (pwadChecked[i]) {
                    names.add(pwads.get(i).name);
                }
            }
        } else {
            names.addAll(savedPwads);   // list not scanned yet: keep what was stored
        }
        e.putStringSet("pwads", names);
        e.apply();
    }

    private void loadSettings() {
        skillSpinner.setSelection(clamp(prefs.getInt("skill", 3), SKILLS.length));
        warpEdit.setText(prefs.getString("warp", ""));
        recordSwitch.setChecked(prefs.getBoolean("record", true));
        resSpinner.setSelection(clamp(prefs.getInt("res", 1), RES_LABELS.length));
        sensBar.setProgress(Math.max(0, Math.min(100, prefs.getInt("sens", 30))));
        sensLabel.setText(String.format(Locale.US, "%.1f×", sensValue()));
        touchUiSwitch.setChecked(prefs.getBoolean("touchUi", true));
        extraEdit.setText(prefs.getString("extra", ""));
        compTouched = prefs.getBoolean("compTouched", false);
        if (compTouched) {
            compSpinner.setSelection(clamp(prefs.getInt("comp", 0), COMPLEVEL_LABELS.length));
        }
        wantedIwad = prefs.getString("iwad", null);
        java.util.Set<String> p = prefs.getStringSet("pwads", null);
        if (p != null) {
            savedPwads.addAll(p);
        }
    }

    private static int clamp(int v, int count) {
        return Math.max(0, Math.min(count - 1, v));
    }

    @Override
    protected void onResume() {
        super.onResume();
        resolveStorage();
        refreshWads();
    }

    // ---------------------------------------------------------------------- storage

    private boolean hasAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        return ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivity(i);
            } catch (Exception e) {
                startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
            }
        } else {
            ActivityCompat.requestPermissions(this, new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE}, REQ_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int code, String[] perms, int[] results) {
        super.onRequestPermissionsResult(code, perms, results);
        resolveStorage();
        refreshWads();
    }

    private void resolveStorage() {
        File chosen = null;
        sharedStorage = false;
        if (hasAllFilesAccess()) {
            File f = new File(Environment.getExternalStorageDirectory(), FOLDER);
            if ((f.isDirectory() || f.mkdirs()) && f.canWrite()) {
                chosen = f;
                sharedStorage = true;
            }
        }
        if (chosen == null) {
            File ext = getExternalFilesDir(null);
            chosen = ext != null ? ext : getFilesDir();
            chosen.mkdirs();
        }
        baseDir = chosen;
        wadDir = new File(baseDir, "WADS");
        dataDir = new File(baseDir, "data");
        wadDir.mkdirs();
        dataDir.mkdirs();

        copyEngineWad();

        if (storageStatus != null) {
            if (sharedStorage) {
                storageStatus.setText("Demos and WADs: " + baseDir.getAbsolutePath());
                grantButton.setVisibility(View.GONE);
            } else {
                storageStatus.setText("No access to shared storage. Currently using: "
                        + baseDir.getAbsolutePath());
                grantButton.setVisibility(View.VISIBLE);
            }
        }
    }

    /** dsda-doom.wad (engine data) is shipped as an asset and kept next to the config. */
    private void copyEngineWad() {
        File out = new File(dataDir, "dsda-doom.wad");
        try (InputStream in = getAssets().open("dsda-doom.wad")) {
            if (out.exists() && out.length() == in.available()) {
                return;
            }
        } catch (Exception e) {
            return;
        }
        try (InputStream in = getAssets().open("dsda-doom.wad");
             OutputStream os = new FileOutputStream(out)) {
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = in.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
        } catch (Exception e) {
            Toast.makeText(this, "Cannot write dsda-doom.wad: " + e.getMessage(),
                    Toast.LENGTH_LONG).show();
        }
    }

    // ------------------------------------------------------------------ WAD scanning

    private void refreshWads() {
        java.util.Set<String> keep = new java.util.HashSet<>();
        for (int i = 0; i < pwadChecked.length && i < pwads.size(); i++) {
            if (pwadChecked[i]) {
                keep.add(pwads.get(i).name);
            }
        }
        if (!pwadsRestored) {
            keep.addAll(savedPwads);
        }
        WadInfo curIwad = (iwadSpinner != null && !iwads.isEmpty()) ? selectedIwad() : null;
        if (curIwad != null) {
            wantedIwad = curIwad.name;
        }
        iwads.clear();
        pwads.clear();
        List<File> candidates = new ArrayList<>();
        collect(wadDir, candidates);
        collect(baseDir, candidates);
        Collections.sort(candidates, (a, b) -> a.getName().compareToIgnoreCase(b.getName()));

        for (File f : candidates) {
            String n = f.getName().toLowerCase(Locale.ROOT);
            if (n.equals("dsda-doom.wad")) {
                continue;
            }
            WadInfo w = new WadInfo();
            w.file = f;
            w.name = f.getName();
            if (n.endsWith(".wad")) {
                scanWad(w);
                if (w.iwad) {
                    iwads.add(w);
                } else {
                    pwads.add(w);
                }
            } else if (n.endsWith(".pk3") || n.endsWith(".pk7") || n.endsWith(".zip")
                    || n.endsWith(".deh") || n.endsWith(".bex")) {
                pwads.add(w);
            }
        }

        if (iwadSpinner != null) {
            List<String> names = new ArrayList<>();
            for (WadInfo w : iwads) {
                names.add(w.name);
            }
            if (names.isEmpty()) {
                names.add("(no IWAD — put one into the WADS folder)");
            }
            iwadSpinner.setAdapter(styledAdapter(names));
            if (wantedIwad != null) {
                for (int i = 0; i < iwads.size(); i++) {
                    if (iwads.get(i).name.equals(wantedIwad)) {
                        iwadSpinner.setSelection(i);
                        break;
                    }
                }
            }
            pwadChecked = new boolean[pwads.size()];
            for (int i = 0; i < pwads.size(); i++) {
                pwadChecked[i] = keep.contains(pwads.get(i).name);
            }
            if (!pwads.isEmpty() || !iwads.isEmpty()) {
                pwadsRestored = true;
            }
            updatePwadSummary();
            applySuggestedComplevel();
        }
    }

    private void collect(File dir, List<File> out) {
        File[] files = dir.listFiles();
        if (files == null) {
            return;
        }
        for (File f : files) {
            if (f.isFile() && !out.contains(f)) {
                out.add(f);
            }
        }
    }

    /** Reads the WAD header + directory: IWAD/PWAD and ExMy / MAPxx style. */
    private void scanWad(WadInfo w) {
        try (RandomAccessFile raf = new RandomAccessFile(w.file, "r")) {
            byte[] head = new byte[12];
            raf.readFully(head);
            String magic = new String(head, 0, 4, "US-ASCII");
            w.iwad = magic.equals("IWAD");
            int num = le32(head, 4);
            int ofs = le32(head, 8);
            if (num <= 0 || num > 200000 || ofs <= 0 || ofs + 16L * num > raf.length()) {
                return;
            }
            byte[] dir = new byte[num * 16];
            raf.seek(ofs);
            raf.readFully(dir);
            for (int i = 0; i < num; i++) {
                String name = new String(dir, i * 16 + 8, 8, "US-ASCII");
                int z = name.indexOf('\0');
                if (z >= 0) {
                    name = name.substring(0, z);
                }
                if (name.equals("MAP01")) {
                    w.doom2Style = true;
                    break;
                }
                if (name.equals("E1M1")) {
                    break;
                }
            }
        } catch (Exception ignored) {
        }
    }

    private static int le32(byte[] b, int o) {
        return (b[o] & 0xff) | (b[o + 1] & 0xff) << 8 | (b[o + 2] & 0xff) << 16 | (b[o + 3] & 0xff) << 24;
    }

    private WadInfo selectedIwad() {
        int i = iwadSpinner.getSelectedItemPosition();
        return (i >= 0 && i < iwads.size()) ? iwads.get(i) : null;
    }

    private void applySuggestedComplevel() {
        if (compTouched || compSpinner == null) {
            return;
        }
        WadInfo w = selectedIwad();
        int level = 9;
        if (w != null) {
            String n = w.name.toLowerCase(Locale.ROOT);
            if (n.contains("plutonia") || n.contains("tnt")) {
                level = 4;
            } else if (w.doom2Style) {
                level = 2;
            } else {
                level = 3;
            }
        }
        for (int i = 0; i < COMPLEVELS.length; i++) {
            if (COMPLEVELS[i] == level) {
                compSpinner.setSelection(i);
            }
        }
    }

    // --------------------------------------------------------------------------- UI

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                getResources().getDisplayMetrics());
    }

    private static final int INK = Color.parseColor("#E9EBF2");
    private static final int DIM = Color.parseColor("#8E92A3");
    private static final int PANEL = Color.parseColor("#20222D");
    private static final int ACCENT = Color.parseColor("#E48800");

    private ArrayAdapter<String> styledAdapter(List<String> items) {
        ArrayAdapter<String> a = new ArrayAdapter<String>(this,
                android.R.layout.simple_spinner_item, items) {
            @Override
            public View getView(int pos, View v, ViewGroup parent) {
                TextView t = (TextView) super.getView(pos, v, parent);
                t.setTextColor(INK);
                t.setTextSize(15);
                return t;
            }

            @Override
            public View getDropDownView(int pos, View v, ViewGroup parent) {
                TextView t = (TextView) super.getDropDownView(pos, v, parent);
                t.setTextColor(INK);
                t.setBackgroundColor(PANEL);
                t.setPadding(dp(16), dp(12), dp(16), dp(12));
                return t;
            }
        };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        return a;
    }

    private Spinner spinner(String[] items) {
        Spinner s = new Spinner(this);
        s.setAdapter(styledAdapter(new ArrayList<>(Arrays.asList(items))));
        return s;
    }

    private EditText edit(String hint) {
        EditText e = new EditText(this);
        e.setHint(hint);
        e.setHintTextColor(DIM);
        e.setTextColor(INK);
        e.setTextSize(15);
        e.setSingleLine(true);
        return e;
    }

    private TextView plain(String text, int sp, int color) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(sp);
        t.setTextColor(color);
        return t;
    }

    /** Small caption above a field. */
    private TextView label(String text) {
        TextView t = plain(text, 14, DIM);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(14);
        t.setLayoutParams(lp);
        return t;
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(12), dp(20), dp(24));
        scroll.addView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // logo
        android.widget.ImageView logo = new android.widget.ImageView(this);
        logo.setImageResource(R.drawable.logo);
        logo.setAdjustViewBounds(true);
        logo.setScaleType(android.widget.ImageView.ScaleType.FIT_CENTER);
        root.addView(logo, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // storage (only shown when something is wrong)
        storageStatus = plain("", 12, DIM);
        storageStatus.setPadding(0, dp(6), 0, 0);
        root.addView(storageStatus);
        grantButton = new MaterialButton(this);
        grantButton.setText("Grant storage access");
        grantButton.setOnClickListener(v -> requestAllFilesAccess());
        root.addView(grantButton);

        // ---- the basics: game, difficulty, map, record ----
        root.addView(label("Game (IWAD)"));
        iwadSpinner = spinner(new String[] {"…"});
        iwadSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
                applySuggestedComplevel();
            }

            @Override
            public void onNothingSelected(AdapterView<?> p) {
            }
        });
        root.addView(iwadSpinner);

        root.addView(label("Difficulty"));
        skillSpinner = spinner(SKILLS);
        skillSpinner.setSelection(3);
        root.addView(skillSpinner);

        root.addView(label("Map (empty = first).  E1M1 → \"1 1\",  MAP05 → \"5\""));
        warpEdit = edit("1 1");
        root.addView(warpEdit);

        recordSwitch = new SwitchMaterial(this);
        recordSwitch.setText("Record demo");
        recordSwitch.setTextColor(INK);
        recordSwitch.setTextSize(16);
        recordSwitch.setChecked(true);
        LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rlp.topMargin = dp(18);
        root.addView(recordSwitch, rlp);

        MaterialButton play = new MaterialButton(this);
        play.setText("PLAY");
        play.setTextSize(18);
        play.setBackgroundTintList(android.content.res.ColorStateList.valueOf(ACCENT));
        play.setTextColor(Color.parseColor("#16171F"));
        LinearLayout.LayoutParams plp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(60));
        plp.topMargin = dp(18);
        play.setLayoutParams(plp);
        play.setOnClickListener(v -> startGame(false, null));
        root.addView(play);

        MaterialButton demos = new MaterialButton(this,
                null, com.google.android.material.R.attr.materialButtonOutlinedStyle);
        demos.setText("Watch a recorded demo");
        LinearLayout.LayoutParams dlp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(52));
        dlp.topMargin = dp(8);
        demos.setLayoutParams(dlp);
        demos.setOnClickListener(v -> pickDemo());
        root.addView(demos);

        // ---- everything else, hidden until asked ----
        final TextView more = plain("Advanced  ▾", 15, ACCENT);
        more.setPadding(0, dp(22), 0, dp(10));
        root.addView(more);

        final LinearLayout adv = new LinearLayout(this);
        adv.setOrientation(LinearLayout.VERTICAL);
        adv.setVisibility(View.GONE);
        root.addView(adv);
        more.setOnClickListener(v -> {
            boolean open = adv.getVisibility() != View.VISIBLE;
            adv.setVisibility(open ? View.VISIBLE : View.GONE);
            more.setText(open ? "Advanced  ▴" : "Advanced  ▾");
        });

        adv.addView(label("Add-ons (PWAD / PK3 / DEH)"));
        pwadSummary = plain("", 15, INK);
        pwadSummary.setPadding(dp(4), dp(10), dp(4), dp(10));
        pwadSummary.setOnClickListener(v -> pickPwads());
        adv.addView(pwadSummary);

        adv.addView(label("Compatibility level"));
        compSpinner = spinner(COMPLEVEL_LABELS);
        compSpinner.setOnTouchListener((v, e) -> {
            compTouched = true;
            return false;
        });
        compSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> p, View v, int pos, long id) {
                saveSettings();
            }

            @Override
            public void onNothingSelected(AdapterView<?> p) {
            }
        });
        adv.addView(compSpinner);

        adv.addView(label("Resolution (lower = faster)"));
        resSpinner = spinner(RES_LABELS);
        resSpinner.setSelection(1);
        adv.addView(resSpinner);

        adv.addView(label("Look sensitivity"));
        LinearLayout sensRow = new LinearLayout(this);
        sensRow.setOrientation(LinearLayout.HORIZONTAL);
        sensRow.setGravity(Gravity.CENTER_VERTICAL);
        sensBar = new SeekBar(this);
        sensBar.setMax(100);
        sensBar.setProgress(30);
        sensLabel = plain("", 15, INK);
        sensLabel.setMinWidth(dp(48));
        sensLabel.setGravity(Gravity.END);
        sensBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean u) {
                sensLabel.setText(String.format(Locale.US, "%.1f×", sensValue()));
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        sensRow.addView(sensBar, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        sensRow.addView(sensLabel);
        adv.addView(sensRow);
        sensLabel.setText(String.format(Locale.US, "%.1f×", sensValue()));

        touchUiSwitch = new SwitchMaterial(this);
        touchUiSwitch.setText("Show on-screen buttons");
        touchUiSwitch.setTextColor(INK);
        touchUiSwitch.setChecked(true);
        LinearLayout.LayoutParams tlp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        tlp.topMargin = dp(14);
        adv.addView(touchUiSwitch, tlp);

        MaterialButton editTouch = new MaterialButton(this,
                null, com.google.android.material.R.attr.materialButtonOutlinedStyle);
        editTouch.setText("Edit Touch");
        LinearLayout.LayoutParams elp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(52));
        elp.topMargin = dp(10);
        editTouch.setLayoutParams(elp);
        editTouch.setOnClickListener(v -> {
            saveSettings();
            touchEditor.launch(new Intent(this, TouchEditActivity.class));
        });
        adv.addView(editTouch);

        adv.addView(label("Custom parameters (e.g. -fast -nomonsters)"));
        extraEdit = edit("");
        adv.addView(extraEdit);

        setContentView(scroll);
    }

    private float sensValue() {
        return 0.3f + sensBar.getProgress() / 100f * 2.7f;
    }

    private void updatePwadSummary() {
        int n = 0;
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < pwadChecked.length; i++) {
            if (pwadChecked[i]) {
                n++;
                if (sb.length() > 0) {
                    sb.append(", ");
                }
                sb.append(pwads.get(i).name);
            }
        }
        if (pwads.isEmpty()) {
            pwadSummary.setText("no files in WADS  ›");
        } else if (n == 0) {
            pwadSummary.setText("none selected (" + pwads.size() + " available)  ›");
        } else {
            pwadSummary.setText(sb + "  ›");
        }
    }

    private void pickPwads() {
        if (pwads.isEmpty()) {
            Toast.makeText(this, "Put PWAD/PK3/DEH files into " + wadDir.getAbsolutePath(),
                    Toast.LENGTH_LONG).show();
            return;
        }
        String[] names = new String[pwads.size()];
        for (int i = 0; i < names.length; i++) {
            names[i] = pwads.get(i).name;
        }
        final boolean[] tmp = pwadChecked.clone();
        new AlertDialog.Builder(this)
                .setTitle("Add-ons (load order = list order)")
                .setMultiChoiceItems(names, tmp, (d, which, checked) -> tmp[which] = checked)
                .setPositiveButton("OK", (d, w) -> {
                    pwadChecked = tmp;
                    updatePwadSummary();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void pickDemo() {
        File[] lmps = baseDir.listFiles((d, n) -> n.toLowerCase(Locale.ROOT).endsWith(".lmp"));
        if (lmps == null || lmps.length == 0) {
            Toast.makeText(this, "No demos in " + baseDir.getAbsolutePath(),
                    Toast.LENGTH_LONG).show();
            return;
        }
        Arrays.sort(lmps, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        final File[] list = lmps;
        String[] names = new String[list.length];
        for (int i = 0; i < names.length; i++) {
            names[i] = list[i].getName();
        }
        new AlertDialog.Builder(this)
                .setTitle("Play demo")
                .setItems(names, (d, which) -> startGame(true, list[which]))
                .show();
    }

    // ------------------------------------------------------------------- launching

    private int renderHeight() {
        return RES_HEIGHTS[resSpinner.getSelectedItemPosition()];
    }

    private void startGame(boolean playDemo, File demoFile) {
        WadInfo iwad = selectedIwad();
        if (iwad == null) {
            new AlertDialog.Builder(this)
                    .setTitle("No IWAD")
                    .setMessage("Copy an IWAD (doom.wad, doom2.wad, plutonia.wad, tnt.wad, "
                            + "freedoom1/2.wad, …) into:\n\n" + wadDir.getAbsolutePath())
                    .setPositiveButton("OK", null)
                    .show();
            return;
        }

        List<String> args = new ArrayList<>();
        args.add("-iwad");
        args.add(iwad.file.getAbsolutePath());

        List<String> files = new ArrayList<>();
        List<String> dehs = new ArrayList<>();
        for (int i = 0; i < pwadChecked.length; i++) {
            if (!pwadChecked[i]) {
                continue;
            }
            String path = pwads.get(i).file.getAbsolutePath();
            String low = path.toLowerCase(Locale.ROOT);
            if (low.endsWith(".deh") || low.endsWith(".bex")) {
                dehs.add(path);
            } else {
                files.add(path);
            }
        }
        if (!files.isEmpty()) {
            args.add("-file");
            args.addAll(files);
        }
        if (!dehs.isEmpty()) {
            args.add("-deh");
            args.addAll(dehs);
        }

        // render resolution follows the real screen aspect so there are no black bars
        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(dm);
        float aspect = (float) Math.max(dm.widthPixels, dm.heightPixels)
                / Math.min(dm.widthPixels, dm.heightPixels);
        int h = renderHeight();
        int w = Math.round(h * aspect / 2f) * 2;
        args.add("-width");
        args.add(String.valueOf(w));
        args.add("-height");
        args.add(String.valueOf(h));

        int complevel = COMPLEVELS[compSpinner.getSelectedItemPosition()];

        if (playDemo) {
            args.add("-playdemo");
            args.add(demoFile.getAbsolutePath());
        } else {
            String warp = warpEdit.getText().toString().trim();
            boolean record = recordSwitch.isChecked();
            args.add("-skill");
            args.add(String.valueOf(skillSpinner.getSelectedItemPosition() + 1));
            args.add("-complevel");
            args.add(String.valueOf(complevel));

            if (warp.isEmpty() && record) {
                warp = iwad.doom2Style ? "1" : "1 1"; // recording needs a map to start on
            }
            if (!warp.isEmpty()) {
                args.add("-warp");
                for (String p : warp.split("[\\s,]+")) {
                    if (!p.isEmpty()) {
                        args.add(p);
                    }
                }
            }
            if (record) {
                String stamp = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
                String base = iwad.name.replaceAll("(?i)\\.wad$", "")
                        .replaceAll("[^A-Za-z0-9_-]", "_");
                String map = warp.isEmpty() ? "" : "_" + warp.replaceAll("[^0-9]+", "-");
                args.add("-record");
                args.add(new File(baseDir, base + map + "_" + stamp).getAbsolutePath());
            }
        }

        String extra = extraEdit.getText().toString().trim();
        if (!extra.isEmpty()) {
            args.addAll(Arrays.asList(extra.split("\\s+")));
        }

        saveSettings();

        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_ARGS, args.toArray(new String[0]));
        i.putExtra(GameActivity.EXTRA_BASE_DIR, baseDir.getAbsolutePath());
        i.putExtra(GameActivity.EXTRA_WAD_DIR, wadDir.getAbsolutePath());
        i.putExtra(GameActivity.EXTRA_TOUCH_SENS, sensValue());
        i.putExtra(GameActivity.EXTRA_TOUCH_UI, touchUiSwitch.isChecked());
        startActivity(i);
    }
}
