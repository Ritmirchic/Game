package com.wallpulse.pocket;

import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentValues;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.app.WallpaperManager;
import android.view.View;
import android.view.Window;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.regex.Pattern;

public final class MainActivity extends Activity {
    private static final int FILE_PICKER_REQUEST = 7041;
    private static final String APP_ORIGIN = "https://wallpulse.local";
    private static final Pattern SAFE_NAME = Pattern.compile("[^\\p{L}\\p{N}._ -]");

    private WebView webView;
    private ValueCallback<Uri[]> fileCallback;
    private Uri pendingUri;
    private OutputStream pendingOutput;
    private String pendingMime;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Window window = getWindow();
        window.setStatusBarColor(Color.rgb(11, 16, 18));
        window.setNavigationBarColor(Color.rgb(11, 16, 18));
        window.getDecorView().setSystemUiVisibility(0);

        webView = new WebView(this);
        webView.setBackgroundColor(Color.rgb(11, 16, 18));
        webView.setOverScrollMode(View.OVER_SCROLL_NEVER);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        webView.addJavascriptInterface(new WallPulseBridge(), "AndroidWallPulse");
        webView.setWebViewClient(new PocketWebViewClient());
        webView.setWebChromeClient(new PocketChromeClient());
        setContentView(webView);
        webView.loadUrl(APP_ORIGIN + "/mobile/index.html");
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != FILE_PICKER_REQUEST || fileCallback == null) return;
        Uri[] results = null;
        if (resultCode == RESULT_OK && data != null) {
            if (data.getClipData() != null) {
                int count = data.getClipData().getItemCount();
                results = new Uri[count];
                for (int i = 0; i < count; i++) results[i] = data.getClipData().getItemAt(i).getUri();
            } else if (data.getData() != null) {
                results = new Uri[]{data.getData()};
            }
        }
        fileCallback.onReceiveValue(results);
        fileCallback = null;
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) webView.goBack();
        else super.onBackPressed();
    }

    private final class PocketChromeClient extends WebChromeClient {
        @Override
        public boolean onShowFileChooser(WebView view, ValueCallback<Uri[]> callback, FileChooserParams params) {
            if (fileCallback != null) fileCallback.onReceiveValue(null);
            fileCallback = callback;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"image/*", "video/*"});
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            try {
                startActivityForResult(intent, FILE_PICKER_REQUEST);
                return true;
            } catch (Exception error) {
                fileCallback = null;
                callback.onReceiveValue(null);
                Toast.makeText(MainActivity.this, "Не удалось открыть файлы", Toast.LENGTH_SHORT).show();
                return false;
            }
        }

        @Override
        public boolean onConsoleMessage(ConsoleMessage message) {
            return true;
        }
    }

    private final class PocketWebViewClient extends WebViewClient {
        @Override
        public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
            Uri uri = request.getUrl();
            String scheme = uri.getScheme();
            if ("content".equals(scheme) || "file".equals(scheme) || "blob".equals(scheme) || "data".equals(scheme)) return null;
            if (!"https".equals(scheme)) return blockedResponse();
            if (!APP_ORIGIN.equals(uri.getScheme() + "://" + uri.getHost())) return blockedResponse();
            String path = uri.getPath();
            if (path == null || !path.startsWith("/mobile/")) return blockedResponse();
            String assetName = path.substring("/mobile/".length());
            if (assetName.isEmpty() || assetName.contains("..") || assetName.contains("\\")) return blockedResponse();
            String mime = mimeType(assetName);
            try {
                InputStream stream = getAssets().open("mobile/" + assetName);
                return new WebResourceResponse(mime, "UTF-8", stream);
            } catch (IOException ignored) {
                return blockedResponse();
            }
        }

        @Override
        public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            Uri uri = request.getUrl();
            if (APP_ORIGIN.equals(uri.getScheme() + "://" + uri.getHost())) return false;
            return true;
        }
    }

    private static WebResourceResponse blockedResponse() {
        return new WebResourceResponse("text/plain", "UTF-8", 404, "Not Found", null,
                new ByteArrayInputStream(new byte[0]));
    }

    private static String mimeType(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".html")) return "text/html";
        if (lower.endsWith(".css")) return "text/css";
        if (lower.endsWith(".js")) return "application/javascript";
        if (lower.endsWith(".svg")) return "image/svg+xml";
        if (lower.endsWith(".webmanifest")) return "application/manifest+json";
        return "application/octet-stream";
    }

    private final class WallPulseBridge {
        @JavascriptInterface
        public synchronized boolean beginSave(String name, String mime) {
            cancelPendingSave();
            String safeName = SAFE_NAME.matcher(name == null ? "wallpaper.jpg" : name).replaceAll("_");
            if (safeName.isEmpty()) safeName = "wallpaper.jpg";
            pendingMime = mime == null || mime.isEmpty() ? "application/octet-stream" : mime;
            try {
                ContentValues values = new ContentValues();
                values.put(MediaStore.MediaColumns.DISPLAY_NAME, safeName);
                values.put(MediaStore.MediaColumns.MIME_TYPE, pendingMime);
                values.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/WallPulse");
                values.put(MediaStore.MediaColumns.IS_PENDING, 1);
                pendingUri = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                if (pendingUri == null) throw new IOException("Не удалось создать файл");
                pendingOutput = getContentResolver().openOutputStream(pendingUri);
                if (pendingOutput == null) throw new IOException("Не удалось открыть файл");
                return true;
            } catch (Exception error) {
                cancelPendingSave();
                showToast("Не удалось начать сохранение");
                return false;
            }
        }

        @JavascriptInterface
        public synchronized boolean writeChunk(String encoded) {
            if (pendingOutput == null || encoded == null) return false;
            try {
                pendingOutput.write(android.util.Base64.decode(encoded, android.util.Base64.DEFAULT));
                return true;
            } catch (Exception error) {
                cancelPendingSave();
                return false;
            }
        }

        @JavascriptInterface
        public synchronized boolean finishSave() {
            if (pendingUri == null || pendingOutput == null) return false;
            Uri uri = pendingUri;
            String mime = pendingMime;
            try {
                pendingOutput.close();
                pendingOutput = null;
                ContentValues ready = new ContentValues();
                ready.put(MediaStore.MediaColumns.IS_PENDING, 0);
                getContentResolver().update(uri, ready, null, null);
                pendingUri = null;
                pendingMime = null;
                if (mime.startsWith("video/") || "image/gif".equals(mime)) {
                    getSharedPreferences("wallpulse-pocket", MODE_PRIVATE).edit()
                            .putString("live-uri", uri.toString())
                            .putString("live-mime", mime)
                            .apply();
                    openLiveWallpaperPicker();
                } else if (mime.startsWith("image/")) {
                    openWallpaperPicker(uri, mime);
                } else {
                    showToast("Сохранено в Загрузки/WallPulse");
                }
                return true;
            } catch (Exception error) {
                pendingUri = null;
                pendingMime = null;
                if (pendingOutput != null) try { pendingOutput.close(); } catch (IOException ignored) { }
                pendingOutput = null;
                getContentResolver().delete(uri, null, null);
                showToast("Не удалось завершить сохранение");
                return false;
            }
        }

        @JavascriptInterface
        public synchronized void cancelSave() {
            cancelPendingSave();
        }
    }

    private void cancelPendingSave() {
        if (pendingOutput != null) {
            try { pendingOutput.close(); } catch (IOException ignored) { }
            pendingOutput = null;
        }
        if (pendingUri != null) {
            getContentResolver().delete(pendingUri, null, null);
            pendingUri = null;
        }
        pendingMime = null;
    }

    private void openLiveWallpaperPicker() {
        runOnUiThread(() -> {
            try {
                Intent intent = new Intent(WallpaperManager.ACTION_CHANGE_LIVE_WALLPAPER);
                intent.putExtra(WallpaperManager.EXTRA_LIVE_WALLPAPER_COMPONENT,
                        new ComponentName(MainActivity.this, VideoWallpaperService.class));
                startActivity(intent);
            } catch (Exception error) {
                Toast.makeText(this, "Не удалось открыть системный выбор живых обоев", Toast.LENGTH_LONG).show();
            }
        });
    }

    private void openWallpaperPicker(Uri uri, String mime) {
        runOnUiThread(() -> {
            try {
                Intent attach = new Intent(Intent.ACTION_ATTACH_DATA);
                attach.setDataAndType(uri, mime);
                attach.putExtra("mimeType", mime);
                attach.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                startActivity(Intent.createChooser(attach, "Установить обои"));
            } catch (Exception ignored) {
                Toast.makeText(this, "Сохранено в Загрузки/WallPulse", Toast.LENGTH_LONG).show();
            }
        });
    }

    private void showToast(String text) {
        runOnUiThread(() -> Toast.makeText(MainActivity.this, text, Toast.LENGTH_LONG).show());
    }

    @Override
    protected void onDestroy() {
        cancelPendingSave();
        if (webView != null) webView.destroy();
        super.onDestroy();
    }

}
