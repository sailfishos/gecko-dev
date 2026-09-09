/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { frameTransform, mapGeometry } from "chrome://embedlitechrome/content/frame-geometry.sys.mjs";

export class EmbedLiteFrameChild extends JSWindowActorChild {
  handleEvent(event) {
    if (!event.isTrusted) return;
    if (event.type === "DOMWindowCreated" || event.type === "pageshow") {
      this.sendAsyncMessage("Ready", {});
    } else if (event.type === "pagehide") {
      this.destroyScope();
      this.sendAsyncMessage("Hidden", {});
    } else {
      this.sendAsyncMessage("Focus", {});
    }
  }

  geometry() {
    return {
      transform: frameTransform(this.contentWindow.windowUtils),
      visualViewport: {
        offsetLeft: this.contentWindow.visualViewport.offsetLeft,
        offsetTop: this.contentWindow.visualViewport.offsetTop,
      },
    };
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Geometry":
        return this.geometry();
      case "Initialize":
        this.initialize();
        break;
      case "Command": {
        if (!this.scope) return;
        let { name, data, geometry } = message.data;
        if (geometry && this.browsingContext.parent) {
          data = mapGeometry(data, geometry.transform, this.geometry().transform);
        }
        for (const listener of [...(this.listeners.get(name) || [])]) {
          const event = { name, data, json: data };
          if (typeof listener === "function") listener(event);
          else listener.receiveMessage(event);
        }
        break;
      }
    }
    return undefined;
  }

  initialize() {
    if (this.scope) return;
    const win = this.contentWindow;
    if (!win || !this.docShell) return;
    this.helperWindow = win;
    const scope = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal(), {
      sandboxName: "EmbedLite document helpers",
      sandboxPrototype: win,
      wantXrays: true,
      freshCompartment: true,
    });
    this.scope = scope;
    this.listeners = new Map();
    this.events = [];
    const globals = {
      content: win, window: win, docShell: this.docShell,
      Components, ChromeUtils, Services, console,
      setTimeout: win.setTimeout.bind(win), clearTimeout: win.clearTimeout.bind(win),
      setInterval: win.setInterval.bind(win), clearInterval: win.clearInterval.bind(win),
      __embedLiteDocumentActor: true,
      addEventListener: (name, listener, capture = false) => {
        const wrapper = event => {
          const document = event.target.ownerDocument || event.target.document || event.target;
          if (document !== win.document) return;
          if (typeof listener === "function") listener(event);
          else listener.handleEvent(event);
        };
        this.events.push({ name, listener, wrapper, capture });
        win.addEventListener(name, wrapper, capture);
      },
      removeEventListener: (name, listener, capture = false) => {
        this.events = this.events.filter(entry => {
          if (entry.name !== name || entry.listener !== listener || entry.capture !== capture) return true;
          win.removeEventListener(name, entry.wrapper, capture);
          return false;
        });
      },
      addMessageListener: (name, listener) => {
        if (!this.listeners.has(name)) this.listeners.set(name, new Set());
        this.listeners.get(name).add(listener);
      },
      removeMessageListener: (name, listener) => this.listeners.get(name)?.delete(listener),
      sendAsyncMessage: (name, data = {}) => {
        let transform;
        if (data && (data.xPos !== undefined || data.selection || data.start)) {
          transform = this.geometry().transform;
        }
        this.sendAsyncMessage("Message", { name, data, transform });
      },
    };
    // Window exposes getter-only globals (including window itself). Define
    // own sandbox properties instead of assigning through that prototype.
    Object.defineProperties(scope, Object.fromEntries(
      Object.entries(globals)
        .filter(([name]) => Object.getOwnPropertyDescriptor(scope, name)?.configurable !== false)
        .map(([name, value]) =>
        [name, { value, writable: true, configurable: true, enumerable: true }])
    ));
    // The old synchronous messages only enforce ordering; the parent bridge
    // already discards their return values. Actor messages preserve that order.
    scope.sendSyncMessage = (name, data) => {
      scope.sendAsyncMessage(name, data);
      return [];
    };
    try {
      Services.scriptloader.loadSubScript("chrome://embedlite/content/embedhelper.js", scope);
      this.sendAsyncMessage("Commands", [...this.listeners.keys()]);
    } catch (error) {
      this.destroyScope();
      throw error;
    }
  }

  destroyScope() {
    if (!this.scope) return;
    const scope = this.scope;
    this.scope = null;
    const clipboard = scope.ClipboardReadPasteHelper;
    clipboard?._respond(false);
    try {
      clipboard?._eventTarget?.removeEventListener("MozClipboardReadPaste", clipboard, false);
    } catch (_) {
      // A navigated WindowProxy may no longer expose the old event target.
    }
    scope.SelectionHandler?._clearTimers();
    if (scope.globalObject) {
      try {
        Services.obs.removeObserver(scope.globalObject, "embedlite-before-first-paint");
      } catch (_) {
        // Initialization can fail before the observer is registered.
      }
    }
    for (const { name, wrapper, capture } of this.events) {
      try {
        this.helperWindow.removeEventListener(name, wrapper, capture);
      } catch (_) {
        // The old document can already be inaccessible during destruction.
      }
    }
    this.events = [];
    this.listeners.clear();
    Cu.nukeSandbox(scope);
    this.helperWindow = null;
  }

  didDestroy() {
    this.destroyScope();
  }
}
