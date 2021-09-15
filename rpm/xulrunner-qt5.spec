%define greversion    78.11.0
%define milestone     %{greversion}

%define embedlite_config merqtxulrunner

%define compile_environment 1
%define system_nspr         1
%define system_nss          1
%define system_sqlite       1
%define system_ffi          1
%define system_hunspell     1
%define system_jpeg         1
%define system_png          1
%define system_icu          1
%define system_zlib         1
%define system_bz2          1
%define system_pixman       1
%define system_libvpx       1
%define system_libwebp      1

%global mozappdir     %{_libdir}/%{name}-%{milestone}
%global mozappdirdev  %{_libdir}/%{name}-devel-%{milestone}

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
Patch1:     0001-sailfishos-gecko-Add-symlink-to-embedlite.-JB-52893.patch
Patch2:     0002-sailfishos-qt-Bring-back-Qt-layer.-JB-50505.patch
Patch3:     0003-sailfishos-ipc-Whitelist-sync-messages-of-EmbedLite..patch
Patch4:     0004-sailfishos-gecko-Remove-x11-dependency-declaration-f.patch
Patch5:     0005-sailfishos-gecko-Disable-unaligned-FP-access-emulati.patch
Patch6:     0006-sailfishos-gecko-Fix-embedlite-building.-JB-50505.patch
Patch7:     0007-sailfishos-gecko-Disable-MOC-code-generation-for-mes.patch
Patch8:     0008-sailfishos-qt-Add-Qt-headers-to-system-headers.-Fixe.patch
Patch9:     0009-sailfishos-gecko-Backport-Embed-MessageLoop-contruct.patch
Patch10:    0010-sailfishos-gecko-Add-including-of-nsRefPtrHashtable..patch
Patch11:    0011-sailfishos-compositor-Fix-GLContextProvider-defines.patch
Patch12:    0012-sailfishos-gecko-Disable-Gecko-Rust-Feature-cubeb-re.patch
Patch13:    0013-Bug-1685883-building-with-disable-marionette-fails-w.patch
Patch14:    0014-sailfishos-gecko-Enable-Pango-for-the-build.-JB-5086.patch
Patch15:    0015-Revert-Bug-1567888-remove-unneeded-QT-related-rules-.patch
Patch16:    0016-sailfishos-qt-Provide-checkbox-radio-renderer-for-Sa.patch
Patch17:    0017-sailfishos-gecko-Add-missing-include-for-nsIObserver.patch
Patch18:    0018-sailfishos-compositor-Make-it-possible-to-extend-Com.patch
Patch19:    0019-sailfishos-mozglue-Introduce-EmbedInitGlue-to-the-mo.patch
Patch20:    0020-sailfishos-gecko-Remove-static-registration-of-the-m.patch
Patch21:    0021-sailfishos-compositor-Allow-compositor-specializatio.patch
Patch22:    0022-sailfishos-gecko-Hackish-fix-for-preferences-usage-i.patch
Patch23:    0023-sailfishos-gecko-Force-to-build-mozglue-and-xpcomglu.patch
Patch24:    0024-Revert-Bug-445128-Stop-putting-the-version-number-in.patch
Patch25:    0025-Revert-Bug-1427455-Remove-unused-variables-from-base.patch
Patch26:    0026-Revert-Bug-1333826-Remove-SDK_FILES-SDK_LIBRARY-and-.patch
Patch27:    0027-Revert-Bug-1333826-Remove-the-make-sdk-build-target-.patch
Patch28:    0028-Revert-Bug-1333826-Remove-a-few-references-from-.mk-.patch
Patch29:    0029-sailfishos-configure-Disable-LTO-for-rust-1.52.1-wit.patch
Patch30:    0030-sailfishos-gecko-Add-missing-GetTotalScreenPixels-fo.patch
Patch31:    0031-sailfishos-fonts-Load-and-set-FTLibrary-for-the-Fact.patch
Patch32:    0032-sailfishos-gecko-Create-EmbedLiteCompositorBridgePar.patch
Patch33:    0033-sailfishos-configure-Read-rustc-host-from-environmen.patch
Patch34:    0034-sailfishos-configure-Drop-thumbv7neon-and-thumbv7a-p.patch
Patch35:    0035-sailfishos-configure-Get-target-and-host-from-enviro.patch
Patch36:    0036-sailfishos-configure-Patch-libloading-to-build-on-ar.patch
Patch37:    0037-sailfishos-configure-Skip-min_libclang_version-test-.patch
Patch38:    0038-sailfishos-configure-Patch-glslopt-to-build-on-arm.patch
Patch39:    0039-sailfishos-embedlite-egl-Fix-mesa-egl-display-initia.patch
Patch40:    0040-sailfishos-egl-Do-not-create-CreateFallbackSurface.-.patch
Patch41:    0041-sailfishos-gecko-Fix-gfxPlatform-AsyncPanZoomEnabled.patch
Patch42:    0042-sailfishos-gecko-Avoid-incorrect-compiler-optimisati.patch
Patch43:    0043-sailfishos-gecko-Drop-static-casting-from-BrowserChi.patch
Patch44:    0044-sailfishos-docshell-Get-ContentFrameMessageManager-v.patch
Patch45:    0045-Revert-Bug-1445570-Remove-EnsureEventualAfterPaint-t.patch
#Patch10:    0010-sailfishos-gecko-Remove-PuppetWidget-from-TabChild-i.patch
#Patch11:    0011-sailfishos-gecko-Make-TabChild-to-work-with-TabChild.patch
#Patch12:    0012-sailfishos-build-Fix-build-error-with-newer-glibc.patch
#Patch15:    0015-sailfishos-gecko-Nullify-delayed-work-timer-after-ca.patch
#Patch17:    0017-sailfishos-gecko-Workaround-for-late-access-message-.patch
#Patch18:    0018-sailfishos-gecko-Limit-surface-area-rather-than-widt.patch
#Patch19:    0019-sailfishos-gecko-Make-TextureImageEGL-hold-a-referen.patch
#Patch20:    0020-sailfishos-loginmanager-Adapt-LoginManager-to-EmbedL.patch
#Patch21:    0021-sailfishos-gecko-Make-fullscreen-enabling-work-as-us.patch
#Patch22:    0022-sailfishos-gecko-Embedlite-doesn-t-have-prompter-imp.patch
#Patch24:    0024-sailfishos-gecko-Use-libcontentaction-for-custom-sch.patch
#Patch25:    0025-sailfishos-gecko-Handle-temporary-directory-similarl.patch
#Patch26:    0026-sailfishos-gecko-Disable-loading-heavier-extensions.patch
#Patch28:    0028-sailfishos-gecko-Avoid-rogue-origin-points-when-clip.patch
#Patch29:    0029-sailfishos-gecko-Allow-render-shaders-to-be-loaded-f.patch
#Patch30:    0030-sailfishos-gecko-Prioritize-GMP-plugins-over-all-oth.patch
#Patch31:    0031-sailfishos-gecko-Delete-startupCache-if-it-s-stale.patch
#Patch33:    0033-sailfishos-gecko-Skip-invalid-WatchId-in-geolocation.patch
#Patch34:    0034-sailfishos-locale-Get-12-24h-timeformat-setting-from.patch
#Patch35:    0035-sailfishos-contentaction-Fix-content-action-integrat.patch
#Patch36:    0036-sailfishos-qt-Initialize-FreeType-library-properly.-.patch
#Patch37:    0037-sailfishos-disable-TLS-1.0-and-1.1.patch
#Patch38:    0038-sailfishos-gecko-Use-registered-IHistory-service-imp.patch
#Patch39:    0039-sailfishos-gecko-Suppress-LoginManagerContent.jsm-ow.patch
#Patch40:    0040-sailfishos-configuration-Configure-application-as-mo.patch
#Patch41:    0041-sailfishos-gecko-Include-XUL-videocontrols-reflow-co.patch
#Patch42:    0042-sailfishos-gecko-Adjust-audio-control-dimensions.-Co.patch
#Patch43:    0043-sailfishos-gecko-Prioritize-loading-of-extension-ver.patch
#Patch44:    0044-sailfishos-media-Ensure-audio-continues-when-screen-.patch
#Patch45:    0045-sailfishos-backport--Make-MOZSIGNALTRAMPOLINE-Andro-.patch
#Patch46:    0046-sailfishos-gecko-Force-recycling-of-gmpdroid-instanc.patch
#Patch47:    0047-sailfishos-gecko-Hardcode-loopback-address-for-profi.patch
#Patch48:    0048-sailfishos-backport-Enable-MOZ_GECKO_PROFILER-on-And.patch
#Patch49:    0049-sailfishos-backport-Implement-DWARF-stack-walker-for.patch
#Patch50:    0050-sailfishos-gecko-Make-button-hit-testing-similar-to-.patch
#Patch51:    0051-sailfishos-gecko-Remove-android-define-from-logging.patch
#Patch52:    0052-sailfishos-gecko-Change-behaviour-of-urlclassifier.s.patch
#Patch54:    0054-sailfishos-gecko-Start-using-user-agent-builder.-JB-.patch
#Patch55:    0055-sailfishos-gecko-Enable-event.srcElement-on-all-chan.patch
#Patch56:    0056-sailfishos-gecko-Hide-accessible-carets-also-with-to.patch
#Patch57:    0057-Bug-1449268-Treat-document-level-touch-event-listene.patch
#Patch58:    0058-sailfishos-gecko-Log-bad-tex-upload-calls-and-errors.patch
#Patch59:    0059-sailfishos-gecko-Ignore-safemode-in-gfxPlatform.-Fix.patch
#Patch60:    sha1: f7510599feade696486db448d1bfec613871adc2
#Patch61:    sha1: f7510599feade696486db448d1bfec613871adc2
#Patch62:    sha1: f7510599feade696486db448d1bfec613871adc2
#Patch60 - Patch62 are not needed as issue was fixed in mozilla62 (https://bugzilla.mozilla.org/show_bug.cgi?id=1467722)
#Patch64:    sha1: 2a51338a5a287eb0e505edb6ec59912ad7eccb33
#Patch64:    Is not needed as the issue was fixed in mozilla72 (https://bugzilla.mozilla.org/show_bug.cgi?id=1586144)
#Patch65:    0065-Fix-flipped-FBO-textures-when-rendering-to-an-offscr.patch
#Patch66:    sha1: d5d87241408eec6071c0a21412d800b2b02c0554
#Patch67:    sha1: d5d87241408eec6071c0a21412d800b2b02c0554
#Patch68:    sha1: d5d87241408eec6071c0a21412d800b2b02c0554
#Patch69:    sha1: d5d87241408eec6071c0a21412d800b2b02c0554
#Patch66 - Patch69 should be checked by Denis
#Patch70:    0070-Do-not-flip-scissor-rects-when-rendering-to-an-offsc.patch

BuildRequires:  rust
BuildRequires:  rust-std-static
BuildRequires:  cargo
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.25.0
%endif
%if %{system_nss}
BuildRequires:  pkgconfig(nss) >= 3.53.1
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
BuildRequires:  cbindgen
BuildRequires:  llvm
BuildRequires:  clang-devel
BuildRequires:  python3-sqlite
BuildRequires:  libatomic

%if %{system_icu}
BuildRequires:  libicu >= 67.1
BuildRequires:  libicu-devel >= 67.1
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
BuildRequires:  libpng >= 1.6.35
%endif
%if %{system_jpeg}
BuildRequires:  libjpeg-turbo-devel
%endif
%ifarch i586 i486 i386 x86_64
BuildRequires:  yasm
BuildRequires:  nasm >= 2.14
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
%if %{system_libwebp}
BuildRequires:  pkgconfig(libwebp)
BuildRequires:  pkgconfig(libwebpdemux)
%endif

%description
Mozilla XUL runner

%package devel
Requires: %{name} = %{version}-%{release}
Conflicts: xulrunner-devel
Summary: Headers for xulrunner
# Auto dependency is not picking this up.
%if %{system_nss}
Requires: pkgconfig(nss) >= 3.53.1
%endif

%description devel
Development files for xulrunner.

%package misc
Requires: %{name} = %{version}-%{release}
Summary: Misc files for xulrunner

%description misc
Tests and misc files for xulrunner.

# Build output directory.
%define BUILD_DIR "$PWD"/../obj-build-mer-qt-xr
# EmbedLite config used to configure the engine.
%define BASE_CONFIG "$PWD"/../embedding/embedlite/config/mozconfig.%{embedlite_config}

%prep
%autosetup -p1 -n %{name}-%{version}/gecko-dev

%ifarch %arm32
%define SB2_TARGET armv7-unknown-linux-gnueabihf
%endif
%ifarch %arm64
%define SB2_TARGET aarch64-unknown-linux-gnu
%endif
%ifarch %ix86
%define SB2_TARGET i686-unknown-linux-gnu
%endif

echo "Target is %SB2_TARGET"

mkdir -p "%BUILD_DIR"
cp -rf "%BASE_CONFIG" "%BUILD_DIR"/mozconfig
echo "export MOZCONFIG=%BUILD_DIR/mozconfig" >> "%BUILD_DIR"/rpm-shared.env
echo "export LIBDIR='%{_libdir}'" >> "%BUILD_DIR"/rpm-shared.env
echo "export QT_QPA_PLATFORM=minimal" >> "%BUILD_DIR"/rpm-shared.env
echo "export MOZ_OBJDIR=%BUILD_DIR" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_HOME=%BUILD_DIR/cargo" >> "%BUILD_DIR"/rpm-shared.env

# When cross-compiling under SB2 rust needs to know what arch to emit
# when nothing is specified on the command line. That usually defaults
# to "whatever rust was built as" but in SB2 rust is accelerated and
# would produce x86 so this is how it knows differently. Not needed
# for native x86 builds
echo "export SB2_RUST_TARGET_TRIPLE=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env
echo "export RUST_HOST_TARGET=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env

echo "export RUST_TARGET=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env
echo "export TARGET=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env
echo "export HOST=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env
echo "export SB2_TARGET=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env

%ifarch %arm32 %arm64
# This should be define...
echo "export CROSS_COMPILE=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env

# This avoids a malloc hang in sb2 gated calls to execvp/dup2/chdir
# during fork/exec. It has no effect outside sb2 so doesn't hurt
# native builds.
export SB2_RUST_EXECVP_SHIM="/usr/bin/env LD_PRELOAD=/usr/lib/libsb2/libsb2.so.1 /usr/bin/env"
export SB2_RUST_USE_REAL_EXECVP=Yes
export SB2_RUST_USE_REAL_FN=Yes
%endif

echo "export CC=gcc" >> "%BUILD_DIR"/rpm-shared.env
echo "export CXX=g++" >> "%BUILD_DIR"/rpm-shared.env
echo "export AR=\"gcc-ar\"" >> "%BUILD_DIR"/rpm-shared.env
echo "export NM=\"gcc-nm\"" >> "%BUILD_DIR"/rpm-shared.env
echo "export RANLIB=\"gcc-ranlib\"" >> "%BUILD_DIR"/rpm-shared.env

echo "export CARGOFLAGS=\" --offline\"" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_NET_OFFLINE=1" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_BUILD_TARGET=armv7-unknown-linux-gnueabihf" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_CFG_TARGET_ARCH=arm" >> "%BUILD_DIR"/rpm-shared.env

# Force MOZ_BUILD_DATE env var in order to have more reproducible builds
# only when we're building from tarball (OBS)
# If you want to have a fixed date, then uncomment the line below
# echo "export MOZ_BUILD_DATE=20210831010100" >> "%BUILD_DIR"/rpm-shared.env
for a in %{_sourcedir}/*.tar.bz2; do
    if [ -f $a ]; then
        TARBALL_DATE=`stat -c %Y $a`
        BUILD_DATE=`date -d @${TARBALL_DATE} +"%Y%m%d%H%M%%S"`
        echo "export MOZ_BUILD_DATE=${BUILD_DATE}" >> "%BUILD_DIR"/rpm-shared.env
    fi
    break
done

%build

source "%BUILD_DIR"/rpm-shared.env

# hack for when not using virtualenv
ln -sf "%BUILD_DIR"/config.status $PWD/build/config.status

%ifarch %arm32 %arm64
# Make stdc++ headers available on a fresh path to work around include_next bug JB#55058
if [ ! -L "%BUILD_DIR"/include ] ; then ln -s /usr/include/c++/8.3.0/ "%BUILD_DIR"/include; fi

# Expose the elf32-i386 libclang.so.10 for use inside the arm target, JB#55042
mkdir -p "%BUILD_DIR"/lib
SBOX_DISABLE_MAPPING=1 cp /usr/lib/libclang.so.10 "%BUILD_DIR"/lib/libclang.so.10
echo "ac_add_options --with-libclang-path='"%BUILD_DIR"/lib/'" >> "$MOZCONFIG"

# Do not build as thumb since it breaks video decoding.
%ifarch %arm32
echo "ac_add_options --with-thumb=no" >> "$MOZCONFIG"
%endif
%endif

echo "mk_add_options MOZ_OBJDIR='%BUILD_DIR'" >> "$MOZCONFIG"
# XXX: gold crashes when building gecko for both i486 and x86_64
#echo "export CFLAGS=\"\$CFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export CXXFLAGS=\"\$CXXFLAGS -fuse-ld=gold \"" >> "$MOZCONFIG"
#echo "export LD=ld.gold" >> "$MOZCONFIG"
echo "ac_add_options --disable-strip" >> "$MOZCONFIG"
echo "ac_add_options --disable-install-strip" >> "$MOZCONFIG"

# Reduce logging from release build
# Doesn't work so disabled for now. Should be made logging-specific.
# %if "%{?qa_stage_name}" == testing || "%{?qa_stage_name}" == release
#echo "export CFLAGS=\"\$CFLAGS -DRELEASE_OR_BETA=1\"" >> "$MOZCONFIG"
#echo "export CXXFLAGS=\"\$CXXFLAGS -DRELEASE_OR_BETA=1\"" >> "$MOZCONFIG"
#%endif

# Override the milestone for building devel gecko when needed
echo "%{milestone}" > "$PWD/config/milestone.txt"

%if %{compile_environment}
  echo "ac_add_options --enable-compile-environment" >> "$MOZCONFIG"
%endif

%if %{system_nss}
  echo "ac_add_options --with-system-nss" >> "$MOZCONFIG"
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

%if %{system_pixman}
  echo "ac_add_options --enable-system-pixman" >> "${MOZCONFIG}"
%endif

%if %{system_libvpx}
  echo "ac_add_options --with-system-libvpx" >> "${MOZCONFIG}"
%endif

%if %{system_libwebp}
  echo "ac_add_options --with-system-webp" >> "${MOZCONFIG}"
%endif

%ifarch %ix86
echo "ac_add_options --disable-startupcache" >> "$MOZCONFIG"
echo "ac_add_options --host=i686-unknown-linux-gnu" >> "$MOZCONFIG"
%endif

%ifarch %arm32
echo "ac_add_options --disable-startupcache" >> "$MOZCONFIG"
echo "ac_add_options --host=armv7-unknown-linux-gnueabihf" >> "$MOZCONFIG"
%endif

%ifarch %arm64
echo "ac_add_options --disable-startupcache" >> "$MOZCONFIG"
echo "ac_add_options --host=aarch64-unknown-linux-gnu" >> "$MOZCONFIG"
%endif

%ifarch %ix86 %arm32
echo "ac_add_options --disable-elf-hack" >> "$MOZCONFIG"
%endif

# Gecko tries to add the gre lib dir to LD_LIBRARY_PATH when loading plugin-container, 
# but as sailfish-browser has privileged EGID, glibc removes it for security reasons. 
# Set ELF RPATH through LDFLAGS. Needed for plugin-container and libxul.so
# Additionally we limit the memory usage during linking
%ifarch %arm32 %arm64
# Garbage collect on arm to reduce memory requirements, JB#55074
echo 'FIX_LDFLAGS="-Wl,--gc-sections -Wl,--reduce-memory-overheads -Wl,--no-keep-memory -Wl,-rpath=%{mozappdir}"' >> "${MOZCONFIG}"
%else
echo 'FIX_LDFLAGS="-Wl,--reduce-memory-overheads -Wl,--no-keep-memory -Wl,-rpath=%{mozappdir}"' >> "${MOZCONFIG}"
%endif
echo 'export LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
echo 'LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
echo 'export WRAP_LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"
echo 'mk_add_options LDFLAGS="$FIX_LDFLAGS"' >> "${MOZCONFIG}"

RPM_BUILD_NCPUS=`nproc`

./mach build -j$RPM_BUILD_NCPUS
# This might be unnecessary but previously some files
# were only behind FASTER_RECURSIVE_MAKE but only adds few
# minutes for the build.
./mach build faster FASTER_RECURSIVE_MAKE=1 -j$RPM_BUILD_NCPUS

%install
source "%BUILD_DIR"/rpm-shared.env
# See above for explanation of SB2_ variables (needed in both build/install phases)
%ifarch %arm32
export SB2_RUST_TARGET_TRIPLE=armv7-unknown-linux-gnueabihf
%endif
%ifarch %arm64
export SB2_RUST_TARGET_TRIPLE=aarch64-unknown-linux-gnu
%endif
%ifarch %arm32 %arm64
export SB2_RUST_EXECVP_SHIM="/usr/bin/env LD_PRELOAD=/usr/lib/libsb2/libsb2.so.1 /usr/bin/env"
export SB2_RUST_USE_REAL_EXECVP=Yes
export SB2_RUST_USE_REAL_FN=Yes
%endif

%{__make} -C %BUILD_DIR/mobile/sailfishos/installer install DESTDIR=%{buildroot}

rm -rf ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/libxul.so
ln -s %{mozappdir}/libxul.so ${RPM_BUILD_ROOT}%{mozappdirdev}/sdk/lib/libxul.so

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
%dir %{mozappdir}
%dir %{mozappdir}/defaults
%{mozappdir}/*.so
%{mozappdir}/omni.ja
%{mozappdir}/dependentlibs.list
%{mozappdir}/dictionaries
%{mozappdir}/plugin-container
%{mozappdir}/platform.ini

%files devel
%defattr(-,root,root,-)
%{mozappdirdev}
%{_libdir}/pkgconfig
%{_includedir}/*

%files misc
%defattr(-,root,root,-)
%{_bindir}/*
%{mozappdir}/*
%exclude %dir %{mozappdir}/defaults
%exclude %{mozappdir}/*.so
%exclude %{mozappdir}/omni.ja
%exclude %{mozappdir}/dependentlibs.list
%exclude %{mozappdir}/dictionaries
%exclude %{mozappdir}/plugin-container
%exclude %{mozappdir}/platform.ini
