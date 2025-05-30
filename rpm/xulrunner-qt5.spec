%define greversion    91.13.1
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
# TODO: Adapt vp9 codec to the new libvpx API. For now, use the internal libvpx (v1.6.1).
%define system_libvpx       0
%define system_libwebp      1


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
Patch1:     0001-sailfishos-gecko-Add-symlink-to-embedlite.-JB-52893.patch
Patch2:     0002-sailfishos-qt-Bring-back-Qt-layer.-JB-50505.patch
Patch3:     0003-sailfishos-gecko-Fix-embedlite-building.-JB-50505.patch
Patch4:     0004-sailfishos-gecko-Revert-Bug-1611386-Drop-support-for.patch
Patch5:     0005-sailfishos-gecko-Fix-build-version-requirements.patch
Patch6:     0006-sailfishos-gecko-Read-rustc-host-from-environment.-J.patch
Patch7:     0007-sailfishos-qt-Provide-checkbox-radio-renderer-for-Sa.patch
Patch8:     0008-sailfishos-compositor-Fix-GLContextProvider-defines.patch
Patch9:     0009-sailfishos-ipc-Whitelist-sync-messages-of-EmbedLite..patch
Patch10:    0010-sailfishos-components-Cleanup-static-components-defi.patch
Patch11:    0011-sailfishos-gecko-Reduce-Rust-build-requirements.patch
Patch12:    0012-sailfishos-gecko-Patch-glslopt-to-build-on-arm.patch
Patch13:    0013-sailfishos-gecko-Disable-MOC-code-generation-for-mes.patch
Patch14:    0014-sailfishos-gecko-Backport-Embed-MessageLoop-contruct.patch
Patch15:    0015-sailfishos-gecko-Work-around-upstream-membarrier-cha.patch
Patch16:    0016-sailfishos-gecko-Allow-compositor-specializations-to.patch
Patch17:    0017-sailfishos-gecko-Revert-Bug-1676576-Remove-unused-fu.patch
Patch18:    0018-sailfishos-gecko-Restore-GLScreenBuffer-and-TextureI.patch
Patch19:    0019-sailfishos-gecko-Hackish-fix-for-preferences-usage-i.patch
Patch20:    0020-sailfishos-gecko-Revert-Bug-1706051-Remove-some-IPC-.patch
Patch21:    0021-sailfishos-gecko-Remove-NS_LITERAL_CSTRING-usage.patch
Patch22:    0022-sailfishos-gecko-Revert-Bug-1494175-Remove-unimpleme.patch
Patch23:    0023-sailfishos-gecko-Fix-embedlite-building.-JB-50505.patch
Patch24:    0024-sailfishos-gecko-Update-ProcInfo.patch
Patch25:    0025-sailfishos-gecko-Revert-Bug-1567888-remove-unneeded-.patch
Patch26:    0026-sailfishos-gecko-Restore-nsAppShell.h.patch
Patch27:    0027-sailfishos-gecko-Add-support-for-aarch64-to-elfhack..patch
Patch28:    0028-sailfishos-gecko-Allow-gen_last_modified.py-to-compl.patch
Patch29:    0029-sailfishos-gecko-Force-to-build-mozglue-and-xpcomglu.patch
Patch30:    0030-sailfishos-gecko-Revert-Bug-445128-Stop-putting-the-.patch
Patch31:    0031-sailfishos-gecko-Revert-Bug-1427455-Remove-unused-va.patch
Patch32:    0032-sailfishos-gecko-Revert-Bug-1333826-Remove-SDK_FILES.patch
Patch33:    0033-sailfishos-gecko-Revert-Bug-1333826-Remove-the-make-.patch
Patch34:    0034-sailfishos-gecko-Revert-Bug-1333826-Remove-a-few-ref.patch
Patch35:    0035-sailfishos-gecko-Introduce-EmbedInitGlue-to-the-mozg.patch
Patch36:    0036-sailfishos-gecko-Split-namespace-into-two-blocks.patch
Patch37:    0037-sailfishos-gecko-Create-EmbedLiteCompositorBridgePar.patch
Patch38:    0038-sailfishos-egl-Do-not-create-CreateFallbackSurface.-.patch
Patch39:    0039-sailfishos-gecko-Make-PresShell-SetIsActive-public.patch
Patch40:    0040-sailfishos-egl-Drop-swap_buffers_with_damage-extensi.patch
Patch41:    0041-sailfishos-gecko-Add-patch-to-fix-32-bit-builds.patch
Patch42:    0042-sailfishos-gecko-Fix-gfxPlatform-AsyncPanZoomEnabled.patch
Patch43:    0043-sailfishos-gecko-Supress-URLQueryStrippingListServic.patch
Patch44:    0044-sailfishos-gecko-Allow-file-scheme-when-loading-Open.patch
Patch45:    0045-sailfishos-gecko-Add-and-adjust-embedlite-static-pre.patch
Patch46:    0046-sailfishos-gecko-Disable-SessionStore-functionality.patch
Patch47:    0047-sailfishos-gecko-Enable-dconf.patch
Patch48:    0048-sailfishos-gecko-Prevent-errors-from-DownloadPrompte.patch
Patch49:    0049-sailfishos-gecko-Restore-NotifyDidPaint-event-and-ti.patch
Patch50:    0050-sailfishos-gecko-Adapt-build-configuration-for-Sailf.patch
Patch51:    0051-sailfishos-webrtc-Update-GN-build-files-for-WebRTC.-.patch
Patch52:    0052-sailfishos-gecko-Disable-desktop-sharing-feature-on-.patch
Patch53:    0053-sailfishos-gecko-Enable-GMP-for-encoding-decoding.-J.patch
Patch54:    0054-sailfishos-webrtc-Implement-video-capture-module.-JB.patch
Patch55:    0055-sailfishos-webrtc-Regenerate-moz.build-files.-JB-537.patch
Patch56:    0056-sailfishos-gecko-Drop-AudioPlayback-messages-if-no-e.patch
Patch57:    0057-sailfishos-gecko-Get-ContentFrameMessageManager-via-.patch
Patch58:    0058-sailfishos-gecko-Convert-panic-into-early-return-in-.patch
Patch59:    0059-sailfishos-gecko-Allow-LoginManagerPrompter-to-find-.patch
Patch60:    0060-sailfishos-gecko-Add-support-for-prefers-color-schem.patch
Patch61:    0061-sailfishos-gecko-Update-hash-for-mapped_hyph.patch
Patch62:    0062-sailfishos-gecko-Fix-content-action-integration-to-w.patch
Patch63:    0063-sailfishos-gecko-Make-fullscreen-enabling-work-as-us.patch
Patch64:    0064-sailfishos-gecko-Prioritize-GMP-plugins-over-all-oth.patch
Patch65:    0065-sailfishos-gecko-Force-recycling-of-gmp-droid-instan.patch
Patch66:    0066-sailfishos-gecko-Force-use-of-mobile-video-controls..patch
Patch67:    0067-sailfishos-gecko-Fix-video-hardware-accelaration-not.patch
Patch68:    0068-sailfishos-gecko-Add-a-video-decoder-based-on-gecko-.patch
Patch69:    0069-sailfishos-gecko-Fix-audio-underruns-for-fullduplex-.patch
Patch70:    0070-sailfishos-gecko-Bug-1750760-Create-ffmpeg59-module-.patch
Patch71:    0071-sailfishos-gecko-Bug-1750760-Open-libavcodec.so.59-l.patch
Patch72:    0072-sailfishos-gecko-Bug-1750760-Update-audio-and-video-.patch
Patch73:    0073-sailfishos-gecko-Bug-1761471-FFmpeg-5.0-Get-frame-co.patch
Patch74:    0074-sailfishos-gecko-Bug-1758948-FFmpeg-Use-AVFrame-pts-.patch
Patch75:    0075-sailfishos-gecko-Ensure-audio-continues-when-screen-.patch
Patch76:    0076-sailfishos-gecko-Fix-build-failure-due-to-rust-lang-.patch
Patch77:    0077-sailfishos-gecko-Fix-unstable-name-collisions-warnin.patch
Patch78:    0078-sailfishos-embedlite-egl-Fix-mesa-egl-display-and-bu.patch
Patch79:    0079-sailfishos-gecko-Delete-startupCache-if-it-s-stale.patch
Patch80:    0080-sailfishos-gecko-Hardcode-loopback-address-for-profi.patch
Patch81:    0081-sailfishos-gecko-Start-using-user-agent-builder.-JB-.patch
Patch82:    0082-sailfishos-gecko-Disallow-page-zooming-if-the-meta-v.patch
Patch83:    0083-sailfishos-gecko-Add-preference-to-bypass-CORS-on-ns.patch
Patch84:    0084-sailfishos-gecko-Get-12-24h-timeformat-setting-from-.patch
Patch85:    0085-Bug-1710603-Allow-stat-on-from-socket-process-for-gl.patch
Patch86:    0086-Bug-1782988-Fix-use-of-arc4random_buf-use-in-ping.cp.patch
Patch87:    0087-Bug-1777674-Add-missing-cstdint-include-to-support-G.patch
Patch88:    0088-Bug-1811714-Add-a-few-missing-cstdint-includes-r-gfx.patch
Patch89:    0089-sailfishos-gecko-Update-content-signature-root-hash..patch
Patch90:    0090-Bug-1766848-Update-libevent-to-version-2.1.12.-r-jld.patch
Patch91:    0091-Bug-1782988-Avoid-build-bustage-when-building-agains.patch
Patch92:    0092-Bug-1773259-Work-around-build-failure-with-newer-cbi.patch

BuildRequires:  rust
BuildRequires:  rust-std-static
BuildRequires:  cargo
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(pango)
BuildRequires:  pkgconfig(alsa)
%if %{system_nspr}
BuildRequires:  pkgconfig(nspr) >= 4.32.0
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
BuildRequires:  pkgconfig(geckocamera)
BuildRequires:  qt5-qttools
BuildRequires:  qt5-default
BuildRequires:  autoconf213
BuildRequires:  automake
BuildRequires:  python3-base
BuildRequires:  python3-sqlite
BuildRequires:  python3-devel
BuildRequires:  zip
BuildRequires:  unzip
BuildRequires:  qt5-plugin-platform-minimal
BuildRequires:  cbindgen >= 0.19.0
BuildRequires:  llvm
BuildRequires:  clang-devel
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

# Expose the elf32-i386 libclang.so.15 for use inside the arm target, JB#55042
mkdir -p "%BUILD_DIR"/lib
SBOX_DISABLE_MAPPING=1 cp /usr/lib/libclang.so.15 "%BUILD_DIR"/lib/
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
# Silence repeating compiler warnings
echo "export CFLAGS=\"\$CFLAGS -Wno-psabi -Wno-attributes \"" >> "$MOZCONFIG"
echo "export CXXFLAGS=\"\$CXXFLAGS -Wno-psabi -Wno-attributes \"" >> "$MOZCONFIG"
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

export MACH_USE_SYSTEM_PYTHON=1

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
