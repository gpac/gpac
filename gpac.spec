# $Id: gpac.spec,v 1.5 2008-12-02 18:04:42 jeanlf Exp $
Summary: Framework for production, encoding, delivery and interactive playback of multimedia content
Name: gpac
Version: 2.4
Release: 2.4
License: LGPL
Group: Applications/Multimedia
Source0: gpac-2.4.tar.gz
URL: https://gpac.io/
BuildRoot: %{_tmppath}/%{name}-root
Requires: SDL
%{!?_without_freetype:Requires: freetype}
%{!?_without_faad:Requires: faad2}
%{!?_without_jpeg:Requires: libjpeg-6b}
%{!?_without_png:Requires: libpng}
%{!?_without_mad:Requires: libmad}
%{!?_without_xvid:Requires: xvidcore}
%{!?_without_ffmpeg:Requires: ffmpeg}
%{!?_without_jack:Requires: libjack}
BuildRequires: SDL-devel
%{!?_without_freetype:BuildRequires: freetype-devel}
%{!?_without_faad:BuildRequires: faad2-devel}
%{!?_without_jpeg:BuildRequires: libjpeg-devel}
%{!?_without_png:BuildRequires: libpng-devel}
%{!?_without_mad:BuildRequires: libmad-devel}
%{!?_without_xvid:BuildRequires: xvidcore-devel}
%{!?_without_ffmpeg:BuildRequires: ffmpeg-devel}
%{!?_without_jack:BuildRequires: jack-audio-connection-kit}

%global debug_package %{nil}

%description

GPAC is a framework for production, encoding, delivery and interactive playback of multimedia content.

GPAC supports many AV codecs, multimedia containers (MP4,fMP4, TS, avi, mov, mpg, mkv ...), complex presentation formats (MPEG-4 Systems, SVG Tiny 1.2, VRML/X3D) and subtitles (SRT, WebVTT, TTXT/TX3G, TTML).

Supported inputs and outputs are pipes, UDP/TCP/UN sockets, local files, HTTP, DASH/HLS, RTP/RTSP, MPEG-2 TS, ATSC 3.0 ROUTE sessions, desktop grabbing, camera/microphone inputs and any input format supported by FFmpeg.

GPAC features a highly configurable media processing pipeline extensible through JavaScript, and can be embedded in Python or NodeJS applications.

GPAC is licensed under the GNU Lesser General Public License.


Available rpmbuild rebuild options :
--without : freetype faad a52 jpeg png mad xvid ffmpeg jack


%prep
%setup -q -n gpac

%build
%configure     --enable-oss-audio     %{?_without_freetype: --disable-ft}     %{?_without_faad: --disable-faad}    %{?_without_jpeg: --disable-jpeg}     %{?_without_png: --disable-png}     %{?_without_mad: --disable-mad}     %{?_without_xvid: --disable-xvid}     %{?_without_ffmpeg: --disable-ffmpeg} %{?_without_jack: --disable-jack}
make

%install
rm -rf %{buildroot}
%makeinstall

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-, root, root)
# %doc Changelog COPYING README.md
%{_bindir}/*
%{_libdir}/*
%{_includedir}/*
%{_datadir}/*

%changelog
* Fri Dec 16 2022 Jean Le Feuvre
- GPAC 2.2 release
* Fri Sep 4 2020 Jean Le Feuvre
- GPAC 1.0 release
* Fri Jul 3 2015 Jean Le Feuvre
- Changed to README.md
* Wed Feb 13 2008 Pierre Souchay
- Added libjack
* Wed Jul 13 2005 Jean Le Feuvre
- Updated for GPAC LGPL release
* Mon Aug 09 2004 Sverker Abrahamsson <sverker@abrahamsson.com>
- Initial RPM release
