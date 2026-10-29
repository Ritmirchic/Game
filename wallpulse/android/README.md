# WallPulse Pocket for Android

Native Android companion app with a bundled offline UI and a real system `WallpaperService` for animated MP4/GIF wallpapers.

## Установка своих живых обоев

1. Откройте приложение и нажмите **＋**, затем выберите MP4 или GIF.
2. Откройте добавленную карточку и нажмите **«Установить живые обои»**.
3. Android откроет системный предпросмотр WallPulse. Нажмите **«Установить»** и выберите домашний экран, экран блокировки или оба — доступные варианты зависят от версии Android и производителя.

Сервис воспроизводит видео без звука по кругу. GIF отображается с масштабированием по ширине/высоте. Анимация приостанавливается, когда обои не видны, чтобы не расходовать батарею в фоне. На некоторых устройствах и в режимах энергосбережения Android может ограничивать частоту кадров.

**Форматы:** рекомендуются MP4 с H.264/AAC (звук отключён) или обычный GIF. Совместимость видео зависит от кодеков телефона. Статические фото перед системным окном автоматически обрезаются ровно по центру под пропорции экрана — без сжатия/растяжения; оригинал сохраняется отдельно.

## Скачать APK

[Прямая ссылка на установочный APK](https://github.com/Ritmirchic/Game/releases/download/wallpulse-pocket-android-v1.2.0/WallPulse-Pocket-Android.apk) · [Страница релиза](https://github.com/Ritmirchic/Game/releases/tag/wallpulse-pocket-android-v1.2.0)

Android 10 (API 29) и новее. APK подписан стандартным Android debug-сертификатом для sideload-тестирования; это не ключ Google Play. При первом открытии Android может попросить разрешить установку приложений для используемого браузера или файлового менеджера.

## Build locally

Requires JDK 17, Gradle 8.9, Android SDK platform 35 and Build Tools 35.0.0:

```bash
cd wallpulse/android
gradle --no-daemon :app:assembleDebug
```

Gradle task `syncPocketWebApp` embeds the maintained UI from `../mobile` before the APK is built.
