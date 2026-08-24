/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

(() => {
  const INTERNAL_STATE = "EmbedLiteChrome:State";
  const INTERNAL_SCRIPT =
    "chrome://embedlitechrome/content/contentbridge-child.js";
  const MAX_CONTENT_NAME_LENGTH = 1024;
  const MAX_CONTENT_DATA_LENGTH = 1024 * 1024;
  const listeners = new WeakMap();
  const loadedFrameScripts = new WeakMap();
  const frameScripts = new Set();
  const messageNames = new Set();

  try {
    ChromeUtils.registerWindowActor("EmbedLiteDOMFullscreen", {
      parent: {
        esModuleURI:
          "chrome://embedlitechrome/content/domfullscreen-parent.sys.mjs",
      },
      child: {
        esModuleURI:
          "chrome://embedlitechrome/content/domfullscreen-child.sys.mjs",
        events: {
          "MozDOMFullscreen:Request": {},
          "MozDOMFullscreen:Entered": {},
          "MozDOMFullscreen:Exit": {},
          "MozDOMFullscreen:Exited": {},
        },
      },
      allFrames: true,
    });
  } catch (error) {
    if (error.name !== "NotSupportedError") {
      throw error;
    }
  }

  function emit(browser, type, name, data) {
    const normalized = data === undefined ? {} : data;
    let json;
    try {
      json = JSON.stringify(normalized);
    } catch (error) {
      console.error("Serializing EmbedLite content message failed", error);
      return;
    }
    if (json === undefined) {
      json = "{}";
    }
    if (!name || name.length > MAX_CONTENT_NAME_LENGTH ||
        json.length > MAX_CONTENT_DATA_LENGTH) {
      return;
    }

    const internal = type === "EmbedLiteChromeContentState";
    const fields = internal && normalized &&
      typeof normalized === "object" ? Object.entries(normalized) : [];
    try {
      browser.setAttribute("data-embedlite-event-name", name || "");
      browser.setAttribute("data-embedlite-event-data", json);
      for (const [key, value] of fields) {
        if (/^[A-Za-z][A-Za-z0-9]*$/.test(key)) {
          browser.setAttribute(`data-embedlite-event-${key}`, String(value));
        }
      }
      browser.dispatchEvent(new Event(type));
    } finally {
      browser.removeAttribute("data-embedlite-event-name");
      browser.removeAttribute("data-embedlite-event-data");
      for (const [key] of fields) {
        if (/^[A-Za-z][A-Za-z0-9]*$/.test(key)) {
          browser.removeAttribute(`data-embedlite-event-${key}`);
        }
      }
    }
  }

  function messageListener(browser, name) {
    if (!browser.messageManager) {
      return;
    }
    let byName = listeners.get(browser);
    if (!byName) {
      byName = new Map();
      listeners.set(browser, byName);
    }
    if (byName.has(name)) {
      return;
    }
    const listener = message => {
      if (name === INTERNAL_STATE) {
        emit(browser, "EmbedLiteChromeContentState", name, message.data);
      } else {
        // SelectionHandler uses sync delivery as ordering, not for its
        // return value. Never nest sync IPC into the external embedder.
        emit(browser, "EmbedLiteChromeContentMessage", name, message.data);
      }
      return {};
    };
    browser.messageManager.addMessageListener(name, listener, true);
    byName.set(name, listener);
  }

  function removeMessageListener(browser, name) {
    const byName = listeners.get(browser);
    const listener = byName?.get(name);
    if (!listener) {
      return;
    }
    if (browser.messageManager) {
      browser.messageManager.removeMessageListener(name, listener);
    }
    byName.delete(name);
  }

  function loadFrameScript(browser, uri) {
    if (!browser.messageManager) {
      return;
    }
    let loaded = loadedFrameScripts.get(browser);
    if (!loaded) {
      loaded = new Set();
      loadedFrameScripts.set(browser, loaded);
    }
    if (loaded.has(uri)) {
      return;
    }
    browser.messageManager.loadFrameScript(uri, true, true);
    loaded.add(uri);
  }

  function attach(browser) {
    if (!browser.messageManager) {
      return;
    }
    messageListener(browser, INTERNAL_STATE);
    loadFrameScript(browser, INTERNAL_SCRIPT);
    for (const script of frameScripts) {
      loadFrameScript(browser, script);
    }
    for (const name of messageNames) {
      messageListener(browser, name);
    }
  }

  document.addEventListener("EmbedLiteChromeContentCommand",
                            handleContentCommand, true);

  document.addEventListener("XULFrameLoaderCreated", event => {
    if (event.target.localName === "browser") {
      listeners.delete(event.target);
      loadedFrameScripts.delete(event.target);
      attach(event.target);
    }
  }, true);

  document.addEventListener("DidChangeBrowserRemoteness", event => {
    if (event.target.localName === "browser") {
      listeners.delete(event.target);
      loadedFrameScripts.delete(event.target);
      attach(event.target);
    }
  }, true);

  function attachExistingBrowsers() {
    for (const browser of document.querySelectorAll("browser")) {
      attach(browser);
    }
  }

  // A frame loader can be created while its browser is still detached, so a
  // document-level XULFrameLoaderCreated listener cannot cover every tab.
  const browserObserver = new MutationObserver(mutations => {
    for (const mutation of mutations) {
      for (const node of mutation.addedNodes) {
        if (node.nodeType !== Node.ELEMENT_NODE) {
          continue;
        }
        if (node.localName === "browser") {
          attach(node);
        }
        for (const browser of node.querySelectorAll("browser")) {
          attach(browser);
        }
      }
    }
  });
  browserObserver.observe(document.documentElement,
                          { childList: true, subtree: true });

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", attachExistingBrowsers,
                              { once: true });
  } else {
    attachExistingBrowsers();
  }

  function handleContentCommand(event) {
    const browser = event.target;
    const command = browser.getAttribute("data-embedlite-command");
    const name = browser.getAttribute("data-embedlite-command-name");
    const data = browser.getAttribute("data-embedlite-command-data");
    try {
      switch (command) {
        case "load-script":
          frameScripts.add(data);
          loadFrameScript(browser, data);
          break;
        case "add-listener":
          messageNames.add(name);
          messageListener(browser, name);
          break;
        case "remove-listener":
          messageNames.delete(name);
          removeMessageListener(browser, name);
          break;
        case "send-message":
          browser.messageManager?.sendAsyncMessage(name, JSON.parse(data));
          break;
        default: {
          if (!browser.messageManager) {
            return;
          }
          const parsed = JSON.parse(data);
          if (name) {
            parsed.text = name;
            parsed.userAgent = name;
          }
          browser.messageManager.sendAsyncMessage(
            "EmbedLiteChrome:Command", { command, data: parsed });
          break;
        }
      }
    } catch (error) {
      console.error("EmbedLite content bridge command failed", command, error);
    }
  }
})();
