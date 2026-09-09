# EmbedLite browser initialization

EmbedLite runs Gecko's parent runtime in the Sailfish application's process
(`sailfish-browser` for Browser), on its Qt main thread.  The Qt event
dispatcher drives `EmbedLiteMessagePump`, and startup posts one explicit
runtime task after the application loop is installed.  There is no separate
"hosted" process, secondary embedding runtime thread, or custom embedding
child process.  Web content runs in Firefox-managed Web Content processes;
Gecko still uses its normal worker and compositor threads.

Every `EmbedLiteWindow` owns an `EmbedLiteWindowParent`, which in turn owns an
`EmbedLiteHostedWindow`.  The hosted window creates the root `nsWindow`, then
asks `nsIAppShellService` for a normal chrome AppWindow.  The temporary
`AutoEmbedLiteChromeWindowHost` reservation attaches the AppWindow's top-level
widget to that root.  Initialization completes from AppWindow and browser
events; teardown runs in the inverse ownership order and reports completion to
Qt only after the AppWindow and widget are gone.

"Hosted" refers to this internal Gecko chrome window and its remote tabs.
The AppWindow is distinct from the application's visible Qt Quick window;
it does not provide Browser's Sailfish toolbar or other QML UI.

`browser.xhtml` contains the chrome tab container.  Each logical tab is a XUL
`<browser remote="true">`, so Firefox creates the standard remote
`PContent`/`PBrowser`/`PWindowGlobal` actors and selects the content process.
EmbedLite does not create a local docshell for web content and does not emulate
a Firefox content actor.

Private windows pass `CHROME_PRIVATE_WINDOW` to
`nsIAppShellService::CreateTopLevelWindow`.  Firefox therefore creates the
private chrome browsing context before the remote browser is attached.
`EmbedLiteChromeSessionChild::CreateBrowserForTab()` also takes the opening
origin attributes (or the parent context's attributes), synchronizes them with
the parent context's `UsePrivateBrowsing()`, and sets `RemoteType` using
`SharedWebRemoteType()` before setting `remote=true`.  The chrome context's
stored origin attributes alone are insufficient: its private ID can remain
zero.  Selecting the initial private remote type avoids starting as `web` and
then stalling while changing to `web=^privateBrowsingId=1` on the first load.
Private hosted windows retain their own AppWindow and lifetime; Browser
excludes private tabs from normal session persistence and restoration.

Popup and target-window requests are handled by
`EmbedLiteBrowserDOMWindow`.  It creates another logical remote browser in the
owning AppWindow and supplies Firefox's `nsIOpenWindowInfo` to that browser.
The parent process never receives a content DOM window.

`EmbedLiteAppService` maps only hosted remote browsing contexts and logical
tab endpoints.  Synchronous embedding messages and APIs that require a local
`nsIWebBrowser` or content `mozIDOMWindowProxy` report that they are not
available.  Input, APZ, content messages, prompts, and snapshots are routed
through the hosted chrome session.

## Qt Quick and Browser integration

QtMozEmbed connects hosted chrome sessions to `QuickMozView` (exposed to QML
as `QmlMozView`).  WebRender frames are imported into the Qt Quick scene
through surfaces and texture leases, with explicit frame-release and shutdown
handling.  `QMozWindow` remains shared infrastructure; the separate
`QMozOpenGLWebPage` presentation path and `QMozGrabResult` have been removed.

Browser's QML window is its sole application window.  Normal tabs, private
tabs, and captive portals use the hosted Qt Quick renderer.  `WebContainer`
is a `QQuickItem` controller; the former `DeclarativeWebPage`, `WebPages`,
`WebPageQueue`, and `WebPageFactory` presentation machinery has been removed.

Normal and private Browser tabs share `HostedTabModel` and
`HostedTabSession`, which reflect runtime tab state and route commands to
Gecko.  Normal-tab persistence remains in Browser.  Sharing this implementation
does not merge privacy contexts: private persistence is disabled and private
thumbnails are held in memory.
