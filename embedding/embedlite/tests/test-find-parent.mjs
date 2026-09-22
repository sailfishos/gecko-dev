/* This Source Code Form is subject to the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import assert from "node:assert/strict";
import fs from "node:fs/promises";

const moduleURL = source => "data:text/javascript;base64," + Buffer.from(source).toString("base64");
globalThis.Services = { prefs: { getBoolPref: () => false } };
globalThis.ChromeUtils = { defineESModuleGetters() {} };
globalThis.Ci = { nsITypeAheadFind: {
  FIND_FOUND: 0, FIND_NOTFOUND: 1, FIND_WRAPPED: 2,
  FIND_INITIAL: 0, FIND_NEXT: 1, FIND_PREVIOUS: 2, FIND_FIRST: 3, FIND_LAST: 4,
} };
// Exercise the actual upstream coordinator; only the IPC endpoints are fake.
const upstream = (await fs.readFile(new URL(
  "../../../gecko-dev/toolkit/modules/FinderParent.sys.mjs", import.meta.url), "utf8"))
  .replace(/import \{[\s\S]*?\} from "resource:\/\/gre\/modules\/FinderSound.sys.mjs";/,
           "const initSound = () => {}, playSound = () => {};");
const source = (await fs.readFile(new URL("../content/find-parent.sys.mjs", import.meta.url), "utf8"))
  .replace('"resource://gre/modules/FinderParent.sys.mjs"', JSON.stringify(moduleURL(upstream)));
const { findInPage } = await import(moduleURL(source));
const queries = [], commands = [];
let release;
function context(text) {
  return { children: [], currentWindowGlobal: { getActor(name) {
    assert.equal(name, "Finder");
    return {
      sendAsyncMessage(name) { commands.push({ text, name }); },
      async sendQuery(name, data) {
        if (name !== "Finder:Find") return {};
        queries.push({ text, ...data });
        if (release) await new Promise(resolve => { release = resolve; });
        const found = text.includes(data.searchString) &&
          ![1, 2].includes(data.mode);
        return { ...data, result: found ? 0 : 1 };
      },
    };
  } } };
}
const top = context("top needle"), child = context("remote needle");
top.children.push(child);
const browser = { browsingContext: top };
const results = [];
const find = (text, again = false, backwards = false) =>
  findInPage(browser, { text, again, backwards }, result => results.push(result.r));
await find("");
assert.equal(commands.length, 0, "Closing an unused finder must not initialize document finders");
await find("remote");
assert.equal(results.at(-1), 0);
assert.deepEqual(queries.slice(0, 2).map(q => q.text), ["top needle", "remote needle"]);
await find("remote", true);
assert.equal(results.at(-1), 2);
await find("remote", true, true);
assert.equal(results.at(-1), 2);
await find("absent");
assert.equal(results.at(-1), 1);
await find("");
assert.equal(commands.filter(c => c.name === "Finder:RemoveSelection").length >= 2, true);

// A reply already in flight must not publish a result after cancellation.
release = true;
const pending = find("top");
await new Promise(resolve => setImmediate(resolve));
const cancelled = find("");
const count = results.length;
const resume = release;
release = null;
resume();
await Promise.all([pending, cancelled]);
assert.equal(results.length, count);

// Nor may an old page publish a result into its replacement.
release = true;
const navigating = find("top");
await new Promise(resolve => setImmediate(resolve));
browser.browsingContext = context("replacement");
const resumeNavigation = release;
release = null;
resumeNavigation();
await navigating;
assert.equal(results.length, count);
await find("replacement");
assert.equal(results.at(-1), 0);
console.log("Cross-frame find, wrapping, cancellation and navigation tests passed");
