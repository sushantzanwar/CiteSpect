"use strict";

/*
 * Targeted presentation tests for the redesigned frontend (frontend/script.js).
 * Dependency-free: runs the real script inside a Node VM against a minimal fake DOM.
 * Does not touch the network or any data files.
 */

var assert = require("assert");
var fs = require("fs");
var path = require("path");
var vm = require("vm");

function createFakeElement(tag, id) {
  var el = {
    tagName: tag.toUpperCase(),
    id: id || "",
    className: "",
    hidden: false,
    disabled: false,
    checked: false,
    value: "",
    href: "",
    type: "",
    attributes: {},
    children: [],
    listeners: {},
    style: {},
    focusCount: 0,
    animateCalls: 0,
    clientWidth: 560,
    _text: "",
    _classes: {},
    classList: {
      add: function (name) { el._classes[name] = true; },
      remove: function (name) { delete el._classes[name]; },
      contains: function (name) { return !!el._classes[name]; }
    },
    setAttribute: function (k, v) { el.attributes[k] = String(v); },
    getAttribute: function (k) { return k in el.attributes ? el.attributes[k] : null; },
    removeAttribute: function (k) { delete el.attributes[k]; },
    appendChild: function (child) { el.children.push(child); return child; },
    addEventListener: function (type, handler) {
      (el.listeners[type] = el.listeners[type] || []).push(handler);
    },
    focus: function () { el.focusCount++; },
    querySelector: function () { return createFakeElement("rect", ""); }
  };
  Object.defineProperty(el, "textContent", {
    get: function () {
      if (el.children.length === 0) { return el._text; }
      return el.children.map(function (c) {
        return c.nodeType === 3 ? c.nodeValue : c.textContent;
      }).join("");
    },
    set: function (v) { el._text = String(v); el.children = []; }
  });
  Object.defineProperty(el, "innerHTML", {
    get: function () { return ""; },
    set: function () { el.children = []; el._text = ""; }
  });
  return el;
}

function fire(el, type, event) {
  (el.listeners[type] || []).forEach(function (h) {
    h(event || { preventDefault: function () {}, key: "" });
  });
}

function findAll(node, predicate, out) {
  out = out || [];
  if (!node || !Array.isArray(node.children)) { return out; }
  if (predicate(node)) { out.push(node); }
  node.children.forEach(function (c) { findAll(c, predicate, out); });
  return out;
}

function load(options) {
  options = options || {};
  var byId = {};
  [
    "search-form", "doi-input", "analyze-button", "status", "paper-section", "paper-heading",
    "paper-details", "authors-heading", "author-list", "analysis-section", "analysis-message",
    "summary-grid", "table-section", "table-context", "table-empty", "citation-rows",
    "graph-section", "graph-container", "graph-legend", "graph-node-panel", "node-panel-title",
    "node-panel-details", "node-panel-close", "coupling-form", "coupling-paper-a",
    "coupling-paper-b", "coupling-button", "coupling-status", "coupling-results",
    "coupling-summary-grid", "coupling-details", "coupling-shared-wrap",
    "coupling-shared-heading", "coupling-shared-list", "comparison-method-hint",
    "mode-coupling", "mode-cocitation"
  ].forEach(function (id) { byId[id] = createFakeElement("div", id); });

  ["paper-section", "analysis-section", "table-section", "graph-section",
    "graph-node-panel", "coupling-results", "coupling-shared-wrap"].forEach(function (id) {
    byId[id].hidden = true;
  });
  byId["mode-coupling"].checked = true;

  var tableWrap = createFakeElement("div", "");
  var context = {
    document: {
      getElementById: function (id) { return byId[id]; },
      createElement: function (tag) { return createFakeElement(tag, ""); },
      createElementNS: function (ns, tag) { return createFakeElement(tag, ""); },
      createTextNode: function (t) { return { nodeType: 3, nodeValue: String(t) }; },
      querySelector: function (sel) { return sel === ".table-wrap" ? tableWrap : createFakeElement("div", ""); }
    },
    window: options.window,
    fetch: function () { throw new Error("network must not be used"); },
    Number: Number, Array: Array, Object: Object, Math: Math, String: String
  };
  vm.runInNewContext(
    fs.readFileSync(path.join(__dirname, "script.js"), "utf8"),
    context,
    { filename: "script.js" }
  );
  return { ctx: context, byId: byId, tableWrap: tableWrap };
}

var results = [];
function test(name, fn) {
  try {
    fn();
    results.push("[PASS] " + name);
  } catch (error) {
    results.push("[FAIL] " + name + "\n       " + error.message);
    process.exitCode = 1;
  }
}

test("R1: paper title becomes the heading and metadata rows render", function () {
  var app = load();
  app.ctx.showPaper({
    paper_id: "W2919115771", title: "Deep learning", year: 2015, doi: "10.1038/nature14539",
    reference_count: 3, authors: ["Yann LeCun", "Yoshua Bengio"]
  });
  assert.strictEqual(app.byId["paper-heading"].textContent, "Deep learning");
  var text = app.byId["paper-details"].textContent;
  ["Year", "2015", "DOI", "10.1038/nature14539", "OpenAlex Work ID", "W2919115771", "References"]
    .forEach(function (s) { assert(text.indexOf(s) !== -1, "metadata missing " + s); });
  var links = findAll(app.byId["paper-details"], function (n) { return n.tagName === "A"; });
  assert.strictEqual(links[0].href, "https://openalex.org/W2919115771");
  assert.strictEqual(app.byId["paper-section"].hidden, false);
  assert.strictEqual(app.byId["author-list"].children.length, 2);
});

test("R2: long author lists collapse and the toggle expands and collapses them", function () {
  var app = load();
  var authors = [];
  for (var i = 1; i <= 12; i++) { authors.push("Author " + i); }
  app.ctx.showPaper({ paper_id: "W1", title: "T", year: 2020, doi: "", reference_count: 0, authors: authors });
  var items = app.byId["author-list"].children;
  var visible = items.filter(function (li) { return !li.hidden && li.className !== "author-more"; });
  assert.strictEqual(visible.length, 8, "8 authors visible while collapsed");
  var toggle = findAll(app.byId["author-list"], function (n) { return n.tagName === "BUTTON"; })[0];
  assert.strictEqual(toggle.textContent, "and 4 more authors");
  assert.strictEqual(toggle.getAttribute("aria-expanded"), "false");
  fire(toggle, "click");
  assert.strictEqual(items.filter(function (li) { return li.hidden; }).length, 0);
  assert.strictEqual(toggle.getAttribute("aria-expanded"), "true");
  fire(toggle, "click");
  assert.strictEqual(toggle.getAttribute("aria-expanded"), "false");

  var small = load();
  small.ctx.showPaper({ paper_id: "W2", title: "T", year: 2020, doi: "", reference_count: 0, authors: authors.slice(0, 9) });
  assert.strictEqual(findAll(small.byId["author-list"], function (n) { return n.tagName === "BUTTON"; }).length, 0,
    "no toggle for 9 authors (hiding one would be pointless)");
});

test("R3: summary keeps the three values in order", function () {
  var app = load();
  app.ctx.showSummary({ analyzed: 7, self: 2, reciprocal_confirmed: 3 });
  var items = app.byId["summary-grid"].children;
  assert.strictEqual(items.length, 3);
  assert.deepStrictEqual(items.map(function (i) { return i.children[1].textContent; }),
    ["Total Citations", "Self-Citations", "Reciprocal Citations"]);
  assert.deepStrictEqual(items.map(function (i) { return i.children[0].textContent; }), ["7", "2", "3"]);
});

test("R4: table renders every record without a citing-paper column", function () {
  var app = load();
  var citations = [
    { citing: "W1", cited: "W2", cited_title: "Known title", self_citation: "SELF", reciprocal_citation: "CONFIRMED RECIPROCAL", shared_authors: [{ id: "A1", name: "Ann" }], error: "" },
    { citing: "W1", cited: "W3", cited_title: "", self_citation: "NOT IDENTIFIED AS SELF", reciprocal_citation: "NOT RECIPROCAL", shared_authors: [], error: "" },
    { citing: "W1", cited: "W4", cited_title: "", self_citation: "FAILED", reciprocal_citation: "UNKNOWN", shared_authors: [], error: "timeout" }
  ];
  app.ctx.showTable(citations, "empty");
  var rows = app.byId["citation-rows"].children;
  assert.strictEqual(rows.length, 3, "one row per record");
  rows.forEach(function (row) { assert.strictEqual(row.children.length, 4, "4 cells per row"); });
  assert.strictEqual(findAll(rows[0], function (n) { return n.className === "cell-title"; })[0].textContent, "Known title");
  assert.strictEqual(findAll(rows[1], function (n) { return n.className === "cell-title"; }).length, 0);
  var labels = rows.map(function (row) {
    return findAll(row, function (n) { return /type-label/.test(n.className); }).map(function (n) { return n.textContent; });
  });
  assert.deepStrictEqual(labels[0], ["SELF", "RECIPROCAL"]);
  assert.deepStrictEqual(labels[1], ["OTHER / UNCONFIRMED SELF", "ONE-WAY"]);
  assert.deepStrictEqual(labels[2], ["FETCH FAILED", "UNVERIFIED"]);
  assert.strictEqual(app.byId["table-section"].hidden, false);
});

test("R5: empty notes read None, not an em dash", function () {
  var app = load();
  var cell = app.ctx.makeNotesCell({ shared_authors: [], error: "", reciprocal_citation: "NOT RECIPROCAL", cited: "W2", citing: "W1" });
  assert.strictEqual(cell.textContent, "None");
});

test("R6: PENDING labels are neutral, not styled as failures", function () {
  var app = load();
  assert.strictEqual(app.ctx.SELF_BADGE_INFO.PENDING.cssClass, "type-pending");
  assert.strictEqual(app.ctx.RECIP_BADGE_INFO.PENDING.cssClass, "type-pending");
  assert.strictEqual(app.ctx.RECIP_BADGE_INFO.UNKNOWN.text, "UNVERIFIED");
  assert.strictEqual(app.ctx.SELF_BADGE_INFO.FAILED.text, "FETCH FAILED");
});

test("R7: graph labels adapt to width, count and ID length instead of overlapping", function () {
  var app = load();
  function ids(n, len) {
    var out = [];
    for (var i = 0; i < n; i++) {
      out.push({ id: "W" + String(i + 1).padStart(len - 1, "7") });
    }
    return out;
  }
  app.byId["graph-container"].clientWidth = 560;
  assert.strictEqual(app.ctx.shouldShowGraphLabels(ids(8, 11), 190, 8), true, "8 labels fit");
  assert.strictEqual(app.ctx.shouldShowGraphLabels(ids(16, 11), 190, 8), true, "16 short IDs still fit");
  assert.strictEqual(app.ctx.shouldShowGraphLabels(ids(16, 13), 190, 8), false, "16 long IDs would touch");
  assert.strictEqual(app.ctx.shouldShowGraphLabels(ids(17, 11), 225, 8), false, "large rings are never labeled");
  app.byId["graph-container"].clientWidth = 320;
  assert.strictEqual(app.ctx.shouldShowGraphLabels(ids(5, 11), 190, 8), false, "labels would be under 9px on a phone");
});

function drawThree(app) {
  app.ctx.drawGraph("W1", [
    { id: "W2", type: "SELF_RECIPROCAL", self_citation: "SELF", reciprocal_citation: "CONFIRMED RECIPROCAL", shared_authors: [], title: "Two" },
    { id: "W3", type: "NOT_SELF", self_citation: "NOT IDENTIFIED AS SELF", reciprocal_citation: "NOT RECIPROCAL", shared_authors: [], title: "" },
    { id: "W4", type: "UNKNOWN", self_citation: "NOT IDENTIFIED AS SELF", reciprocal_citation: "UNKNOWN", shared_authors: [], title: "" }
  ]);
  var svg = app.byId["graph-container"].children[0];
  var nodes = findAll(svg, function (n) { return n.getAttribute("class") === "graph-node"; });
  return nodes;
}

test("R8: node selection opens the panel; keyboard moves focus there, mouse does not", function () {
  var app = load();
  var nodes = drawThree(app);
  assert.strictEqual(nodes.length, 3);
  assert.strictEqual(app.byId["graph-section"].hidden, false);
  assert.strictEqual(app.byId["graph-node-panel"].hidden, true);

  fire(nodes[1], "click");
  assert.strictEqual(app.byId["graph-node-panel"].hidden, false);
  assert.strictEqual(app.byId["node-panel-title"].focusCount, 0, "mouse selection leaves focus alone");
  assert(app.byId["node-panel-details"].textContent.indexOf("W3") !== -1);

  fire(nodes[0], "keydown", { key: "Enter", preventDefault: function () {} });
  assert.strictEqual(app.byId["node-panel-title"].focusCount, 1, "keyboard selection focuses the panel");
  assert.strictEqual(nodes[0].getAttribute("aria-pressed"), "true");
  assert.strictEqual(nodes[1].getAttribute("aria-pressed"), "false");
  assert.strictEqual(app.byId["graph-node-panel"].hidden, false, "switching nodes keeps the panel open");
});

test("R9: closing the panel (button or Escape) returns focus to the node", function () {
  var app = load();
  var nodes = drawThree(app);
  fire(nodes[2], "keydown", { key: " ", preventDefault: function () {} });
  fire(app.byId["node-panel-close"], "click");
  assert.strictEqual(app.byId["graph-node-panel"].hidden, true);
  assert.strictEqual(nodes[2].focusCount, 1, "focus returned to the node");
  assert.strictEqual(nodes[2].getAttribute("aria-pressed"), "false");

  fire(nodes[0], "keydown", { key: "Enter", preventDefault: function () {} });
  fire(app.byId["graph-node-panel"], "keydown", { key: "Escape" });
  assert.strictEqual(app.byId["graph-node-panel"].hidden, true);
  assert.strictEqual(nodes[0].focusCount, 1);
});

test("R10: SELF + RECIPROCAL dot carries a ring, plain ones do not", function () {
  var app = load();
  var nodes = drawThree(app);
  var dots = nodes.map(function (g) {
    return findAll(g, function (n) { return n.getAttribute("class") === "graph-node-dot"; })[0];
  });
  assert.strictEqual(dots[0].getAttribute("stroke"), "#2B5C9E");
  assert.strictEqual(dots[1].getAttribute("stroke"), null);
});

test("R11: animation runs when allowed and is skipped under reduced motion", function () {
  var allowed = load({ window: { matchMedia: function () { return { matches: false }; } } });
  var el = createFakeElement("section", "");
  el.hidden = true;
  el.animate = function () { el.animateCalls++; };
  allowed.ctx.reveal(el, 6, 220);
  assert.strictEqual(el.hidden, false);
  assert.strictEqual(el.animateCalls, 1, "animates once when it was hidden");
  allowed.ctx.reveal(el, 6, 220);
  assert.strictEqual(el.animateCalls, 1, "no repeat animation for an already visible section");

  var reduced = load({ window: { matchMedia: function () { return { matches: true }; } } });
  var el2 = createFakeElement("section", "");
  el2.hidden = true;
  el2.animate = function () { el2.animateCalls++; };
  reduced.ctx.reveal(el2, 6, 220);
  assert.strictEqual(el2.hidden, false, "content still appears");
  assert.strictEqual(el2.animateCalls, 0, "reduced motion disables scripted animation");
});

test("R12: content appears if animation is unavailable or throws", function () {
  var app = load({ window: { matchMedia: function () { return { matches: false }; } } });
  var noAnimate = createFakeElement("section", "");
  noAnimate.hidden = true;
  app.ctx.reveal(noAnimate, 6, 220);
  assert.strictEqual(noAnimate.hidden, false);

  var throwing = createFakeElement("section", "");
  throwing.hidden = true;
  throwing.animate = function () { throw new Error("boom"); };
  app.ctx.reveal(throwing, 6, 220);
  assert.strictEqual(throwing.hidden, false);

  var broken = load({ window: { matchMedia: function () { throw new Error("no media"); } } });
  var el = createFakeElement("section", "");
  el.hidden = true;
  el.animate = function () { el.animateCalls++; };
  broken.ctx.reveal(el, 6, 220);
  assert.strictEqual(el.hidden, false);
  assert.strictEqual(el.animateCalls, 0, "unknown preference is treated as reduced");
});

console.log(results.join("\n"));
var failed = results.filter(function (r) { return r.indexOf("[FAIL]") === 0; }).length;
console.log("\nResults: " + (results.length - failed) + " passed, " + failed + " failed");
