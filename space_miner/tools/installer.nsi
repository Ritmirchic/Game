Unicode true
!include "MUI2.nsh"
Name "SPACE MINER - The Helix Expedition"
OutFile "../../releases/SPACE_MINER_SETUP.exe"
InstallDir "$LOCALAPPDATA\Programs\SpaceMiner"
RequestExecutionLevel user
SetCompressor /SOLID lzma
Icon "../assets/icon.ico"
!define MUI_ICON "../assets/icon.ico"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\SPACE_MINER.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Section "Space Miner"
 SetOutPath "$INSTDIR"
 File "../../releases/SPACE_MINER.exe"
 File "../PLAYER_GUIDE.md"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateDirectory "$SMPROGRAMS\Space Miner"
 CreateShortcut "$SMPROGRAMS\Space Miner\Space Miner.lnk" "$INSTDIR\SPACE_MINER.exe"
 CreateShortcut "$SMPROGRAMS\Space Miner\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner" "DisplayName" "SPACE MINER"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner" "DisplayIcon" "$INSTDIR\SPACE_MINER.exe"
SectionEnd
Section "Uninstall"
 Delete "$INSTDIR\SPACE_MINER.exe"
 Delete "$INSTDIR\PLAYER_GUIDE.md"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Space Miner\Space Miner.lnk"
 Delete "$SMPROGRAMS\Space Miner\Uninstall.lnk"
 RMDir "$SMPROGRAMS\Space Miner"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner"
 ; Intentionally retain saved expeditions in AppData.
SectionEnd
