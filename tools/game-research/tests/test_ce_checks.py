"""CE diagnostic regressions using synthetic PE memory (requires lupa Lua 5.3)."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build/game-research-test-tools"))
try:
  from lupa.lua53 import LuaRuntime
except ImportError:
  LuaRuntime = None


@unittest.skipIf(LuaRuntime is None, "Install lupa to exercise the fake CE runtime")
class CheckTests(unittest.TestCase):
  def setUp(self):
    self.lua = LuaRuntime(unpack_returned_tuples=True)
    self.lua.execute("""
     GAME_RESEARCH_CONFIG = {}; memory = {}; unreadable = false
     function put(address, value, width)
      for i = 0, width - 1 do
       memory[address + i] = value % 256; value = math.floor(value / 256)
      end
     end
     function bytes(address, values)
      for i, value in ipairs(values) do memory[address + i - 1] = value end
     end
     function getAddressSafe(name) return 0x100000 end
     function readBytes(address, count, as_table)
      if unreadable and address >= 0x102000 then return nil end
      local result = {}
      for i = 0, count - 1 do result[i + 1] = memory[address + i] or 0 end
      if as_table then return result else return result[1] end
     end
     function integer(address, width)
      local result = 0
      for i = width - 1, 0, -1 do result = result * 256 + (memory[address + i] or 0) end
      return result
     end
     function readInteger(address) return integer(address, 4) end
     function readSmallInteger(address) return integer(address, 2) end
     function readPointer(address) return integer(address, 8) end
     put(0x100000, 0x5A4D, 2); put(0x10003C, 0x80, 4)
     put(0x100080, 0x4550, 4); put(0x100086, 2, 2)
     put(0x100094, 0xF0, 2); put(0x100098, 0x20B, 2)
     put(0x1000D0, 0x6000, 4)
     -- Two adjacent executable sections, each only 0x400 bytes long.
     for i = 0, 1 do
      local header = 0x100188 + i * 40
      put(header + 8, 0x400, 4); put(header + 12, 0x2000 + i * 0x400, 4)
      put(header + 36, 0x60000020, 4)
     end
     function treasure(address)
      bytes(address, {0xFF,0x46,0x08}); bytes(address + 0x2C, {0x83,0x6E,0x08,1})
     end
     function menu(address)
      bytes(address, {0xC6,0x81,0x5B,2,0,0,0,0x84,0xD2,0x74,0x1C})
      bytes(address + 0x141, {0xC6,0x83,0x5B,2,0,0,1,0x48,0x8B,1,0xFF,0x50,0x30})
     end
    """)
    self.lua.globals().GAME_RESEARCH_CONFIG.checkout = str(ROOT)

  def run_tool(self, name="check-signatures"):
    return json.loads(self.lua.eval("dofile")(str(ROOT / f"tools/game-research/ce/{name}.lua")))

  def test_unique_pairs(self):
    self.lua.execute("treasure(0x102010); menu(0x102100)")
    report = self.run_tool()
    self.assertEqual(report["status"], "pass")
    self.assertEqual(report["checks"][0]["details"]["matches"][0]["enter_rva"], "0x2010")

  def test_missing_and_ambiguous(self):
    self.assertEqual(self.run_tool()["checks"][0]["details"]["candidate_pairs"], 0)
    self.lua.execute("treasure(0x102010); treasure(0x102410); treasure(0x102480)")
    details = self.run_tool()["checks"][0]["details"]
    self.assertEqual(details["candidate_pairs"], 2)
    self.assertTrue(details["count_is_lower_bound"])

  def test_no_pair_crossing_section_boundary(self):
    self.lua.execute("treasure(0x1023E0)")
    self.assertEqual(self.run_tool()["checks"][0]["details"]["candidate_pairs"], 0)

  def test_unreadable_section(self):
    self.lua.execute("unreadable = true")
    self.assertEqual(self.run_tool()["status"], "unknown")

  def test_source_parse_failure(self):
    self.lua.execute("""
     local open = io.open
     io.open = function(path, mode)
      if path:match('patterns.hpp$') then
       return {read = function() return 'unsupported source' end, close = function() end}
      end
      return open(path, mode)
     end
    """)
    self.assertEqual(self.run_tool()["status"], "unknown")

  def test_ui_null_root_is_unknown(self):
    report = self.run_tool("inspect-ui")
    self.assertEqual(report["status"], "unknown")
    self.assertEqual(report["baseline_version"], "2.03.02")

  def make_ui(self):
    self.lua.execute("""
     put(0x100000 + 0x6C8CC00, 0x200000, 8)
     local owner = 0x200000
     for _, offset in ipairs({0x30, 0x18, 0x88, 0x78, 0}) do
      put(owner + offset, owner + 0x1000, 8); owner = owner + 0x1000
     end
     put(owner + 0x30EB8, 0x300000, 8); put(owner + 0x30EC0, 2, 4)
     for i = 0, 1 do
      local entry = 0x400000 + i * 0x1000
      local node = entry + 0x200; local root = entry + 0x400; local script = entry + 0x800
      put(0x300000 + i * 8, entry, 8); put(entry + 0xA0, node, 8)
      put(node + 0x10, root, 8); put(root + 0x118, script, 8); put(script, 0x101800, 8)
      put(root + 0x25B, 0, 1)
     end
     put(0x1017F8, 0x101900, 8); put(0x101900, 1, 4)
     put(0x10190C, 0x1A00, 4); put(0x101914, 0x1900, 4)
     local name = '.?AVUIGamePlayControl_Root_MainMenu@uiCommonScript@pa@@'
     for i = 1, #name do put(0x101A10 + i - 1, name:byte(i), 1) end
    """)

  def test_duplicate_menu_rtti_is_unknown(self):
    self.make_ui()
    check = next(c for c in self.run_tool("inspect-ui")["checks"] if c["id"] == "menu-identity")
    self.assertEqual(check["status"], "unknown")
    self.assertEqual(check["details"]["candidate_count"], 2)

  def test_unreadable_array_invalidates_menu_uniqueness(self):
    self.make_ui()
    self.lua.execute("""
     local read = readPointer
     readPointer = function(address)
      if address == 0x300008 then return nil end
      return read(address)
     end
    """)
    check = next(c for c in self.run_tool("inspect-ui")["checks"] if c["id"] == "menu-identity")
    self.assertEqual(check["details"]["candidate_count"], 1)
    self.assertFalse(check["details"]["complete"])
    self.assertEqual(check["status"], "unknown")

  def test_invalid_menu_state(self):
    self.make_ui()
    self.lua.execute("put(0x300008, 0, 8); put(0x400400 + 0x25B, 7, 1)")
    check = next(c for c in self.run_tool("inspect-ui")["checks"] if c["id"] == "ui-objects")
    self.assertEqual(check["details"]["objects"][0]["status"], "unknown")
    self.assertEqual(check["details"]["objects"][0]["field_address"], "0x40065B")

  def test_json_empty_array_and_escape(self):
    encoder = self.lua.eval("dofile")(str(ROOT / "tools/game-research/ce/json.lua"))
    self.assertEqual(json.loads(encoder.encode(encoder.array())), [])
    self.assertEqual(json.loads(encoder.encode('a\n"\\')), 'a\n"\\')


if __name__ == "__main__":
  unittest.main()
