"""Automatic V-Sync (never chosen by the player): a launch starts with V-Sync off when the last session
found G-Sync or FreeSync, instead of on until it is found again. A choice in the panel still wins, and
a display found to be fixed turns V-Sync back on and is remembered too."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")


class VsyncRememberedWiringTest(unittest.TestCase):
    def setUp(self):
        self.settings = source("port/runtime/gx/pc_settings.cpp")

    def test_read_and_written(self):
        self.assertIn('else if (key == "vrrfound") options.vrr_found = value == "1";', self.settings)
        self.assertIn('<< (options.vrr_found ? "\\nvrrfound 1" : "")', self.settings)
        self.assertIn("bool vrr_found = false;", source("port/runtime/gx/render_options.h"))

    def test_only_while_automatic(self):
        self.assertIn("if (options.vsync_auto && options.vrr_found) {\n    options.vsync = false;", self.settings.replace("\r\n", "\n"))
        # The panel's toggle ends the automatic rule, as before.
        self.assertIn('if (settings_toggle("VSync (prevents tearing)", &options.vsync)) { changed = true; options.vsync_auto = false; }', self.settings)

    def test_unknown_keeps_the_last_finding_and_a_verdict_is_saved(self):
        self.assertIn("const bool want = vrr == 1 ? false : vrr == 0 ? true : !options.vrr_found;", self.settings)
        self.assertIn("if (vrr >= 0 && options.vrr_found != (vrr == 1)) {", self.settings)


if __name__ == "__main__":
    unittest.main()
