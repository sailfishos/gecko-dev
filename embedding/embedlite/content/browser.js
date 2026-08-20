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
    if (listener) {
      browser.messageManager.removeMessageListener(name, listener);
      byName.delete(name);
    }
  }

  function attach(browser) {
    if (!browser.messageManager) {
      return;
    }
    browser.messageManager.loadFrameScript(INTERNAL_SCRIPT, true, true);
    messageListener(browser, INTERNAL_STATE);
  }

  document.addEventListener("XULFrameLoaderCreated", event => {
    if (event.target.localName === "browser") {
      attach(event.target);
    }
  }, true);

  document.addEventListener("DidChangeBrowserRemoteness", event => {
    if (event.target.localName === "browser") {
      listeners.delete(event.target);
      attach(event.target);
    }
  }, true);

  document.addEventListener("EmbedLiteChromeContentCommand", event => {
    const browser = event.target;
    const command = browser.getAttribute("data-embedlite-command");
    const name = browser.getAttribute("data-embedlite-command-name");
    const data = browser.getAttribute("data-embedlite-command-data");
    if (!browser.messageManager) {
      return;
    }
    try {
      switch (command) {
        case "load-script":
          browser.messageManager.loadFrameScript(data, true, true);
          break;
        case "add-listener":
          messageListener(browser, name);
          break;
        case "remove-listener":
          removeMessageListener(browser, name);
          break;
        case "send-message":
          browser.messageManager.sendAsyncMessage(name, JSON.parse(data));
          break;
        default: {
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
  }, true);
})();
