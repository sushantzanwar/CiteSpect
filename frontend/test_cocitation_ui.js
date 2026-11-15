"use strict";

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
    target: "",
    rel: "",
    attributes: {},
    children: [],
    listeners: {},
    style: {},
    _text: "",
    classList: {
      add: function () {},
      remove: function () {}
    },
    setAttribute: function (k, v) {
      this.attributes[k] = String(v);
    },
    appendChild: function (child) {
      this.children.push(child);
      return child;
    },
    addEventListener: function (type, handler) {
      if (!this.listeners[type]) {
        this.listeners[type] = [];
      }
      this.listeners[type].push(handler);
    },
    focus: function () {},
    querySelector: function () {
      return null;
    }
  };

  Object.defineProperty(el, "textContent", {
    get: function () {
      if (this.children.length === 0) {
        return this._text;
      }
      return this.children.map(function (c) {
        return c.nodeType === 3 ? c.nodeValue : c.textContent;
      }).join("");
    },
    set: function (val) {
      this._text = String(val);
      this.children = [];
    }
  });

  Object.defineProperty(el, "innerHTML", {
    get: function () {
      return "";
    },
    set: function () {
      this.children = [];
      this._text = "";
    }
  });

  return el;
}

function collectAnchors(node, out) {
  out = out || [];
  if (!node || !Array.isArray(node.children)) {
    return out;
  }
  if (node.tagName === "A") {
    out.push(node);
  }
  node.children.forEach(function (child) {
    collectAnchors(child, out);
  });
  return out;
}

async function dispatch(el, type, eventObj) {
  var handlers = el.listeners[type] || [];
  for (var i = 0; i < handlers.length; i++) {
    await handlers[i](eventObj || { preventDefault: function () {} });
  }
}

async function runTests() {
  var byId = {};
  var ids = [
    "search-form", "doi-input", "analyze-button", "status",
    "paper-section", "paper-details", "authors-heading", "author-list",
    "analysis-section", "analysis-message", "summary-grid",
    "table-section", "table-empty", "citation-rows",
    "graph-section", "graph-container", "graph-legend",
    "graph-node-panel", "node-panel-details", "node-panel-close",
    "coupling-form", "coupling-paper-a", "coupling-paper-b", "coupling-button",
    "coupling-status", "coupling-results", "coupling-summary-grid",
    "coupling-details", "coupling-shared-wrap", "coupling-shared-heading",
    "coupling-shared-list", "comparison-method-hint",
    "mode-coupling", "mode-cocitation"
  ];
  ids.forEach(function (id) {
    byId[id] = createFakeElement("div", id);
  });
  byId["mode-coupling"].checked = true;
  byId["coupling-results"].hidden = true;
  byId["coupling-shared-wrap"].hidden = true;
  var tableWrap = createFakeElement("div", "");

  var fetchCalls = [];
  var nextFetchResponse = null;

  var context = {
    document: {
      getElementById: function (id) {
        return byId[id] || createFakeElement("div", id);
      },
      createElement: function (tag) {
        return createFakeElement(tag, "");
      },
      createElementNS: function (ns, tag) {
        return createFakeElement(tag, "");
      },
      createTextNode: function (txt) {
        return { nodeType: 3, nodeValue: String(txt) };
      },
      querySelector: function (sel) {
        return sel === ".table-wrap" ? tableWrap : createFakeElement("div", "");
      }
    },
    fetch: async function (url, opts) {
      fetchCalls.push({ url: url, body: JSON.parse(opts.body) });
      return {
        ok: true,
        status: 200,
        json: async function () {
          return nextFetchResponse;
        }
      };
    },
    Number: Number,
    Array: Array,
    Object: Object,
    Math: Math,
    String: String
  };

  var scriptPath = path.join(__dirname, "script.js");
  var scriptCode = fs.readFileSync(scriptPath, "utf8");
  vm.runInNewContext(scriptCode, context, { filename: "script.js" });

  // T1: Switching to Co-Citation mode updates explanation and clears old results
  byId["coupling-results"].hidden = false;
  byId["coupling-status"].textContent = "Old message";
  byId["mode-coupling"].checked = false;
  byId["mode-cocitation"].checked = true;
  await dispatch(byId["mode-cocitation"], "change");
  assert(byId["comparison-method-hint"].textContent.indexOf("third-party papers cite both") !== -1,
    "T1: Co-Citation hint text must explain third-party co-citation");
  assert.strictEqual(byId["coupling-results"].hidden, true, "T1: Results hidden on mode switch");
  assert.strictEqual(byId["coupling-status"].textContent, "", "T1: Status cleared on mode switch");

  // T2: Identical IDs rejected in Co-Citation mode without network call
  fetchCalls = [];
  byId["coupling-paper-a"].value = "W4395000590";
  byId["coupling-paper-b"].value = "W4395000590";
  await dispatch(byId["coupling-form"], "submit", { preventDefault: function () {} });
  assert.strictEqual(fetchCalls.length, 0, "T2: Identical IDs must not trigger fetch");
  assert(byId["coupling-status"].textContent.indexOf("distinct") !== -1,
    "T2: Error message must mention distinct IDs");

  // T3: Complete YES Co-Citation result renders exact Jaccard & clickable OpenAlex links
  nextFetchResponse = {
    success: true,
    paper_a: "W4395000590",
    paper_a_title: "Paper A Title",
    paper_b: "W4401955319",
    paper_b_title: "Paper B Title",
    status: "YES",
    data_complete: true,
    unique_citers_a: 3,
    unique_citers_b: 4,
    common_count: 2,
    union_count: 5,
    jaccard_similarity: 40.0,
    co_citing_papers: [
      { id: "W7129575785", title: "Citing Paper 1" },
      { id: "W7129575786", title: "" }
    ]
  };
  byId["coupling-paper-a"].value = "W4395000590";
  byId["coupling-paper-b"].value = "https://openalex.org/W4401955319";
  await dispatch(byId["coupling-form"], "submit", { preventDefault: function () {} });
  assert.strictEqual(fetchCalls.length, 1, "T3: Must call fetch once");
  assert.strictEqual(fetchCalls[0].url, "/api/cocitation", "T3: Must POST to /api/cocitation");
  assert.strictEqual(byId["coupling-results"].hidden, false, "T3: Results must be visible");
  assert(byId["coupling-summary-grid"].textContent.indexOf("40.00%") !== -1,
    "T3: Summary must show 40.00%");
  var sharedAnchors = collectAnchors(byId["coupling-shared-list"]);
  assert.strictEqual(sharedAnchors.length, 2, "T3: Common co-citing papers must render clickable links");
  assert.strictEqual(sharedAnchors[0].href, "https://openalex.org/W7129575785");
  assert.strictEqual(sharedAnchors[0].target, "_blank");

  // T4: Partial YES (incomplete data) must show UNVERIFIED instead of 0%
  nextFetchResponse = {
    success: true,
    paper_a: "W4395000590",
    paper_a_title: "",
    paper_b: "W4401955319",
    paper_b_title: "",
    status: "YES",
    data_complete: false,
    unique_citers_a: 1000,
    unique_citers_b: 2,
    common_count: 1,
    union_count: 1001,
    jaccard_similarity: null,
    co_citing_papers: [{ id: "W7129575785", title: "" }]
  };
  await dispatch(byId["coupling-form"], "submit", { preventDefault: function () {} });
  assert(byId["coupling-summary-grid"].textContent.indexOf("UNVERIFIED") !== -1,
    "T4: Incomplete YES must show UNVERIFIED in summary");
  assert.strictEqual(byId["coupling-summary-grid"].textContent.indexOf("0%"), -1,
    "T4: Incomplete YES must never show 0%");

  // T5: UNKNOWN co-citation result must show UNVERIFIED instead of 0%
  nextFetchResponse = {
    success: true,
    paper_a: "W4395000590",
    paper_a_title: "",
    paper_b: "W4401955319",
    paper_b_title: "",
    status: "UNKNOWN",
    data_complete: false,
    unique_citers_a: null,
    unique_citers_b: null,
    common_count: null,
    union_count: null,
    jaccard_similarity: null,
    co_citing_papers: []
  };
  await dispatch(byId["coupling-form"], "submit", { preventDefault: function () {} });
  assert(byId["coupling-summary-grid"].textContent.indexOf("UNKNOWN") !== -1);
  assert(byId["coupling-summary-grid"].textContent.indexOf("UNVERIFIED") !== -1);
  assert.strictEqual(byId["coupling-shared-wrap"].hidden, true);

  // T6: Switching back to Bibliographic Coupling preserves existing coupling workflow
  byId["mode-cocitation"].checked = false;
  byId["mode-coupling"].checked = true;
  await dispatch(byId["mode-coupling"], "change");
  fetchCalls = [];
  nextFetchResponse = {
    success: true,
    paper_a: "W2919115771",
    paper_a_title: "",
    paper_b: "W1901616594",
    paper_b_title: "",
    status: "YES",
    unique_references_a: 3,
    unique_references_b: 3,
    common_count: 2,
    union_count: 4,
    jaccard_similarity: 50.0,
    shared_references: [{ id: "W999", title: "" }]
  };
  byId["coupling-paper-a"].value = "W2919115771";
  byId["coupling-paper-b"].value = "W1901616594";
  await dispatch(byId["coupling-form"], "submit", { preventDefault: function () {} });
  assert.strictEqual(fetchCalls[0].url, "/api/coupling", "T6: Coupling mode must POST to /api/coupling");
  assert(byId["coupling-summary-grid"].textContent.indexOf("50.00%") !== -1);
  assert(byId["coupling-shared-heading"].textContent.indexOf("Shared references") !== -1);

  console.log("All 6 Co-Citation frontend JavaScript tests PASSED.");
}

runTests().catch(function (err) {
  console.error(err);
  process.exit(1);
});

