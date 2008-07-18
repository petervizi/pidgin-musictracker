; The name of the installer
Name "MusicTracker Plugin for Pidgin"

; The file to write
OutFile "musictracker-0.4.exe"

; The default installation directory
InstallDir $PROGRAMFILES\Pidgin

;--------------------------------

; Pages

Page directory
Page instfiles

;--------------------------------

; The stuff to install
Section "" ;No components page, name is not important

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put files there
  File wmp9.dll
  File /oname=plugins\musictracker.dll musictracker.dll
  
SectionEnd ; end the section

