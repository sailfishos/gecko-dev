/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { mapGeometry } from "chrome://embedlitechrome/content/frame-geometry.sys.mjs";

const states = new WeakMap();
const HELPER = "chrome://embedlite/content/embedhelper.js";
const responses = new Map([
  ["embedui:selectresponse", "embed:selectasync"],
  ["embedui:clipboardreadpasteresponse", "embed:clipboardreadpaste"],
]);

function stateFor(browser) {
  let state = states.get(browser);
  if (!state) {
    state = { enabled: false, actors: new Set(), requests: new Map(),
              focused: null, selectionOwner: null, interacted: false, emit: null, names: new Set() };
    states.set(browser, state);
  }
  return state;
}

function current(actor) {
  try {
    return actor && actor.active !== false && actor.manager?.isCurrentGlobal;
  } catch (_) {
    return false;
  }
}

export function attachFrameBridge(browser, emit) {
  stateFor(browser).emit = emit;
}

export function loadDocumentHelper(browser, uri) {
  if (uri !== HELPER) return false;
  const state = stateFor(browser);
  state.enabled = true;
  for (const actor of state.actors) {
    if (current(actor)) actor.sendAsyncMessage("Initialize", {});
  }
  return true;
}

export function listenDocumentMessage(browser, name, enabled) {
  const names = stateFor(browser).names;
  if (enabled) names.add(name);
  else names.delete(name);
}

async function rootGeometry(browser) {
  const global = browser.browsingContext?.currentWindowGlobal;
  const actor = global?.getActor("EmbedLiteFrame");
  if (!actor) throw new Error("No current top-level document");
  const result = await actor.sendQuery("Geometry", {});
  if (browser.browsingContext?.currentWindowGlobal !== global) {
    throw new Error("Top-level document changed");
  }
  return result;
}

export function sendDocumentMessage(browser, name, data) {
  const state = stateFor(browser);
  if (!state.enabled) return false;
  const response = responses.get(name);
  if (response) {
    const key = response + ":" + String(data.id);
    const actor = state.requests.get(key);
    state.requests.delete(key);
    if (current(actor)) actor.sendAsyncMessage("Command", { name, data });
    // A late reply must never fall back to the new document's frame script.
    return true;
  }
  const actors = [...state.actors].filter(current);
  const top = actors.find(actor => !actor.browsingContext.parent);
  const focused = current(state.focused) ? state.focused : (state.interacted ? null : top);
  const documentCommand = /^(Browser:|Gesture:|embed:ContextMenu)/.test(name);
  const startsSelection = /^(Browser:Selection(Start|Attach)|Browser:CaretAttach)$/.test(name);
  if (startsSelection) state.selectionOwner = focused;
  const selectionCommand = /^Browser:(Selection|Caret)/.test(name);
  const owner = selectionCommand && !startsSelection && state.selectionOwner
    ? state.selectionOwner : focused;
  const broadcast = name === "Viewport:Change" || name === "Browser:SelectionColorUpdate";
  const targets = broadcast ? actors : [documentCommand ? owner : top];
  if (!actors.some(actor => actor.commands?.has(name))) return false;
  void rootGeometry(browser).then(geometry => {
    for (const actor of targets) {
      if (current(actor) && actor.commands?.has(name)) {
        actor.sendAsyncMessage("Command", { name, data, geometry });
      }
    }
  }).catch(console.error);
  return true;
}

export class EmbedLiteFrameParent extends JSWindowActorParent {
  receiveMessage(message) {
    if (message.name === "Ready") this.active = true;
    const browser = this.browsingContext.top.embedderElement;
    if (!browser || !current(this)) return;
    const state = stateFor(browser);
    this.state = state;
    state.actors.add(this);
    switch (message.name) {
      case "Hidden":
        this.didDestroy();
        break;
      case "Ready":
        if (state.enabled) this.sendAsyncMessage("Initialize", {});
        break;
      case "Commands":
        this.commands = new Set(message.data);
        break;
      case "Focus":
        state.focused = this;
        state.interacted = true;
        break;
      case "Message":
        // Serialize asynchronous coordinate queries to preserve helper ordering.
        this.pending = (this.pending || Promise.resolve()).then(async () => {
          const { name, data, transform } = message.data;
          if (!current(this) || !state.names.has(name)) return;
          let output = data;
          if (transform && this.browsingContext.parent) {
            const geometry = await rootGeometry(browser);
            output = mapGeometry(data, transform, geometry.transform);
            if (output.visualViewport) output.visualViewport = geometry.visualViewport;
          }
          if (!current(this)) return;
          if ([...responses.values()].includes(name)) {
            state.requests.set(name + ":" + String(output.id), this);
          }
          state.emit?.(name, output);
        }).catch(console.error);
        break;
    }
  }

  didDestroy() {
    this.active = false;
    const state = this.state;
    if (!state) return;
    state.actors.delete(this);
    for (const [key, actor] of state.requests) {
      if (actor === this) {
        state.requests.delete(key);
        if (key.startsWith("embed:selectasync:") && state.names.has("embed:selectabort")) {
          state.emit?.("embed:selectabort", { id: key.slice("embed:selectasync:".length) });
        }
      }
    }
    if (state.focused === this) {
      state.focused = null;
      for (const [name, data] of [
        ["Content:HandlerShutdown", {}], ["InputMethodHandler:ResetInputContext", []],
        ["InputMethodHandler:ResetInputAttributes", []], ["FormAssist:Hide", []],
      ]) {
        if (state.names.has(name)) state.emit?.(name, data);
      }
    }
  }
}
