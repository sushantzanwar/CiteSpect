import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import app as bridge_app


def write_isolated_csvs(workdir):
    data_dir = os.path.join(workdir, "data")
    os.makedirs(data_dir, exist_ok=True)

    with open(os.path.join(data_dir, "papers.csv"), "w", encoding="utf-8") as f:
        f.write("paper_id,title,year,doi\n")
        f.write('W1001,"Main Synthetic Paper",2024,"10.1000/main"\n')
        f.write('W2001,"Cited Self Reciprocal",2023,"10.1000/c1"\n')
        f.write('W2002,"Cited Cross Reciprocal",2023,"10.1000/c2"\n')
        f.write('W2003,"Cited Empty Refs",2022,"10.1000/c3"\n')

    with open(os.path.join(data_dir, "authors.csv"), "w", encoding="utf-8") as f:
        f.write("paper_id,author_id,author_name\n")
        f.write('W1001,A1001,"Alice"\n')
        f.write('W2001,A1001,"Alice"\n')
        f.write('W2002,A2002,"Bob"\n')
        f.write('W2003,A3003,"Carol"\n')

    with open(os.path.join(data_dir, "citations.csv"), "w", encoding="utf-8") as f:
        f.write("citing_paper_id,cited_paper_id\n")
        f.write("W1001,W2001\n")
        f.write("W1001,W2002\n")
        f.write("W1001,W2003\n")
        f.write("W1001,W2001\n")
        f.write("W1001,W1001\n")
        f.write("W2001,W1001\n")
        f.write("W2002,W1001\n")
        f.write("W2003,\n")


class CiteSpectFlaskTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        write_isolated_csvs(self.temp_dir.name)
        self.env_patch = patch.dict(os.environ, {"CITESPECT_WORKDIR": self.temp_dir.name})
        self.env_patch.start()
        self.client = bridge_app.app.test_client()

    def tearDown(self):
        self.env_patch.stop()
        self.temp_dir.cleanup()

    def test_01_valid_cached_analyze_by_work_id_and_doi(self):
        res_id = self.client.post("/api/analyze", json={"doi": "W1001"})
        self.assertEqual(res_id.status_code, 200)
        body_id = res_id.get_json()
        self.assertTrue(body_id["success"])
        self.assertEqual(body_id["paper_id"], "W1001")
        self.assertEqual(body_id["authors"], ["Alice"])
        self.assertEqual(body_id["reference_count"], 5)

        res_doi = self.client.post("/api/analyze", json={"doi": "10.1000/main"})
        self.assertEqual(res_doi.status_code, 200)
        self.assertEqual(res_doi.get_json()["paper_id"], "W1001")

    def test_02_valid_citation_analysis_and_summary_counts(self):
        res = self.client.post("/api/citation-analysis", json={"paper_id": "W1001"})
        self.assertEqual(res.status_code, 200)
        body = res.get_json()
        self.assertTrue(body["success"])
        self.assertEqual(body["paper_id"], "W1001")

        summary = body["summary"]
        self.assertEqual(summary["analyzed"], 3)
        self.assertEqual(summary["self"], 1)
        self.assertEqual(summary["not_self"], 2)
        self.assertEqual(summary["reciprocal_confirmed"], 2)
        self.assertEqual(summary["reciprocal_not"], 1)
        self.assertEqual(summary["reciprocal_unknown"], 0)
        self.assertEqual(summary["self_references"], 1)
        self.assertEqual(summary["duplicates"], 1)
        self.assertEqual(summary["records_found"], 5)

        by_id = {c["cited"]: c for c in body["citations"]}
        self.assertEqual(by_id["W2001"]["type"], "SELF_RECIPROCAL")
        self.assertEqual(by_id["W2001"]["cited_title"], "Cited Self Reciprocal")
        self.assertEqual(by_id["W2002"]["type"], "RECIPROCAL")
        self.assertEqual(by_id["W2003"]["type"], "NOT_SELF")

    def test_03_invalid_work_id(self):
        res = self.client.post("/api/citation-analysis", json={"paper_id": "BAD_ID"})
        self.assertEqual(res.status_code, 400)
        self.assertFalse(res.get_json()["success"])

        res_missing = self.client.post("/api/analyze", json={"doi": "W999999999"})
        self.assertEqual(res_missing.status_code, 404)
        self.assertFalse(res_missing.get_json()["success"])

    def test_04_invalid_doi(self):
        res = self.client.post("/api/analyze", json={"doi": "not-a-valid-doi"})
        self.assertEqual(res.status_code, 502)
        body = res.get_json()
        self.assertFalse(body["success"])
        self.assertIn("OpenAlex request failed", body["error"])

    def test_05_malformed_json(self):
        res_a = self.client.post("/api/analyze", data="{bad json", content_type="application/json")
        self.assertEqual(res_a.status_code, 400)
        self.assertFalse(res_a.get_json()["success"])

        res_c = self.client.post("/api/citation-analysis", data="{bad json", content_type="application/json")
        self.assertEqual(res_c.status_code, 400)
        self.assertFalse(res_c.get_json()["success"])

    def test_06_missing_request_fields(self):
        res_a = self.client.post("/api/analyze", json={})
        self.assertEqual(res_a.status_code, 400)
        self.assertFalse(res_a.get_json()["success"])

        res_c = self.client.post("/api/citation-analysis", json={"paper_id": "  "})
        self.assertEqual(res_c.status_code, 400)
        self.assertFalse(res_c.get_json()["success"])

    def test_07_c_executable_failure(self):
        with patch.object(bridge_app, "EXE_PATH", os.path.join(self.temp_dir.name, "missing.exe")):
            res_a = self.client.post("/api/analyze", json={"doi": "10.1000/uncached"})
            self.assertEqual(res_a.status_code, 500)
            self.assertFalse(res_a.get_json()["success"])

            res_c = self.client.post("/api/citation-analysis", json={"paper_id": "W1001"})
            self.assertEqual(res_c.status_code, 500)
            self.assertFalse(res_c.get_json()["success"])

        res_not_saved = self.client.post("/api/citation-analysis", json={"paper_id": "W888888"})
        self.assertEqual(res_not_saved.status_code, 404)
        self.assertFalse(res_not_saved.get_json()["success"])

    def test_08_c_subprocess_timeout(self):
        with patch.object(bridge_app, "run_c_program", side_effect=subprocess.TimeoutExpired("citespect", 90)):
            res_a = self.client.post("/api/analyze", json={"doi": "10.1000/timeout"})
            self.assertEqual(res_a.status_code, 504)
            self.assertFalse(res_a.get_json()["success"])

        with patch.object(bridge_app, "run_c_analysis", side_effect=subprocess.TimeoutExpired("citespect", 300)):
            res_c = self.client.post("/api/citation-analysis", json={"paper_id": "W1001"})
            self.assertEqual(res_c.status_code, 504)
            self.assertFalse(res_c.get_json()["success"])

    def test_09_malformed_c_output(self):
        with patch.object(bridge_app, "run_c_program", return_value=["GARBAGE_LINE", "NO_RESULT_END"]):
            res_a = self.client.post("/api/analyze", json={"doi": "10.1000/malformed"})
            self.assertEqual(res_a.status_code, 502)
            self.assertFalse(res_a.get_json()["success"])

        with patch.object(bridge_app, "run_c_analysis", return_value=["GARBAGE_OUTPUT"]):
            res_c = self.client.post("/api/citation-analysis", json={"paper_id": "W1001"})
            self.assertEqual(res_c.status_code, 502)
            self.assertFalse(res_c.get_json()["success"])

    def test_10_coupling_yes_and_no_with_real_c_exe(self):
        # W2001 and W2002 both cite W1001 (unique_a=1, unique_b=1, common=1, union=1, Jaccard=100.0%)
        res_yes = self.client.post("/api/coupling", json={"paper_a": "W2001", "paper_b": "W2002"})
        self.assertEqual(res_yes.status_code, 200)
        by = res_yes.get_json()
        self.assertTrue(by["success"])
        self.assertEqual(by["status"], "YES")
        self.assertEqual(by["paper_a"], "W2001")
        self.assertEqual(by["paper_b"], "W2002")
        self.assertEqual(by["paper_a_title"], "Cited Self Reciprocal")
        self.assertEqual(by["paper_b_title"], "Cited Cross Reciprocal")
        self.assertEqual(by["unique_references_a"], 1)
        self.assertEqual(by["unique_references_b"], 1)
        self.assertEqual(by["common_count"], 1)
        self.assertEqual(by["union_count"], 1)
        self.assertEqual(by["jaccard_similarity"], 100.0)
        self.assertEqual(by["shared_references"], [{"id": "W1001", "title": "Main Synthetic Paper"}])

        # W2001 (cites W1001) vs W2003 (verified 0 refs) -> NO, 0.0%
        res_no = self.client.post("/api/coupling", json={"paper_a": "W2001", "paper_b": "W2003"})
        self.assertEqual(res_no.status_code, 200)
        bn = res_no.get_json()
        self.assertTrue(bn["success"])
        self.assertEqual(bn["status"], "NO")
        self.assertEqual(bn["unique_references_a"], 1)
        self.assertEqual(bn["unique_references_b"], 0)
        self.assertEqual(bn["common_count"], 0)
        self.assertEqual(bn["union_count"], 1)
        self.assertEqual(bn["jaccard_similarity"], 0.0)
        self.assertEqual(bn["shared_references"], [])

    def test_11_coupling_unknown_and_error_handling(self):
        with patch.object(
            bridge_app,
            "run_c_coupling",
            return_value=[
                "========== BIBLIOGRAPHIC COUPLING ==========",
                "Paper A: W2001",
                "Paper B: W9999",
                "Bibliographic Coupling: UNKNOWN",
            ],
        ):
            res_unk = self.client.post("/api/coupling", json={"paper_a": "W2001", "paper_b": "W9999"})
        self.assertEqual(res_unk.status_code, 200)
        bu = res_unk.get_json()
        self.assertTrue(bu["success"])
        self.assertEqual(bu["status"], "UNKNOWN")
        self.assertIsNone(bu["jaccard_similarity"])
        self.assertIsNone(bu["common_count"])
        self.assertIsNone(bu["union_count"])

        res_bad = self.client.post("/api/coupling", json={"paper_a": "BAD", "paper_b": "W2001"})
        self.assertEqual(res_bad.status_code, 400)
        self.assertFalse(res_bad.get_json()["success"])

        with patch.object(bridge_app, "run_c_coupling", side_effect=subprocess.TimeoutExpired("citespect", 180)):
            res_to = self.client.post("/api/coupling", json={"paper_a": "W2001", "paper_b": "W2002"})
        self.assertEqual(res_to.status_code, 504)

        with patch.object(bridge_app, "run_c_coupling", return_value=["GARBAGE"]):
            res_mal = self.client.post("/api/coupling", json={"paper_a": "W2001", "paper_b": "W2002"})
        self.assertEqual(res_mal.status_code, 502)


if __name__ == "__main__":
    unittest.main(verbosity=2)
