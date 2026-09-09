/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import assert from "node:assert/strict";
import fs from "node:fs/promises";
import { mapGeometry, inverseTransform } from "../content/frame-geometry.sys.mjs";

const identity = [1, 0, 0, 1, 0, 0];
const transform = [0, 2, -2, 0, 80, 30];
const original = { xPos: 3, yPos: 7, id: "doc:1", cursorPosition: 12,
                   start: { xPos: 4, yPos: 8 } };
assert.deepEqual(mapGeometry(mapGeometry(original, identity, transform), transform, identity), original);
assert.deepEqual(original, { xPos: 3, yPos: 7, id: "doc:1", cursorPosition: 12,
                            start: { xPos: 4, yPos: 8 } });
assert.throws(() => inverseTransform([0, 0, 0, 0, 0, 0]));
assert.deepEqual(mapGeometry({ element: { left: 0, top: 0, right: 4, bottom: 6 } }, transform, identity),
                 { element: { left: 68, top: 30, right: 80, bottom: 38 } });

const start = { xPos: 10, yPos: 20 };
const aliased = { start, caret: start, end: { xPos: 40, yPos: 20 } };
const mapped = mapGeometry(aliased, [1, 0, 0, 1, 30, 100], identity);
assert.deepEqual(mapped.start, { xPos: 40, yPos: 120 });
assert.equal(mapped.caret, mapped.start);
assert.deepEqual(mapped.end, { xPos: 70, yPos: 120 });
assert.deepEqual(start, { xPos: 10, yPos: 20 });

globalThis.JSWindowActorParent = class {
  sendAsyncMessage(name, data) { this.sent.push({ name, data }); }
  sendQuery() { return Promise.resolve({ transform: identity, visualViewport: { offsetLeft: 0, offsetTop: 0 } }); }
};
const url = new URL("../content/frame-parent.sys.mjs", import.meta.url);
const source = (await fs.readFile(url, "utf8")).replace(
  '"chrome://embedlitechrome/content/frame-geometry.sys.mjs"',
  JSON.stringify(new URL("../content/frame-geometry.sys.mjs", import.meta.url).href));
const bridge = await import("data:text/javascript;base64," + Buffer.from(source).toString("base64"));
const browser = { browsingContext: {} };
const emitted = [];
bridge.attachFrameBridge(browser, (name, data) => emitted.push({ name, data }));
bridge.listenDocumentMessage(browser, "embed:selectasync", true);
bridge.loadDocumentHelper(browser, "chrome://embedlite/content/embedhelper.js");
function actor(parent) {
  const result = new bridge.EmbedLiteFrameParent();
  result.manager = { isCurrentGlobal: true };
  result.browsingContext = { top: { embedderElement: browser }, parent };
  result.sent = [];
  result.receiveMessage({ name: "Ready" });
  return result;
}
const top = actor(null);
browser.browsingContext.currentWindowGlobal = { getActor: () => top };
const frame = actor({});
frame.receiveMessage({ name: "Message", data: { name: "embed:selectasync", data: { id: "frame:1", options: [] } } });
await frame.pending;
assert.equal(emitted.length, 1);
assert.equal(bridge.sendDocumentMessage(browser, "embedui:selectresponse", { id: "frame:1", result: [] }), true);
assert.equal(frame.sent.at(-1).name, "Command");
const count = frame.sent.length;
bridge.sendDocumentMessage(browser, "embedui:selectresponse", { id: "frame:1", result: [] });
assert.equal(frame.sent.length, count, "Replies are consumed once");
frame.receiveMessage({ name: "Message", data: { name: "embed:selectasync", data: { id: "frame:2" } } });
await frame.pending;
frame.manager.isCurrentGlobal = false;
frame.didDestroy();
const replacement = actor({});
const replacementCount = replacement.sent.length;
bridge.sendDocumentMessage(browser, "embedui:selectresponse", { id: "frame:2", result: [] });
assert.equal(replacement.sent.length, replacementCount, "Old replies never reach replacement documents");
assert.equal(top.sent.filter(m => m.name === "Command").length, 0, "Frame replies never fall back to top");
console.log("Frame geometry and document reply routing tests passed");

// Selection stays with the document that started it even if focus changes.
top.receiveMessage({ name: "Commands", data: ["Browser:SelectionStart", "Browser:SelectionMove"] });
replacement.receiveMessage({ name: "Commands", data: ["Browser:SelectionStart", "Browser:SelectionMove"] });
replacement.receiveMessage({ name: "Focus" });
bridge.sendDocumentMessage(browser, "Browser:SelectionStart", { xPos: 10, yPos: 20 });
await new Promise(resolve => setImmediate(resolve));
assert.equal(replacement.sent.at(-1).data.name, "Browser:SelectionStart");
replacement.receiveMessage({ name: "Hidden" });
const next = actor({});
next.receiveMessage({ name: "Commands", data: ["Browser:SelectionStart", "Browser:SelectionMove"] });
next.receiveMessage({ name: "Focus" });
const nextCount = next.sent.length;
bridge.sendDocumentMessage(browser, "Browser:SelectionMove", { xPos: 12, yPos: 22 });
await new Promise(resolve => setImmediate(resolve));
assert.equal(next.sent.length, nextCount, "Stale selection movement cannot edit the newly focused frame");

// Destroying a document closes its native select request.
bridge.listenDocumentMessage(browser, "embed:selectabort", true);
next.receiveMessage({ name: "Message", data: { name: "embed:selectasync", data: { id: "next:1" } } });
await next.pending;
next.receiveMessage({ name: "Hidden" });
assert.deepEqual(emitted.at(-1), { name: "embed:selectabort", data: { id: "next:1" } });
console.log("Document teardown and selection ownership tests passed");

// BFCache reactivates the same actor with a fresh helper request ID.
for (const [request, reply] of [
  ["embed:selectasync", "embedui:selectresponse"],
  ["embed:clipboardreadpaste", "embedui:clipboardreadpasteresponse"],
]) {
  bridge.listenDocumentMessage(browser, request, true);
  next.receiveMessage({ name: "Ready" });
  next.receiveMessage({ name: "Message", data: { name: request, data: { id: "before" } } });
  await next.pending;
  next.receiveMessage({ name: "Hidden" });
  next.receiveMessage({ name: "Ready" });
  next.receiveMessage({ name: "Message", data: { name: request, data: { id: "restored" } } });
  await next.pending;
  const count = next.sent.length;
  bridge.sendDocumentMessage(browser, reply, { id: "before" });
  assert.equal(next.sent.length, count, "Pre-cache replies cannot answer restored helpers");
  bridge.sendDocumentMessage(browser, reply, { id: "restored" });
  assert.equal(next.sent.length, count + 1);
  assert.equal(next.sent.at(-1).data.name, reply);
  next.receiveMessage({ name: "Hidden" });
}
console.log("BFCache actor reactivation tests passed");
