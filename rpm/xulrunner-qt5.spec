%define greversion 31.7.0
%global __provides_exclude_from %{_libdir}/%{name}-%{greversion}/.*[^libmozalloc]\\.so

Name:       xulrunner-qt5
Summary:    XUL runner
Version:    %{greversion}
Release:    1
Group:      Applications/Internet
License:    Mozilla License
URL:        http://hg.mozilla.org/mozilla-central
Source0:    %{name}-%{version}.tar.bz2
Patch1:     0001-Configure-system-sqlite-to-use-jemalloc.-jb25229.patch
Patch2:     0002-Workaround-for-bug-977015.patch
Patch3:     0003-Invalidate-obsolete-scroll-offset.-jb20430.patch
Patch4:     0004-Workaround-wrong-viewport-in-wikipedia.patch
Patch5:     0005-Implement-transition-from-pinching-to-panning.patch
Patch6:     0006-Workaround-for-wrong-viewport-after-orientation-chan.patch
Patch7:     0007-Workaround-for-blank-page-after-scroll.-jb21266.patch
Patch8:     0008-Supply-source-uri-to-gstreamer-pipeline.patch
Patch9:     0009-Workaround-for-late-access-message-loop.-jb10381.patch
Patch10:    0010-Define-HAS_NEMO_RESOURCE-in-config.patch
Patch11:    0011-Disable-destructible.patch
Patch12:    0012-Don-t-zoom-video-when-in-full-screen.patch
Patch13:    0013-Disallow-image-locking-no-matter-what.patch
Patch14:    0014-Notify-UI-about-change-in-composition-bounds.-jb1799.patch
Patch15:    0015-limit-surface-area-rather-than-width-and-height.patch
Patch16:    0016-Enable-external-window-usage.patch
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(libproxy-1.0)
BuildRequires:  pkgconfig(gstreamer-0.10)
BuildRequires:  pkgconfig(gstreamer-app-0.10)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
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
Tests and misc files for xulrunner

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
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
%patch16 -p1

%build
export DONT_POPULATE_VIRTUALENV=1
export PYTHONPATH=$PWD/python:$PWD/config:$PWD/build:$PWD/xpcom/typelib/xpt/tools:$PWD/dom/bindings:$PWD/dom/bindings/parser:$PWD/other-licenses/ply:$PWD/media/webrtc/trunk/tools/gyp/pylib/
for i in $(find $PWD/python $PWD/testing/mozbase -mindepth 1 -maxdepth 1 -type d); do
  export PYTHONPATH+=:$i
done
export SBOX_REDIRECT_FORCE=/usr/bin/python
# hack for when not using virtualenv
ln -sf $PWD/obj-build-mer-qt-xr/config.status $PWD/build/config.status

cp -rf embedding/embedlite/config/mozconfig.merqtxulrunner mozconfig

%ifarch %arm
echo "ac_add_options --with-arm-kuser" >> mozconfig
echo "ac_add_options --with-float-abi=toolchain-default" >> mozconfig
# No need for this, this should be managed by toolchain
echo "ac_add_options --with-thumb=toolchain-default" >> mozconfig
%endif
echo "mk_add_options MOZ_MAKE_FLAGS='-j%jobs'" >> mozconfig
echo "export CFLAGS=\"\$CFLAGS -fuse-ld=gold \"" >> mozconfig
echo "export CXXFLAGS=\"\$CXXFLAGS -fuse-ld=gold \"" >> mozconfig
echo "export LD=ld.gold" >> mozconfig
echo "ac_add_options --disable-tests" >> mozconfig
echo "ac_add_options --enable-system-hunspell" >> mozconfig
echo "ac_add_options --enable-system-sqlite" >> mozconfig
echo "ac_add_options --enable-libproxy" >> mozconfig
echo "ac_add_options --enable-jemalloc" >> mozconfig
echo "ac_add_options --disable-strip" >> mozconfig
echo "ac_add_options --disable-mochitest" >> mozconfig
echo "ac_add_options --disable-installer" >> mozconfig
echo "ac_add_options --disable-javaxpcom" >> mozconfig
echo "ac_add_options --disable-crashreporter" >> mozconfig
echo "ac_add_options --without-x" >> mozconfig
echo "ac_add_options --with-app-name=%{name}" >> mozconfig
export MOZCONFIG=$PWD/mozconfig
%{__make} -f client.mk build STRIP="/bin/true" %{?jobs:MOZ_MAKE_FLAGS="-j%jobs"}

%install
export DONT_POPULATE_VIRTUALENV=1
export PYTHONPATH=$PWD/python:$PWD/config:$PWD/build:$PWD/xpcom/typelib/xpt/tools:$PWD/dom/bindings:$PWD/dom/bindings/parser:$PWD/other-licenses/ply:$PWD/media/webrtc/trunk/tools/gyp/pylib/
for i in $(find $PWD/python $PWD/testing/mozbase -mindepth 1 -maxdepth 1 -type d); do
  export PYTHONPATH+=:$i
done
export SBOX_REDIRECT_FORCE=/usr/bin/python

export MOZCONFIG=$PWD/mozconfig
%{__make} -f client.mk install DESTDIR=%{buildroot}
%{__chmod} +x %{buildroot}%{_libdir}/%{name}-%{greversion}/*.so
%fdupes -s %{buildroot}%{_includedir}
%fdupes -s %{buildroot}%{_libdir}
chmod +x %{buildroot}%{_libdir}/%{name}-%{greversion}/*.so
# Use the system hunspell dictionaries
%{__rm} -rf ${RPM_BUILD_ROOT}%{_libdir}/%{name}-%{greversion}/dictionaries
ln -s %{_datadir}/myspell ${RPM_BUILD_ROOT}%{_libdir}/%{name}-%{greversion}/dictionaries
mkdir ${RPM_BUILD_ROOT}%{_libdir}/%{name}-%{greversion}/defaults

# symlink to the system libnssckbi.so (CA trust library). It is replaced by
# the p11-kit-nss-ckbi package to use p11-kit's trust store.
# There is a strong binary compatibility guarantee.
rm ${RPM_BUILD_ROOT}%{_libdir}/%{name}-%{greversion}/libnssckbi.so
ln -s %{_libdir}/libnssckbi.so ${RPM_BUILD_ROOT}%{_libdir}/%{name}-%{greversion}/libnssckbi.so

%post
# >> post
touch /var/lib/_MOZEMBED_CACHE_CLEAN_
# << post

%files
%defattr(-,root,root,-)
%attr(755,-,-) %{_bindir}/*
%dir %{_libdir}/%{name}-%{greversion}
%dir %{_libdir}/%{name}-%{greversion}/defaults
%{_libdir}/%{name}-%{greversion}/*.so
%{_libdir}/%{name}-%{greversion}/omni.ja
%{_libdir}/%{name}-%{greversion}/dependentlibs.list
%{_libdir}/%{name}-%{greversion}/dictionaries

%files devel
%defattr(-,root,root,-)
%{_datadir}/*
%{_libdir}/%{name}-devel-%{greversion}
%{_libdir}/pkgconfig
%{_includedir}/*

%files misc
%defattr(-,root,root,-)
%{_libdir}/%{name}-%{greversion}/*
%exclude %{_libdir}/%{name}-%{greversion}/*.so
%exclude %{_libdir}/%{name}-%{greversion}/omni.ja
%exclude %{_libdir}/%{name}-%{greversion}/dependentlibs.list
