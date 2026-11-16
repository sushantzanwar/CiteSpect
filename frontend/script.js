var API_ANALYZE = "/api/analyze";
var API_ANALYSIS = "/api/citation-analysis";
var API_COUPLING = "/api/coupling";
var API_COCITATION = "/api/cocitation";
var SVG_NS = "http://www.w3.org/2000/svg";

var COUPLING_BADGE_INFO = {
  "YES": { text: "YES", cssClass: "type-self" },
  "NO": { text: "NO", cssClass: "type-not-self" },
  "UNKNOWN": { text: "UNKNOWN", cssClass: "type-unknown" }
};

var SELF_BADGE_INFO = {
  "SELF": { text: "SELF", cssClass: "type-self" },
  "NOT IDENTIFIED AS SELF": { text: "OTHER / UNCONFIRMED SELF", cssClass: "type-not-self" },
  "FAILED": { text: "FETCH FAILED", cssClass: "type-failed" },
  "PENDING": { text: "PENDING", cssClass: "type-pending" }
};

var RECIP_BADGE_INFO = {
  "CONFIRMED RECIPROCAL": { text: "RECIPROCAL", cssClass: "type-reciprocal" },
  "NOT RECIPROCAL": { text: "ONE-WAY", cssClass: "type-not-reciprocal" },
  "UNKNOWN": { text: "UNVERIFIED", cssClass: "type-unknown" },
  "PENDING": { text: "PENDING", cssClass: "type-pending" }
};

var GRAPH_TYPE_INFO = {
  SELF_RECIPROCAL: { text: "SELF + RECIPROCAL", legend: "Self and reciprocal", color: "#246B57", ring: "#2B5C9E", cssClass: "type-reciprocal" },
  SELF: { text: "SELF", legend: "Self-citation", color: "#246B57", cssClass: "type-self" },
  RECIPROCAL: { text: "RECIPROCAL", legend: "Reciprocal citation", color: "#2B5C9E", cssClass: "type-reciprocal" },
  NOT_SELF: { text: "OTHER / UNCONFIRMED SELF", legend: "Other / unconfirmed self (one-way)", color: "#5A645D", cssClass: "type-not-self" },
  UNKNOWN: { text: "UNVERIFIED", legend: "Unverified", color: "#8A5A00", cssClass: "type-unknown" },
  FAILED: { text: "FETCH FAILED", legend: "Metadata retrieval failed", color: "#A23B2F", cssClass: "type-failed" },
  PENDING: { text: "PENDING", legend: "Cited paper (pending analysis)", color: "#8A938C", cssClass: "" }
};

var SUMMARY_ITEMS = [
  { key: "analyzed", label: "Total Citations", css: "" },
  { key: "self", label: "Self-Citations", css: "self" },
  { key: "reciprocal_confirmed", label: "Reciprocal Citations", css: "reciprocal" }
];

var searchForm = document.getElementById("search-form");
var doiInput = document.getElementById("doi-input");
var analyzeButton = document.getElementById("analyze-button");
var statusLine = document.getElementById("status");

var paperSection = document.getElementById("paper-section");
var paperHeading = document.getElementById("paper-heading");
var paperDetails = document.getElementById("paper-details");
var authorsHeading = document.getElementById("authors-heading");
var authorList = document.getElementById("author-list");

var analysisSection = document.getElementById("analysis-section");
var analysisMessage = document.getElementById("analysis-message");
var summaryGrid = document.getElementById("summary-grid");

var tableSection = document.getElementById("table-section");
var tableContext = document.getElementById("table-context");
var tableEmpty = document.getElementById("table-empty");
var citationRows = document.getElementById("citation-rows");

var graphSection = document.getElementById("graph-section");
var graphContainer = document.getElementById("graph-container");
var graphLegend = document.getElementById("graph-legend");
var nodePanel = document.getElementById("graph-node-panel");
var nodePanelTitle = document.getElementById("node-panel-title");
var nodePanelDetails = document.getElementById("node-panel-details");
var nodePanelClose = document.getElementById("node-panel-close");

var couplingForm = document.getElementById("coupling-form");
var couplingInputA = document.getElementById("coupling-paper-a");
var couplingInputB = document.getElementById("coupling-paper-b");
var couplingButton = document.getElementById("coupling-button");
var couplingStatus = document.getElementById("coupling-status");
var couplingResults = document.getElementById("coupling-results");
var couplingSummaryGrid = document.getElementById("coupling-summary-grid");
var couplingDetails = document.getElementById("coupling-details");
var couplingSharedWrap = document.getElementById("coupling-shared-wrap");
var couplingSharedHeading = document.getElementById("coupling-shared-heading");
var couplingSharedList = document.getElementById("coupling-shared-list");
var comparisonMethodHint = document.getElementById("comparison-method-hint");
var modeCouplingRadio = document.getElementById("mode-coupling");
var modeCocitationRadio = document.getElementById("mode-cocitation");

var isBusy = false;
var isCouplingBusy = false;
var selectedGraphState = null;
var lastGraph = null;
var AUTHORS_COLLAPSED = 8;


function prefersReducedMotion() {
  try {
    return typeof window !== "undefined" && typeof window.matchMedia === "function" &&
      window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  } catch (error) {
    return true;
  }
}

function animateIn(element, rise, duration) {
  if (!element || typeof element.animate !== "function" || prefersReducedMotion()) {
    return;
  }
  try {
    element.animate(
      [
        { opacity: 0, transform: "translateY(" + rise + "px)" },
        { opacity: 1, transform: "translateY(0)" }
      ],
      { duration: duration, easing: "ease-out" }
    );
  } catch (error) {
    // The content is already visible; animation is optional.
  }
}

function fadeIn(element, duration) {
  if (!element || typeof element.animate !== "function" || prefersReducedMotion()) {
    return;
  }
  try {
    element.animate([{ opacity: 0 }, { opacity: 1 }], { duration: duration, easing: "ease-out" });
  } catch (error) {
    // The content is already visible; animation is optional.
  }
}

function reveal(element, rise, duration) {
  var wasHidden = element.hidden;
  element.hidden = false;
  if (wasHidden) {
    animateIn(element, rise, duration);
  }
}


function showStatus(element, message, kind) {
  element.textContent = message;
  element.className = kind ? "status " + kind : "status";
}

function setBusy(busy) {
  isBusy = busy;
  analyzeButton.disabled = busy;
  analyzeButton.textContent = busy ? "Working..." : "Analyze";
}

function normalizeQuery(text) {
  var q = text.trim();
  q = q.replace(/^https?:\/\/(dx\.)?doi\.org\//i, "");
  q = q.replace(/^https?:\/\/openalex\.org\//i, "");
  q = q.replace(/^doi:\s*/i, "");
  return q;
}

function isValidQuery(query) {
  if (/^W[A-Za-z0-9_]{1,28}$/i.test(query)) {
    return true;
  }
  return /^10\.\d{1,9}\/[A-Za-z0-9\-._;()\/:]+$/.test(query);
}

function isOpenAlexId(text) {
  return /^[AW]\d+$/.test(text);
}


async function postJson(url, body) {
  var response;
  try {
    response = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    });
  } catch (networkError) {
    throw new Error("Could not connect to the CiteSpect server. Check that the Flask bridge is running.");
  }

  var data;
  try {
    data = await response.json();
  } catch (parseError) {
    throw new Error("The server sent an answer this page could not read (HTTP " + response.status + ").");
  }

  if (!response.ok || data.success !== true) {
    throw new Error(data.error || "The request failed (HTTP " + response.status + ").");
  }
  return data;
}


function addDetailTo(container, label, valueNode) {
  var term = document.createElement("dt");
  term.textContent = label;
  var description = document.createElement("dd");
  description.appendChild(valueNode);
  container.appendChild(term);
  container.appendChild(description);
}

function textNode(text) {
  return document.createTextNode(String(text));
}

function addMetaTo(container, label, valueNode) {
  var group = document.createElement("div");
  var term = document.createElement("dt");
  term.textContent = label;
  var description = document.createElement("dd");
  description.appendChild(valueNode);
  group.appendChild(term);
  group.appendChild(description);
  container.appendChild(group);
}

function showAuthors(authors) {
  authorList.innerHTML = "";
  authorsHeading.hidden = authors.length === 0;

  var collapsed = authors.length > AUTHORS_COLLAPSED + 1;
  var extraItems = [];

  authors.forEach(function (name, index) {
    var item = document.createElement("li");
    item.textContent = name;
    if (collapsed && index >= AUTHORS_COLLAPSED) {
      item.hidden = true;
      extraItems.push(item);
    }
    authorList.appendChild(item);
  });

  if (!collapsed) {
    return;
  }

  var moreItem = document.createElement("li");
  moreItem.className = "author-more";
  var toggle = document.createElement("button");
  toggle.type = "button";
  toggle.className = "link-button";
  toggle.setAttribute("aria-expanded", "false");
  var hiddenCount = authors.length - AUTHORS_COLLAPSED;
  toggle.textContent = "and " + hiddenCount + " more authors";
  toggle.addEventListener("click", function () {
    var expand = toggle.getAttribute("aria-expanded") !== "true";
    extraItems.forEach(function (item) {
      item.hidden = !expand;
    });
    toggle.setAttribute("aria-expanded", expand ? "true" : "false");
    toggle.textContent = expand ? "show fewer authors" : "and " + hiddenCount + " more authors";
  });
  moreItem.appendChild(toggle);
  authorList.appendChild(moreItem);
}

function showPaper(paper) {
  paperDetails.innerHTML = "";

  paperHeading.textContent = paper.title || paper.paper_id || "Untitled paper";

  if (Number.isInteger(paper.year) && paper.year > 0) {
    addMetaTo(paperDetails, "Year", textNode(paper.year));
  }
  if (paper.doi) {
    addMetaTo(paperDetails, "DOI", textNode(paper.doi));
  }
  addMetaTo(paperDetails, "OpenAlex Work ID", makeIdNode(paper.paper_id));
  if (Number.isInteger(paper.reference_count)) {
    addMetaTo(paperDetails, "References", textNode(paper.reference_count));
  }

  showAuthors(Array.isArray(paper.authors) ? paper.authors : []);

  reveal(paperSection, 6, 220);
}


function makeIdNode(id) {
  if (!isOpenAlexId(id)) {
    return textNode(id);
  }
  var link = document.createElement("a");
  link.href = "https://openalex.org/" + id;
  link.target = "_blank";
  link.rel = "noopener noreferrer";
  link.textContent = id;
  return link;
}

function makeSummaryItem(item, value) {
  var box = document.createElement("div");
  box.className = item.css ? "summary-item " + item.css : "summary-item";
  var number = document.createElement("span");
  number.className = "value";
  number.textContent = value;
  var label = document.createElement("span");
  label.className = "label";
  label.textContent = item.label;
  box.appendChild(number);
  box.appendChild(label);
  return box;
}

function showSummary(summary) {
  summaryGrid.innerHTML = "";
  SUMMARY_ITEMS.forEach(function (item) {
    var val = Number.isInteger(summary[item.key]) ? summary[item.key] : 0;
    summaryGrid.appendChild(makeSummaryItem(item, val));
  });
}


function makeSelfBadge(selfStatus) {
  var info = SELF_BADGE_INFO[selfStatus] || SELF_BADGE_INFO["NOT IDENTIFIED AS SELF"];
  var label = document.createElement("span");
  label.className = "type-label " + info.cssClass;
  label.textContent = info.text;
  return label;
}

function makeRecipBadge(recipStatus) {
  var info = RECIP_BADGE_INFO[recipStatus] || RECIP_BADGE_INFO["UNKNOWN"];
  var label = document.createElement("span");
  label.className = "type-label " + info.cssClass;
  label.textContent = info.text;
  return label;
}

function formatSharedAuthors(citation) {
  if (!Array.isArray(citation.shared_authors) || citation.shared_authors.length === 0) {
    return "";
  }
  return citation.shared_authors.map(function (author) {
    return author.name ? author.name + " (" + author.id + ")" : author.id;
  }).join(", ");
}

function makeNotesCell(citation) {
  var cell = document.createElement("td");
  var parts = [];
  var sharedText = formatSharedAuthors(citation);

  if (sharedText) {
    parts.push("Shared: " + sharedText);
  }
  if (citation.error) {
    parts.push("Metadata retrieval failed: " + citation.error);
  } else if (citation.reciprocal_citation === "CONFIRMED RECIPROCAL") {
    parts.push("Mutual citation verified (" + citation.cited + " \u2192 " + citation.citing + ")");
  } else if (citation.reciprocal_citation === "UNKNOWN") {
    parts.push("Cited paper reference list unavailable");
  }

  cell.textContent = parts.length > 0 ? parts.join(" | ") : "None";
  return cell;
}

function makeCitedCell(citation) {
  var cell = document.createElement("td");
  cell.appendChild(makeIdNode(citation.cited));
  if (citation.cited_title) {
    var title = document.createElement("span");
    title.className = "cell-title";
    title.textContent = citation.cited_title;
    cell.appendChild(title);
  }
  return cell;
}

function makeCitationRow(citation) {
  var row = document.createElement("tr");

  var selfCell = document.createElement("td");
  selfCell.appendChild(makeSelfBadge(citation.self_citation));

  var recipCell = document.createElement("td");
  recipCell.appendChild(makeRecipBadge(citation.reciprocal_citation));

  row.appendChild(makeCitedCell(citation));
  row.appendChild(selfCell);
  row.appendChild(recipCell);
  row.appendChild(makeNotesCell(citation));
  return row;
}

function showTable(citations, emptyMessage) {
  citationRows.innerHTML = "";
  citations.forEach(function (citation) {
    citationRows.appendChild(makeCitationRow(citation));
  });

  document.querySelector(".table-wrap").hidden = citations.length === 0;
  tableEmpty.hidden = citations.length > 0;
  tableEmpty.textContent = emptyMessage;
  reveal(tableSection, 6, 220);
}


function svgElement(name, attributes) {
  var element = document.createElementNS(SVG_NS, name);
  Object.keys(attributes).forEach(function (key) {
    element.setAttribute(key, attributes[key]);
  });
  return element;
}

function createArrowMarker(id, fillColor) {
  var marker = svgElement("marker", {
    id: id,
    viewBox: "0 0 10 10",
    refX: "8",
    refY: "5",
    markerWidth: "7",
    markerHeight: "7",
    orient: "auto-start-reverse"
  });
  marker.appendChild(svgElement("path", { d: "M 0 1.5 L 8.5 5 L 0 8.5 z", fill: fillColor }));
  return marker;
}

function clearSelectionStyles() {
  if (selectedGraphState) {
    selectedGraphState.nodeGroup.classList.remove("is-selected");
    selectedGraphState.nodeGroup.setAttribute("aria-pressed", "false");
    selectedGraphState.edgeLine.classList.remove("is-selected");
    selectedGraphState.edgeLine.setAttribute("marker-end", "url(#arrow-default)");
    selectedGraphState = null;
  }
}

function clearNodeSelection() {
  clearSelectionStyles();
  nodePanelDetails.innerHTML = "";
  nodePanel.hidden = true;
}

function closeNodePanel() {
  var previousNode = selectedGraphState ? selectedGraphState.nodeGroup : null;
  clearNodeSelection();
  if (previousNode && typeof previousNode.focus === "function") {
    previousNode.focus();
  }
}

function showNodePanel(cited) {
  nodePanelDetails.innerHTML = "";

  addDetailTo(nodePanelDetails, "Work ID", textNode(cited.id));
  addDetailTo(nodePanelDetails, "Title", textNode(cited.title || "Not available in local cache"));
  addDetailTo(nodePanelDetails, "Self-citation", makeSelfBadge(cited.self_citation || "PENDING"));
  addDetailTo(nodePanelDetails, "Reciprocal", makeRecipBadge(cited.reciprocal_citation || "PENDING"));

  var sharedText = formatSharedAuthors(cited);
  if (cited.error) {
    addDetailTo(nodePanelDetails, "Notes", textNode("Metadata retrieval failed: " + cited.error));
  } else {
    addDetailTo(nodePanelDetails, "Shared authors", textNode(sharedText || "None"));
  }

  if (isOpenAlexId(cited.id)) {
    var extLink = document.createElement("a");
    extLink.href = "https://openalex.org/" + cited.id;
    extLink.target = "_blank";
    extLink.rel = "noopener noreferrer";
    extLink.textContent = "View " + cited.id + " on OpenAlex";
    addDetailTo(nodePanelDetails, "OpenAlex", extLink);
  }

  reveal(nodePanel, 8, 200);
}

function focusNodePanel() {
  if (nodePanelTitle && typeof nodePanelTitle.focus === "function") {
    nodePanelTitle.focus();
  }
}

function selectGraphNode(state, viaKeyboard) {
  if (selectedGraphState && selectedGraphState.nodeGroup === state.nodeGroup) {
    if (viaKeyboard) {
      focusNodePanel();
    }
    return;
  }
  clearSelectionStyles();
  selectedGraphState = state;
  state.nodeGroup.classList.add("is-selected");
  state.nodeGroup.setAttribute("aria-pressed", "true");
  state.edgeLine.classList.add("is-selected");
  state.edgeLine.setAttribute("marker-end", "url(#arrow-active)");
  showNodePanel(state.cited);
  if (viaKeyboard) {
    focusNodePanel();
  }
}

function showHoverLabel(hoverGroup, x, y, text) {
  var rect = hoverGroup.querySelector("rect");
  var label = hoverGroup.querySelector("text");
  label.textContent = text;
  var width = Math.max(72, text.length * 7.2 + 14);
  var boxX = Math.min(Math.max(x - width / 2, 4), 600 - width - 4);
  var boxY = y > 40 ? y - 28 : y + 14;
  rect.setAttribute("x", boxX);
  rect.setAttribute("y", boxY);
  rect.setAttribute("width", width);
  label.setAttribute("x", boxX + width / 2);
  label.setAttribute("y", boxY + 14);
  hoverGroup.removeAttribute("hidden");
}

function shouldShowGraphLabels(citedPapers, ringRadius, dotRadius) {
  var renderedWidth = graphContainer.clientWidth || 560;
  var labelPx = 11 * renderedWidth / 600;
  if (labelPx < 9 || ringRadius > 190) {
    return false;
  }
  var longest = 0;
  citedPapers.forEach(function (cited) {
    longest = Math.max(longest, String(cited.id).length);
  });
  var labelWidth = longest * 6.4 + 10;
  var circumference = 2 * Math.PI * (ringRadius + dotRadius + 10);
  return citedPapers.length * labelWidth <= circumference;
}

function drawGraph(centerId, citedPapers) {
  var size = 600;
  var middle = size / 2;
  var centerRadius = 24;
  var ringRadius = citedPapers.length <= 16 ? 190 : 225;
  var dotRadius = citedPapers.length > 40 ? 6 : 8;

  clearNodeSelection();
  reveal(graphSection, 6, 220);
  var showAllLabels = shouldShowGraphLabels(citedPapers, ringRadius, dotRadius);
  lastGraph = { centerId: centerId, citedPapers: citedPapers, labelsShown: showAllLabels };

  var svg = svgElement("svg", { viewBox: "0 0 " + size + " " + size, role: "img" });
  svg.setAttribute("aria-label", centerId + " cites " + citedPapers.length + " papers. Select any cited node to view details.");

  var defs = svgElement("defs", {});
  defs.appendChild(createArrowMarker("arrow-default", "#8A938C"));
  defs.appendChild(createArrowMarker("arrow-active", "#202420"));
  svg.appendChild(defs);

  var edgesLayer = svgElement("g", { class: "graph-edges" });
  var nodesLayer = svgElement("g", { class: "graph-nodes" });
  svg.appendChild(edgesLayer);
  svg.appendChild(nodesLayer);

  var hoverGroup = svgElement("g", { class: "graph-hover-pill", hidden: "true" });
  hoverGroup.appendChild(svgElement("rect", { height: 20, rx: 4, ry: 4 }));
  hoverGroup.appendChild(svgElement("text", { "text-anchor": "middle", "font-size": 11 }));

  var usedTypes = {};
  citedPapers.forEach(function (cited, index) {
    var angle = (2 * Math.PI * index) / citedPapers.length - Math.PI / 2;
    var cosA = Math.cos(angle);
    var sinA = Math.sin(angle);
    var x = middle + ringRadius * cosA;
    var y = middle + ringRadius * sinA;
    var x1 = middle + centerRadius * cosA;
    var y1 = middle + centerRadius * sinA;
    var x2 = x - (dotRadius + 4) * cosA;
    var y2 = y - (dotRadius + 4) * sinA;

    var info = GRAPH_TYPE_INFO[cited.type] || GRAPH_TYPE_INFO.PENDING;
    usedTypes[cited.type] = true;

    var edgeLine = svgElement("line", {
      x1: x1,
      y1: y1,
      x2: x2,
      y2: y2,
      class: "graph-edge",
      "marker-end": "url(#arrow-default)"
    });
    edgesLayer.appendChild(edgeLine);

    var selfInfo = SELF_BADGE_INFO[cited.self_citation] || SELF_BADGE_INFO["PENDING"];
    var recipInfo = RECIP_BADGE_INFO[cited.reciprocal_citation] || RECIP_BADGE_INFO["PENDING"];
    var nodeGroup = svgElement("g", {
      class: "graph-node",
      tabindex: "0",
      role: "button",
      "aria-pressed": "false",
      "aria-label": cited.id + " - Self-citation: " + selfInfo.text + ", Reciprocal: " + recipInfo.text
    });

    var dotAttributes = { cx: x, cy: y, r: dotRadius, fill: info.color, class: "graph-node-dot" };
    if (info.ring) {
      dotAttributes.stroke = info.ring;
      dotAttributes["stroke-width"] = 3;
    }
    nodeGroup.appendChild(svgElement("circle", { cx: x, cy: y, r: dotRadius + 5, class: "graph-node-halo" }));
    nodeGroup.appendChild(svgElement("circle", dotAttributes));

    var tip = svgElement("title", {});
    tip.textContent = cited.id + (cited.title ? " (" + cited.title + ")" : "") + (info.text ? " [" + info.text + "]" : "");
    nodeGroup.appendChild(tip);

    if (showAllLabels) {
      var labelRadius = ringRadius + dotRadius + 10;
      var lx = middle + labelRadius * cosA;
      var ly = middle + labelRadius * sinA + 4;
      var anchor = cosA > 0.25 ? "start" : (cosA < -0.25 ? "end" : "middle");
      var nodeLabel = svgElement("text", { x: lx, y: ly, "text-anchor": anchor, class: "graph-node-label" });
      nodeLabel.textContent = cited.id;
      nodeGroup.appendChild(nodeLabel);
    }

    var state = { nodeGroup: nodeGroup, edgeLine: edgeLine, cited: cited };
    nodeGroup.addEventListener("click", function () {
      selectGraphNode(state, false);
    });
    nodeGroup.addEventListener("keydown", function (event) {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        selectGraphNode(state, true);
      }
    });
    if (!showAllLabels) {
      nodeGroup.addEventListener("mouseenter", function () {
        showHoverLabel(hoverGroup, x, y, cited.id);
      });
      nodeGroup.addEventListener("focus", function () {
        showHoverLabel(hoverGroup, x, y, cited.id);
      });
      nodeGroup.addEventListener("mouseleave", function () {
        hoverGroup.setAttribute("hidden", "true");
      });
      nodeGroup.addEventListener("blur", function () {
        hoverGroup.setAttribute("hidden", "true");
      });
    }

    nodesLayer.appendChild(nodeGroup);
  });

  svg.appendChild(svgElement("circle", { cx: middle, cy: middle, r: centerRadius, fill: "#202420" }));
  var centerLabel = svgElement("text", { x: middle, y: middle + 42, "text-anchor": "middle", "font-size": 15, "font-weight": 600, fill: "#202420" });
  centerLabel.textContent = centerId;
  svg.appendChild(centerLabel);

  if (!showAllLabels) {
    svg.appendChild(hoverGroup);
  }

  graphContainer.innerHTML = "";
  graphContainer.appendChild(svg);
  showLegend(usedTypes);
}

function showLegend(usedTypes) {
  graphLegend.innerHTML = "";
  Object.keys(usedTypes).forEach(function (type) {
    var info = GRAPH_TYPE_INFO[type] || GRAPH_TYPE_INFO.PENDING;
    var item = document.createElement("li");
    var swatch = document.createElement("span");
    swatch.className = "swatch";
    swatch.style.background = info.color;
    if (info.ring) {
      swatch.style.border = "2px solid " + info.ring;
    }
    item.appendChild(swatch);
    item.appendChild(textNode(info.legend));
    graphLegend.appendChild(item);
  });

  var arrowItem = document.createElement("li");
  var arrowSwatch = document.createElement("span");
  arrowSwatch.className = "legend-arrow";
  arrowSwatch.textContent = "\u2192";
  arrowItem.appendChild(arrowSwatch);
  arrowItem.appendChild(textNode("Direction: Citing \u2192 Cited"));
  graphLegend.appendChild(arrowItem);
}


function resetResults() {
  paperSection.hidden = true;
  analysisSection.hidden = true;
  tableSection.hidden = true;
  graphSection.hidden = true;
  lastGraph = null;
  summaryGrid.innerHTML = "";
  citationRows.innerHTML = "";
  clearNodeSelection();
  showStatus(analysisMessage, "", "");
}

function drawBasicGraph(paper) {
  var cited = Array.isArray(paper.citation_graph) ? paper.citation_graph : [];
  if (cited.length === 0) {
    return;
  }
  drawGraph(paper.graph_paper || paper.paper_id, cited.map(function (id) {
    return {
      id: id,
      type: "PENDING",
      self_citation: "PENDING",
      reciprocal_citation: "PENDING",
      shared_authors: [],
      title: ""
    };
  }));
}

function emptyTableMessage(paper) {
  if (paper.reference_count > 0) {
    return "OpenAlex lists " + paper.reference_count + " references for this paper, but none are stored in the local citation records, so there is nothing to analyze.";
  }
  return "No citation records are available for this paper.";
}

function showAnalysis(analysis, paper) {
  var citations = Array.isArray(analysis.citations) ? analysis.citations : [];

  if (analysis.message) {
    showStatus(analysisMessage, analysis.message, "");
    tableContext.textContent = "";
    showTable([], emptyTableMessage(paper));
    return;
  }

  showStatus(analysisMessage, "", "");
  showSummary(analysis.summary || {});
  tableContext.textContent = citations.length > 0
    ? "Papers cited by " + paper.paper_id + ": " + citations.length + " records."
    : "";
  showTable(citations, "No citation records are available for this paper.");
  if (citations.length > 0) {
    drawGraph(paper.paper_id, citations.map(function (citation) {
      return {
        id: citation.cited,
        citing: citation.citing,
        title: citation.cited_title || "",
        type: citation.type,
        self_citation: citation.self_citation,
        reciprocal_citation: citation.reciprocal_citation,
        shared_authors: citation.shared_authors || [],
        error: citation.error || ""
      };
    }));
  }
}

async function runAnalysis(paper) {
  showStatus(statusLine, "Step 2 of 2: analyzing citations (Self-Citation Hash Table & Reciprocal Graph Search)...", "working");
  reveal(analysisSection, 6, 220);
  showStatus(analysisMessage, "Analyzing citations...", "working");

  try {
    var analysis = await postJson(API_ANALYSIS, { paper_id: paper.paper_id });
    showAnalysis(analysis, paper);
    showStatus(statusLine, "Analysis complete.", "");
  } catch (error) {
    showStatus(analysisMessage, error.message, "error");
    showStatus(statusLine, "The paper was found, but its citations could not be analyzed.", "error");
  }
}

async function handleSubmit(event) {
  event.preventDefault();
  if (isBusy) {
    return;
  }

  var query = normalizeQuery(doiInput.value);
  if (!isValidQuery(query)) {
    showStatus(statusLine, "Please enter a valid DOI (e.g. 10.1038/nature14539) or OpenAlex Work ID (e.g. W2919115771).", "error");
    doiInput.focus();
    return;
  }

  setBusy(true);
  resetResults();
  showStatus(statusLine, "Step 1 of 2: finding paper metadata...", "working");

  try {
    var paper = await postJson(API_ANALYZE, { doi: query });
    if (!paper.paper_id) {
      throw new Error("The server answer did not contain a paper ID.");
    }
    showPaper(paper);
    drawBasicGraph(paper);
    await runAnalysis(paper);
  } catch (error) {
    showStatus(statusLine, error.message, "error");
  }

  setBusy(false);
}

function makeCouplingPaperNode(id, title) {
  var wrap = document.createElement("span");
  wrap.appendChild(makeIdNode(id));
  if (title) {
    wrap.appendChild(textNode(" (" + title + ")"));
  }
  return wrap;
}

function makeCouplingBadge(status) {
  var info = COUPLING_BADGE_INFO[status] || COUPLING_BADGE_INFO["UNKNOWN"];
  var badge = document.createElement("span");
  badge.className = "type-label " + info.cssClass;
  badge.textContent = info.text;
  return badge;
}

function getSelectedComparisonMode() {
  return modeCocitationRadio && modeCocitationRadio.checked ? "cocitation" : "coupling";
}

function updateComparisonMethodHint() {
  if (!comparisonMethodHint) {
    return;
  }
  if (getSelectedComparisonMode() === "cocitation") {
    comparisonMethodHint.textContent =
      "Two distinct papers are co-cited when one or more third-party papers cite both of them (C \u2192 A and C \u2192 B). Exact Jaccard similarity is shown only when complete OpenAlex citing sets are verified.";
  } else {
    comparisonMethodHint.textContent =
      "Two papers are bibliographically coupled when they cite one or more common references (A \u2192 R and B \u2192 R). Jaccard similarity measures reference overlap, not research quality.";
  }
  couplingResults.hidden = true;
  showStatus(couplingStatus, "", "");
  fadeIn(comparisonMethodHint, 160);
}

function showCouplingResult(data) {
  var isUnknown = data.status === "UNKNOWN";
  var jaccardText = isUnknown
    ? "UNVERIFIED"
    : Number(data.jaccard_similarity || 0).toFixed(2) + "%";
  var commonText = isUnknown ? "-" : data.common_count;
  var summaryCss = isUnknown ? "unknown" : (data.status === "YES" ? "self" : "");

  couplingSummaryGrid.innerHTML = "";
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Bibliographic Coupling", css: summaryCss }, data.status)
  );
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Common References", css: "" }, commonText)
  );
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Jaccard Similarity", css: isUnknown ? "unknown" : "reciprocal" }, jaccardText)
  );

  couplingDetails.innerHTML = "";
  addDetailTo(couplingDetails, "Paper A", makeCouplingPaperNode(data.paper_a, data.paper_a_title));
  addDetailTo(couplingDetails, "Paper B", makeCouplingPaperNode(data.paper_b, data.paper_b_title));
  addDetailTo(couplingDetails, "Coupling status", makeCouplingBadge(data.status));

  if (isUnknown) {
    addDetailTo(
      couplingDetails,
      "Notes",
      textNode("Reference metadata is unavailable for one or both papers, so Jaccard similarity cannot be determined.")
    );
  } else {
    addDetailTo(couplingDetails, "Paper A unique refs", textNode(data.unique_references_a));
    addDetailTo(couplingDetails, "Paper B unique refs", textNode(data.unique_references_b));
    addDetailTo(couplingDetails, "Common references", textNode(data.common_count));
    addDetailTo(couplingDetails, "Union references", textNode(data.union_count));
    addDetailTo(couplingDetails, "Jaccard similarity", textNode(jaccardText));
  }

  couplingSharedHeading.textContent = "Shared references (A \u2192 R and B \u2192 R)";
  couplingSharedList.innerHTML = "";
  var shared = Array.isArray(data.shared_references) ? data.shared_references : [];
  couplingSharedWrap.hidden = isUnknown || shared.length === 0;
  shared.forEach(function (ref) {
    var item = document.createElement("li");
    item.appendChild(makeCouplingPaperNode(ref.id, ref.title));
    couplingSharedList.appendChild(item);
  });

  reveal(couplingResults, 6, 220);
}

function showCoCitationResult(data) {
  var isComplete = data.data_complete === true;
  var hasExactJaccard = isComplete && typeof data.jaccard_similarity === "number";
  var jaccardText = hasExactJaccard
    ? Number(data.jaccard_similarity).toFixed(2) + "%"
    : "UNVERIFIED";
  var commonText = (data.status === "UNKNOWN" && data.common_count === null) ? "-" : data.common_count;
  var summaryCss = data.status === "UNKNOWN" ? "unknown" : (data.status === "YES" ? "self" : "");

  couplingSummaryGrid.innerHTML = "";
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Co-Citation", css: summaryCss }, data.status)
  );
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Co-Citing Papers", css: "" }, commonText)
  );
  couplingSummaryGrid.appendChild(
    makeSummaryItem({ label: "Jaccard Similarity", css: hasExactJaccard ? "reciprocal" : "unknown" }, jaccardText)
  );

  couplingDetails.innerHTML = "";
  addDetailTo(couplingDetails, "Paper A", makeCouplingPaperNode(data.paper_a, data.paper_a_title));
  addDetailTo(couplingDetails, "Paper B", makeCouplingPaperNode(data.paper_b, data.paper_b_title));
  addDetailTo(couplingDetails, "Co-citation status", makeCouplingBadge(data.status));
  addDetailTo(
    couplingDetails,
    "Citing data complete",
    textNode(isComplete ? "YES (complete OpenAlex citing sets)" : "NO (partial or unverified citing data)")
  );
  if (Number.isInteger(data.unique_citers_a)) {
    addDetailTo(couplingDetails, "Paper A unique citers", textNode(data.unique_citers_a));
  }
  if (Number.isInteger(data.unique_citers_b)) {
    addDetailTo(couplingDetails, "Paper B unique citers", textNode(data.unique_citers_b));
  }
  if (Number.isInteger(data.common_count)) {
    addDetailTo(couplingDetails, "Common co-citing papers", textNode(data.common_count));
  }
  if (Number.isInteger(data.union_count)) {
    addDetailTo(couplingDetails, "Union citers", textNode(data.union_count));
  }
  addDetailTo(couplingDetails, "Jaccard similarity", textNode(jaccardText));

  if (!isComplete) {
    addDetailTo(
      couplingDetails,
      "Notes",
      textNode("Incoming citing-paper sets are incomplete or truncated, so exact Jaccard similarity is marked UNVERIFIED.")
    );
  }

  couplingSharedHeading.textContent = "Common co-citing papers (C \u2192 A and C \u2192 B)";
  couplingSharedList.innerHTML = "";
  var citers = Array.isArray(data.co_citing_papers) ? data.co_citing_papers : [];
  couplingSharedWrap.hidden = citers.length === 0;
  citers.forEach(function (citer) {
    var item = document.createElement("li");
    item.appendChild(makeCouplingPaperNode(citer.id, citer.title));
    couplingSharedList.appendChild(item);
  });

  reveal(couplingResults, 6, 220);
}

async function handleCouplingSubmit(event) {
  event.preventDefault();
  if (isCouplingBusy) {
    return;
  }

  var mode = getSelectedComparisonMode();
  var idA = normalizeQuery(couplingInputA.value).toUpperCase();
  var idB = normalizeQuery(couplingInputB.value).toUpperCase();
  var workIdRegex = /^W[A-Za-z0-9_]{1,28}$/;

  if (!workIdRegex.test(idA) || !workIdRegex.test(idB)) {
    showStatus(couplingStatus, "Please enter two valid OpenAlex Work IDs (e.g. W2919115771 and W1901616594).", "error");
    return;
  }
  if (mode === "cocitation" && idA === idB) {
    showStatus(couplingStatus, "Paper A and Paper B must have distinct OpenAlex Work IDs for Co-Citation analysis.", "error");
    return;
  }

  isCouplingBusy = true;
  couplingButton.disabled = true;
  couplingButton.textContent = "Comparing...";
  couplingResults.hidden = true;
  showStatus(
    couplingStatus,
    mode === "cocitation"
      ? "Checking incoming citations (OpenAlex and local cache)..."
      : "Comparing outgoing reference sets...",
    "working"
  );

  try {
    if (mode === "cocitation") {
      var cociteResult = await postJson(API_COCITATION, { paper_a: idA, paper_b: idB });
      showCoCitationResult(cociteResult);
    } else {
      var couplingResult = await postJson(API_COUPLING, { paper_a: idA, paper_b: idB });
      showCouplingResult(couplingResult);
    }
    showStatus(couplingStatus, "Comparison complete.", "");
  } catch (error) {
    showStatus(couplingStatus, error.message, "error");
  }

  isCouplingBusy = false;
  couplingButton.disabled = false;
  couplingButton.textContent = "Compare";
}

nodePanelClose.addEventListener("click", closeNodePanel);
nodePanel.addEventListener("keydown", function (event) {
  if (event.key === "Escape") {
    closeNodePanel();
  }
});
if (typeof window !== "undefined" && typeof window.addEventListener === "function") {
  var graphResizeTimer = null;
  window.addEventListener("resize", function () {
    if (!lastGraph) {
      return;
    }
    clearTimeout(graphResizeTimer);
    graphResizeTimer = setTimeout(function () {
      var ring = lastGraph.citedPapers.length <= 16 ? 190 : 225;
      var dot = lastGraph.citedPapers.length > 40 ? 6 : 8;
      if (shouldShowGraphLabels(lastGraph.citedPapers, ring, dot) !== lastGraph.labelsShown) {
        drawGraph(lastGraph.centerId, lastGraph.citedPapers);
      }
    }, 150);
  });
}
searchForm.addEventListener("submit", handleSubmit);
couplingForm.addEventListener("submit", handleCouplingSubmit);
if (modeCouplingRadio) {
  modeCouplingRadio.addEventListener("change", updateComparisonMethodHint);
}
if (modeCocitationRadio) {
  modeCocitationRadio.addEventListener("change", updateComparisonMethodHint);
}
