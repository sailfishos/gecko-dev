# EmbedLite browser initialization

EmbedLite runs Gecko in the Sailfish application's Qt main thread.  The Qt
event dispatcher drives `EmbedLiteMessagePump`, and startup posts one explicit
runtime task after the application loop is installed.  No secondary Gecko
runtime thread or embedding child process exists.

Every `EmbedLiteWindow` owns an `EmbedLiteWindowParent`, which in turn owns an
`EmbedLiteHostedWindow`.  The hosted window creates the root `nsWindow`, then
asks `nsIAppShellService` for a normal chrome AppWindow.  The temporary
`AutoEmbedLiteChromeWindowHost` reservation attaches the AppWindow's top-level
widget to that root.  Initialization completes from AppWindow and browser
events; teardown runs in the inverse ownership order and reports completion to
Qt only after the AppWindow and widget are gone.

`browser.xhtml` contains the chrome tab container.  Each logical tab is a XUL
`<browser remote="true">`, so Firefox creates the standard remote
`PContent`/`PBrowser`/`PWindowGlobal` actors and selects the content process.
EmbedLite does not create a local docshell for web content and does not emulate
a Firefox content actor.

Private windows pass `CHROME_PRIVATE_WINDOW` to
`nsIAppShellService::CreateTopLevelWindow`.  Firefox therefore creates the
private chrome browsing context before the remote browser is attached, and
the frame loader inherits the private origin attributes.  Private windows own
their AppWindow, logical tab, and lifetime; they do not enter the normal
session-restore or persistence path.

Popup and target-window requests are handled by
`EmbedLiteBrowserDOMWindow`.  It creates another logical remote browser in the
owning AppWindow and supplies Firefox's `nsIOpenWindowInfo` to that browser.
The parent process never receives a content DOM window.

`EmbedLiteAppService` maps only hosted remote browsing contexts and logical
tab endpoints.  Synchronous embedding messages and APIs that require a local
`nsIWebBrowser` or content `mozIDOMWindowProxy` report that they are not
available.  Input, APZ, content messages, prompts, and snapshots are routed
through the hosted chrome session.
