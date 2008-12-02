BGGradient 

XPStyle on
WindowIcon on
Icon "..\Osmo4.ico"
UninstallIcon "..\Osmo4.ico"

!define GPAC_VERSION	0.4.5
!define /date RELDATE "%Y%m%d"

Name "GPAC Framework ${GPAC_VERSION}"
OutFile "GPAC.Framework.Setup-${RELDATE}.exe"

InstallDir $PROGRAMFILES\GPAC
InstallDirRegKey HKLM SOFTWARE\GPAC "Install_Dir"


LicenseText "GPAC Licence"
LicenseData "..\..\..\COPYING"

DirText "This will install the GPAC Framework on your computer. Choose a directory"

InstType Normal


ComponentText "This will install the GPAC Framework on your computer. Select which optional things you want installed."

Function InsertGDIPLUS
   Push $R0
   Push $R1
   ReadRegStr $R0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
   StrCmp $R0 "" 0 lbl_winnt

   ;NOT NT
   ReadRegStr $R0 HKLM SOFTWARE\Microsoft\Windows\CurrentVersion VersionNumber

   StrCpy $R1 $R0 1
   ; win95, NOT SUPPORTED
   StrCmp $R1 '4' 0 lbl_err_95
   StrCpy $R1 $R0 3
   StrCmp $R1 '4.0' lbl_err_95
   ;winME or 98 otherwise
   StrCmp $R1 '4.9' lbl_add lbl_add

lbl_err_nt:
   MessageBox MB_OK "Microsoft GDI+ cannot be installed on NT 3 Systems"
   Goto lbl_done

lbl_err_95:
   MessageBox MB_OK "Microsoft GDI+ cannot be installed on Windows 95 and older Systems"
   Goto lbl_done

lbl_winnt:
   StrCpy $R1 $R0 1
   StrCmp $R1 '3' lbl_err_nt
   StrCmp $R1 '4' lbl_add
   StrCpy $R1 $R0 3
   StrCmp $R1 '5.0' lbl_add	;2000
   StrCmp $R1 '5.1' lbl_xp	;XP
   StrCmp $R1 '5.2' lbl_done	;.NET server

lbl_add:
   File ".\Gdiplus.dll"

lbl_xp:
   File "..\gm_gdip_raster.dll"

lbl_done:
FunctionEnd

;osmo4 install
Section "Osmo4/GPAC Player"
  SectionIn RO
  SetOutPath $INSTDIR

  File /oname=ReadMe.txt "..\..\..\README"
  File /oname=License.txt "..\..\..\COPYING"
  File /oname=Changelog.txt "..\..\..\Changelog"
  File "..\..\..\doc\configuration.html"
  File "..\..\..\doc\gpac.mp4"

  File "..\Osmo4.exe"
  File "..\Osmo4.ico"
  File "..\libgpac.dll"
  File "..\gm_dummy_in.dll"
  File "..\gm_dx_hw.dll"
  File "..\js32.dll"

  ;create default cache
  SetOutPath $INSTDIR\cache

  SetOutPath $INSTDIR


  WriteRegStr HKLM SOFTWARE\GPAC "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Osmo4" "DisplayName" "Osmo4/GPAC (remove only)"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Osmo4" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"

SectionEnd

SubSection "GPAC Plugins"


;
;	2 install modes, normal one and full one

Section "MPEG-4 BIFS Decoder"
  SectionIn 1
  File "..\gm_bifs_dec.dll"
SectionEnd

Section "MPEG-4 ODF Decoder"
  SectionIn 1
  File "..\gm_odf_dec.dll"
SectionEnd

Section "MPEG-4 LASeR Decoder"
  SectionIn 1
  File "..\gm_laser_dec.dll"
SectionEnd

Section "MPEG-4 SAF Demultiplexer"
  SectionIn 1
  File "..\gm_saf_in.dll"
SectionEnd

Section "Generic Scene Description File Loader"
  SectionIn 1
  File "..\gm_ctx_load.dll"
SectionEnd

Section "Image Package (PNG, JPEG, BMP)"
  SectionIn 1
  File "..\gm_img_in.dll"
SectionEnd

Section "AAC Audio support (FAAD decoder, AAC files and Radios)"
  SectionIn 1
  File "..\gm_aac_in.dll"
SectionEnd

Section "MP3 Audio support (MAD decoder, MP3 files and Radios)"
  SectionIn 1
  File "..\gm_mp3_in.dll"
SectionEnd

Section "AC3 Audio support (A52 decoder, AC3 files and Radios)"
  SectionIn 1
  File "..\gm_ac3_in.dll"
SectionEnd

Section "FFMPEG Reader and Decoder"
  SectionIn 1
  File "..\gm_ffmpeg_in.dll"
  File "..\avcodec-51.dll"
  File "..\avformat-51.dll"
  File "..\avutil-49.dll"
SectionEnd

Section "XviD Video Decoder"
  SectionIn 1
  File "..\gm_xvid_dec.dll"
SectionEnd

Section "3GPP AMR NB & WB Speech Decoder"
  SectionIn 1
  File "..\gm_amr_float_dec.dll"
SectionEnd

Section "Subtitle & TimedText Support"
  SectionIn 1
  File "..\gm_timedtext.dll"
SectionEnd

Section "MP4 and 3GPP File Reader"
  SectionIn 1
  File "..\gm_isom_in.dll"
SectionEnd

Section "MPEG-2 TS Reader"
  SectionIn 1
  File "..\gm_mpegts_in.dll"
SectionEnd

Section "Real-Time Streaming (RTP/RTSP/RTP) Support"
  SectionIn 1
  File "..\gm_rtp_in.dll"
SectionEnd

Section "Progressive SVG Support"
  SectionIn 1
  File "..\gm_svg_in.dll"
SectionEnd

Section "GDI+ Rasterizer"
  SectionIn 1
  call InsertGDIPLUS
SectionEnd

Section "GPAC 2D Rasterizer"
  SectionIn 1
  File "..\gm_soft_raster.dll"
SectionEnd

Section "FreeType Font Outliner"
  SectionIn 1
  File "..\gm_ft_font.dll"
SectionEnd

Section "Windows MME Audio Output"
  SectionIn 1
  File "..\gm_wav_out.dll"
SectionEnd

Section "Xiph Ogg Reader - Vorbis and Theora Decoders"
  SectionIn 1
  File "..\gm_ogg_xiph.dll"
SectionEnd


SubSectionEnd

SubSection "Osmo4 Shortcuts"

Section "Add Start Menu Shortcuts"
  SectionIn 1
  CreateDirectory "$SMPROGRAMS\Osmo4"
  CreateShortCut "$SMPROGRAMS\Osmo4\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Osmo4\Osmo4.lnk" "$INSTDIR\Osmo4.exe" "" "$INSTDIR\Osmo4.exe" 0
  CreateShortCut "$SMPROGRAMS\Osmo4\Readme.lnk" "$INSTDIR\ReadMe.txt"
  CreateShortCut "$SMPROGRAMS\Osmo4\License.lnk" "$INSTDIR\License.txt"
  CreateShortCut "$SMPROGRAMS\Osmo4\History.lnk" "$INSTDIR\changelog.txt"
  CreateShortCut "$SMPROGRAMS\Osmo4\Configuration Info.lnk" "$INSTDIR\configuration.html"
SectionEnd

Section "Add shortcut to QuickLaunch"
  SectionIn 1
  CreateShortCut "$QUICKLAUNCH\Osmo4.lnk" "$INSTDIR\Osmo4.exe" "" "$INSTDIR\Osmo4.exe" 0
SectionEnd

Section "Add shortcut to Desktop"
  SectionIn 1
  CreateShortCut "$DESKTOP\Osmo4.lnk" "$INSTDIR\Osmo4.exe" "" "$INSTDIR\Osmo4.exe" 0
SectionEnd

!define SHCNE_ASSOCCHANGED 0x08000000
!define SHCNF_IDLIST 0

Section "Make Osmo4 the default MPEG-4 Player"
  SectionIn 1
  ;write file association
  WriteRegStr HKCR GPAC\mp4\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  WriteRegStr HKCR GPAC\mp4\Shell\open\command "" '$INSTDIR\Osmo4.exe "%L" '
  WriteRegStr HKCR .mp4 "" "GPAC\mp4"

  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'

SectionEnd

Section "Associate 3GPP files (3GP) with Osmo4"
  SectionIn 1
  ;write file association
  WriteRegStr HKCR GPAC\3gp\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  WriteRegStr HKCR GPAC\3gp\Shell\open\command "" '$INSTDIR\Osmo4.exe "%L" '
  WriteRegStr HKCR .3gp "" "GPAC\3gp"
  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
SectionEnd

Section "Associate 3GPP2 files (3G2) with Osmo4"
  SectionIn 1
  ;write file association
  WriteRegStr HKCR GPAC\3g2\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  WriteRegStr HKCR GPAC\3g2\Shell\open\command "" '$INSTDIR\Osmo4.exe "%L" '
  WriteRegStr HKCR .3g2 "" "GPAC\3g2"
  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
SectionEnd

SubSectionEnd


Section "MP4Box (Command-line MPEG-4 tool)"
  SectionIn 1
  SetOutPath $INSTDIR
  File "..\MP4Box.exe"

  Push $INSTDIR
  Call AddToPath
SectionEnd


!define HK_MOZ "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0"

Section "Osmozilla (GPAC Plugin for Mozilla)"
  SectionIn 1
  SetOutPath $INSTDIR\mozilla
  File "..\nposmozilla.dll"
  File "..\nposmozilla.xpt"

  WriteRegStr HKCR GPAC "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Path" "$INSTDIR\mozilla\nposmozilla.dll"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "XPTPath" "$INSTDIR\mozilla\nposmozilla.xpt"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Version" "${GPAC_VERSION}"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Vendor" "GPAC"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Description" "GPAC plugin"
  WriteRegStr HKLM "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "ProductName" "Osmozilla"


SectionEnd


Section "GPAX (GPAC ActiveX Control)"
  SectionIn 1
  SetOutPath $INSTDIR
  File "..\GPAX.dll"
  WriteRegStr HKCR GPAC "InstallDir" "$INSTDIR"
  RegDLL "$INSTDIR\GPAX.dll"
SectionEnd


Section "MP4Client (GPAC Command-line client/grabber)"
  SectionIn 1
  SetOutPath $INSTDIR
  File "..\MP4Client.exe"
SectionEnd


; Function .onInstSuccess
;  MessageBox MB_YESNO "GPAC Framework installation complete. Do you want to launch the Osmo4 player?" IDNO NoLaunch
;    Exec $INSTDIR\Osmo4.exe
;  NoLaunch:
; FunctionEnd





; uninstall stuff

UninstallText "This will uninstall OSMO4/GPAC from your computer. Hit next to continue."

; special uninstall section.
Section "Uninstall"
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Osmo4"
  DeleteRegKey HKLM SOFTWARE\GPAC
  DeleteRegKey HKCR mp4file\DefaultIcon
  DeleteRegKey HKCR mp4file\shell
  DeleteRegKey HKCR .mp4
  DeleteRegKey HKCR btfile\shell
  DeleteRegKey HKCR .bt

  Delete $INSTDIR\cache\*.*
  RMDir "$INSTDIR\cache"
  Delete $INSTDIR\mozilla\*.*
  RMDir "$INSTDIR\mozilla"
  Delete $INSTDIR\*.*
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Osmo4\*.*"
  RMDir "$SMPROGRAMS\Osmo4"
  Delete "$QUICKLAUNCH\Osmo4.lnk"
  Delete "$DESKTOP\Osmo4.lnk"
  UnRegDLL "$INSTDIR\GPAX.dll"
  Push $INSTDIR
  Call un.RemoveFromPath

SectionEnd

;path modif functions
!verbose 3
!include "WinMessages.NSH"
!verbose 4

; AddToPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot

Function AddToPath
  Exch $0
  Push $1
  Push $2
  Push $3

  # don't add if the path doesn't exist
  IfFileExists $0 "" AddToPath_done

  ReadEnvStr $1 PATH
  Push "$1;"
  Push "$0;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$0\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  GetFullPathName /SHORT $3 $0
  Push "$1;"
  Push "$3;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done
  Push "$1;"
  Push "$3\;"
  Call StrStr
  Pop $2
  StrCmp $2 "" "" AddToPath_done

  Call IsNT
  Pop $1
  StrCmp $1 1 AddToPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" a
    FileSeek $1 -1 END
    FileReadByte $1 $2
    IntCmp $2 26 0 +2 +2 # DOS EOF
      FileSeek $1 -1 END # write over EOF
    FileWrite $1 "$\r$\nSET PATH=%PATH%;$3$\r$\n"
    FileClose $1
    SetRebootFlag true
    Goto AddToPath_done

  AddToPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $2 $1 1 -1 # copy last char
    StrCmp $2 ";" 0 +2 # if last char == ;
      StrCpy $1 $1 -1 # remove last char
    StrCmp $1 "" AddToPath_NTdoIt
      StrCpy $0 "$1;$0"
    AddToPath_NTdoIt:
      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $0
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  AddToPath_done:
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

; RemoveFromPath - Remove a given dir from the path
;     Input: head of the stack

Function un.RemoveFromPath
  Exch $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $5
  Push $6

  IntFmt $6 "%c" 26 # DOS EOF

  Call un.IsNT
  Pop $1
  StrCmp $1 1 unRemoveFromPath_NT
    ; Not on NT
    StrCpy $1 $WINDIR 2
    FileOpen $1 "$1\autoexec.bat" r
    GetTempFileName $4
    FileOpen $2 $4 w
    GetFullPathName /SHORT $0 $0
    StrCpy $0 "SET PATH=%PATH%;$0"
    Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoop:
      FileRead $1 $3
      StrCpy $5 $3 1 -1 # read last char
      StrCmp $5 $6 0 +2 # if DOS EOF
        StrCpy $3 $3 -1 # remove DOS EOF so we can compare
      StrCmp $3 "$0$\r$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0$\n" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "$0" unRemoveFromPath_dosLoopRemoveLine
      StrCmp $3 "" unRemoveFromPath_dosLoopEnd
      FileWrite $2 $3
      Goto unRemoveFromPath_dosLoop
      unRemoveFromPath_dosLoopRemoveLine:
        SetRebootFlag true
        Goto unRemoveFromPath_dosLoop

    unRemoveFromPath_dosLoopEnd:
      FileClose $2
      FileClose $1
      StrCpy $1 $WINDIR 2
      Delete "$1\autoexec.bat"
      CopyFiles /SILENT $4 "$1\autoexec.bat"
      Delete $4
      Goto unRemoveFromPath_done

  unRemoveFromPath_NT:
    ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH"
    StrCpy $5 $1 1 -1 # copy last char
    StrCmp $5 ";" +2 # if last char != ;
      StrCpy $1 "$1;" # append ;
    Push $1
    Push "$0;"
    Call un.StrStr ; Find `$0;` in $1
    Pop $2 ; pos of our dir
    StrCmp $2 "" unRemoveFromPath_done
      ; else, it is in path
      # $0 - path to add
      # $1 - path var
      StrLen $3 "$0;"
      StrLen $4 $2
      StrCpy $5 $1 -$4 # $5 is now the part before the path to remove
      StrCpy $6 $2 "" $3 # $6 is now the part after the path to remove
      StrCpy $3 $5$6

      StrCpy $5 $3 1 -1 # copy last char
      StrCmp $5 ";" 0 +2 # if last char == ;
        StrCpy $3 $3 -1 # remove last char

      WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "PATH" $3
      SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  unRemoveFromPath_done:
    Pop $6
    Pop $5
    Pop $4
    Pop $3
    Pop $2
    Pop $1
    Pop $0
FunctionEnd

###########################################
#            Utility Functions            #
###########################################

; IsNT
; no input
; output, top of the stack = 1 if NT or 0 if not
;
; Usage:
;   Call IsNT
;   Pop $R0
;  ($R0 at this point is 1 or 0)

!macro IsNT un
Function ${un}IsNT
  Push $0
  ReadRegStr $0 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
  StrCmp $0 "" 0 IsNT_yes
  ; we are not NT.
  Pop $0
  Push 0
  Return

  IsNT_yes:
    ; NT!!!
    Pop $0
    Push 1
FunctionEnd
!macroend
!insertmacro IsNT ""
!insertmacro IsNT "un."

; StrStr
; input, top of stack = string to search for
;        top of stack-1 = string to search in
; output, top of stack (replaces with the portion of the string remaining)
; modifies no other variables.
;
; Usage:
;   Push "this is a long ass string"
;   Push "ass"
;   Call StrStr
;   Pop $R0
;  ($R0 at this point is "ass string")

!macro StrStr un
Function ${un}StrStr
Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
done:
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd
!macroend
!insertmacro StrStr ""
!insertmacro StrStr "un."

