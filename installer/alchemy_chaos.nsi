; ==============================================================================
; ALCHEMY CHAOS - Windows 64-bit Installer (NSIS 3.10 Modern UI 2)
; ==============================================================================
Unicode True
SetCompressor /SOLID lzma
SetCompressorDictSize 32

!include "MUI2.nsh"

!define APP_NAME "ALCHEMY CHAOS"
!define APP_SLUG "AlchemyChaos"
!define APP_PUBLISHER "Sanctum Hermetic Studios"
!define APP_VERSION "1.0.0"
!define APP_EXE "ALCHEMY_CHAOS.exe"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_SLUG}"

Name "${APP_NAME}"
OutFile "..\ALCHEMY_CHAOS_SETUP.exe"
InstallDir "$LOCALAPPDATA\Programs\Alchemy Chaos"
InstallDirRegKey HKCU "Software\${APP_SLUG}" "InstallDir"
RequestExecutionLevel user
BrandingText "${APP_NAME} v${APP_VERSION} - ${APP_PUBLISHER}"

; --- Custom Artwork & Icons ---
!define MUI_ICON "..\alchemy_chaos\assets\branding\app_icon.ico"
!define MUI_UNICON "..\alchemy_chaos\assets\branding\app_icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "wizard_sidebar.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "wizard_sidebar.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header_banner.bmp"
!define MUI_HEADERIMAGE_RIGHT
!define MUI_ABORTWARNING

; --- Finish Page Launch Option ---
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ALCHEMY CHAOS now"

; --- Wizard Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Win32 PE Version Metadata on Setup EXE ---
VIProductVersion "1.0.0.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${APP_NAME} Setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${APP_NAME} Windows 64-bit Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "1.0.0.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright (C) 2026 ${APP_PUBLISHER}"

Section "ALCHEMY CHAOS Core Game" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "..\ALCHEMY_CHAOS.exe"
  File "..\alchemy_chaos\assets\branding\app_icon.ico"
  File /oname=README.md "..\README.md"

  ; Persist install directory & Write uninstaller
  WriteRegStr HKCU "Software\${APP_SLUG}" "InstallDir" "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Start Menu & Desktop Shortcuts
  CreateDirectory "$SMPROGRAMS\Alchemy Chaos"
  CreateShortcut "$SMPROGRAMS\Alchemy Chaos\ALCHEMY CHAOS.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\app_icon.ico" 0
  CreateShortcut "$SMPROGRAMS\Alchemy Chaos\Uninstall ALCHEMY CHAOS.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\app_icon.ico" 0
  CreateShortcut "$DESKTOP\ALCHEMY CHAOS.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\app_icon.ico" 0

  ; Windows Add/Remove Programs (Apps & Features) Registration
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APP_EXE},0"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" 73512
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\${APP_EXE}"
  Delete "$INSTDIR\app_icon.ico"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Alchemy Chaos\ALCHEMY CHAOS.lnk"
  Delete "$SMPROGRAMS\Alchemy Chaos\Uninstall ALCHEMY CHAOS.lnk"
  RMDir "$SMPROGRAMS\Alchemy Chaos"
  Delete "$DESKTOP\ALCHEMY CHAOS.lnk"

  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\${APP_SLUG}"
SectionEnd
