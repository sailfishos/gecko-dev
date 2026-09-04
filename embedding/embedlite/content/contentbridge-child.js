/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

(() => {
  let firstPaint = false;
  let observedViewport = null;
  let endpointId = 0;
  let dynamicToolbarHeight = null;
  let timeoutsSuspended = false;
  let nextPopupId = 0;
  let lastOrientation = "";
  let orientationPaintBaseline = 0;
  let orientationPaintPending = false;
  const blockedPopups = new Map();
  const viewportChanged = () => state();

  function contentGeometry() {
    return {
      width: Math.max(0, Math.trunc(content.innerWidth)),
      height: Math.max(0, Math.trunc(content.innerHeight)),
    };
  }

  function contentOrientation() {
    const { width, height } = contentGeometry();
    const landscape = width >= height;
    const orientation = content.screen?.mozOrientation ||
      content.screen?.orientation?.type;
    if (["portrait-primary", "portrait-secondary", "landscape-primary",
         "landscape-secondary"].includes(orientation) &&
        orientation.startsWith(landscape ? "landscape" : "portrait")) {
      return orientation;
    }
    return landscape ? "landscape-primary" : "portrait-primary";
  }

  function reportOrientation(force = false, painted = false) {
    const orientation = contentOrientation();
    if (force || orientation !== lastOrientation) {
      lastOrientation = orientation;
      const geometry = contentGeometry();
      sendAsyncMessage("embed:contentOrientationChanged", {
        orientation,
        width: geometry.width,
        height: geometry.height,
        painted,
      });
    }
  }

  function waitForOrientationPaint() {
    try {
      orientationPaintBaseline = content.windowUtils.lastTransactionId;
    } catch (error) {
      orientationPaintBaseline = 0;
    }
    orientationPaintPending = true;
  }

  function applyDynamicToolbarHeight() {
    if (dynamicToolbarHeight === null) {
      return;
    }
    try {
      content.windowUtils.setDynamicToolbarMaxHeight(dynamicToolbarHeight);
    } catch (error) {
      // A new document may not have constructed its PresShell yet.
    }
  }

  function allocatePopupId() {
    return nextPopupId < Number.MAX_SAFE_INTEGER ? ++nextPopupId : 0;
  }

  function popupDenied(principal) {
    if (!principal) {
      return false;
    }
    for (const permission of Services.perms.getAllForPrincipal(principal)) {
      if (permission.type === "popup" &&
          permission.capability ===
            Components.interfaces.nsIPermissionManager.DENY_ACTION) {
        return true;
      }
    }
    return false;
  }

  addEventListener("DOMPopupBlocked", event => {
    const requestingWindow = event.requestingWindow || content;
    const requestingDocument = requestingWindow?.document;
    if (blockedPopups.size >= 64 ||
        popupDenied(requestingDocument?.nodePrincipal)) {
      return;
    }

    const popupUri = event.popupWindowURI?.spec || "about:blank";
    let host = popupUri;
    try {
      host = requestingDocument?.documentURIObject?.displaySpec || popupUri;
    } catch (error) {
      // Hostless requesting documents (for example about:blank) are valid.
    }
    const popupId = allocatePopupId();
    if (!popupId) {
      return;
    }
    blockedPopups.set(popupId, {
      requestingWindow,
      requestingDocument,
      popupUri,
      name: event.popupWindowName || "",
      features: event.popupWindowFeatures || "",
    });
    sendAsyncMessage("embed:popupblocked", {
      host,
      popupUri,
      popupId,
      winId: endpointId,
    });
  }, true);

  addMessageListener("embedui:popupblocked", message => {
    const popupId = Number(message.data?.popupId);
    if (!Number.isSafeInteger(popupId) || popupId <= 0 ||
        (message.data?.winId !== undefined &&
         Number(message.data.winId) !== endpointId)) {
      return;
    }
    const popup = blockedPopups.get(popupId);
    blockedPopups.delete(popupId);
    if (!message.data?.allow || !popup?.requestingWindow ||
        popup.requestingWindow.document !== popup.requestingDocument) {
      return;
    }
    try {
      popup.requestingWindow.open(
        popup.popupUri, popup.name, popup.features);
    } catch (error) {
      console.error("Opening hosted blocked popup failed", error);
    }
  });

  function state(paintX = 0, paintY = 0) {
    const root = content.document?.scrollingElement ||
      content.document?.documentElement;
    let presShellId = 0;
    let viewId = "0";
    try {
      presShellId = content.windowUtils.getPresShellId();
      if (root) {
        viewId = content.windowUtils.getViewId(root).toString();
      }
    } catch (error) {
      // The initial about:blank may not have a root scroll frame yet.
    }
    const viewport = content.visualViewport;
    let innerWindowId = "0";
    try {
      innerWindowId = String(content.windowGlobalChild?.innerWindowId || 0);
    } catch (error) {
      // The initial global may not be associated with a WindowGlobalChild.
    }
    sendAsyncMessage("EmbedLiteChrome:State", {
      innerWindowId,
      fullscreen: !!content.document?.fullscreenElement,
      firstPaint,
      firstPaintX: paintX,
      firstPaintY: paintY,
      scrollWidth: Math.max(0, root?.scrollWidth || 0),
      scrollHeight: Math.max(0, root?.scrollHeight || 0),
      scrollX: Math.trunc(content.scrollX),
      scrollY: Math.trunc(content.scrollY),
      viewportX: Number.isFinite(viewport?.pageLeft)
        ? viewport.pageLeft : content.scrollX,
      viewportY: Number.isFinite(viewport?.pageTop)
        ? viewport.pageTop : content.scrollY,
      viewportWidth: Math.max(0, Number.isFinite(viewport?.width)
        ? viewport.width : content.innerWidth),
      viewportHeight: Math.max(0, Number.isFinite(viewport?.height)
        ? viewport.height : content.innerHeight),
      presShellId,
      viewId,
    });
  }

  addEventListener("DOMWindowCreated", event => {
    if (event.target !== content.document) {
      return;
    }
    firstPaint = false;
    orientationPaintPending = false;
    if (observedViewport) {
      observedViewport.removeEventListener("scroll", viewportChanged);
      observedViewport.removeEventListener("resize", viewportChanged);
    }
    observedViewport = content.visualViewport;
    observedViewport?.addEventListener("scroll", viewportChanged);
    observedViewport?.addEventListener("resize", viewportChanged);
    applyDynamicToolbarHeight();
    state();
    reportOrientation(true);
  }, true);
  addEventListener("DOMContentLoaded", event => {
    if (event.target !== content.document) {
      return;
    }
    sendAsyncMessage("chrome:contentloaded", {
      docuri: content.document.documentURI || "",
    });
    reportOrientation();
  }, true);
  addEventListener("pagehide", event => {
    for (const [popupId, popup] of blockedPopups) {
      if (popup.requestingDocument === event.target) {
        blockedPopups.delete(popupId);
      }
    }
    if (event.target !== content.document) {
      return;
    }
    firstPaint = false;
    orientationPaintPending = false;
  }, true);

  addEventListener("MozAfterPaint", event => {
    if (!firstPaint) {
      firstPaint = true;
      applyDynamicToolbarHeight();
      state(Math.trunc(event.boundingClientRect?.x || 0),
            Math.trunc(event.boundingClientRect?.y || 0));
    }
    if (orientationPaintPending &&
        event.transactionId > orientationPaintBaseline) {
      orientationPaintPending = false;
      reportOrientation(true, true);
    }
  }, true);
  addEventListener("fullscreenchange", () => state(), true);
  addEventListener("scroll", event => {
    if (event.target === content || event.target === content.document) {
      state();
    }
  }, true);
  addEventListener("resize", () => {
    state();
    waitForOrientationPaint();
  }, true);
  addEventListener(
    "mozorientationchange", () => waitForOrientationPaint(), true);
  content.screen?.orientation?.addEventListener(
    "change", () => waitForOrientationPaint());

  observedViewport = content.visualViewport;
  observedViewport?.addEventListener("scroll", viewportChanged);
  observedViewport?.addEventListener("resize", viewportChanged);

  addMessageListener("EmbedLiteChrome:Command", message => {
    const { command, data } = message.data;
    switch (command) {
      case "set-endpoint":
        endpointId = Number.isSafeInteger(data.endpointId) &&
          data.endpointId > 0 ? data.endpointId : 0;
        reportOrientation(true);
        break;
      case "scroll-to":
        content.scrollTo(data.x, data.y);
        break;
      case "scroll-by":
        content.scrollBy(data.x, data.y);
        break;
      case "set-user-agent":
        docShell.customUserAgent = data.userAgent;
        break;
      case "suspend-timeouts":
        if (!timeoutsSuspended) {
          content.windowUtils.suspendTimeouts();
          timeoutsSuspended = true;
        }
        break;
      case "resume-timeouts":
        if (timeoutsSuspended) {
          content.windowUtils.resumeTimeouts();
          timeoutsSuspended = false;
        }
        break;
      case "set-toolbar-height":
        if (Number.isSafeInteger(data.height) && data.height >= 0) {
          dynamicToolbarHeight = data.height;
          applyDynamicToolbarHeight();
        }
        break;
      case "request-state":
        state();
        break;
    }
  });

  state();
})();
