Unicode True
!include "MUI2.nsh"
Name "SPACE MINER: BUILD & CONQUER"
OutFile "..\dist\SPACE_MINER_SETUP.exe"
InstallDir "$LOCALAPPDATA\SpaceMinerBuildConquer"
RequestExecutionLevel user
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Section "Game (required)"
 SectionIn RO
 SetOutPath "$INSTDIR"
 File "..\dist\SPACE_MINER.exe"
 File "..\dist\SPACE_MINER.pck"
 File "..\dist\LICENSES.md"
 File "..\dist\GODOT_COPYRIGHT.txt"
 File "..\dist\GODOT_LICENSE.txt"
 File "..\dist\README.md"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 CreateDirectory "$SMPROGRAMS\Space Miner Build Conquer"
 CreateShortcut "$SMPROGRAMS\Space Miner Build Conquer\Space Miner.lnk" "$INSTDIR\SPACE_MINER.exe"
 CreateShortcut "$SMPROGRAMS\Space Miner Build Conquer\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMinerBuildConquer" "DisplayName" "SPACE MINER: BUILD & CONQUER"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMinerBuildConquer" "UninstallString" '$"$INSTDIR\Uninstall.exe$"'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMinerBuildConquer" "DisplayVersion" "0.1.0"
SectionEnd
Section /o "Desktop shortcut"
 CreateShortcut "$DESKTOP\Space Miner Build Conquer.lnk" "$INSTDIR\SPACE_MINER.exe"
SectionEnd
Section "Uninstall"
 Delete "$INSTDIR\SPACE_MINER.exe"
 Delete "$INSTDIR\SPACE_MINER.pck"
 Delete "$INSTDIR\LICENSES.md"
 Delete "$INSTDIR\README.md"
 Delete "$INSTDIR\GODOT_COPYRIGHT.txt"
 Delete "$INSTDIR\GODOT_LICENSE.txt"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR"
 Delete "$DESKTOP\Space Miner Build Conquer.lnk"
 Delete "$SMPROGRAMS\Space Miner Build Conquer\Space Miner.lnk"
 Delete "$SMPROGRAMS\Space Miner Build Conquer\Uninstall.lnk"
 RMDir "$SMPROGRAMS\Space Miner Build Conquer"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMinerBuildConquer"
SectionEnd
