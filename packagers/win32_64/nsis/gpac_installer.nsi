;--------------------------------
;General
!define GPAC_VERSION	0.5.1-DEV
!include default.out

!define GPAC_ROOT ..\..\..
!ifdef IS_WIN64
!define GPAC_BIN ${GPAC_ROOT}\bin\x64\release
!define GPAC_EXTRA_LIB ${GPAC_ROOT}\extra_lib\lib\x64\release
InstallDir "$PROGRAMFILES64\GPAC"
!else
!define GPAC_BIN ${GPAC_ROOT}\bin\win32\release
!define GPAC_EXTRA_LIB ${GPAC_ROOT}\extra_lib\lib\win32\release
InstallDir "$PROGRAMFILES32\GPAC"
!endif

InstallDirRegKey HKCU "SOFTWARE\GPAC" "InstallDir"

RequestExecutionLevel highest
!include LogicLib.nsh

Function .onInit
!ifdef IS_WIN64
!include "x64.nsh"
${If} ${RunningX64}
${Else}
	MessageBox MB_OK|MB_ICONSTOP "This installer is for 64bits operating systems only.$\n Please go to our website and get the 32 BITS installer."
	Quit
${Endif}
!endif

UserInfo::GetAccountType
pop $0
FunctionEnd

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

WindowIcon on
Icon "${GPAC_ROOT}\doc\osmo4.ico"
UninstallIcon "${GPAC_ROOT}\doc\osmo4.ico"

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

Var DIALOG
Var Label
Var Confirm

LangString PAGE_TITLE ${LANG_ENGLISH} "Title"
LangString PAGE_SUBTITLE ${LANG_ENGLISH} "Subtitle"

Function EnableNext
  Pop $R1
  ${NSD_GetState} $Confirm $R1
	GetDlgItem $0 $HWNDPARENT 1
	${If} $R1 == ${BST_CHECKED}
   EnableWindow $0 1
  ${Else} 
   EnableWindow $0 0
  ${Endif} 
FunctionEnd

Function customPage
	!insertmacro MUI_HEADER_TEXT "Patents and Royalties" "Please read carefully the following clause."
	GetDlgItem $0 $HWNDPARENT 1
	EnableWindow $0 0
	nsDialogs::Create 1018
	Pop $DIALOG

  ${NSD_CreateLabel} 0 0 100% 120u "Multimedia technologies are often covered by various patents which are most of the time hard to identify. These patents may or may not apply in your local jurisdiction. By installing this software, you acknowledge that you may have to pay royaltee fees in order to legally use this software. Do not proceed with this setup if you do not understand or do not agree with these terms. In any case, the authors and/or distributors bears no liability for any infringing usage of this software, which is provided for educational or research purposes."
	Pop $Label

  ${NSD_CreateCheckBox} 0 -30 100% 12u "I understand and accept the conditions"
  Pop $Confirm
	GetFunctionAddress $0 EnableNext
	nsDialogs::OnClick $Confirm $0
  
  
  nsDialogs::Show
FunctionEnd

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "${GPAC_ROOT}\COPYING" 
  Page custom customPage
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
    
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !insertmacro MUI_LANGUAGE "English"

ComponentText "This will install the GPAC Framework on your computer. Select which optional things you want installed."
DirText "This will install the GPAC Framework on your computer. Choose a directory"


Function FctWriteRegStrAuth
   ;local var
   Push $0
   Push $R0
   Push $R1
   Push $R2
   Push $R3
   ;pop function arguments
   Exch 5
   Pop $R3
   Exch 5
   Pop $R2
   Exch 5
   Pop $R1
   Exch 5
   Pop $R0

   ;test if calling HKCR
   StrCmp $R0 "HKCR" +1 +3
   WriteRegStr HKCR $R1 $R2 $R3
   goto lbl_end
   
   #has current user admin privileges?
   userInfo::getAccountType
   Pop $0
   StrCmp $0 "Admin" lbl_admin lbl_not_admin

   lbl_admin:
      WriteRegStr HKLM $R1 $R2 $R3
      goto lbl_end

   lbl_not_admin:
      WriteRegStr HKCU $R1 $R2 $R3

   lbl_end:
      Pop $R3
      Pop $R2
      Pop $R1
      Pop $R0
      Pop $0
FunctionEnd

!macro WriteRegStrAuth HKREG SUBREG ENTRY VALUESTR
    Push "${HKREG}"
    Push "${SUBREG}"
    Push "${ENTRY}"
    Push "${VALUESTR}"
    Call FctWriteRegStrAuth
!macroend
 
!define WriteRegStrAuth "!insertmacro WriteRegStrAuth"


Function un.FctDeleteRegKeyAuth
   ;local var
   Push $0
   Push $R0
   Push $R1
   ;pop function arguments
   Exch 3
   Pop $R1
   Exch 3
   Pop $R0

   ;test if calling HKCR
   StrCmp $R0 "HKCR" +1 +3
   DeleteRegKey HKCR $R1
   goto lbl_end
   
   #has current user admin privileges?
   userInfo::getAccountType
   Pop $0
   StrCmp $0 "Admin" lbl_admin lbl_not_admin
   
   lbl_admin:
      DeleteRegKey HKLM $R1
      goto lbl_end
   
   lbl_not_admin:
      DeleteRegKey HKCU $R1

   lbl_end:
      Pop $0
      Pop $R1
      Pop $R0
FunctionEnd

!macro DeleteRegKeyAuth HKREG SUBREG
    Push "${HKREG}"
    Push "${SUBREG}"
    Call un.FctDeleteRegKeyAuth
!macroend
 
!define DeleteRegKeyAuth "!insertmacro DeleteRegKeyAuth"

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
 !ifndef IS_WIN64
   File "${GPAC_BIN}\Gdiplus.dll"
  !endif

lbl_xp:
   File "${GPAC_BIN}\gm_gdip_raster.dll"

lbl_done:
FunctionEnd

;GPAC core
Section "GPAC Core" SecGPAC
  SectionIn RO
  SetOutPath $INSTDIR

  File /oname=ReadMe.txt "${GPAC_ROOT}\README"
  File /oname=License.txt "${GPAC_ROOT}\COPYING"
  File /oname=Changelog.txt "${GPAC_ROOT}\Changelog"
  File "${GPAC_ROOT}\doc\configuration.html"
  File "${GPAC_ROOT}\doc\gpac.mp4"
  File "${GPAC_ROOT}\doc\osmo4.ico"
  File "${GPAC_BIN}\libgpac.dll"
  File "${GPAC_BIN}\js.dll"
  File "${GPAC_BIN}\libeay32.dll"
  File "${GPAC_BIN}\ssleay32.dll"

  ;create default cache
  SetOutPath $INSTDIR\cache

  SetOutPath $INSTDIR
  
  ${WriteRegStrAuth} HKCU "SOFTWARE\GPAC" "InstallDir" "$INSTDIR"
  ${WriteRegStrAuth} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPAC" "DisplayName" "GPAC (remove only)"
  ${WriteRegStrAuth} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPAC" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteUninstaller "uninstall.exe"

SectionEnd

;player install
Section "GPAC Player" SecOsmo4
  SectionIn 1

  !ifndef IS_WIN64
  File "${GPAC_BIN}\Osmo4.exe"
  !endif
  File "${GPAC_BIN}\gm_dummy_in.dll"
  File "${GPAC_BIN}\gm_dx_hw.dll"
  File "${GPAC_BIN}\gm_gpac_js.dll"
  File "${GPAC_BIN}\gm_ismacryp.dll"

  ;copy GUI
  SetOutPath $INSTDIR\gui
  File "${GPAC_ROOT}\gui\gui.bt"
  File "${GPAC_ROOT}\gui\gui.js"
  File "${GPAC_ROOT}\gui\gwlib.js"
  File "${GPAC_ROOT}\gui\mpegu-core.js"
  File "${GPAC_ROOT}\gui\webvtt-renderer.js"
  SetOutPath $INSTDIR\gui\icons
  File /r /x .svn ${GPAC_ROOT}\gui\icons\*
  SetOutPath $INSTDIR\gui\extensions
  File /r /x .svn ${GPAC_ROOT}\gui\extensions\*

  SetOutPath $INSTDIR
SectionEnd

SubSection "GPAC Plugins" SecPlugins


;
;	2 install modes, normal one and full one

Section "MPEG-4 BIFS Decoder" SecBIFS
  SectionIn 1
  File "${GPAC_BIN}\gm_bifs_dec.dll"
SectionEnd

Section "MPEG-4 ODF Decoder" SecODF
  SectionIn 1
  File "${GPAC_BIN}\gm_odf_dec.dll"
SectionEnd

Section "MPEG-4 LASeR Decoder" SecLASeR
  SectionIn 1
  File "${GPAC_BIN}\gm_laser_dec.dll"
SectionEnd

Section "MPEG-4 SAF Demultiplexer" SecSAF
  SectionIn 1
  File "${GPAC_BIN}\gm_saf_in.dll"
SectionEnd

Section "Textual MPEG-4 Loader" SecTextLoad
  SectionIn 1
  File "${GPAC_BIN}\gm_ctx_load.dll"
SectionEnd

Section "Image Package (PNG, JPEG, BMP)" SecIMG
  SectionIn 1
  File "${GPAC_BIN}\gm_img_in.dll"
SectionEnd

Section "AAC Audio" SecAAC
  SectionIn 1
  File "${GPAC_BIN}\gm_aac_in.dll"
SectionEnd

Section "MP3 Audio" SecMP3
  SectionIn 1
  File "${GPAC_BIN}\gm_mp3_in.dll"
SectionEnd

Section "AC3 Audio" SecAC3
  SectionIn 1
  File "${GPAC_BIN}\gm_ac3_in.dll"
SectionEnd

Section "FFMPEG" SecFFMPEG
  SectionIn 1
  File "${GPAC_BIN}\gm_ffmpeg_in.dll"
  File "${GPAC_BIN}\avcodec-*.dll"
  File "${GPAC_BIN}\avdevice-*.dll"
  File "${GPAC_BIN}\avfilter-*.dll"
  File "${GPAC_BIN}\avformat-*.dll"
  File "${GPAC_BIN}\avutil-*.dll"
  File "${GPAC_BIN}\avresample-*.dll"
  File "${GPAC_BIN}\swresample-*.dll"
  File "${GPAC_BIN}\swscale-*.dll"
  File "${GPAC_BIN}\libx264-*.dll"
SectionEnd

Section "XviD Video Decoder" SecXVID
  SectionIn 1
  File "${GPAC_BIN}\gm_xvid_dec.dll"
SectionEnd

;Section "AMR NB & WB" SecAMRFT
;  SectionIn 1
;  File "..\gm_amr_float_dec.dll"
;SectionEnd

Section "Subtitles" SecSUBS
  SectionIn 1
  File "${GPAC_BIN}\gm_timedtext.dll"
SectionEnd

Section "ISO File Format" SecISOFF
  SectionIn 1
  File "${GPAC_BIN}\gm_isom_in.dll"
SectionEnd

Section "MPEG-2 TS" SecM2TS
  SectionIn 1
  File "${GPAC_BIN}\gm_mpegts_in.dll"
SectionEnd

Section "RTP/RTSP" SecRTP
  SectionIn 1
  File "${GPAC_BIN}\gm_rtp_in.dll"
SectionEnd

Section "SVG" SecSVG
  SectionIn 1
  File "${GPAC_BIN}\gm_svg_in.dll"
SectionEnd

Section "WebVTT" SecWebVTT
  SectionIn 1
  File "${GPAC_BIN}\gm_vtt_in.dll"
SectionEnd

Section "GDI+" SecGDIP
  SectionIn 1
  call InsertGDIPLUS
SectionEnd

Section "GPAC 2D Raster" SecG2DS
  SectionIn 1
  File "${GPAC_BIN}\gm_soft_raster.dll"
SectionEnd

Section "FreeType" SecFT
  SectionIn 1
  File "${GPAC_BIN}\gm_ft_font.dll"
SectionEnd

Section "Windows MME Audio" SecWAVE
  SectionIn 1
  File "${GPAC_BIN}\gm_wav_out.dll"
SectionEnd

Section "Xiph" SecXIPH
  SectionIn 1
  File "${GPAC_BIN}\gm_ogg.dll"
SectionEnd

Section "OpenSVC Decoder" SecOSVC
  SectionIn 1
  File "${GPAC_BIN}\OpenSVCDecoder.dll"
  File "${GPAC_BIN}\gm_opensvc_dec.dll"
SectionEnd

Section "OpenHEVC Decoder" SecOHEVC
  SectionIn 1
  File "${GPAC_BIN}\libLibOpenHevcWrapper.dll"
  File "${GPAC_BIN}\gm_openhevc_dec.dll"
SectionEnd

Section "MPEG DASH Suppport" SecDASH
  SectionIn 1
  File "${GPAC_BIN}\gm_mpd_in.dll"
SectionEnd

Section "HTML 5 Media Source Extensions Suppport" SecMSE
  SectionIn 1
  File "${GPAC_BIN}\gm_mse_in.dll"
SectionEnd

Section "UPnP Support" SecUPnP
  SectionIn 1
  File "${GPAC_BIN}\gm_platinum.dll"
SectionEnd

Section "Widget Manager" SecMPEGU
  SectionIn 1
  File "${GPAC_BIN}\gm_widgetman.dll"
SectionEnd

;Section "MobileIP Framework" SecMobIP
;  SectionIn 1
;  File "..\gm_mobile_ip.dll"
;  File "..\MobileSession.dll"
;SectionEnd


;Section "OFFIS Audio compressor" SecOffisComp
;  SectionIn 1
;  ;copy GUI
;  File "..\gm_offis_compressor.dll"
;  File "..\QtCore4.dll"
;  File "..\QtGui4.dll"
;  SetOutPath $INSTDIR\gui\extensions
;  File /r /x .svn ..\..\..\..\gui\extensions\offis*
;  SetOutPath $INSTDIR
;SectionEnd

SubSectionEnd


Section "MP4Box" SecMP4B
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\MP4Box.exe"
  Push $INSTDIR
  Call AddToPath
SectionEnd

Section "MP42TS" SecMP42TS
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\MP42TS.exe"
  Push $INSTDIR
  Call AddToPath
SectionEnd

Section "DashCast" SecDC
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\dashcast.exe"
  Push $INSTDIR
  Call AddToPath
SectionEnd


Section "GPAC SDK" SecSDK
  SectionIn 1
  SetOutPath $INSTDIR\sdk\include
  File /r /x CVS ${GPAC_ROOT}\include\*.h
  SetOutPath $INSTDIR\sdk\lib
  File ${GPAC_BIN}\libgpac.lib
  File ${GPAC_EXTRA_LIB}\js.lib
SectionEnd


!ifndef IS_WIN64
!define HK_MOZ "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0"

Section "Osmozilla" SecZILLA
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\nposmozilla.dll"
  File "${GPAC_BIN}\nposmozilla.xpt"

  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Path" "$INSTDIR\nposmozilla.dll"
  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "XPTPath" "$INSTDIR\nposmozilla.xpt"
  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Version" "${GPAC_VERSION}"
  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Vendor" "GPAC"
  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "Description" "GPAC plugin"
  ${WriteRegStrAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0" "ProductName" "Osmozilla"
SectionEnd
!endif

Section "GPAX" SecGPAX
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\GPAX.dll"
  RegDLL "$INSTDIR\GPAX.dll"
SectionEnd


Section "MP4Client" SecMP4C
  SectionIn 1
  SetOutPath $INSTDIR
  File "${GPAC_BIN}\MP4Client.exe"
SectionEnd   
                 
                 
                  
Section "Windows Runtime Libraries" SecMSVCRT
  SectionIn 1
;  File "..\Microsoft.VC100.CRT.manifest"
;  File "..\Microsoft.VC100.MFC.manifest"       
  File "${GPAC_BIN}\msvcr100.dll"
  File "${GPAC_BIN}\mfc100.dll"
SectionEnd




SubSection "GPAC Shortcuts"

Section "Add Start Menu Shortcuts"
  SectionIn 1
  #has current user admin privileges?
  userInfo::getAccountType
  Pop $0
  StrCmp $0 "Admin" +1 +2
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\GPAC"
  CreateShortCut "$SMPROGRAMS\GPAC\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\GPAC\Osmo4/MP4Client.lnk" "$INSTDIR\MP4Client.exe" "-gui" 
  CreateShortCut "$SMPROGRAMS\GPAC\Osmo4/MP4Client (with Console).lnk" "$INSTDIR\MP4Client.exe" ""
  !ifndef IS_WIN64
  CreateShortCut "$SMPROGRAMS\GPAC\Osmo4 (Old UI).lnk" "$INSTDIR\Osmo4.exe" ""
  !endif
  CreateShortCut "$SMPROGRAMS\GPAC\Readme.lnk" "$INSTDIR\ReadMe.txt"
  CreateShortCut "$SMPROGRAMS\GPAC\License.lnk" "$INSTDIR\License.txt"
  CreateShortCut "$SMPROGRAMS\GPAC\History.lnk" "$INSTDIR\changelog.txt"
  CreateShortCut "$SMPROGRAMS\GPAC\Configuration Info.lnk" "$INSTDIR\configuration.html"
SectionEnd

!ifndef IS_WIN64
Section "Add shortcut to QuickLaunch"
  SectionIn 1
  CreateShortCut "$QUICKLAUNCH\GPAC.lnk" "$INSTDIR\Osmo4.exe" "" "$INSTDIR\Osmo4.exe" 0
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
  ${WriteRegStrAuth} HKCR GPAC\mp4\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  ${WriteRegStrAuth} HKCR GPAC\mp4\Shell\open\command "" "$INSTDIR\Osmo4.exe %L"
  ${WriteRegStrAuth} HKCR .mp4 "" "GPAC\mp4"
  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'

SectionEnd

Section "Associate 3GPP files (3GP) with Osmo4"
  SectionIn 1
  ;write file association
  ${WriteRegStrAuth} HKCR GPAC\3gp\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  ${WriteRegStrAuth} HKCR GPAC\3gp\Shell\open\command "" "$INSTDIR\Osmo4.exe %L"
  ${WriteRegStrAuth} HKCR .3gp "" "GPAC\3gp"
  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
SectionEnd

Section "Associate 3GPP2 files (3G2) with Osmo4"
  SectionIn 1
  ;write file association
  ${WriteRegStrAuth} HKCR GPAC\3g2\DefaultIcon "" "$INSTDIR\Osmo4.ico, 0"
  ${WriteRegStrAuth} HKCR GPAC\3g2\Shell\open\command "" "$INSTDIR\Osmo4.exe %L"
  ${WriteRegStrAuth} HKCR .3g2 "" "GPAC\3g2"
  !system 'shell32.dll::SHChangeNotify(i, i, i, i) v (${SHCNE_ASSOCCHANGED}, ${SHCNF_IDLIST}, 0, 0)'
SectionEnd
!endif
  
SubSectionEnd



!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGPAC} "GPAC Core"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecOsmo4} "GPAC Player"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecPlugins} "GPAC Plugins"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecBIFS} "MPEG-4 BIFS Scene Decoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecODF} "MPEG-4 Object Descriptor Decoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecLASeR} "MPEG-4 LASeR Scene Decoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecTextLoad} "Support for uncompressed MPEG-4 (BT and XMT), VRML and X3D textual formats"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSAF} "MPEG-4 SAF Demultiplexer"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecIMG} "Support for PNG, JPEG, BMP and JPEG2000 images"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAAC} "Support for MPEG-4 Audio HE-AAC decoder and web radios"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMP3} "Support for MPEG-1/2 Audio (inc. MP3) decoder and web radios"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAC3} "Support for Dolby AC3 decoder and web radios"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecFFMPEG} "Support for FFMPEG libraries for various format decoding and demultiplexing"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecXVID} "Support for XVID library for MPEG-4 Video Part 2 decoding"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAMRFT} "Support for AMR and AMR WideBand decoder and web radios"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSUBS} "Subtitle support include SRT, SUB, 3GPP and MPEG-4 Text formats"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecISOFF} "Support for ISO-based file formats (3GP, MP4, MJ2K)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecM2TS} "Support for MPEG-2 Transport Stream"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecRTP} "Support for RTP and RTSP IP streaming"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSVG} "Support for SVG including progressive loading"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecWebVTT} "Support for WebVTT subtitles"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGDIP} "GDIPlus-based rasterizer"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecG2DS} "GPAC software rasterizer"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecFT} "FreeType font parsing"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecWAVE} "Windows MME Audio output support"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecXIPH} "Support for XIPP OGG, Vorbis and Theora media"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecOSVC} "Support for SVC decoding through OpenSVC Decoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecOHEVC} "Support for HEVC decoding through OpenHEVC Decoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDASH} "HTTP Streaming using MPEG DASH"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMSE}  "HTTP Streaming using HTML 5 Media Source Extensions"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecUPnP} "Support for UPnP based on Platinum"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMPEGU} "Support for W3C and MPEG-U Widgets"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMobIP} "UNIGE Mobile IP Framework"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecOffisComp} "OFFIS Audio Compressor"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMP4B} "MP4Box command-line tool for various multimedia operations"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMP42TS} "MP42TS command-line tool for MPEG-2 TS multiplexing"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDC} "DashCast offline and live MPEG-DASH Encoder"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecSDK} "GPAC SDK: headers and library files needed to develop modules for GPAC or appllication based on GPAC"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecZILLA} "GPAC playback support NPAPI-based browsers (FireFox/Gecko, Safari/WebKit)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecGPAX} "GPAC playback support using ActiveX component (Internet Explorer)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMP4C} "GPAC command-line player and AVI dumper"
  
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Function .onInstSuccess
;  MessageBox MB_YESNO "GPAC Framework installation complete. Do you want to launch the player?" IDNO NoLaunch
;  Exec $INSTDIR\Osmo4.exe
;  NoLaunch:
FunctionEnd





; uninstall stuff

UninstallText "This will uninstall GPAC from your computer. Hit next to continue."

; special uninstall section.
Section "Uninstall"
  ; remove registry keys
  ${DeleteRegKeyAuth} HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPAC"
  ${DeleteRegKeyAuth} HKCU "SOFTWARE\GPAC"
  ${DeleteRegKeyAuth} HKCU "SOFTWARE\MozillaPlugins\@gpac/osmozilla,version=1.0"
  ${DeleteRegKeyAuth} HKCR GPAC\mp4\DefaultIcon
  ${DeleteRegKeyAuth} HKCR GPAC\mp4\shell\open\command
  ${DeleteRegKeyAuth} HKCR GPAC\mp4
  ${DeleteRegKeyAuth} HKCR .mp4
  ${DeleteRegKeyAuth} HKCR GPAC\3gp\DefaultIcon
  ${DeleteRegKeyAuth} HKCR GPAC\3gp\shell\open\command
  ${DeleteRegKeyAuth} HKCR GPAC\3gp
  ${DeleteRegKeyAuth} HKCR .3gp
  ${DeleteRegKeyAuth} HKCR GPAC\3g2\DefaultIcon
  ${DeleteRegKeyAuth} HKCR GPAC\3g2\shell\open\command
  ${DeleteRegKeyAuth} HKCR GPAC\3g2
  ${DeleteRegKeyAuth} HKCR .3g2
  ${DeleteRegKeyAuth} HKCR GPAC

  UnRegDLL "$INSTDIR\GPAX.dll"
  RMDir /r $INSTDIR
  Push $INSTDIR
  Call un.RemoveFromPath
  #has current user admin privileges?
  userInfo::getAccountType
  Pop $0
  StrCmp $0 "Admin" +1 +2
  SetShellVarContext all
  Delete "$SMPROGRAMS\GPAC\*.*"
  RMDir "$SMPROGRAMS\GPAC"
  Delete "$QUICKLAUNCH\Osmo4.lnk"
  Delete "$DESKTOP\Osmo4.lnk"

SectionEnd

;path modif functions
!verbose 3
!include "WinMessages.NSH"
!verbose 4

!ifndef WriteEnvStr_RegKey
  !ifdef ALL_USERS
    !define WriteEnvStr_RegKey \
       'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
  !else
    !define WriteEnvStr_RegKey 'HKCU "Environment"'
  !endif
!endif

; AddToPath - Adds the given dir to the search path.
;        Input - head of the stack
;        Note - Win9x systems requires reboot

Function AddToPath
  Exch $0
  Push $1
  Push $2
  Push $3
 
  # don't add if the path doesn't exist
  IfFileExists "$0\*.*" "" AddToPath_done
 
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
    ReadRegStr $1 ${WriteEnvStr_RegKey} "PATH"
    StrCpy $2 $1 1 -1 # copy last char
    StrCmp $2 ";" 0 +2 # if last char == ;
      StrCpy $1 $1 -1 # remove last char
    StrCmp $1 "" AddToPath_NTdoIt
      StrCpy $0 "$1;$0"
    AddToPath_NTdoIt:
      WriteRegExpandStr ${WriteEnvStr_RegKey} "PATH" $0
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
    ReadRegStr $1 ${WriteEnvStr_RegKey} "PATH"
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
 
      WriteRegExpandStr ${WriteEnvStr_RegKey} "PATH" $3
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
