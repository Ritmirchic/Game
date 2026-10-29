package com.wallpulse.pocket;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Movie;
import android.graphics.Paint;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.service.wallpaper.WallpaperService;
import android.view.SurfaceHolder;
import android.widget.Toast;

import java.io.InputStream;

public final class VideoWallpaperService extends WallpaperService {
    @Override
    public Engine onCreateEngine() {
        return new PlaybackEngine();
    }

    private final class PlaybackEngine extends Engine {
        private final Handler handler = new Handler(Looper.getMainLooper());
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
        private Uri mediaUri;
        private String mediaMime;
        private MediaPlayer player;
        private Movie movie;
        private boolean surfaceReady;
        private boolean errorShown;
        private long gifStartTime;

        private final Runnable gifFrame = new Runnable() {
            @Override public void run() {
                if (!isVisible() || !surfaceReady || movie == null) return;
                drawGifFrame();
                handler.postDelayed(this, 33L);
            }
        };

        PlaybackEngine() {
            String uri = getSharedPreferences("wallpulse-pocket", MODE_PRIVATE).getString("live-uri", null);
            mediaMime = getSharedPreferences("wallpulse-pocket", MODE_PRIVATE).getString("live-mime", "");
            if (uri != null) mediaUri = Uri.parse(uri);
            setTouchEventsEnabled(false);
        }

        @Override
        public void onSurfaceCreated(SurfaceHolder holder) {
            super.onSurfaceCreated(holder);
            surfaceReady = true;
            startWallpaperMedia();
        }

        @Override
        public void onSurfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            super.onSurfaceChanged(holder, format, width, height);
            if (player != null) {
                try {
                    player.setDisplay(holder);
                    player.setVideoScalingMode(MediaPlayer.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING);
                } catch (IllegalStateException ignored) { }
            }
            if (movie != null && isVisible()) drawGifFrame();
        }

        @Override
        public void onSurfaceDestroyed(SurfaceHolder holder) {
            surfaceReady = false;
            handler.removeCallbacks(gifFrame);
            releasePlayer();
            movie = null;
            super.onSurfaceDestroyed(holder);
        }

        @Override
        public void onVisibilityChanged(boolean visible) {
            super.onVisibilityChanged(visible);
            if (visible) startWallpaperMedia();
            else {
                handler.removeCallbacks(gifFrame);
                if (player != null && player.isPlaying()) {
                    try { player.pause(); } catch (IllegalStateException ignored) { }
                }
            }
        }

        @Override
        public void onDestroy() {
            handler.removeCallbacks(gifFrame);
            releasePlayer();
            movie = null;
            super.onDestroy();
        }

        private void startWallpaperMedia() {
            if (!surfaceReady || !isVisible()) return;
            if (mediaUri == null) {
                drawMessage("Добавьте видео или GIF в WallPulse");
                return;
            }
            if (mediaMime != null && mediaMime.startsWith("video/")) startVideo();
            else if ("image/gif".equals(mediaMime) || mediaUri.getLastPathSegment() != null && mediaUri.getLastPathSegment().toLowerCase().endsWith(".gif")) startGif();
            else drawMessage("Этот файл не поддерживается как живые обои");
        }

        private void startVideo() {
            if (player != null) {
                if (player.isPlaying()) return;
                try { player.start(); } catch (IllegalStateException ignored) { }
                return;
            }
            MediaPlayer candidate = new MediaPlayer();
            player = candidate;
            try {
                candidate.setVolume(0f, 0f);
                candidate.setLooping(true);
                candidate.setOnPreparedListener(prepared -> {
                    if (player != prepared) { prepared.release(); return; }
                    try { prepared.setVideoScalingMode(MediaPlayer.VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING); }
                    catch (IllegalStateException ignored) { }
                    if (isVisible() && surfaceReady) {
                        try { prepared.start(); }
                        catch (IllegalStateException error) { showPlaybackError(); }
                    }
                });
                candidate.setOnErrorListener((failed, what, extra) -> {
                    if (player == failed) releasePlayer();
                    showPlaybackError();
                    return true;
                });
                candidate.setDataSource(VideoWallpaperService.this, mediaUri);
                candidate.setDisplay(getSurfaceHolder());
                candidate.prepareAsync();
            } catch (Exception error) {
                releasePlayer();
                showPlaybackError();
            }
        }

        @SuppressWarnings("deprecation")
        private void startGif() {
            if (movie == null) {
                try (InputStream input = getContentResolver().openInputStream(mediaUri)) {
                    if (input != null) movie = Movie.decodeStream(input);
                } catch (Exception ignored) { movie = null; }
                if (movie == null || movie.width() <= 0 || movie.height() <= 0) {
                    showPlaybackError();
                    return;
                }
            }
            gifStartTime = SystemClock.uptimeMillis();
            handler.removeCallbacks(gifFrame);
            handler.post(gifFrame);
        }

        private void drawGifFrame() {
            if (movie == null || !surfaceReady) return;
            Canvas canvas = null;
            try {
                canvas = getSurfaceHolder().lockCanvas();
                if (canvas == null) return;
                canvas.drawColor(Color.BLACK);
                int duration = Math.max(1, movie.duration());
                movie.setTime((int) ((SystemClock.uptimeMillis() - gifStartTime) % duration));
                float scale = Math.max((float) canvas.getWidth() / movie.width(), (float) canvas.getHeight() / movie.height());
                float scaledWidth = movie.width() * scale;
                float scaledHeight = movie.height() * scale;
                canvas.save();
                canvas.translate((canvas.getWidth() - scaledWidth) / 2f, (canvas.getHeight() - scaledHeight) / 2f);
                canvas.scale(scale, scale);
                movie.draw(canvas, 0f, 0f);
                canvas.restore();
            } catch (Exception ignored) {
            } finally {
                if (canvas != null) {
                    try { getSurfaceHolder().unlockCanvasAndPost(canvas); } catch (Exception ignored) { }
                }
            }
        }

        private void showPlaybackError() {
            if (!errorShown) {
                errorShown = true;
                handler.post(() -> Toast.makeText(VideoWallpaperService.this,
                        "Не удалось воспроизвести файл. Попробуйте MP4 (H.264) или GIF.", Toast.LENGTH_LONG).show());
            }
            drawMessage("Формат не поддерживается · попробуйте MP4 / GIF");
        }

        private void drawMessage(String text) {
            if (!surfaceReady) return;
            Canvas canvas = null;
            try {
                canvas = getSurfaceHolder().lockCanvas();
                if (canvas == null) return;
                canvas.drawColor(Color.rgb(11, 16, 18));
                paint.setColor(Color.WHITE);
                paint.setTextSize(30f);
                float x = (canvas.getWidth() - paint.measureText(text)) / 2f;
                float y = canvas.getHeight() / 2f;
                canvas.drawText(text, Math.max(20f, x), y, paint);
            } catch (Exception ignored) {
            } finally {
                if (canvas != null) {
                    try { getSurfaceHolder().unlockCanvasAndPost(canvas); } catch (Exception ignored) { }
                }
            }
        }

        private void releasePlayer() {
            MediaPlayer old = player;
            player = null;
            if (old != null) {
                try { old.setOnPreparedListener(null); old.setOnErrorListener(null); old.stop(); }
                catch (Exception ignored) { }
                try { old.release(); } catch (Exception ignored) { }
            }
        }
    }
}
