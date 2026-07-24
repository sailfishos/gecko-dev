%define greversion    115.37.0
%define milestone     %{greversion}

%define embedlite_config merqtxulrunner

%define compile_environment 1
%define system_nspr         1
%define system_nss          1
%define system_sqlite       1
%define system_ffi          1
%define system_jpeg         1
%define system_png          1
%define system_icu          1
%define system_zlib         1
%define system_pixman       1
%define system_libvpx       1
%define system_libwebp      1


%global mozappdir     %{_libdir}/%{name}-%{greversion}
%global mozappdirdev  %{_libdir}/%{name}-devel-%{greversion}

# Private/bundled libs the final package should not provide or depend on.
%global privlibs             libfreebl3
%global privlibs %{privlibs}|libmozalloc
%if !%{system_sqlite}
%global privlibs %{privlibs}|libmozsqlite3
%endif
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

# Conditional for using a workaround which moves the .git away before build
# in order to prevent cargo from updating git modules
# This should only be needed for local builds on SDK as tar_git strips out
# the .git directory already.
%bcond_with git_workaround


Name:       xulrunner-qt5
Summary:    XUL runner
Version:    %{greversion}
Release:    1
License:    MPLv2.0
URL:        https://github.com/sailfishos/gecko-dev
Source0:    %{name}-%{version}.tar.bz2
Patch1:     0001-Add-symlink-to-embedlite.-JB-52893.patch
Patch2:     0002-Bring-back-Qt-layer.-JB-50505.patch
Patch3:     0003-Fix-embedlite-building.-JB-50505.patch
Patch4:     0004-Read-rustc-host-from-environment.-JB-53019-OMP-JOLLA.patch
Patch5:     0005-Provide-checkbox-radio-renderer-for-Sailfish-OS.-Con.patch
Patch6:     0006-Fix-GLContextProvider-defines.patch
Patch7:     0007-Whitelist-sync-messages-of-EmbedLite.-JB-50505.patch
Patch8:     0008-Cleanup-static-components-definitions.-JB-55835-OMP-.patch
Patch9:     0009-Reduce-Rust-build-requirements.patch
Patch10:     0010-Patch-glslopt-to-build-on-arm.patch
Patch11:     0011-Disable-MOC-code-generation-for-message_pump_qt.patch
Patch12:     0012-Backport-Embed-MessageLoop-contructor-back-sha1-eb2d.patch
Patch13:     0013-Allow-compositor-specializations-to-override-the-com.patch
Patch14:     0014-Revert-Bug-1676576-Remove-unused-functions-of-Compos.patch
Patch15:     0015-Hackish-fix-for-preferences-usage-in-Parent-process-.patch
Patch16:     0016-Revert-Bug-1706051-Remove-some-IPC-messages-that-are.patch
Patch17:     0017-Revert-Bug-1494175-Remove-unimplemented-nsIWebBrowse.patch
Patch18:     0018-Fix-embedlite-building.-JB-50505.patch
Patch19:     0019-Revert-Bug-1567888-remove-unneeded-QT-related-rules-.patch
Patch20:     0020-Allow-gen_last_modified.py-to-complete.patch
Patch21:     0021-Force-to-build-mozglue-and-xpcomglue-static-librarie.patch
Patch22:     0022-Revert-Bug-445128-Stop-putting-the-version-number-in.patch
Patch23:     0023-Revert-Bug-1427455-Remove-unused-variables-from-base.patch
Patch24:     0024-Revert-Bug-1333826-Remove-SDK_FILES-SDK_LIBRARY-and-.patch
Patch25:     0025-Revert-Bug-1333826-Remove-the-make-sdk-build-target-.patch
Patch26:     0026-Revert-Bug-1333826-Remove-a-few-references-from-.mk-.patch
Patch27:     0027-Introduce-EmbedInitGlue-to-the-mozglue.-JB-50788.patch
Patch28:     0028-Split-namespace-into-two-blocks.patch
Patch29:     0029-Do-not-create-CreateFallbackSurface.-JB-55226-OMP-JO.patch
Patch30:     0030-Make-PresShell-SetIsActive-public.patch
Patch31:     0031-Drop-swap_buffers_with_damage-extension-support.-Fix.patch
Patch32:     0032-Add-patch-to-fix-32-bit-builds.patch
Patch33:     0033-Fix-gfxPlatform-AsyncPanZoomEnabled-for-embedlite.-J.patch
Patch34:     0034-Supress-URLQueryStrippingListService.jsm-error.patch
Patch35:     0035-Allow-file-scheme-when-loading-OpenSearch-providers.patch
Patch36:     0036-Add-and-adjust-embedlite-static-prefs.patch
Patch37:     0037-Disable-SessionStore-functionality.patch
Patch38:     0038-Prevent-errors-from-DownloadPrompter.patch
Patch39:     0039-Restore-NotifyDidPaint-event-and-timers.patch
Patch40:     0040-Adapt-build-configuration-for-SailfishOS.-JB-53756.patch
Patch41:     0041-Update-GN-build-files-for-WebRTC.-JB-53756.patch
Patch42:     0042-Disable-desktop-sharing-feature-on-SFOS.-JB-53756.patch
Patch43:     0043-Implement-video-capture-module.-JB-53982.patch
Patch44:     0044-Regenerate-moz.build-files.-JB-53756.patch
Patch45:     0045-Drop-AudioPlayback-messages-if-no-embedder-element-i.patch
Patch46:     0046-Get-ContentFrameMessageManager-via-nsIDocShellTreeOw.patch
Patch47:     0047-Convert-panic-into-early-return-in-Hyphenator.patch
Patch48:     0048-Allow-LoginManagerPrompter-to-find-its-window.-JB-55.patch
Patch49:     0049-Add-support-for-prefers-color-scheme-JB-58394.patch
Patch50:     0050-Update-hash-for-mapped_hyph.patch
Patch51:     0051-Fix-content-action-integration-to-work.-Fixes-JB-512.patch
Patch52:     0052-Make-fullscreen-enabling-work-as-used-to-with-pref-f.patch
Patch53:     0053-Force-use-of-mobile-video-controls.-JB-55484-OMP-JOL.patch
Patch54:     0054-Add-a-video-decoder-based-on-gecko-camera.-JB-56755.patch
Patch55:     0055-Fix-audio-underruns-for-fullduplex-mode.-JB-55461.patch
Patch56:     0056-Ensure-audio-continues-when-screen-is-locked.-Contri.patch
Patch57:     0057-Delete-startupCache-if-it-s-stale.patch
Patch58:     0058-Hardcode-loopback-address-for-profile-lock-filename..patch
Patch59:     0059-Start-using-user-agent-builder.-JB-52068.patch
Patch60:     0060-Disallow-page-zooming-if-the-meta-viewport-scale-is-.patch
Patch61:     0061-Add-preference-to-bypass-CORS-on-nsContentSecurityMa.patch
Patch62:     0062-Get-12-24h-timeformat-setting-from-dconf.-Fixes-JB-5.patch
Patch63:     0063-Bug-1710603-Allow-stat-on-from-socket-process-for-gl.patch
Patch64:     0064-Update-content-signature-root-hash.-JB-63099.patch
Patch65:     0065-Bug-1773259-Work-around-build-failure-with-newer-cbi.patch
Patch66:     0066-Bug-1880013-Update-glslopt-to-0.1.10-for-C-20-fix.-r.patch
Patch67:     0067-Bug-1913645-Update-glslopt-to-0.1.11-to-fix-aarch64-.patch
Patch68:     0068-Bug-1998927-Update-glslopt-to-explicitly-define-MOZI.patch
Patch69:     0069-Bug-1999625-Update-glslopt-to-0.1.13.-r-gfx-reviewer.patch
Patch70:     0070-Bug-2017954-Update-glslopt-to-0.1.14-r-jnicol-gfx-re.patch
Patch71:     0071-Clean-up-Gecko-configure-integration.patch
Patch72:     0072-Adapt-EmbedLite-WebRender-offscreen-compositing.patch
Patch73:     0073-Restore-EmbedLite-native-prompt-dialogs.patch
Patch74:     0074-Guard-media-sink-suspend-before-initialization.patch
Patch75:     0075-Allow-disabling-default-protocol-handler-injection.patch
Patch76:     0076-Fix-EmbedLite-toolkit-error-pages.patch
Patch77:     0077-Use-EmbedLite-helper-app-dialog-static-registration.patch
Patch78:     0078-Load-EmbedLite-search-engines-from-settings.patch
Patch79:     0079-Dispatch-MDSM-initialization-without-tail-dispatch.patch
Patch80:     0080-Preload-autoplay-metadata-before-inaudible-check.patch
Patch81:     0081-Fix-Qt-form-control-theme-rendering.patch
Patch82:     0082-Keep-about-support-snapshot-resilient-in-EmbedLite.patch
Patch83:     0083-Register-gecko-camera-decoder-in-remote-video-paths.patch
Patch84:     0084-Use-system-sqlite.patch
Patch85:     0085-Support-Qt-EGL-display-on-Mesa.patch
Patch86:     0086-Support-software-WebRender-on-EmbedLite.patch
Patch87:     0087-Add-safe-Qt-display-fallback-for-hybris.patch

BuildRequires:  rust >= 1.66.0
BuildRequires:  rust-std-static
BuildRequires:  cargo >= 1.66.0
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(alsa)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.32.0
%endif
%if %{system_nss}
BuildRequires:  pkgconfig(nss) >= 3.90
%endif
%if %{system_sqlite}
BuildRequires:  pkgconfig(sqlite3) >= 3.41.2
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
BuildRequires:  pkgconfig(geckocamera)
BuildRequires:  qt5-qttools
BuildRequires:  qt5-default
BuildRequires:  autoconf213
BuildRequires:  automake
BuildRequires:  python3-base
BuildRequires:  python3-curses
BuildRequires:  python3-sqlite
BuildRequires:  python3-devel
BuildRequires:  zip
BuildRequires:  unzip
BuildRequires:  qt5-plugin-platform-minimal
BuildRequires:  cbindgen >= 0.24.3
BuildRequires:  llvm
BuildRequires:  clang-devel
BuildRequires:  libatomic

%if %{system_icu}
BuildRequires:  libicu >= 73.1
BuildRequires:  libicu-devel >= 73.1
%endif
# Required by bundled freetype bzip2 support; ESR115 has no system-bz2 toggle.
BuildRequires:  bzip2-devel
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
Requires: pkgconfig(nss) >= 3.90
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

# Keep system library toggles in the spec so OBS and local SDK builds use the
# same switches, and avoid duplicate configure options from the base mozconfig.
sed -i \
    -e '/^ac_add_options --with-system-jpeg$/d' \
    -e '/^ac_add_options --with-system-nspr$/d' \
    -e '/^ac_add_options --with-system-nss$/d' \
    "%BUILD_DIR"/mozconfig

# ESR115 libxul debug sections are too large for the SailfishOS aarch64
# debugedit/strip path. Build the packaged engine without C/C++ debug symbols;
# Rust debug info is already disabled in the patch stack.
sed -i \
    -e '/^export MOZ_DEBUG_SYMBOLS=/d' \
    -e '/^ac_add_options --enable-debug-symbols/d' \
    "%BUILD_DIR"/mozconfig
echo "ac_add_options --disable-debug-symbols" >> "%BUILD_DIR"/mozconfig

: > "%BUILD_DIR"/rpm-shared.env
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
echo "export READELF=readelf" >> "%BUILD_DIR"/rpm-shared.env
echo "export RANLIB=\"gcc-ranlib\"" >> "%BUILD_DIR"/rpm-shared.env

echo "export CARGOFLAGS=\" --offline\"" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_NET_OFFLINE=1" >> "%BUILD_DIR"/rpm-shared.env
echo "export CARGO_BUILD_TARGET=%SB2_TARGET" >> "%BUILD_DIR"/rpm-shared.env
%ifarch %arm32
echo "export CARGO_CFG_TARGET_ARCH=arm" >> "%BUILD_DIR"/rpm-shared.env
%endif
%ifarch %arm64
echo "export CARGO_CFG_TARGET_ARCH=aarch64" >> "%BUILD_DIR"/rpm-shared.env
%endif

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

# Move the .git directory out of the way as cargo gets confused and thinks it
# needs to update our submodule.
%if %{with git_workaround}
%__mv %_builddir/.git %_builddir/.git-disabled ||:
%endif

source "%BUILD_DIR"/rpm-shared.env

# hack for when not using virtualenv
ln -sf "%BUILD_DIR"/config.status $PWD/build/config.status

%ifarch %arm32 %arm64
# Make stdc++ headers available on a fresh path to work around include_next bug JB#55058
if [ ! -L "%BUILD_DIR"/include ] ; then ln -s /usr/include/c++/*/ "%BUILD_DIR"/include; fi

# Expose the elf32-i386 libclang.so.17 for use inside the arm target, JB#55042
mkdir -p "%BUILD_DIR"/lib
SBOX_DISABLE_MAPPING=1 cp /usr/lib/libclang.so.17 "%BUILD_DIR"/lib/
echo "export MOZ_LIBCLANG_BINDGEN_PATH='"%BUILD_DIR"/lib/'" >> "$MOZCONFIG"

%ifarch %arm64
echo "ac_add_options --with-libclang-path='/usr/lib64/'" >> "$MOZCONFIG"
%else
echo "ac_add_options --with-libclang-path='/usr/lib/'" >> "$MOZCONFIG"
%endif

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
# Silence repeating compiler warnings
echo "export CFLAGS=\"\$CFLAGS -Wno-psabi -Wno-attributes \"" >> "$MOZCONFIG"
echo "export CXXFLAGS=\"\$CXXFLAGS -Wno-psabi -Wno-attributes \"" >> "$MOZCONFIG"
# llvm-readelf maps the whole libxul.so and can run out of memory under the
# devel SDK/qemu when debug info is enabled. GNU readelf streams this check.
echo "export READELF=readelf" >> "$MOZCONFIG"
echo "ac_add_options --disable-strip" >> "$MOZCONFIG"
echo "ac_add_options --disable-install-strip" >> "$MOZCONFIG"
# The elfhack self-test executes target binaries through qemu during the
# aarch64 devel SDK build and currently crashes after libxul links.
echo "ac_add_options --disable-elf-hack" >> "$MOZCONFIG"

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

%if %{system_nspr}
  echo "ac_add_options --with-system-nspr" >> "$MOZCONFIG"
%endif

%if %{system_nss}
  echo "ac_add_options --with-system-nss" >> "$MOZCONFIG"
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
echo "ac_add_options --host=armv7-unknown-linux-gnueabihf" >> "$MOZCONFIG"
echo "ac_add_options --disable-debug-symbols" >> "$MOZCONFIG"
%endif

%ifarch %arm64
echo "ac_add_options --host=aarch64-unknown-linux-gnu" >> "$MOZCONFIG"
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

export MACH_BUILD_PYTHON_NATIVE_PACKAGE_SOURCE=system

./mach build -j$RPM_BUILD_NCPUS
# This might be unnecessary but previously some files
# were only behind FASTER_RECURSIVE_MAKE but only adds few
# minutes for the build.
./mach build faster FASTER_RECURSIVE_MAKE=1 -j$RPM_BUILD_NCPUS

# Restore .git directory after build
%if %{with git_workaround}
%__mv %_builddir/.git-disabled %_builddir/.git ||:
%endif

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
%dir %{mozappdir}
%dir %{mozappdir}/defaults
%{mozappdir}/*.so
%{mozappdir}/omni.ja
%{mozappdir}/dependentlibs.list
%{mozappdir}/dictionaries
%{mozappdir}/plugin-container
%{mozappdir}/platform.ini

%files devel
%{mozappdirdev}
%{_libdir}/pkgconfig
%{_includedir}/*

%files misc
%{_bindir}/*
%{mozappdir}/*
%exclude %dir %{mozappdir}/defaults
%exclude %{mozappdir}/*.so
%exclude %{mozappdir}/omni.ja
%exclude %{mozappdir}/dependentlibs.list
%exclude %{mozappdir}/dictionaries
%exclude %{mozappdir}/plugin-container
%exclude %{mozappdir}/platform.ini
