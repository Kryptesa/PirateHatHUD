import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "compare-reports.py"
SPEC = importlib.util.spec_from_file_location("compare_reports", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def report(checks=None):
  return {"schema_version": 1, "tool": "check-signatures", "status": "ok",
      "checks": checks or []}


class ReportTests(unittest.TestCase):
  def load(self, data):
    with tempfile.TemporaryDirectory() as directory:
      path = Path(directory) / "report.json"
      path.write_text(json.dumps(data), encoding="utf-8")
      return MODULE.load_report(path)

  def test_check_order_does_not_change_evidence(self):
    checks = [{"id": key, "status": "ok", "details": {"rva": "0x1234"}}
         for key in ("treasure", "menu")]
    self.assertEqual([], MODULE.compare(report(checks), report(checks[::-1])))

  def test_nested_changes_missing_checks_and_status(self):
    old = report([{"id": "root", "status": "ok", "details": {"chain": ["0x100", 1]}},
           {"id": "gone", "status": "unknown", "details": None}])
    new = report([{"id": "root", "status": "failed", "details": {"chain": ["0x200", 1]}},
           {"id": "new", "status": "ok", "details": []}])
    output = "\n".join(MODULE.compare(old, new))
    for expected in ('["root"]["details"]["chain"][0]', '"0x100" -> "0x200"',
            '"ok" -> "failed"', '["gone"]: removed', '["new"]: added'):
      self.assertIn(expected, output)

  def test_rejects_invalid_schema(self):
    cases = [[], {}, {**report(), "schema_version": True},
        {**report(), "schema_version": 2}, {**report(), "checks": {}},
        report([{"id": "root", "status": "ok"}]),
        report([{"id": "", "status": "ok", "details": None}]),
        report([{"id": "a", "status": "ok", "details": None}] * 2)]
    for data in cases:
      with self.subTest(data=data), self.assertRaises(ValueError):
        self.load(data)

  def test_duplicate_keys_and_nonstandard_numbers_are_rejected(self):
    with tempfile.TemporaryDirectory() as directory:
      path = Path(directory) / "report.json"
      for raw in ('{"schema_version": 1, "schema_version": 1}', '{"value": NaN}'):
        path.write_text(raw, encoding="utf-8")
        with self.assertRaises(ValueError):
          MODULE.load_report(path)

  def test_different_tools_are_rejected(self):
    with self.assertRaisesRegex(ValueError, "same tool"):
      MODULE.compare(report(), {**report(), "tool": "inspect-ui"})

  def test_type_change_is_not_equal(self):
    self.assertTrue(MODULE.differences(True, 1, "value"))

  def test_cli_and_invalid_json(self):
    with tempfile.TemporaryDirectory() as directory:
      old = Path(directory) / "old.json"
      new = Path(directory) / "new.json"
      old.write_text(json.dumps(report()), encoding="utf-8-sig")
      new.write_text(json.dumps(report()), encoding="utf-8")
      run = subprocess.run([sys.executable, str(SCRIPT), str(old), str(new)],
                capture_output=True, text=True)
      self.assertEqual(0, run.returncode)
      self.assertIn("No reported evidence changed", run.stdout)
      new.write_text("invalid", encoding="utf-8")
      run = subprocess.run([sys.executable, str(SCRIPT), str(old), str(new)],
                capture_output=True, text=True)
      self.assertEqual(2, run.returncode)
      self.assertIn("new.json:", run.stderr)


if __name__ == "__main__":
  unittest.main()
