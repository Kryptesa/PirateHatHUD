#!/usr/bin/env python3
"""Compare diagnostic evidence; changes are not a compatibility verdict."""

import argparse
import json
import sys
from pathlib import Path


def unique_object(pairs):
  result = {}
  for key, value in pairs:
    if key in result:
      raise ValueError(f"duplicate JSON key: {key}")
    result[key] = value
  return result


def reject_constant(value):
  raise ValueError(f"invalid JSON constant: {value}")


def load_report(path):
  try:
    report = json.loads(
      Path(path).read_text(encoding="utf-8-sig"),
      object_pairs_hook=unique_object,
      parse_constant=reject_constant)
    if not isinstance(report, dict) or type(report.get("schema_version")) is not int:
      raise ValueError("schema_version must be integer 1")
    if report["schema_version"] != 1:
      raise ValueError("unsupported schema_version (expected 1)")
    if not isinstance(report.get("tool"), str) or not report["tool"].strip():
      raise ValueError("tool must be a nonempty string")
    if not isinstance(report.get("status"), str) or not report["status"].strip():
      raise ValueError("status must be a nonempty string")
    if not isinstance(report.get("checks"), list):
      raise ValueError("checks must be an array")
    ids = set()
    for index, check in enumerate(report["checks"]):
      if not isinstance(check, dict):
        raise ValueError(f"checks[{index}] must be an object")
      for key in ("id", "status"):
        if not isinstance(check.get(key), str) or not check[key].strip():
          raise ValueError(f"checks[{index}].{key} must be a nonempty string")
      if "details" not in check:
        raise ValueError(f"checks[{index}].details is required")
      if check["id"] in ids:
        raise ValueError(f"duplicate check id: {check['id']}")
      ids.add(check["id"])
    return report
  except (OSError, UnicodeError, ValueError) as error:
    raise ValueError(f"{Path(path).name}: {error}") from error


def display(value):
  return json.dumps(value, ensure_ascii=False, sort_keys=True)


def differences(before, after, path):
  if type(before) is not type(after):
    return [f"{path}: {display(before)} -> {display(after)}"]
  if isinstance(before, dict):
    result = []
    for key in sorted(before.keys() | after.keys()):
      child = f"{path}[{display(key)}]"
      if key not in before:
        result.append(f"{child}: added {display(after[key])}")
      elif key not in after:
        result.append(f"{child}: removed {display(before[key])}")
      else:
        result.extend(differences(before[key], after[key], child))
    return result
  if isinstance(before, list):
    result = []
    for index in range(max(len(before), len(after))):
      child = f"{path}[{index}]"
      if index >= len(before):
        result.append(f"{child}: added {display(after[index])}")
      elif index >= len(after):
        result.append(f"{child}: removed {display(before[index])}")
      else:
        result.extend(differences(before[index], after[index], child))
    return result
  return [] if before == after else [f"{path}: {display(before)} -> {display(after)}"]


def compare(before, after):
  if before["tool"] != after["tool"]:
    raise ValueError("reports must come from the same tool")
  old = {check["id"]: check for check in before["checks"]}
  new = {check["id"]: check for check in after["checks"]}
  result = differences(
    {k: v for k, v in before.items() if k != "checks"},
    {k: v for k, v in after.items() if k != "checks"}, "report")
  result.extend(differences(old, new, "checks"))
  return result


def main(argv=None):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("before", help="baseline JSON report")
  parser.add_argument("after", help="new JSON report from the same tool")
  args = parser.parse_args(argv)
  try:
    changes = compare(load_report(args.before), load_report(args.after))
  except ValueError as error:
    print(f"Error: {error}", file=sys.stderr)
    return 2
  print("Evidence comparison only; in-game behavior still requires validation.")
  print("\n".join(changes) if changes else "No reported evidence changed.")
  return 0


if __name__ == "__main__":
  sys.exit(main())
