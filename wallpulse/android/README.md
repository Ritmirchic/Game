# WallPulse Pocket for Android

Native installable Android wrapper for the WallPulse Pocket mobile app. The UI is bundled inside the APK and works offline; photo/GIF/video imports stay in the app's local browser storage.

## Android features

- Android 10 (API 29) and newer; portrait-first UI.
- Opens Android's document picker for images and videos.
- Saves files to `Downloads/WallPulse` without broad storage permissions.
- After exporting an image, opens the Android system **Set wallpaper** chooser. Videos are saved so they can be opened in a compatible live-wallpaper app.
- The APK does not require network access and keeps imported media on-device.

## Download APK

The GitHub Actions workflow builds and verifies the signed debug APK, then publishes it as a direct release asset:

`https://github.com/Ritmirchic/Game/releases/download/wallpulse-pocket-android-v1.0.0/WallPulse-Pocket-Android.apk`

The APK is signed with Android's standard debug certificate for direct sideload/testing; it is not a Google Play production signing key. On the phone, download and open the APK, allow installation from the browser/files app when Android asks, then continue installation.

## Build locally

Requires JDK 17, Gradle 8.9, Android SDK platform 35 and Build Tools 35.0.0:

```bash
cd wallpulse/android
gradle --no-daemon :app:assembleDebug
```

The Gradle `syncPocketWebApp` task embeds the maintained PWA files from `../mobile` into Android assets before the APK is built.
