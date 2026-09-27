"""Validate a release tag against the versioned files and commit subject."""
import argparse
from pathlib import Path
import re


def validate(root: Path, tag: str, subject: str) -> str:
  if not re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", tag):
    raise ValueError("Tag must be vX.Y.Z without leading zeros")
  version = tag[1:]
  if subject != f"Release {version}":
    raise ValueError(f"Release commit subject must be exactly: Release {version}")
  expectations = {
    "CMakeLists.txt": rf"project\(PirateHatHUD\s+VERSION\s+{re.escape(version)}\s",
    "README.md": rf"\*\*Current version: {re.escape(version)}\*\*",
    "CHANGELOG.md": rf"^## {re.escape(version)}(?:\s|$)",
    "release_notes.md": rf"\A# Pirate Hat HUD {re.escape(version)}\s*(?:\n|$)",
  }
  for name, pattern in expectations.items():
    if not re.search(pattern, (root / name).read_text(encoding="utf-8"), re.MULTILINE):
      raise ValueError(f"{name} does not declare release version {version}")
  return version


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--tag", required=True)
  parser.add_argument("--subject", required=True)
  parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
  args = parser.parse_args()
  try:
    print(f"Release metadata valid: {validate(args.root, args.tag, args.subject)}")
  except (ValueError, OSError) as error:
    parser.exit(1, f"{error}\n")
