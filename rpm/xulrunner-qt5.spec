%define greversion    38.0.5
%global mozappdir     %{_libdir}/%{name}-%{greversion}
%global mozappdirdev  %{_libdir}/%{name}-devel-%{greversion}

# Private/bundled libs the final package should not provide or depend on.
%global privlibs             libfreebl3
%global privlibs %{privlibs}|libmozalloc
%global privlibs %{privlibs}|libmozsqlite3
%global privlibs %{privlibs}|libnspr4
%global privlibs %{privlibs}|libnss3
%global privlibs %{privlibs}|libnssdbm3
%global privlibs %{privlibs}|libnssutil3
%global privlibs %{privlibs}|libplc4
%global privlibs %{privlibs}|libplds4
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
License:    Mozilla License
URL:        http://hg.mozilla.org/mozilla-central
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(sqlite3)
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
BuildRequires:  hunspell-devel
BuildRequires:  libjpeg-turbo-devel
%ifarch i586 i486 i386 x86_64
BuildRequires:  yasm
%endif
BuildRequires:  fdupes
# See below on why the system version of this library is used
Requires: nss-ckbi >= 3.16.6
Requires: gstreamer1.0-plugins-good

%description
Mozilla XUL runner

%package devel
Group: Applications/Internet
Requires: %{name} = %{version}-%{release}
Conflicts: xulrunner-devel
Summary: Headers for xulrunner

%description devel
Development files for xulrunner.

%package misc
Group: Applications/Internet
Requires: %{name} = %{version}-%{release}
Summary: Misc files for xulrunner

%description misc
Tests and misc files for xulrunner.

# Build output directory.
%define BUILD_DIR "$PWD"/obj-build-mer-qt-xr
# EmbedLite config used to configure the engine.
%define BASE_CONFIG "$PWD"/embedding/embedlite/config/mozconfig.merqtxulrunner

%prep
%setup -q -n %{name}-%{version}

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
echo "ac_add_options --enable-system-hunspell" >> "$MOZCONFIG"
echo "ac_add_options --disable-strip" >> "$MOZCONFIG"
echo "ac_add_options --disable-mochitest" >> "$MOZCONFIG"
echo "ac_add_options --disable-installer" >> "$MOZCONFIG"
echo "ac_add_options --disable-javaxpcom" >> "$MOZCONFIG"
echo "ac_add_options --with-app-name=%{name}" >> "$MOZCONFIG"

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

# symlink to the system libnssckbi.so (CA trust library). It is replaced by
# the p11-kit-nss-ckbi package to use p11-kit's trust store.
# There is a strong binary compatibility guarantee.
rm ${RPM_BUILD_ROOT}%{mozappdir}/libnssckbi.so
ln -s %{_libdir}/libnssckbi.so ${RPM_BUILD_ROOT}%{mozappdir}/libnssckbi.so

# Fix some of the RPM lint errors.
find "%{buildroot}%{_includedir}" -type f -name '*.h' -exec chmod 0644 {} +;

%post
# >> post
touch /var/lib/_MOZEMBED_CACHE_CLEAN_
# << post

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
