package com.ds.da;

import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.button.MaterialButton;

import java.util.Locale;

/**
 * "Edit Touch": drag the on-screen buttons where you want them and resize them.
 * Opened from the launcher (Advanced). Launch starts the game right away to try the layout.
 */
public class TouchEditActivity extends AppCompatActivity {

    public static final String RESULT_LAUNCH = "launch";

    private static final float MIN_SCALE = 0.5f, MAX_SCALE = 2.0f;

    private EditorView editor;
    private TextView selLabel;
    private SeekBar sizeBar;
    private boolean updatingBar;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (Build.VERSION.SDK_INT >= 28) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        FrameLayout root = new FrameLayout(this);
        editor = new EditorView();
        root.addView(editor, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        root.addView(buildPanel());
        setContentView(root);
        hideBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideBars();
        }
    }

    @SuppressWarnings("deprecation")
    private void hideBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }

    private int dp(float v) {
        return (int) (v * getResources().getDisplayMetrics().density + 0.5f);
    }

    /** Small control panel at the bottom centre (the one place no button lives by default). */
    private View buildPanel() {
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(dp(14), dp(8), dp(14), dp(10));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.argb(215, 18, 20, 28));
        bg.setCornerRadius(dp(14));
        panel.setBackground(bg);

        selLabel = new TextView(this);
        selLabel.setTextColor(Color.parseColor("#E9EBF2"));
        selLabel.setTextSize(14);
        selLabel.setGravity(Gravity.CENTER);
        panel.addView(selLabel);

        sizeBar = new SeekBar(this);
        sizeBar.setMax(150);   // 0.5x .. 2.0x
        sizeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int p, boolean user) {
                if (user && !updatingBar) {
                    editor.setSelectedScale(MIN_SCALE + p / 100f);
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        panel.addView(sizeBar);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(panelButton("Exit", v -> finish()), rowParams());
        row.addView(panelButton("Reset to default", v -> confirmReset()), rowParams());
        row.addView(panelButton("Launch", v -> {
            setResult(RESULT_OK, new Intent().putExtra(RESULT_LAUNCH, true));
            finish();
        }), rowParams());
        panel.addView(row);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                Math.min(dp(430), getResources().getDisplayMetrics().widthPixels / 2),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
        lp.bottomMargin = dp(8);
        panel.setLayoutParams(lp);
        updateSelection();
        return panel;
    }

    private LinearLayout.LayoutParams rowParams() {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        p.leftMargin = p.rightMargin = dp(3);
        return p;
    }

    private MaterialButton panelButton(String text, View.OnClickListener l) {
        MaterialButton bt = new MaterialButton(this);
        bt.setText(text);
        bt.setTextSize(12);
        bt.setAllCaps(false);
        bt.setInsetTop(0);
        bt.setInsetBottom(0);
        bt.setPadding(dp(4), 0, dp(4), 0);
        bt.setMaxLines(1);
        bt.setOnClickListener(l);
        return bt;
    }

    private void confirmReset() {
        new AlertDialog.Builder(this)
                .setTitle("Reset to default")
                .setMessage("Put all buttons back to their original position and size?")
                .setPositiveButton("Reset", (d, w) -> {
                    TouchLayout.reset(this);
                    editor.reload();
                    updateSelection();
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    private void updateSelection() {
        int s = editor == null ? -1 : editor.selected;
        updatingBar = true;
        if (s < 0) {
            selLabel.setText("Drag a button to move it. Tap it to resize.");
            sizeBar.setEnabled(false);
            sizeBar.setProgress(50);
        } else {
            float sc = editor.scale[s];
            selLabel.setText(String.format(Locale.US, "%s  ·  size %d%%",
                    TouchLayout.NAMES[s], Math.round(sc * 100)));
            sizeBar.setEnabled(true);
            sizeBar.setProgress(Math.round((sc - MIN_SCALE) * 100));
        }
        updatingBar = false;
    }

    // ------------------------------------------------------------------ canvas

    private class EditorView extends View {
        final int n = TouchLayout.IDS.length;
        final float[] nx = new float[n], ny = new float[n], scale = new float[n];
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Path path = new Path();
        int selected = -1;
        float lastX, lastY;
        boolean dragging;

        EditorView() {
            super(TouchEditActivity.this);
        }

        @Override
        protected void onSizeChanged(int w, int h, int ow, int oh) {
            reload();
        }

        void reload() {
            float w = getWidth(), h = getHeight();
            if (w <= 0 || h <= 0) {
                return;
            }
            for (int i = 0; i < n; i++) {
                float[] v = TouchLayout.load(getContext(), i);
                if (v != null) {
                    nx[i] = v[0];
                    ny[i] = v[1];
                    scale[i] = v[2];
                } else {
                    float[] d = TouchLayout.defaults(i, w, h);
                    nx[i] = d[0] / w;
                    ny[i] = d[1] / h;
                    scale[i] = 1f;
                }
            }
            invalidate();
        }

        float radius(int i) {
            return TouchLayout.defaults(i, getWidth(), getHeight())[2] * scale[i];
        }

        void setSelectedScale(float s) {
            if (selected < 0) {
                return;
            }
            scale[selected] = Math.max(MIN_SCALE, Math.min(MAX_SCALE, s));
            TouchLayout.save(getContext(), selected, nx[selected], ny[selected], scale[selected]);
            updateSelection();
            invalidate();
        }

        int hit(float x, float y) {
            int best = -1;
            float bestD = Float.MAX_VALUE;
            for (int i = 0; i < n; i++) {
                float d = (float) Math.hypot(x - nx[i] * getWidth(), y - ny[i] * getHeight());
                if (d <= radius(i) * 1.25f && d < bestD) {
                    bestD = d;
                    best = i;
                }
            }
            return best;
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    selected = hit(e.getX(), e.getY());
                    lastX = e.getX();
                    lastY = e.getY();
                    dragging = selected >= 0;
                    updateSelection();
                    invalidate();
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if (dragging && selected >= 0) {
                        float w = getWidth(), h = getHeight();
                        float r = radius(selected) * 0.6f;
                        float x = Math.max(r, Math.min(w - r, nx[selected] * w + e.getX() - lastX));
                        float y = Math.max(r, Math.min(h - r, ny[selected] * h + e.getY() - lastY));
                        nx[selected] = x / w;
                        ny[selected] = y / h;
                        lastX = e.getX();
                        lastY = e.getY();
                        invalidate();
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (dragging && selected >= 0) {
                        TouchLayout.save(getContext(), selected, nx[selected], ny[selected],
                                scale[selected]);
                    }
                    dragging = false;
                    return true;
                default:
                    return true;
            }
        }

        // -- drawing: same look as the in-game overlay --

        private void line(Canvas c, float x0, float y0, float x1, float y1, float th) {
            p.setStrokeWidth(th);
            p.setStrokeCap(Paint.Cap.ROUND);
            c.drawLine(x0, y0, x1, y1, p);
        }

        private void glyph(Canvas c, int i, float cx, float cy, float k, float th) {
            p.setStyle(Paint.Style.STROKE);
            switch (TouchLayout.IDS[i]) {
                case "NEXT":
                    line(c, cx - k * .4f, cy - k, cx + k * .5f, cy, th);
                    line(c, cx + k * .5f, cy, cx - k * .4f, cy + k, th);
                    break;
                case "PREV":
                    line(c, cx + k * .4f, cy - k, cx - k * .5f, cy, th);
                    line(c, cx - k * .5f, cy, cx + k * .4f, cy + k, th);
                    break;
                case "MENU":
                    line(c, cx - k, cy - k * .6f, cx + k, cy - k * .6f, th);
                    line(c, cx - k, cy, cx + k, cy, th);
                    line(c, cx - k, cy + k * .6f, cx + k, cy + k * .6f, th);
                    break;
                case "MAP":
                    path.reset();
                    path.moveTo(cx, cy - k);
                    path.lineTo(cx + k, cy);
                    path.lineTo(cx, cy + k);
                    path.lineTo(cx - k, cy);
                    path.close();
                    p.setStrokeWidth(th);
                    c.drawPath(path, p);
                    break;
                case "USE":
                    p.setStrokeWidth(th);
                    c.drawCircle(cx, cy, k * .9f, p);
                    p.setStyle(Paint.Style.FILL);
                    c.drawCircle(cx, cy, k * .28f, p);
                    break;
                case "FIRE":
                    p.setStyle(Paint.Style.FILL);
                    c.drawCircle(cx, cy, k * .34f, p);
                    break;
                default:
                    break;
            }
        }

        @Override
        protected void onDraw(Canvas c) {
            float w = getWidth(), h = getHeight(), u = h / 100f;
            c.drawColor(Color.parseColor("#15171E"));

            // hints of the game screen: horizon and the status bar strip at the bottom
            p.setStyle(Paint.Style.FILL);
            p.setColor(Color.parseColor("#1D202A"));
            c.drawRect(0, h * 0.5f, w, h * 0.85f, p);
            p.setColor(Color.parseColor("#2A2D38"));
            c.drawRect(0, h * 0.85f, w, h, p);

            for (int i = 0; i < n; i++) {
                float cx = nx[i] * w, cy = ny[i] * h, r = radius(i);
                boolean sel = i == selected;
                boolean fire = TouchLayout.IDS[i].equals("FIRE");
                boolean stick = TouchLayout.IDS[i].equals("STICK");
                int base = fire ? Color.rgb(255, 150, 38) : Color.rgb(235, 240, 250);

                p.setStyle(Paint.Style.FILL);
                p.setColor(withAlpha(base, sel ? 90 : 38));
                c.drawCircle(cx, cy, r, p);
                p.setStyle(Paint.Style.STROKE);
                p.setStrokeWidth(u * 0.38f);
                p.setColor(withAlpha(base, sel ? 255 : 150));
                c.drawCircle(cx, cy, r - u * 0.19f, p);

                p.setColor(withAlpha(fire ? Color.rgb(255, 170, 70) : Color.rgb(240, 244, 252), 220));
                if (stick) {
                    p.setStyle(Paint.Style.FILL);
                    c.drawCircle(cx, cy, 5.4f * u * scale[i], p);
                } else {
                    glyph(c, i, cx, cy, r * 0.42f, u * 0.34f);
                }

                if (sel) {   // selection outline
                    p.setStyle(Paint.Style.STROKE);
                    p.setStrokeWidth(u * 0.3f);
                    p.setColor(Color.parseColor("#E48800"));
                    p.setPathEffect(new android.graphics.DashPathEffect(new float[] {u, u}, 0));
                    c.drawCircle(cx, cy, r * 1.18f, p);
                    p.setPathEffect(null);
                }
            }
        }

        private int withAlpha(int color, int a) {
            return Color.argb(a, Color.red(color), Color.green(color), Color.blue(color));
        }
    }
}
