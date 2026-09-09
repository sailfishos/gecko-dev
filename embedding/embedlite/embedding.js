pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_touch_events.legacy_apis.enabled", true);
pref("dom.meta-viewport.enabled", true);

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz_buttonhovertext", "#101010");
pref("ui.-moz-combobox", "#fff");
pref("ui.-moz-comboboxtext", "#101010");
pref("ui.buttonface", "#ece7e2");
pref("ui.buttonhighlight", "#fff");
pref("ui.buttonshadow", "#aea194");
pref("ui.buttontext", "#101010");
pref("ui.captiontext", "#101010");
pref("ui.graytext", "#b1a598");
pref("ui.highlight", "#fad184");
pref("ui.highlighttext", "#1a1a1a");
pref("ui.infobackground", "#f5f5b5");
pref("ui.infotext", "#000");
pref("ui.menu", "#f7f5f3");
pref("ui.menutext", "#101010");
pref("ui.threeddarkshadow", "#000");
pref("ui.threedface", "#ece7e2");
pref("ui.threedhighlight", "#fff");
pref("ui.threedlightshadow", "#ece7e2");
pref("ui.threedshadow", "#aea194");
pref("ui.window", "#efebe7");
pref("ui.windowtext", "#101010");
pref("ui.windowframe", "#efebe7");
// WebGL prefs
// Disable MSAA on mobile (similar to Android)
pref("webgl.msaa-samples", 0);
// Enable webgl by force
pref("webgl.force-enabled", true);
// Setup dumping enabled for development
pref("browser.dom.window.dump.enabled", true);
pref("layers.async-pan-zoom.enabled", true);
// Match Firefox Android's grayscale text antialiasing policy.
pref("gfx.webrender.enable-subpixel-aa", false);
// Avoid stalling the render thread if frames are missed
pref("gfx.vsync.compositor.unobserve-count", 40);
// APZC preferences.
pref("apz.allow_zooming", true);
// Match Firefox Android's 0.06-inch touch-start threshold to reduce scroll lag
// (mobile/android/app/geckoview-prefs.js, Mozilla bug 1230077).
pref("apz.touch_start_tolerance", "0.06");
pref("apz.fling_accel_base_mult", "1.125");
pref("apz.min_skate_speed", "1.0");

// APZ physics settings, tuned by UX designers
pref("apz.fling_curve_function_x1", "0.0");
pref("apz.fling_curve_function_y1", "0.0");
pref("apz.fling_curve_function_x2", "0.58");
pref("apz.fling_curve_function_y2", "1.0");
pref("apz.fling_curve_threshold_inches_per_ms", "0.03");
pref("apz.fling_friction", "0.003");
pref("apz.max_velocity_inches_per_ms", "0.07");

// Tweak default displayport values to reduce the risk of running out of
// memory when zooming in
pref("apz.x_skate_size_multiplier", "1.25");
pref("apz.y_skate_size_multiplier", "1.5");
pref("apz.x_stationary_size_multiplier", "1.5");
pref("apz.y_stationary_size_multiplier", "1.8");
pref("apz.enlarge_displayport_when_clipped", true);
// Use "sticky" axis locking
pref("apz.axis_lock.mode", 2);

// Overscroll-related settings
pref("apz.overscroll.enabled", false);
pref("apz.overscroll.spring_stiffness", "0.001");
pref("apz.overscroll.stop_distance_threshold", "5.0");

// Improves the responsiveness of content actions, see bug #1247280
pref("apz.content_response_timeout", 600);

pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);

pref("extensions.update.enabled", false);
pref("extensions.systemAddon.update.enabled", false);

// EmbedLite does not yet provide the LSNG browser-process plumbing needed by
// ESR115, so keep using the legacy localStorage backend.
pref("dom.storage.enable_unsupported_legacy_implementation", true);
/* new html5 forms */
// Support for input type=month and type=week. By default, disabled.
pref("dom.forms.datetime.others", false);
pref("extensions.getAddons.cache.enabled", true);

pref("browser.viewport.desktopWidth", 980);

/* cache prefs */
pref("browser.cache.disk.capacity", 20480); // kilobytes
pref("browser.cache.disk.max_entry_size", 4096); // kilobytes
pref("browser.cache.disk.smart_size.enabled", true);

pref("browser.cache.memory.capacity", 1024); // kilobytes

// Sailfish handles download confirmation and lifecycle UI outside Gecko.
pref("browser.download.skipConfirmLaunchExecutable", true);

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

// Automatically shrink-to-fit image documents.
pref("browser.enable_automatic_image_resizing", true);

// Default action for unlisted external protocol handlers
pref("network.protocol-handler.external-default", true);      // OK to load
pref("network.protocol-handler.warn-external-default", false); // Do not warn

// This pref controls the default settings.  Per protocol settings can be used
// to override this value. See nsDocShell::OnLinkClickSync and
// nsExternalHelperAppService::IsExposedProtocol. Protocol preference
// takes precedence.
pref("network.protocol-handler.expose-all", true);
pref("network.protocol-handler.expose.sms", false);
pref("network.protocol-handler.expose.mailto", false);
pref("network.protocol-handler.expose.tel", false);
pref("network.protocol-handler.expose.geo", false);

// Embedlite delegates external protocols to the platform. Do not add Firefox
// web-handler stubs such as Gmail for mailto:, since they bypass Sailfish
// content-action handling.
pref("gecko.handlerService.disableDefaultProtocolHandlers", true);
// Sailfish provides the external-protocol confirmation through its platform
// content-action UI rather than Gecko's browser-chrome permission dialog.
pref("security.external_protocol_requires_permission", false);

/* disable some protocol warnings */
pref("network.protocol-handler.warn-external.tel", false);
pref("network.protocol-handler.warn-external.sms", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.vnd.youtube", false);

/* http prefs */
pref("network.http.keep-alive.timeout", 109);
pref("network.http.max-connections", 40);
pref("network.http.max-persistent-connections-per-server", 6);
pref("network.http.max-persistent-connections-per-proxy", 20);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// Hosted tab sessions populate parent-owned history when a restored tab is
// materialized. Sailfish persists the lightweight session data itself, so keep
// parent BFCache and Firefox's platform SessionStore collection disabled.
pref("fission.bfcacheInParent", false);
pref("browser.sessionstore.disable_platform_collection", true);

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);

/* these should help performance */
pref("layout.css.report_errors", false);
pref("layout.reflow.synthMouseMove", false);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.autofillForms", true);
pref("signon.debug", false);

/* form helper */
pref("formhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

// SSL error page behaviour
pref("browser.xul.error_pages.expert_bad_cert", false);
pref("security.certerrors.mitm.priming.enabled", true);
pref("security.certerrors.mitm.priming.endpoint", "https://mitmdetection.services.mozilla.com/");
pref("security.certerrors.permanentOverride", true);

// disable logging for the search service by default
pref("browser.search.log", false);

// disable updating
pref("browser.search.update", false);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);

// disable color management
pref("gfx.color_management.mode", 0);

// disable Graphite font shaping by default on Android until memory footprint
// of using the Charis SIL fonts that we ship with the product is addressed
// (see bug 700023, bug 846832, bug 847344)
pref("gfx.font_rendering.graphite.enabled", false);

// Use the Skia canvas backend.
pref("gfx.canvas.azure.backends", "skia");

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

pref("font.size.inflation.minTwips", 120);

// When true, zooming will be enabled on all sites, even ones that declare user-scalable=no.
pref("browser.ui.zoom.force-user-scalable", false);

// Maximum scripts runtime before showing an alert
// Disable the watchdog thread for B2G. See bug 870043 comment 31.
pref("dom.use_watchdog", false);

// The slow script dialog can be triggered from inside the JS engine as well,
// ensure that those calls don't accidentally trigger the dialog.
pref("dom.max_script_run_time", 0);
pref("dom.max_chrome_script_run_time", 0);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);

// prevent video elements from preloading too much data
pref("media.preload.default", 1); // default to preload none
pref("media.preload.auto", 2);    // preload metadata if preload=auto
pref("media.cache_size", 32768);    // 32MB media cache
// Try to save battery by not resuming reading from a connection until we fall
// below 10s of buffered data.
pref("media.cache_resume_threshold", 10);
pref("media.cache_readahead_limit", 30);

// Number of video frames we buffer while decoding video.
// On Android this is decided by a similar value which varies for
// each OMX decoder |OMX_PARAM_PORTDEFINITIONTYPE::nBufferCountMin|. This
// number must be less than the OMX equivalent or gecko will think it is
// chronically starved of video frames. All decoders seen so far have a value
// of at least 4.
pref("media.video-queue.default-size", 3);

// Use Gecko/FFmpeg/gecko-camera media paths; gmp-droid is no longer shipped.
// Decode video in RDD and audio in the utility process.
pref("media.rdd-process.enabled", true);
pref("media.utility-process.enabled", true);
pref("media.allow-audio-non-utility", false);
pref("media.gmp.decoder.enabled", false);
pref("media.decoder.recycle.enabled", true);

pref("extensions.blocklist.enabled", false);
pref("extensions.logging.enabled", false);
pref("extensions.strictCompatibility", false);

// Enable HTML fullscreen API in content.
pref("full-screen-api.enabled", true);
// Don't make top-level widgets fullscreen. This only applies when running in
// "metrodesktop" mode, not when running in full metro mode. This prevents the
// window from changing size when we go fullscreen; the content expands to fill
// the window, the window size doesn't change. This pref has no effect when
// running in actual Metro mode, as the widget will already be fullscreen then.
pref("full-screen-api.ignore-widgets", true);

// Disable telemetry services explicitly.
pref("toolkit.telemetry.unified", false);
pref("toolkit.telemetry.enabled", false);

// Align security prefs from Android FF
// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);
// Enable pinning
pref("security.cert_pinning.enforcement_level", 1);
// Only fetch OCSP for EV certificates
pref("security.OCSP.enabled", 2);

// The audio backend, see cubeb_init && CubebUtils.cpp (sCubebBackendName)
pref("media.cubeb.backend", "pulse");

// Desktop capture is not supported by EmbedLite.
pref("media.getdisplaymedia.enabled", false);
pref("media.getusermedia.browser.enabled", false);
pref("media.getusermedia.screensharing.enabled", false);

// Enable serviceworkers
pref("dom.serviceWorkers.enabled", true);

// No native handle support (yet) for video frames, so higher resolution degrade performance
pref("media.navigator.video.default_width", 320);
pref("media.navigator.video.default_height", 240);

pref("media.webrtc.hw.h264.enabled", true);

// Many browsers prefer VP9 over H264. If the sailfish-browser is the initiator of the session,
// then the remote peer may override our preference and put VP9 in front of h264. Due to some bug,
// the gecko skips the peer's preference and creates an h264 decoder. As a workaround, disable VP9
// until the bug is fixed.
pref("media.peerconnection.video.vp9_enabled", false);

// Use the platform decoder for VPX-encoded video during a WebRTC call
pref("media.navigator.mediadatadecoder_vpx_enabled", true);
