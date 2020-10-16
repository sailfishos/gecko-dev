%define greversion    60.9.1

%define embedlite_config merqtxulrunner

%define system_nspr       1
%define system_nss        1
%define system_sqlite     1
%define system_ffi        1
%define system_hunspell   1
%define system_jpeg       1
%define system_png        1
%define system_icu        1
%define system_zlib       1
%define system_bz2        1
%define system_pixman     1
%define system_libvpx     1

%global mozappdir     %{_libdir}/%{name}-%{greversion}
%global mozappdirdev  %{_libdir}/%{name}-devel-%{greversion}

# Private/bundled libs the final package should not provide or depend on.
%global privlibs             libfreebl3
%global privlibs %{privlibs}|libmozalloc
%global privlibs %{privlibs}|libmozsqlite3
%global privlibs %{privlibs}|libnspr4
%global privlibs %{privlibs}|libplc4
%global privlibs %{privlibs}|libplds4
%global privlibs %{privlibs}|libnss3
%global privlibs %{privlibs}|libnssdbm3
%global privlibs %{privlibs}|libnssutil3
%global privlibs %{privlibs}|libsmime3
%global privlibs %{privlibs}|libsoftokn3
%global privlibs %{privlibs}|libssl3

%global __provides_exclude ^(%{privlibs})\\.so
%global __requires_exclude ^(%{privlibs})\\.so

Name:       xulrunner-qt5
Summary:    XUL runner
Version:    %{greversion}
Release:    1
Group:      Applications/Internet
License:    MPLv2.0
URL:        https://git.sailfishos.org/mer-core/gecko-dev
Source0:    %{name}-%{version}.tar.bz2
Patch1:     0001-sailfishos-qt-Bring-back-Qt-layer.-JB-50505.patch
Patch2:     0002-sailfishos-gecko-Fix-embedlite-building.-JB-50505.patch
Patch3:     0003-sailfishos-gecko-Hackish-fix-for-preferences-usage-i.patch
Patch4:     0004-sailfishos-gecko-Hack-message_pump_qt-s-moc-generati.patch
Patch5:     0005-sailfishos-gecko-Backport-Embed-MessageLoop-contruct.patch
Patch6:     0006-sailfishos-compositor-Fix-GLContextProvider-defines.patch
Patch7:     0007-sailfishos-compositor-Make-it-possible-to-extend-Com.patch
Patch8:     0008-sailfishos-compositor-Allow-compositor-specializatio.patch
Patch9:     0009-sailfishos-gecko-Create-EmbedLiteCompositorBridgePar.patch
Patch10:    0010-sailfishos-gecko-Remove-PuppetWidget-from-TabChild-i.patch
Patch11:    0011-sailfishos-gecko-Make-TabChild-to-work-with-TabChild.patch
Patch12:    0012-sailfishos-build-Fix-build-error-with-newer-glibc.patch
Patch13:    0013-sailfishos-gecko-Enable-Pango-for-the-build.-JB-5086.patch
Patch14:    0014-sailfishos-gecko-Fix-gfxPlatform-AsyncPanZoomEnabled.patch
Patch15:    0015-sailfishos-gecko-Nullify-delayed-work-timer-after-ca.patch
Patch16:    0016-sailfishos-compositor-Respect-gfxPrefs-ClearCompoisi.patch
Patch17:    0017-sailfishos-gecko-Workaround-for-late-access-message-.patch
Patch18:    0018-sailfishos-gecko-Limit-surface-area-rather-than-widt.patch
Patch19:    0019-sailfishos-gecko-Make-TextureImageEGL-hold-a-referen.patch
Patch20:    0020-sailfishos-loginmanager-Adapt-LoginManager-to-EmbedL.patch
Patch21:    0021-sailfishos-gecko-Make-fullscreen-enabling-work-as-us.patch
Patch22:    0022-sailfishos-gecko-Embedlite-doesn-t-have-prompter-imp.patch
Patch23:    0023-sailfishos-gecko-Disable-Marionette.patch
Patch24:    0024-sailfishos-gecko-Use-libcontentaction-for-custom-sch.patch
Patch25:    0025-sailfishos-gecko-Handle-temporary-directory-similarl.patch
Patch26:    0026-sailfishos-gecko-Disable-loading-heavier-extensions.patch
Patch27:    0027-sailfishos-gecko-Avoid-incorrect-compiler-optimisati.patch
Patch28:    0028-sailfishos-gecko-Avoid-rogue-origin-points-when-clip.patch
Patch29:    0029-sailfishos-gecko-Allow-render-shaders-to-be-loaded-f.patch
Patch30:    0030-sailfishos-gecko-Prioritize-GMP-plugins-over-all-oth.patch
Patch31:    0031-sailfishos-gecko-Delete-startupCache-if-it-s-stale.patch
Patch32:    0032-sailfishos-mozglue-Introduce-EmbedInitGlue-to-the-mo.patch
Patch33:    0033-sailfishos-gecko-Skip-invalid-WatchId-in-geolocation.patch
Patch34:    0034-sailfishos-locale-Get-12-24h-timeformat-setting-from.patch
Patch35:    0035-sailfishos-contentaction-Fix-content-action-integrat.patch
Patch36:    0036-sailfishos-qt-Initialize-FreeType-library-properly.-.patch
Patch37:    0037-sailfishos-disable-TLS-1.0-and-1.1.patch
Patch38:    0038-sailfishos-gecko-Use-registered-IHistory-service-imp.patch
Patch39:    0039-sailfishos-gecko-Suppress-LoginManagerContent.jsm-ow.patch
Patch40:    0040-sailfishos-configuration-Configure-application-as-mo.patch
Patch41:    0041-sailfishos-gecko-Include-XUL-videocontrols-reflow-co.patch
Patch42:    0042-sailfishos-gecko-Adjust-audio-control-dimensions.-Co.patch

BuildRequires:  rust
BuildRequires:  rust-std-static
BuildRequires:  cargo

BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.12.0
%endif
%if %{system_nss}
BuildRequires:  pkgconfig(nss) >= 3.21.3
%endif
%if %{system_sqlite}
BuildRequires:  pkgconfig(sqlite3) >= 3.8.9
%endif
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libproxy-1.0)
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavfilter)
BuildRequires:  pkgconfig(libavformat)
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswresample)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  pkgconfig(contentaction5)
BuildRequires:  pkgconfig(dconf)
BuildRequires:  qt5-qttools
BuildRequires:  qt5-default
BuildRequires:  autoconf213
BuildRequires:  automake
BuildRequires:  python
BuildRequires:  python-devel
BuildRequires:  zip
BuildRequires:  unzip
BuildRequires:  qt5-plugin-platform-minimal
%if %{system_icu}
BuildRequires:  libicu-devel
%endif
%if %{system_hunspell}
BuildRequires:  hunspell-devel
%endif
%if %{system_bz2}
BuildRequires:  bzip2-devel
%endif
%if %{system_zlib}
BuildRequires:  zlib
%endif
%if %{system_png}
BuildRequires:  libpng
%endif
%if %{system_jpeg}
BuildRequires:  libjpeg-turbo-devel
%endif
%ifarch i586 i486 i386 x86_64
BuildRequires:  yasm
%endif
BuildRequires:  fdupes
# See below on why the system version of this library is used
Requires: nss-ckbi >= 3.16.6
%if %{system_ffi}
BuildRequires:  libffi-devel
%endif
%if %{system_pixman}
BuildRequires:  pkgconfig(pixman-1)
%endif
%if %{system_libvpx}
BuildRequires:  pkgconfig(vpx)
%endif

%description
Mozilla XUL runner

%package devel
Requires: %{name} = %{version}-%{release}
Conflicts: xulrunner-devel
Summary: Headers for xulrunner

%description devel
Development files for xulrunner.

%package misc
Requires: %{name} = %{version}-%{release}
Summary: Misc files for xulrunner

%description misc
Tests and misc files for xulrunner.

# Build output directory.
%define BUILD_DIR "$PWD"/obj-build-mer-qt-xr
# EmbedLite config used to configure the engine.
%define BASE_CONFIG "$PWD"/embedding/embedlite/config/mozconfig.%{embedlite_config}

%prep
%autosetup -p1 -n %{name}-%{version}

mkdir -p "%BUILD_DIR"
cp -rf "%BASE_CONFIG" "%BUILD_DIR"/mozconfig
echo "export MOZCONFIG=%BUILD_DIR/mozconfig" >> "%BUILD_DIR"/rpm-shared.env
echo "export LIBDIR='%{_libdir}'" >> "%BUILD_DIR"/rpm-shared.env
echo "export QT_QPA_PLATFORM=minimal" >> "%BUILD_DIR"/rpm-shared.env
echo "export MOZ_OBJDIR=%BUILD_DIR" >> "%BUILD_DIR"/rpm-shared.env

%build
source "%BUILD_DIR"/rpm-shared.env
# hack for when not using virtualenv
ln -sf "%BUILD_DIR"/config.status $PWD/build/config.status

printf "#\n# Added by xulrunner-qt.spec:\n#" >> "$MOZCONFIG"

echo "mk_add_options MOZ_OBJDIR='%BUILD_DIR'" >> "$MOZCONFIG"
# XXX: gold crashes when building gecko for both i486 and x86_64
#echo "export CFLAGS=\"\$CFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export CXXFLAGS=\"\$CXXFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export LD=ld.gold" >> "$MOZCONFIG"
echo "ac_add_options --disable-tests" >> "$MOZCONFIG"
echo "ac_add_options --disable-strip" >> "$MOZCONFIG"
echo "ac_add_options --disable-install-strip" >> "$MOZCONFIG"
echo "ac_add_options --with-app-name=%{name}" >> "$MOZCONFIG"

# Reduce logging from release build
%if "%{?qa_stage_name}" == testing || "%{?qa_stage_name}" == release
echo "export CFLAGS=\"\$CFLAGS -DRELEASE_OR_BETA=1\"" >> "$MOZCONFIG"
echo "export CXXFLAGS=\"\$CXXFLAGS -DRELEASE_OR_BETA=1\"" >> "$MOZCONFIG"
%endif

%if %{system_nss}
  echo "ac_add_options --with-system-nss" >> "$MOZCONFIG"
%endif

%if %{system_hunspell}
  echo "ac_add_options --enable-system-hunspell" >> "$MOZCONFIG"
%endif

%if %{system_sqlite}
  echo "ac_add_options --enable-system-sqlite" >> "$MOZCONFIG"
%endif

%if %{system_ffi}
  echo "ac_add_options --with-system-ffi" >> "${MOZCONFIG}"
%endif

%if %{system_icu}
  echo "ac_add_options --with-system-icu" >> "${MOZCONFIG}"
%endif

%if %{system_png}
  echo "ac_add_options --with-system-png" >> "${MOZCONFIG}"
%endif

%if %{system_jpeg}
  echo "ac_add_options --with-system-jpeg" >> "${MOZCONFIG}"
%endif

%if %{system_zlib}
  echo "ac_add_options --with-system-zlib" >> "${MOZCONFIG}"
%endif

%if %{system_bz2}
  echo "ac_add_options --with-system-bz2" >> "${MOZCONFIG}"
%endif

%if %{system_pixman}
  echo "ac_add_options --enable-system-pixman" >> "${MOZCONFIG}"
%endif

%if %{system_libvpx}
  echo "ac_add_options --with-system-libvpx" >> "${MOZCONFIG}"
%endif

%ifarch %ix86
echo "ac_add_options --disable-startupcache" >> "$MOZCONFIG"
%endif

# Gecko tries to add the gre lib dir to LD_LIBRARY_PATH when loading plugin-container, 
# but as sailfish-browser has privileged EGID, glibc removes it for security reasons. 
# Set ELF RPATH through LDFLAGS. Needed for plugin-container and libxul.so
# Additionally we limit the memory usage during linking
 echo 'FIX_LDFLAGS="-Wl,--reduce-memory-overheads -Wl,--no-keep-memory -Wl,-rpath=%{mozappdir}"' >> "${MOZCONFIG}"
 echo 'export LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
 echo 'LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
 echo 'export WRAP_LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
 echo 'mk_add_options LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"

./mach build
# This might be unnecessary but previously some files
# were only behind FASTER_RECURSIVE_MAKE but only adds few
# minutes for the build.
./mach build faster FASTER_RECURSIVE_MAKE=1

%install
source "%BUILD_DIR"/rpm-shared.env

%{__make} -C %BUILD_DIR/embedding/embedlite/installer install DESTDIR=%{buildroot}

for i in $(cd ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/; ls *.so); do
    rm ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/$i
    ln -s %{mozappdir}/$i ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/$i
done
%fdupes -s %{buildroot}%{_includedir}
%fdupes -s %{buildroot}%{_libdir}
%{__chmod} +x %{buildroot}%{mozappdir}/*.so
# Use the system hunspell dictionaries
%{__rm} -rf ${RPM_BUILD_ROOT}%{mozappdir}/dictionaries
ln -s %{_datadir}/myspell ${RPM_BUILD_ROOT}%{mozappdir}/dictionaries
mkdir ${RPM_BUILD_ROOT}%{mozappdir}/defaults

%if !%{system_nss}
# symlink to the system libnssckbi.so (CA trust library). It is replaced by
# the p11-kit-nss-ckbi package to use p11-kit's trust store.
# There is a strong binary compatibility guarantee.
rm ${RPM_BUILD_ROOT}%{mozappdir}/libnssckbi.so
ln -s %{_libdir}/libnssckbi.so ${RPM_BUILD_ROOT}%{mozappdir}/libnssckbi.so
%endif

# Fix some of the RPM lint errors.
find "%{buildroot}%{_includedir}" -type f -name '*.h' -exec chmod 0644 {} +;

%post
touch /var/lib/_MOZEMBED_CACHE_CLEAN_

%files
%defattr(-,root,root,-)
%attr(755,-,-) %{_bindir}/*
%dir %{mozappdir}
%dir %{mozappdir}/defaults
%{mozappdir}/*.so
%{mozappdir}/omni.ja
%{mozappdir}/dependentlibs.list
%{mozappdir}/dictionaries
%{mozappdir}/plugin-container
%{mozappdir}/platform.ini
%exclude %{mozappdir}/gmp-fake
%exclude %{mozappdir}/gmp-clearkey
%exclude %{mozappdir}/gmp-fakeopenh264
%exclude %{mozappdir}/run-mozilla.sh
%exclude %{mozappdir}/chrome.manifest

%files devel
%defattr(-,root,root,-)
%{_datadir}/*
%{mozappdirdev}
%{_libdir}/pkgconfig
%{_includedir}/*

%files misc
%defattr(-,root,root,-)
%{mozappdir}/*
%exclude %{mozappdir}/*.so
%exclude %{mozappdir}/omni.ja
%exclude %{mozappdir}/dependentlibs.list
