# EmbedLite browser initialization in ESR153

## Existing creation paths

### App-created legacy view

`EmbedLiteApp::CreateView()` creates `PEmbedLiteView` from the UI-side
`EmbedLiteAppParent`.  In `EMBED_PROCESS` that constructor crosses from
`EmbedLiteAppProcessParent` to `EmbedLiteAppProcessChild`; in `EMBED_THREAD`
the linked actors run in the same process on separate loops.  The child-side
`EmbedLiteViewChild` creates the puppet widget, browsing context, docshell and
`nsWebBrowser`.  The UI side owns only the `EmbedLiteView` facade, its view
parent, and compositor/input routing.

This is an explicit windowless standalone path.  It has no `PBrowser` or
`PWindowGlobal` endpoint, and therefore is not a substitute for a remote
content window.  It must use a new content browsing context and a null
principal created with that context's origin attributes.

### Content-created popup

`nsWindowWatcher` creates `nsOpenWindowInfo`, then calls the EmbedLite
`WindowCreator` in the content-side EmbedLite app. Before this transition it
sent a raw `BrowsingContext*` cast to `uintptr_t` through the synchronous
`PEmbedLiteApp::CreateWindow` request. The UI process cannot own or safely
dereference that pointer, and the other `nsOpenWindowInfo` state was lost.

For the supported same-process path, the originating EmbedLite child creates
the popup `BrowsingContext` and initial `WindowGlobalInit`; it is the only
side with the source `nsOpenWindowInfo` and opener document. The EmbedLite UI
process only chooses a native host/view and relays immutable construction data
back to that child. It never creates, inspects, or retains a DOM pointer. For
remote content, those objects are instead owned by Gecko's `ContentChild` and
`BrowserChild` path described below.

### Chrome-hosted logical tab

`EmbedLiteWindowChild::CreateChromeAppWindow()` creates a normal chrome
`AppWindow`.  `EmbedLiteChromeSessionChild::CreateBrowserForTab()` inserts a
XUL `<browser type="content" remote="true">` and gives its frame loader the
real `nsIOpenWindowInfo`.  `nsFrameLoader` creates the browsing context and,
for a remote browser, Gecko's normal parent/content `BrowserParent`/
`BrowserChild` construction path owns the remote docshell, initial window
global, and web browser.  This path does not use `EmbedLiteViewChild` and must
continue to preserve the supplied open-window info unchanged.

## Actor roles and ESR153 contract

`BrowserChildHelper` remains the legacy view's `nsIBrowserChild` implementation
needed by `WebBrowserChrome` and `nsDocShellTreeOwner`; it supplies EmbedLite
message-manager, APZ, and widget integration only.  Gecko's real
content-process actor is `BrowserChild`, constructed by
`ContentChild::RecvConstructBrowser`.  That path creates a
`WindowGlobalChild` from `WindowGlobalInit`, binds both actor endpoints, and
calls `BrowserChild::Init`.  `BrowserChild::Init` passes the browser's
browsing context, initial window global, and matching `nsIOpenWindowInfo` to
`nsWebBrowser::Create`.

`nsWebBrowser::Create` calls `nsDocShell::InitWindow`, which calls
`nsDocShell::CreateInitialDocumentViewer`.  The viewer obtains the inheriting
principal and partitioned principal, base URI, policy container, and COEP from
`nsIOpenWindowInfo`.  `nsWindowWatcher`, `nsFrameLoader`, and
`CanonicalBrowsingContext` establish the corresponding opener, origin
attributes, remote state, user activation, and
`topLevelCreatedByWebContent` contract before this point.  In particular,
`nsFrameLoader` does not attach an opener when `forceNoOpener` is set.

EmbedLite's custom process does not currently host Gecko's `PContent` actor,
so a legacy `PEmbedLiteView` cannot pretend to be a `BrowserChild`. A remote
EmbedLite view must be constructed through
`ContentChild::RecvConstructBrowser` / `BrowserChild::Init`, with real
`PBrowser` and `PWindowGlobal` endpoints. A future migration therefore needs
to bridge that native actor construction, rather than extending this relay
protocol to emulate it with `BrowserChildHelper`.

## Missing data and proposed IPC change

Before this change `PEmbedLiteApp` carries only a parent view id, a raw parent
pointer, chrome flags, and a hidden/printing boolean.  It is missing:

- browsing-context id and the initial `WindowGlobalInit`;
- principal and partitioned principal;
- origin attributes (private browsing, container, partition key, and the
  other origin attributes); and
- base URI, policy container, COEP, opener/`forceNoOpener`, remote/fission
  state, printing state, user/text-directive activation, and
  `topLevelCreatedByWebContent`.

The retained parent-process, non-Fission EmbedLite path therefore defines an
IPC `EmbedLiteOpenWindowInfoData` record containing those
`nsOpenWindowInfo` fields, including explicit `OriginAttributes` and
serializable `PolicyContainerArgs`, plus `EmbedLiteBrowserInitData` containing
that record, the `WindowGlobalInit`, and the chrome flags. `CreateWindow`
carries the complete init record from the originating child to the UI parent,
and the `PEmbedLiteView` constructor carries it back to the selected child.
The UI parent keeps the record only while invoking the embedding listener; it
never receives a DOM pointer. Direct `CreateView` calls have no init record
and remain standalone-only.

For a supported same-process content-created popup, `WindowCreator` creates
the detached content browsing context with the opener only when
`forceNoOpener` is false, sets its origin attributes before attaching it,
creates a dummy null principal from the new context's attributes, and obtains
`WindowGlobalInit` with `WindowGlobalActor::AboutBlankInitializer`.  On the
receiving child, the window global is reconstructed from that init data.  The
full open-window data is reconstructed, cloned with the dummy principal for
the initial actor/document contract, and later applies the original initial
principal exactly as Gecko's `ContentChild::ProvideWindowCommon` does.  This
keeps policy, base URI, COEP, opener, activation, and the original principals
available through the transition rather than inventing an attribute-less
principal in `EmbedLiteViewChild`.

`nsWebBrowser::Create` must be called with both the initial window global and
matching open-window info for this path.  Its `nsresult` and output pointer are
checked before any use.  On failure the disconnected actor is destroyed if it
never acquired a window global, EmbedLite releases its partial widget/chrome
state, and the view actor is deleted without claiming initialization success.

Remote/Fission creation is deliberately rejected at the legacy
`WindowCreator` boundary until it is routed through the native Gecko actor
path. This avoids creating an apparently remote browsing context without the
`BrowserChild` needed for subsequent window-global creation and navigation.
