pref("dom.w3c_touch_events.enabled", 1);
pref("dom.w3c_touch_events.legacy_apis.enabled", true);
pref("dom.w3c_pointer_events.enabled", true);
pref("dom.meta-viewport.enabled", true);
pref("plugins.force.wmode", "opaque");
pref("browser.xul.error_pages.enabled", true);
pref("nglayout.debug.paint_flashing", false);
pref("nglayout.debug.widget_update_flashing", false);

// Override some named colors to avoid inverse OS themes
pref("ui.-moz-dialog", "#efebe7");
pref("ui.-moz-dialogtext", "#101010");
pref("ui.-moz-field", "#fff");
pref("ui.-moz-fieldtext", "#1a1a1a");
pref("ui.-moz-buttonhoverface", "#f3f0ed");
pref("ui.-moz-buttonhovertext", "#101010");
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
pref("gl.msaa-level", 0);
// Enable webgl by force
pref("webgl.force-enabled", true);
// Setup dumping enabled for development
pref("browser.dom.window.dump.enabled", true);
pref("layers.acceleration.draw-fps", false);
pref("layers.use-deprecated-textures", false);
pref("layers.enable-tiles", true);
pref("layers.async-pan-zoom.enabled", true);
pref("font.size.inflation.disabledInMasterProcess", true);
// We want to limit layers for two reasons:
// 1) We can't scroll smoothly if we have to many draw calls
// 2) Pages that have too many layers consume too much memory and crash.
// By limiting the number of layers on mobile we're making the main thread
// work harder keep scrolling smooth and memory low.
pref("layers.max-active", 20);
// Avoid stalling the render thread if frames are missed
pref("gfx.vsync.compositor.unobserve-count", 40);

// APZC preferences.
pref("apz.allow_zooming", true);
pref("apz.fling_accel_base_mult", "1.125f");
pref("apz.min_skate_speed", "1.0f");

// Gaia relies heavily on scroll events for now, so lets fire them
// more often than the default value (100).

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
pref("apz.subframe.enabled", true);

// Overscroll-related settings
pref("apz.overscroll.enabled", false);
pref("apz.overscroll.stretch_factor", "0.5");
pref("apz.overscroll.spring_stiffness", "0.001");
pref("apz.overscroll.spring_friction", "0.015");
pref("apz.overscroll.stop_distance_threshold", "5.0");
pref("apz.overscroll.stop_velocity_threshold", "0.01");

// Improves the responsiveness of content actions, see bug #1247280
pref("apz.content_response_timeout", 600);
// Turning off touch-action gives more responsive touch panning
// counteracting the effect of the increased response timeout
pref("layout.css.touch_action.enabled", false);

pref("ui.dragThresholdX", 25);
pref("ui.dragThresholdY", 25);
pref("embedlite.dispatch_mouse_events", false); // Will dispatch mouse events if page using them
pref("media.gstreamer.enabled", true);
pref("media.prefer-gstreamer", true);
pref("media.gstreamer.enable-blacklist", false);
// Disable X backend on GTK
pref("gfx.xrender.enabled", false);

pref("extensions.update.enabled", false);
pref("extensions.systemAddon.update.enabled", false);

pref("toolkit.storage.synchronous", 0);
/* new html5 forms */
// Support for input type=color. By default, disabled.
pref("dom.forms.color", false);
// Support for input type=date and type=time. By default, disabled.
pref("dom.forms.datetime", false);
// Support for input type=month, type=week and type=datetime-local. By default,
// disabled.
pref("dom.forms.datetime.others", false);
pref("dom.experimental_forms", false);
pref("extensions.getAddons.cache.enabled", true);
pref("toolkit.browser.contentViewExpire", 3000);

pref("browser.viewport.desktopWidth", 980);
// The default fallback zoom level to render pages at. Set to -1 to fit page; otherwise
// the value is divided by 1000 and clamped to hard-coded min/max scale values.
pref("browser.viewport.defaultZoom", -1);

/* cache prefs */
pref("browser.cache.disk.capacity", 20480); // kilobytes
pref("browser.cache.disk.max_entry_size", 4096); // kilobytes
pref("browser.cache.disk.smart_size.enabled", true);
pref("browser.cache.disk.smart_size.first_run", true);

pref("browser.cache.memory.capacity", 1024); // kilobytes
pref("browser.cache.memory_limit", 5120); // 5 MB

/* image cache prefs */
pref("image.cache.size", 1048576); // bytes

/* offline cache prefs */
pref("browser.offline-apps.notify", true);
pref("browser.cache.offline.enable", true);
pref("browser.cache.offline.capacity", 5120); // kilobytes
pref("offline-apps.quota.warn", 1024); // kilobytes

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

// spdy
pref("network.http.spdy.push-allowance", 32768);
pref("network.http.spdy.default-hpack-buffer", 4096); // 4k

// Racing the cache with the network should be disabled to prevent accidental
// data usage.
pref("network.http.rcwn.enabled", false);

// See bug 545869 for details on why these are set the way they are
pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  16384);

// predictive actions
pref("network.predictor.enabled", true);
pref("network.predictor.max-db-size", 2097152); // bytes
pref("network.predictor.preserve", 50); // percentage of predictor data to keep when cleaning up

/* session history */
pref("browser.sessionhistory.max_total_viewers", 1);
pref("browser.sessionhistory.max_entries", 50);
pref("browser.sessionhistory.contentViewerTimeout", 360);

/* these should help performance */
pref("mozilla.widget.force-24bpp", true);
pref("mozilla.widget.use-buffer-pixmap", true);
pref("layout.css.report_errors", false);
pref("layout.reflow.synthMouseMove", false);
pref("mozilla.widget.disable-native-theme", false);
pref("layers.enable-tiles", true);
pref("layers.low-precision-buffer", true);
pref("layers.low-precision-opacity", "1.0");
pref("layers.progressive-paint", true);

/* password manager */
pref("signon.rememberSignons", true);
pref("signon.autofillForms", true);
pref("signon.expireMasterPassword", false);
pref("signon.debug", false);

/* form helper */
// 0 = disabled, 1 = enabled, 2 = dynamic depending on screen size
pref("formhelper.mode", 2);
pref("formhelper.autozoom", true);
pref("formhelper.autozoom.caret", true);
pref("formhelper.restore", false);

/* find helper */
pref("findhelper.autozoom", true);

/* autocomplete */
pref("browser.formfill.enable", true);

/* spellcheck */
pref("layout.spellcheckDefault", 0);

/* block popups by default, and notify the user about blocked popups */
pref("dom.disable_open_during_load", true);
pref("privacy.popups.showBrowserMessage", true);

/* disable opening windows with the dialog feature */
pref("dom.disable_window_open_dialog_feature", true);
pref("dom.disable_window_showModalDialog", true);
pref("dom.disable_window_print", true);
pref("dom.disable_window_find", true);

// SSL error page behaviour
pref("browser.ssl_override_behavior", 2);
pref("browser.xul.error_pages.expert_bad_cert", false);

// disable logging for the search service by default
pref("browser.search.log", false);

// disable updating
pref("browser.search.update", false);
pref("browser.search.update.log", false);
pref("browser.search.updateinterval", 6);

// disable search suggestions by default
pref("browser.search.suggest.enabled", false);
pref("browser.search.suggest.prompted", false);

// tell the search service that we don't really expose the "current engine"
pref("browser.search.noCurrentEngine", true);

// Let the faviconservice know that we display favicons as 32x32px so that it
// uses the right size when optimizing favicons
pref("places.favicons.optimizeToDimension", 32);

// disable color management
pref("gfx.color_management.mode", 0);

// 0=fixed margin, 1=velocity bias, 2=dynamic resolution, 3=no margins, 4=prediction bias
pref("gfx.displayport.strategy", 1);

// disable Graphite font shaping by default on Android until memory footprint
// of using the Charis SIL fonts that we ship with the product is addressed
// (see bug 700023, bug 846832, bug 847344)
pref("gfx.font_rendering.graphite.enabled", false);

// Enable hardware-accelerated Skia canvas
pref("gfx.canvas.azure.backends", "skia");
pref("gfx.canvas.azure.accelerated", true);

// don't allow JS to move and resize existing windows
pref("dom.disable_window_move_resize", true);

// prevent click image resizing for nsImageDocument
pref("browser.enable_click_image_resizing", false);

// open in tab preferences
// 0=default window, 1=current window/tab, 2=new window, 3=new tab in most window
pref("browser.link.open_external", 3);
pref("browser.link.open_newwindow", 3);
// 0=force all new windows to tabs, 1=don't force, 2=only force those with no features set
pref("browser.link.open_newwindow.restriction", 0);

// controls which bits of private data to clear. by default we clear them all.
pref("privacy.item.cache", true);
pref("privacy.item.cookies", true);
pref("privacy.item.offlineApps", true);
pref("privacy.item.history", true);
pref("privacy.item.formdata", true);
pref("privacy.item.downloads", true);
pref("privacy.item.passwords", true);
pref("privacy.item.sessions", true);
pref("privacy.item.geolocation", true);
pref("privacy.item.siteSettings", true);
pref("privacy.item.syncAccount", true);

// Disable the JS engine's gc on memory pressure, since we do one in the mobile
// browser (bug 669346).
pref("javascript.options.gc_on_memory_pressure", false);

// Garbage collection configuration, slightly tweaked for low memory devices
pref("javascript.options.mem.high_water_mark", 64);

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
pref("dom.max_child_script_run_time", 0);

// plugins
pref("plugin.disable", true);
pref("dom.ipc.plugins.enabled", false);

pref("plugins.click_to_play", true);
// The default value for nsIPluginTag.enabledState (STATE_CLICKTOPLAY = 1)
pref("plugin.default.state", 1);

pref("notification.feature.enabled", true);
pref("dom.webnotifications.enabled", true);

// prevent tooltips from showing up
pref("browser.chrome.toolbar_tips", false);
pref("dom.indexedDB.warningQuota", 5);

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

// Enable GMP plugins for media decoding to use gmp-droid
pref("media.gmp.decoder.enabled", true);
pref("media.decoder.recycle.enabled", true);

// SimplePush
pref("services.push.enabled", false);

// controls if we want camera support
pref("device.camera.enabled", true);
pref("media.realtime_decoder.enabled", true);

pref("dom.report_all_js_exceptions", true);
pref("javascript.options.showInConsole", true);

// Coalesce touch events to prevent them from flooding the event queue
pref("dom.event.touch.coalescing.enabled", false);

// On memory pressure, release dirty but unused pages held by jemalloc
// back to the system.
pref("memory.free_dirty_pages", true);

// Enable Web Audio for Firefox for Android in Nightly and Aurora
pref("media.webaudio.enabled", true);

// Make <audio> and <video> talk to the AudioChannelService.
pref("media.useAudioChannelService", true);

pref("extensions.blocklist.enabled", false);
pref("extensions.logging.enabled", false);
pref("extensions.strictCompatibility", false);
pref("extensions.minCompatibleAppVersion", "11.0");

// Enable sub layers for apzc
pref("apz.subframe.enabled", true);

// Enable HTML fullscreen API in content.
pref("full-screen-api.enabled", true);
// Don't make top-level widgets fullscreen. This only applies when running in
// "metrodesktop" mode, not when running in full metro mode. This prevents the
// window from changing size when we go fullscreen; the content expands to fill
// the window, the window size doesn't change. This pref has no effect when
// running in actual Metro mode, as the widget will already be fullscreen then.
pref("full-screen-api.ignore-widgets", true);

// Match defaults for android and b2g, see: modules/libpref/init/all.js
// Remove and test once mozilla bug #1158392 is fixed.
// Test cases are mentioned in https://bugzilla.mozilla.org/show_bug.cgi?id=1158392#c3
pref("layout.scroll.root-frame-containers", true);

// Disable health report / telemetry services explicitly.
pref("toolkit.telemetry.unified", false);
pref("toolkit.telemetry.enabled", false);
pref("experiments.enabled", false);
pref("experiments.supported", false);
pref("datareporting.healthreport.service.enabled", false);

// Align security prefs from Android FF
// Name of alternate about: page for certificate errors (when undefined, defaults to about:neterror)
pref("security.alternate_certificate_error_page", "certerror");
// Block insecure active content on https pages
pref("security.mixed_content.block_active_content", true);
// Enable pinning
pref("security.cert_pinning.enforcement_level", 1);
// Only fetch OCSP for EV certificates
pref("security.OCSP.enabled", 2);

// The audio backend, see cubeb_init && CubebUtils.cpp (sCubebBackendName)
pref("media.cubeb.backend", "pulse");

// On ESR60 customelements is only enabled for nightly. Enable for us.
pref("dom.webcomponents.customelements.enabled", true);

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

// Enable the Visual Viewport API
pref("dom.visualviewport.enabled", true);

// Use the platform decoder for VPX-encoded video during a WebRTC call
pref("media.navigator.mediadatadecoder_vpx_enabled", true);

// Support for the dialog element.
pref("dom.dialog_element.enabled", true);
