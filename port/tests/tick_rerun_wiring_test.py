"""Tick re-run (experimental): off unless MELEE_TICK_REDO=1, never in an online match, a pending one, replay
playback or under the console rollback, only while an offline VS match is running, and only when the
tick's first picture is still waiting in its frame buffer (so the scene loop never waits inside the copy)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8-sig").replace("\r\n", "\n")


class TickRerunWiringTest(unittest.TestCase):
    def test_off_without_the_switch(self):
        host = source("port/runtime/host/host.cpp")
        self.assertIn('static const bool g_redo_on = [] { const char* v = std::getenv("MELEE_TICK_REDO"); return v && *v == \'1\'; }();', host)
        self.assertIn("static bool redo_watching() { return (g_redo_on && g_redo_armed && !g_redo_active) || input_midtick_test(); }", host)
        app = source("port/app/source_host.cpp")
        self.assertIn("if (!host::tick_redo_enabled()) return 0;", app)
        shim = source("sourceport/game/shim/mu_redo.c")
        self.assertIn('const char* v = getenv("MELEE_TICK_REDO");', shim)

    def test_never_online_or_in_playback(self):
        shim = source("sourceport/game/shim/mu_redo.c")
        self.assertIn("return !mu_online_active() && !mu_online_pending() && !mu_console_rollback_active() && !mu_replay_abi_active();", shim)
        gate = shim[shim.index("int mu_redo_gate(void)"):shim.index("/* Top of an engine body")]
        self.assertIn("!redo_on() || !redo_armed || !redo_allowed() || redo_pending_buffer() < 0", gate)
        online = source("sourceport/game/shim/mu_online.c")
        # Offline only: the gate is asked where the online hooks are off.
        block = online[online.index("int mu_online_engine_gate(int* count)"):online.index("mu_online.loop_entered = 1;")]
        self.assertIn("if (!hooks_on()) {\n        int mu_redo_gate(void);\n        if (*count == 0 && mu_redo_gate()) {", block)

    def test_only_a_running_offline_vs_match(self):
        app = source("port/app/source_host.cpp")
        self.assertIn("const bool playing = major == 2 && minor == 2 && match_frame > 60 && (rerun || match_frame != last_match_frame);", app)

    def test_goes_back_to_the_ticks_own_start(self):
        app = source("port/app/source_host.cpp")
        self.assertIn("if (!have_start || !g_savestates.active() || !g_savestates.load_keep(start_id)) {", app)
        self.assertIn("if (!rerun) { have_start = have_end; start_id = end_id; }", app)

    def test_host_stands_still_while_a_tick_runs_again(self):
        app = source("port/app/source_host.cpp")
        self.assertIn("if (host::tick_redo_active()) return;   // a tick being run again: no time passes, nothing is due", app)
        self.assertIn("if (!host::tick_redo_presleep()) return;", app)

    def test_sounds_of_a_tick_run_twice_are_adopted(self):
        audio = source("sourceport/game/shim/mu_online_audio.c")
        self.assertIn("return mu_online_active() || mu_console_rollback_active() || mu_redo_tracking();", audio)

    def test_held_frames_are_reported(self):
        self.assertIn("host::note_frame_handover();", source("port/runtime/gx/gx_core.cpp"))


if __name__ == "__main__":
    unittest.main()
