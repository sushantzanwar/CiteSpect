import csv
import os
import re
import subprocess
import threading

from flask import Flask, jsonify, request

c_program_lock = threading.Lock()

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BACKEND_DIR = os.path.join(BASE_DIR, "..", "backend")
FRONTEND_DIR = os.path.join(BASE_DIR, "..", "frontend")
EXE_PATH = os.path.join(DEFAULT_BACKEND_DIR, "citespect.exe")

app = Flask(__name__, static_folder=FRONTEND_DIR, static_url_path="")


def get_workdir():
    return os.environ.get("CITESPECT_WORKDIR", DEFAULT_BACKEND_DIR)


@app.route("/")
def home():
    return app.send_static_file("index.html")


def run_c_program(doi):
    workdir = get_workdir()
    with c_program_lock:
        result = subprocess.run(
            [EXE_PATH, "--doi", doi],
            cwd=workdir,
            capture_output=True,
            timeout=90,
        )
    return result.stdout.decode("utf-8", errors="replace").splitlines()


def run_c_analysis(paper_id):
    workdir = get_workdir()
    menu_input = "4\n" + paper_id + "\n5\n"
    with c_program_lock:
        result = subprocess.run(
            [EXE_PATH],
            input=menu_input.encode("utf-8"),
            cwd=workdir,
            capture_output=True,
            timeout=300,
        )
    return result.stdout.decode("utf-8", errors="replace").splitlines()


def run_c_coupling(paper_a_id, paper_b_id):
    workdir = get_workdir()
    with c_program_lock:
        result = subprocess.run(
            [EXE_PATH, "--coupling", paper_a_id, paper_b_id],
            cwd=workdir,
            capture_output=True,
            timeout=180,
        )
    return result.stdout.decode("utf-8", errors="replace").splitlines()


def parse_output(lines):
    data = {"authors": [], "references": [], "citation_graph": []}
    inside = False

    for line in lines:
        line = line.strip()
        if line == "RESULT_START":
            inside = True
        elif line == "RESULT_END":
            inside = False
        elif inside and "=" in line:
            key, value = line.split("=", 1)
            if key == "AUTHOR":
                data["authors"].append(value)
            elif key == "REFERENCE":
                data["references"].append(value)
            elif key == "GRAPH_EDGE":
                data["citation_graph"].append(value)
            else:
                data[key.lower()] = value
    return data


def load_cached_paper_from_csv(query):
    """Look up a paper already stored in local CSVs by DOI or OpenAlex Work ID."""
    workdir = get_workdir()
    papers_csv = os.path.join(workdir, "data", "papers.csv")
    authors_csv = os.path.join(workdir, "data", "authors.csv")
    citations_csv = os.path.join(workdir, "data", "citations.csv")

    if not os.path.exists(papers_csv):
        return None

    matched_row = None
    query_lower = query.lower()
    with open(papers_csv, "r", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pid = (row.get("paper_id") or "").strip()
            pdoi = (row.get("doi") or "").strip()
            if pid.lower() == query_lower or (pdoi and pdoi.lower() == query_lower):
                matched_row = row
                break

    if not matched_row:
        return None

    paper_id = matched_row["paper_id"].strip()
    authors = []
    if os.path.exists(authors_csv):
        with open(authors_csv, "r", encoding="utf-8", errors="replace") as f:
            for row in csv.DictReader(f):
                if (row.get("paper_id") or "").strip() == paper_id:
                    name = (row.get("author_name") or "").strip()
                    if name:
                        authors.append(name)

    references = []
    if os.path.exists(citations_csv):
        with open(citations_csv, "r", encoding="utf-8", errors="replace") as f:
            for row in csv.DictReader(f):
                if (row.get("citing_paper_id") or "").strip() == paper_id:
                    cited = (row.get("cited_paper_id") or "").strip()
                    if cited:
                        references.append(cited)

    try:
        year = int(matched_row.get("year") or 0)
    except ValueError:
        year = 0

    return {
        "success": True,
        "paper_id": paper_id,
        "title": matched_row.get("title", ""),
        "year": year,
        "doi": matched_row.get("doi", ""),
        "authors": authors,
        "reference_count": len(references),
        "references": references,
        "graph_paper": paper_id,
        "citation_graph": references,
        "save_status": 1,
    }


@app.route("/api/analyze", methods=["POST"])
def analyze():
    body = request.get_json(silent=True) or {}
    doi = str(body.get("doi", "")).strip()
    if doi == "":
        return jsonify({"success": False, "error": "Please enter a DOI or OpenAlex Work ID."}), 400
    if not os.path.exists(EXE_PATH):
        return jsonify({"success": False, "error": "citespect.exe not found. Compile the C project first."}), 500

    # If the user entered an OpenAlex Work ID (W...) or a cached synthetic DOI, check local CSVs first
    if re.fullmatch(r"W\d{1,20}", doi, re.IGNORECASE):
        cached = load_cached_paper_from_csv(doi.upper())
        if cached:
            return jsonify(cached)
        return jsonify({"success": False, "error": f"Paper {doi.upper()} is not in local CSV storage. Add it using its DOI first."}), 404

    cached = load_cached_paper_from_csv(doi)
    if cached:
        return jsonify(cached)

    try:
        lines = run_c_program(doi)
    except subprocess.TimeoutExpired:
        return jsonify({"success": False, "error": "OpenAlex request failed (timeout)."}), 504

    for line in lines:
        if line.startswith("ERROR="):
            return jsonify({"success": False, "error": "OpenAlex request failed: " + line[6:]}), 502

    data = parse_output(lines)
    if "paper_id" not in data:
        return jsonify({"success": False, "error": "The C program returned no result."}), 502

    return jsonify({
        "success": True,
        "paper_id": data["paper_id"],
        "title": data.get("title", ""),
        "year": int(data.get("year", 0)),
        "doi": data.get("doi", ""),
        "authors": data["authors"],
        "reference_count": int(data.get("reference_count", 0)),
        "references": data["references"],
        "graph_paper": data.get("graph_paper", data["paper_id"]),
        "citation_graph": data["citation_graph"],
        "save_status": int(data.get("save_status", 2)),
    })


SUMMARY_LABELS = {
    "Citation records found": "records_found",
    "Total Citations Analyzed": "analyzed",
    "Citations Checked": "analyzed",
    "Self Citations": "self",
    "Confirmed Reciprocal Citations": "reciprocal_confirmed",
    "Not Reciprocal": "reciprocal_not",
    "Reciprocal Unknown": "reciprocal_unknown",
    "Not Identified": "not_self",
    "Self-references": "self_references",
    "Cites its own Work ID": "self_references",
    "Duplicate records": "duplicates",
}


def value_after_colon(text):
    return text.split(":", 1)[1].strip()


def new_citation(citing_id=""):
    return {
        "citing": citing_id,
        "cited": "",
        "self_citation": "NOT IDENTIFIED AS SELF",
        "reciprocal_citation": "UNKNOWN",
        "type": "NOT_SELF",
        "shared_authors": [],
        "reason": "",
        "error": "",
    }


def read_shared_author(text):
    val = value_after_colon(text)
    match = re.match(r"^(\S+)\s+\((.*)\)$", val)
    if match:
        return {"id": match.group(1), "name": match.group(2)}
    return {"id": val, "name": ""}


def compute_combined_type(cit):
    if cit["error"]:
        return "FAILED"
    if cit["self_citation"] == "SELF" and cit["reciprocal_citation"] == "CONFIRMED RECIPROCAL":
        return "SELF_RECIPROCAL"
    if cit["self_citation"] == "SELF":
        return "SELF"
    if cit["reciprocal_citation"] == "CONFIRMED RECIPROCAL":
        return "RECIPROCAL"
    if cit["reciprocal_citation"] == "UNKNOWN":
        return "UNKNOWN"
    return "NOT_SELF"


def parse_analysis(lines):
    citations = []
    summary = {}
    citing_paper = ""
    current = None

    for line in lines:
        text = line.strip()
        if text.startswith("Citing Paper:"):
            citing_paper = value_after_colon(text)
        elif text.startswith("Cited Paper:"):
            current = new_citation(citing_paper)
            current["cited"] = value_after_colon(text)
        elif current is not None and text.startswith("Self Citation:"):
            raw_self = value_after_colon(text)
            fail_match = re.match(r"^NOT IDENTIFIED AS SELF\s*\(metadata retrieval failed:\s*(.+)\)$", raw_self)
            if fail_match:
                current["self_citation"] = "FAILED"
                current["error"] = fail_match.group(1)
            else:
                current["self_citation"] = raw_self
        elif current is not None and text.startswith("Reciprocal Citation:"):
            current["reciprocal_citation"] = value_after_colon(text)
        elif current is not None and text.startswith("Shared Author"):
            current["shared_authors"].append(read_shared_author(text))
        elif text.startswith("-----") and current is not None:
            current["type"] = compute_combined_type(current)
            citations.append(current)
            current = None
        else:
            match = re.match(r"^(.+?)\s*:\s*(\d+)", text)
            if match:
                label = match.group(1).strip()
                if label in SUMMARY_LABELS:
                    summary[SUMMARY_LABELS[label]] = int(match.group(2))

    if "analyzed" in summary:
        analyzed = summary["analyzed"]
        self_count = summary.get("self", 0)
        summary.setdefault("not_self", max(0, analyzed - self_count))
        self_refs = summary.get("self_references", 0)
        dups = summary.get("duplicates", 0)
        summary.setdefault("records_found", analyzed + self_refs + dups)

    return citations, summary


def load_local_titles():
    """Return a {paper_id: title} map from local papers.csv without network calls."""
    papers_csv = os.path.join(get_workdir(), "data", "papers.csv")
    titles = {}
    if not os.path.exists(papers_csv):
        return titles
    with open(papers_csv, "r", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            pid = (row.get("paper_id") or "").strip()
            title = (row.get("title") or "").strip()
            if pid and title:
                titles[pid] = title
    return titles


@app.route("/api/citation-analysis", methods=["POST"])
def citation_analysis():
    body = request.get_json(silent=True) or {}
    paper_id = str(body.get("paper_id", "")).strip()
    if not re.fullmatch(r"W[A-Za-z0-9_]{1,28}", paper_id):
        return jsonify({"success": False, "error": "A valid OpenAlex Work ID (like W2919115771) is required."}), 400
    if not os.path.exists(EXE_PATH):
        return jsonify({"success": False, "error": "citespect.exe not found. Compile the C project first."}), 500

    try:
        lines = run_c_analysis(paper_id)
    except subprocess.TimeoutExpired:
        return jsonify({"success": False, "error": "The analysis took too long (more than 5 minutes)."}), 504

    citations, summary = parse_analysis(lines)
    local_titles = load_local_titles()
    for citation in citations:
        citation["citing"] = citation["citing"] or paper_id
        citation["cited_title"] = local_titles.get(citation["cited"], "")
    if "analyzed" in summary or "records_found" in summary:
        return jsonify({"success": True, "paper_id": paper_id, "summary": summary, "citations": citations})

    for line in lines:
        no_records = re.search(r"No citation records found.*", line)
        not_saved = re.search(r"Paper \S+ is not in .*", line)
        if no_records:
            return jsonify({"success": True, "paper_id": paper_id, "summary": {}, "citations": [], "message": no_records.group(0)})
        if not_saved:
            return jsonify({"success": False, "error": not_saved.group(0)}), 404
    return jsonify({"success": False, "error": "The C program returned no analysis result."}), 502


def parse_coupling(lines):
    data = {
        "paper_a": "",
        "paper_b": "",
        "status": "",
        "unique_references_a": None,
        "unique_references_b": None,
        "common_count": None,
        "union_count": None,
        "jaccard_similarity": None,
        "shared_references": [],
    }
    int_keys = {
        "Paper A Unique References": "unique_references_a",
        "Paper B Unique References": "unique_references_b",
        "Common References": "common_count",
        "Union References": "union_count",
    }

    for line in lines:
        text = line.strip()
        if ":" not in text:
            continue
        label, raw_val = [part.strip() for part in text.split(":", 1)]
        if label == "Paper A":
            data["paper_a"] = raw_val
        elif label == "Paper B":
            data["paper_b"] = raw_val
        elif label == "Bibliographic Coupling":
            data["status"] = raw_val
        elif label in int_keys:
            try:
                data[int_keys[label]] = int(raw_val)
            except ValueError:
                pass
        elif label == "Jaccard Similarity":
            try:
                data["jaccard_similarity"] = float(raw_val.rstrip("%").strip())
            except ValueError:
                pass
        elif label == "Shared Reference" and raw_val:
            data["shared_references"].append(raw_val)

    if data["status"] == "UNKNOWN":
        data["unique_references_a"] = None
        data["unique_references_b"] = None
        data["common_count"] = None
        data["union_count"] = None
        data["jaccard_similarity"] = None
        data["shared_references"] = []

    return data


@app.route("/api/coupling", methods=["POST"])
def coupling_analysis():
    body = request.get_json(silent=True) or {}
    paper_a = str(body.get("paper_a", "")).strip().upper()
    paper_b = str(body.get("paper_b", "")).strip().upper()
    work_id_pattern = r"W[A-Za-z0-9_]{1,28}"

    if not re.fullmatch(work_id_pattern, paper_a) or not re.fullmatch(work_id_pattern, paper_b):
        return jsonify({
            "success": False,
            "error": "Two valid OpenAlex Work IDs (like W2919115771 and W1901616594) are required.",
        }), 400
    if not os.path.exists(EXE_PATH):
        return jsonify({"success": False, "error": "citespect.exe not found. Compile the C project first."}), 500

    try:
        lines = run_c_coupling(paper_a, paper_b)
    except subprocess.TimeoutExpired:
        return jsonify({"success": False, "error": "The bibliographic coupling comparison took too long."}), 504

    parsed = parse_coupling(lines)
    if parsed["status"] not in ("YES", "NO", "UNKNOWN"):
        return jsonify({"success": False, "error": "The C program returned no bibliographic coupling result."}), 502

    local_titles = load_local_titles()
    return jsonify({
        "success": True,
        "paper_a": parsed["paper_a"] or paper_a,
        "paper_a_title": local_titles.get(parsed["paper_a"] or paper_a, ""),
        "paper_b": parsed["paper_b"] or paper_b,
        "paper_b_title": local_titles.get(parsed["paper_b"] or paper_b, ""),
        "status": parsed["status"],
        "unique_references_a": parsed["unique_references_a"],
        "unique_references_b": parsed["unique_references_b"],
        "common_count": parsed["common_count"],
        "union_count": parsed["union_count"],
        "jaccard_similarity": parsed["jaccard_similarity"],
        "shared_references": [
            {"id": rid, "title": local_titles.get(rid, "")}
            for rid in parsed["shared_references"]
        ],
    })


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000)
