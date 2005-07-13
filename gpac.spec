# $Id: gpac.spec,v 1.1.1.1 2005-07-13 14:34:14 jeanlf Exp $
Summary: GPAC is a multimedia framework covering MPEG-4, VRML/X3D and SVG.
Name: gpac
Version: 0.4.0
Release: DEV
License: LGPL
Group: Applications/Multimedia
Source0: gpac-0.4.0-DEV.tar.gz%{?_with_amr:Source1:http://www.3gpp.org/ftp/Specs/archive/26_series/26.073/26073-530.zip}
URL: http://gpac.sourceforge.net/
BuildRoot: %{_tmppath}/%{name}-root
Requires: SDL
%{!?_without_js:Requires: js}
%{!?_without_freetype:Requires: freetype}
%{!?_without_faad:Requires: faad2}
%{!?_without_jpeg:Requires: libjpeg-6b}
%{!?_without_png:Requires: libpng}
%{!?_without_mad:Requires: libmad}
%{!?_without_xvid:Requires: xvidcore}
%{!?_without_ffmpeg:Requires: ffmpeg}
BuildRequires: SDL-devel
%{!?_without_js:BuildRequires: js-devel}
%{!?_without_freetype:BuildRequires: freetype-devel}
%{!?_without_faad:BuildRequires: faad2-devel}
%{!?_without_jpeg:BuildRequires: libjpeg-devel}
%{!?_without_png:BuildRequires: libpng-devel}
%{!?_without_mad:BuildRequires: libmad-devel}
%{!?_without_xvid:BuildRequires: xvidcore-devel}
%{!?_without_ffmpeg:BuildRequires: ffmpeg-devel}

%description
GPAC is a multimedia framework for MPEG-4, VRML/X3D and SVG/SMIL. GPAC is built upon an implementation of the MPEG-4 Systems 
standard (ISO/IEC 14496-1) developed from scratch in C.

The main development goal is to provide a clean (a.k.a. readable by as many
people as possible), small and flexible alternative to the MPEG-4 Systems 
reference software (known as IM1 and distributed in ISO/IEC 14496-5). 

The second development goal is to achieve integration of recent multimedia 
standards (SVG/SMIL, VRML, X3D, SWF, etc) into a single framework. 
This stage is still under drafting but GPAC already supports most of VRML97, 
some X3D as well as very simple SVG.

GPAC already features 2D and 3D multimedia playback, MPEG-4 Systems 
encoders/multiplexers and publishing tools for content distribution.

GPAC is licensed under the GNU Lesser General Public License.

The current GPAC release (0.4.0) already covers a very large part of the MPEG-4 standard,
and features what can probably be seen as the most advanced and robust 2D MPEG-4 
Player available worldwide, as well as a decent 3D MPEG-4/VRML player.

Available rpmbuild rebuild options :
--without : js freetype faad jpeg png mad xvid ffmpeg
--with : amr

To build with amr, download
http://www.3gpp.org/ftp/Specs/archive/26_series/26.073/26073-530.zip
and put it in the rpm source directory. Then invoke rpmbuild with the 
"--with amr" option.

%prep
%setup -q -n gpac
%if %{?_with_amr:1}%{!?_with_amr:0}
mkdir -p modules/amr_dec/amr_nb
cd Plugins/amr_dec/AMR_NB
unzip -j /usr/src/redhat/SOURCES/26073-530.zip
unzip -j 26073-530_ANSI_C_source_code.zip
cd ../../..
%endif

%build
%configure     --enable-oss-audio     %{?_with_amr: --enable-amr-nb}     %{?_without_js: --disable-js}     %{?_without_freetype: --disable-ft}     %{?_without_faad: --disable-faad}     %{?_without_jpeg: --disable-jpeg}     %{?_without_png: --disable-png}     %{?_without_mad: --disable-mad}     %{?_without_xvid: --disable-xvid}     %{?_without_ffmpeg: --disable-ffmpeg}
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
%doc AUTHORS BUGS Changelog COPYING README TODO
%{_bindir}/*
%{_libdir}/*
%{_mandir}/man1/*

%changelog
* Wed Jul 13 2005 Jean Le Feuvre
- Updated for GPAC LGPL release
* Mon Aug 09 2004 Sverker Abrahamsson <sverker@abrahamsson.com>
- Initial RPM release
