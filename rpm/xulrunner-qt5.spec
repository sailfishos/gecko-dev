%define greversion    45.5.2
%define sf_extra_ver  1

%define embedlite_config merqtxulrunner

%define system_nspr       1
%define system_nss        0
%define system_sqlite     1
%define system_ffi        1
%define system_hunspell   1
%define system_jpeg       1
%define system_png        1
%define system_icu        0
%define system_zlib       1
%define system_bz2        1
%define system_pixman     1
%define system_cairo      1

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
Version:    %{greversion}.%{sf_extra_ver}
Release:    1
Group:      Applications/Internet
License:    MPLv2
URL:        https://github.com/tmeshkova/gecko-dev
Source0:    %{name}-%{version}.tar.bz2
Patch1:     0001-Supply-source-uri-to-gstreamer-pipeline.patch
Patch2:     0002-Workaround-for-late-access-message-loop.patch
Patch3:     0003-Revert-Bug-1114594-Remove-promptForSaveToFile-in-fav.patch
Patch4:     0004-Define-HAS_NEMO_RESOURCE-in-config.patch
Patch5:     0005-Limit-surface-area-rather-than-width-and-height.patch
Patch6:     0006-Make-TextureImageEGL-hold-a-reference-to-GLContext.-.patch
Patch7:     0007-Limit-maximum-scale-to-4x.-Fixes-JB-25377.patch
Patch8:     0008-Adapt-LoginManager-to-EmbedLite.-Fixes-JB21980.patch
Patch9:     0009-Bug-1253215-Initialize-RequestSyncService-only-if-it.patch
Patch10:    0010-Don-t-print-errors-from-DataReportingService.patch
Patch11:    0011-Don-t-try-to-access-undefined-app-list-of-AppsServic.patch
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.10.8
%endif
%if %{system_nss}
BuildRequires:  pkgconfig(nss) >= 3.18.1
%endif
%if %{system_sqlite}
BuildRequires:  pkgconfig(sqlite3) >= 3.8.9
Requires:  sqlite >= 3.8.9
%endif
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libproxy-1.0)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-app-1.0)
BuildRequires:  pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  qt5-qttools
BuildRequires:  qt5-default
BuildRequires:  autoconf213
BuildRequires:  automake
BuildRequires:  python
BuildRequires:  python-devel
BuildRequires:  zip
BuildRequires:  unzip
%if %{system_icu}
BuildRequires:  libicu52-devel
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
Requires: gstreamer1.0-plugins-good
%if %{system_ffi}
BuildRequires:  libffi-devel
%endif
%if %{system_pixman}
BuildRequires:  pkgconfig(pixman-1)
%endif
%if %{system_cairo}
BuildRequires:  pkgconfig(cairo)
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
%setup -q -n %{name}-%{version}
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1

mkdir -p "%BUILD_DIR"
cp -rf "%BASE_CONFIG" "%BUILD_DIR"/mozconfig

%ifarch %ix86
PYTHONPATH="$PWD"/python:"$PWD"/config:"$PWD"/build:"$PWD"/xpcom/typelib/xpt/tools
PYTHONPATH+=:"$PWD"/dom/bindings:$PWD/dom/bindings/parser
PYTHONPATH+=:"$PWD"/other-licenses/ply:$PWD/media/webrtc/trunk/tools/gyp/pylib
PYTHONPATH+=:"$PWD"/testing/taskcluster
PYTHONPATH+=:"$PWD"/testing/web-platform
PYTHONPATH+=:"$PWD"/testing/web-platform/harness
PYTHONPATH+=:"$PWD"/layout/tools/reftest
PYTHONPATH+=:"$PWD"/dom/bindings
for i in $(find "$PWD"/python "$PWD"/testing/mozbase -mindepth 1 -maxdepth 1 -type d); do
  PYTHONPATH+=:"$i:$i/lib"
done
echo "export DONT_POPULATE_VIRTUALENV=1" > "%BUILD_DIR"/rpm-shared.env
echo "export PYTHONPATH=$PYTHONPATH" >> "%BUILD_DIR"/rpm-shared.env
echo "export SBOX_REDIRECT_FORCE=/usr/bin/python" >> "%BUILD_DIR"/rpm-shared.env
%endif
echo "export MOZCONFIG=%BUILD_DIR/mozconfig" >> "%BUILD_DIR"/rpm-shared.env

%build
source "%BUILD_DIR"/rpm-shared.env
# hack for when not using virtualenv
ln -sf "%BUILD_DIR"/config.status $PWD/build/config.status

printf "#\n# Added by xulrunner-qt.spec:\n#" >> "$MOZCONFIG"
%ifarch %arm
echo "ac_add_options --with-arm-kuser" >> "$MOZCONFIG"
echo "ac_add_options --with-float-abi=toolchain-default" >> "$MOZCONFIG"
# No need for this, this should be managed by toolchain
echo "ac_add_options --with-thumb=toolchain-default" >> "$MOZCONFIG"
%endif

echo "mk_add_options MOZ_MAKE_FLAGS='%{?jobs:-j%jobs}'" >> "$MOZCONFIG"
echo "mk_add_options MOZ_OBJDIR='%BUILD_DIR'" >> "$MOZCONFIG"
# XXX: gold crashes when building gecko for both i486 and x86_64
#echo "export CFLAGS=\"\$CFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export CXXFLAGS=\"\$CXXFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export LD=ld.gold" >> "$MOZCONFIG"
echo "ac_add_options --disable-tests" >> "$MOZCONFIG"
echo "ac_add_options --disable-strip" >> "$MOZCONFIG"
echo "ac_add_options --with-app-name=%{name}" >> "$MOZCONFIG"

%if %{system_hunspell}
  echo "ac_add_options --enable-system-hunspell" >> "$MOZCONFIG"
%endif

%if %{system_sqlite}
  echo "ac_add_options --enable-system-sqlite" >> "$MOZCONFIG"
%endif

%if %{system_ffi}
  echo "ac_add_options --enable-system-ffi" >> "${MOZCONFIG}"
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

%if %{system_cairo}
  echo "ac_add_options --enable-system-cairo" >> "${MOZCONFIG}"
%endif

# https://bugzilla.mozilla.org/show_bug.cgi?id=1002002
echo "ac_add_options --disable-startupcache" >> "$MOZCONFIG"

%{__make} -f client.mk build STRIP="/bin/true" %{?jobs:MOZ_MAKE_FLAGS="-j%jobs"}

%install
source "%BUILD_DIR"/rpm-shared.env

%{__make} -f client.mk install DESTDIR=%{buildroot}
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
%dir %{mozappdir}/defaults
%{mozappdir}/*.so
%{mozappdir}/omni.ja
%{mozappdir}/dependentlibs.list
%{mozappdir}/dictionaries

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
