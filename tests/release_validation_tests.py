import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("check_release", Path(__file__).resolve().parents[1] / "tools/release/check_release.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReleaseValidationTests(unittest.TestCase):
  def test_versions_and_release_intent(self):
    with tempfile.TemporaryDirectory() as folder:
      root = Path(folder)
      contents = {
        "CMakeLists.txt": "project(PirateHatHUD VERSION 1.2.3 LANGUAGES CXX RC)\n",
        "README.md": "**Current version: 1.2.3**\n",
        "CHANGELOG.md": "# Changelog\n\n## Unreleased\n\n## 1.2.3 - date\n",
        "release_notes.md": "# Pirate Hat HUD 1.2.3\n\nNotes\n",
      }
      for name, text in contents.items():
        (root / name).write_text(text, encoding="utf-8")
      self.assertEqual(module.validate(root, "v1.2.3", "Release 1.2.3"), "1.2.3")
      for tag in ("1.2.3", "v01.2.3", "v1.2.3-beta", "v1.2.4"):
        with self.subTest(tag=tag), self.assertRaises(ValueError):
          module.validate(root, tag, "Release 1.2.3")
      with self.assertRaises(ValueError):
        module.validate(root, "v1.2.3", "Add a feature")
      for name, text in contents.items():
        (root / name).write_text(text.replace("1.2.3", "1.2.2"), encoding="utf-8")
        with self.subTest(file=name), self.assertRaises(ValueError):
          module.validate(root, "v1.2.3", "Release 1.2.3")
        (root / name).write_text(text, encoding="utf-8")
      (root / "release_notes.md").unlink()
      with self.assertRaises(OSError):
        module.validate(root, "v1.2.3", "Release 1.2.3")


if __name__ == "__main__":
  unittest.main()
