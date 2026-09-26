"""Read-only recorder regression tests using a fake CE runtime (requires lupa)."""
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


@unittest.skipIf(LuaRuntime is None, "Optional lupa Lua 5.3 runtime is unavailable")
class TransitionTests(unittest.TestCase):
  def setUp(self):
    self.lua = LuaRuntime(unpack_returned_tuples=True)
    self.lua.execute("""
     tick = 0; byte = 0; unreadable = false; fail_clock = false
     timers = {}; destroyed = 0
     function getTickCount()
      if fail_clock then error('clock failure') end
      return tick
     end
     function getAddressSafe(address) return 4096 end
     function readBytes(address, count, as_table)
      if unreadable then return nil end
      local bytes = {}; for i = 1, count do bytes[i] = byte end
      return bytes
     end
     function createTimer(owner, enabled)
      local timer = {Enabled = enabled}
      timer.destroy = function() destroyed = destroyed + 1 end
      timers[#timers + 1] = timer
      return timer
     end
     function advance(ms)
      tick = tick + ms
      local timer = timers[#timers]
      if timer and timer.Enabled and timer.OnTimer then timer.OnTimer() end
     end
    """)
    self.api = self.lua.eval("dofile")(
      str(ROOT / "tools/game-research/ce/capture-transitions.lua")
    )

  def start(self, **options):
    options["fields"] = self.lua.table_from([
      self.lua.table_from({"label": "candidate", "address": 4096})
    ])
    return self.api.start(self.lua.table_from(options))

  def report(self):
    return json.loads(self.api.result())

  def test_changes_unknown_and_completion(self):
    self.start(duration_ms=300)
    self.lua.execute("byte = 1; advance(100); unreadable = true; advance(100)")
    self.lua.execute("advance(100)")
    report = self.report()
    self.assertEqual(report["status"], "completed")
    self.assertEqual([e["current"] for e in report["events"]],
            ["00", "01", "unknown"])
    self.assertEqual(report["checks"][0]["status"], "unknown")
    self.assertEqual(self.lua.globals().destroyed, 1)
    self.assertIsNone(self.lua.globals().timers[1].OnTimer)

  def test_stop_restart_handle_and_reload(self):
    first = self.start()
    self.start()
    self.assertEqual(first.status(), "stopped")
    first.stop()
    self.assertEqual(self.api.status(), "running")
    self.lua.eval("dofile")(str(ROOT / "tools/game-research/ce/capture-transitions.lua"))
    self.assertEqual(self.api.status(), "stopped")
    self.assertEqual(self.lua.globals().destroyed, 2)

  def test_timer_error_cleanup(self):
    self.start()
    self.lua.execute("fail_clock = true; advance(100)")
    self.assertEqual(self.report()["status"], "error")
    self.assertEqual(self.lua.globals().destroyed, 1)

  def test_sample_limit_and_bound_validation(self):
    self.start(max_samples=1)
    self.assertEqual(self.report()["stop_reason"], "sample_limit")
    self.assertEqual(len(self.report()["samples"]), 1)
    self.assertEqual(len(self.lua.globals().timers), 0)
    with self.assertRaises(Exception):
      self.start(duration_ms=120001)
    with self.assertRaises(Exception):
      self.start(interval_ms=19)

  def test_tick_wrap(self):
    self.lua.execute("tick = 4294967290")
    self.start(duration_ms=100)
    self.lua.execute("tick = 5; advance(0)")
    self.assertEqual(self.report()["samples"][1]["elapsed_ms"], 11)
    self.api.stop()


if __name__ == "__main__":
  unittest.main()
