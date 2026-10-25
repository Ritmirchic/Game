; SPACE MINER — per-user Windows x64 installer
Unicode true
SetCompressor /SOLID lzma
SetCompressorDictSize 64

!define PRODUCT "SPACE MINER"
!define VERSION "1.0.0"
!define PUBLISHER "Ceres Interactive"
!define INSTALL_REG "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner"

!ifndef ROOT
!define ROOT "."
!endif

Name "${PRODUCT} ${VERSION}"
OutFile "${ROOT}\dist\SPACE_MINER_SETUP.exe"
InstallDir "$LOCALAPPDATA\Programs\SpaceMiner"
InstallDirRegKey HKCU "Software\SpaceMiner" "InstallDir"
RequestExecutionLevel user
Icon "${ROOT}\assets\art\ui\app_icon.ico"
UninstallIcon "${ROOT}\assets\art\ui\app_icon.ico"
BrandingText "CERES RELAY  •  DEEP BELT OPERATIONS"
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT} Setup"
VIAddVersionKey "LegalCopyright" "© 2026 ${PUBLISHER}"

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\SPACE_MINER.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch SPACE MINER"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "SPACE MINER (required)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /oname=SPACE_MINER.exe "${ROOT}\dist\SPACE_MINER.exe"
  File /oname=PLAYER_GUIDE.txt "${ROOT}\docs\PLAYER_GUIDE.txt"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\SpaceMiner" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${INSTALL_REG}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKCU "${INSTALL_REG}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${INSTALL_REG}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${INSTALL_REG}" "DisplayIcon" "$INSTDIR\SPACE_MINER.exe"
  WriteRegStr HKCU "${INSTALL_REG}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${INSTALL_REG}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKCU "${INSTALL_REG}" "NoModify" 1
  WriteRegDWORD HKCU "${INSTALL_REG}" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\SpaceMiner"
  CreateShortcut "$SMPROGRAMS\SpaceMiner\SPACE MINER.lnk" "$INSTDIR\SPACE_MINER.exe" "" "$INSTDIR\SPACE_MINER.exe" 0
  CreateShortcut "$SMPROGRAMS\SpaceMiner\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section /o "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\SPACE MINER.lnk" "$INSTDIR\SPACE_MINER.exe" "" "$INSTDIR\SPACE_MINER.exe" 0
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\SPACE_MINER.exe"
  Delete "$INSTDIR\PLAYER_GUIDE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$SMPROGRAMS\SpaceMiner\SPACE MINER.lnk"
  Delete "$SMPROGRAMS\SpaceMiner\Uninstall.lnk"
  RMDir "$SMPROGRAMS\SpaceMiner"
  Delete "$DESKTOP\SPACE MINER.lnk"
  DeleteRegKey HKCU "${INSTALL_REG}"
  DeleteRegKey HKCU "Software\SpaceMiner"
  RMDir "$INSTDIR"
SectionEnd
