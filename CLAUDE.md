# Melee Unlocked: macOS fork

This is BeIllegalBeagle's fork of Hero88go/melee-unlocked. The work here is a macOS build of the
game, kept on the `mac-build` branch. Everything else follows upstream.

## Git and GitHub

- `origin` is the fork (git@github.com:BeIllegalBeagle/melee-unlocked.git). Push only there, over SSH.
- `upstream` is Hero88go/melee-unlocked and is fetch-only. Never push or open anything there.
- Commit and push as the owner's personal account and identity, never a work one. Before committing,
  `git var GIT_AUTHOR_IDENT` must show the personal identity from the global `~/.gitconfig`.
- The `gh` CLI on this Mac holds more than one account, and the active one may not be the personal one.
  For any `gh` command on this repo, select the account explicitly:
  `GH_TOKEN="$(gh auth token --user BeIllegalBeagle)" gh pr create ...`
- To pick up a new upstream version: `git fetch upstream`, merge `upstream/main` into `mac-build`, then
  rebuild both apps and run the checks under Testing (release parity and the cross-architecture
  pairings). `tools/prepare_native_sources.py` brings the prepared decomp up to date with a changed
  `melee-native.patch` by rewriting only the files that changed. It compares the tree with the one it
  recorded as `refs/mu/prepared` in the submodule, and stops if the tree has edits of its own.

## Building the Mac app

```sh
port/mac/build_deps.sh                                 # once: fills ../deps beside the repo
port/mac/build_all.sh --arch arm64 path/to/melee.iso   # native Apple silicon app (build-mac-full-arm64-squad/)
port/mac/build_all.sh path/to/melee.iso                # x86-64 app under Rosetta (build-mac-full-squad/)
./play-mac.command                                     # the native app when it exists
```

Each build takes a few minutes clean; later runs rebuild only what changed.

`port/mac/README.md` ("Building from a clone") documents the steps. Things that are not obvious:

- The host loads the game as a Linux ELF (`melee_game.so`) through `port/mac/elf_loader.cpp`, for both
  the native arm64 host and the x86-64 one under Rosetta. The game is built with a GCC cross compiler
  that targets Linux (`port/mac/linux-cross.cmake`; GCC is needed for `scalar_storage_order`), with
  `-fno-stack-protector`, because the loader refuses stack-canary code. Rosetta's general support ends
  after macOS 27, so the arm64 build is the one to keep working.
- Native arm64 keeps the low 4 GB unmapped, so the console window (0x7f000000..0x84000000: MEM1, then
  the game image at 0x82800000) sits at a 4 GB-aligned base there. The game keeps the low 32 bits of
  its addresses, identical to x86-64, and widens them with `MU_WIDEN`/`MU_Z`/`DP`
  (`sourceport/game/include/mu_window.h`). New game code that turns a 32-bit word into a pointer must
  go through one of these, never a plain `(T*)(uintptr_t)` cast. A missed one faults at a console
  address below 4 GB (for example `address 0x8017e000`); run the app under
  `lldb -b -o run -k "bt 24" -- <app> ...` and map game frames with the cross addr2line on
  `melee_game.dbg` (subtract the base, logged as `Mac layout: owned window`). Narrow int-to-pointer casts
  show up as `-Wint-to-pointer-cast` when the game is compiled without `-w`.
- A 32-bit word that becomes a pointer-sized integer, such as an `HSD_IDKey`, needs `MU_WIDEN` too.
  Nothing faults or warns there: the lookup just misses. One unwidened ID lookup in `aobj.c` made
  Randall on Yoshi's Story follow a detached copy of its path without the stage's scale, and online
  games there desynced against x86-64.
- Decomp changes for the Mac live in `sourceport/patches/melee-native-mac.patch`, applied after the
  released `melee-native.patch` by `tools/prepare_native_sources.py`. Leave upstream's patch untouched.
- On x86-64 these changes must compile to the same code (`MU_WIDEN` is the old zero-extension there).
  Compare the x86-64 library's functions against a reference build after changing game code.
- `port/mac/aarch64_like_x86.py`, the AArch64 build's compiler launcher, makes two things behave as
  they do with x86-64 GCC, and the game depends on both:
  - Static data alignment. AArch64 GCC aligns static data to 8, where x86-64 GCC uses 32 for anything of
    32 bytes or more. The game's DMA buffers depend on the 32-byte alignment.
  - Float-to-unsigned conversion. AArch64's `fcvtzu` turns a negative value into 0. x86-64 and the
    console keep the low bits of the signed result, so (u8)(-128.0f) is 0x80. With `fcvtzu`, Nana
    ignored Popo's stick held left or down, and every Ice Climbers game desynced against x86-64. The
    launcher compiles each unit to assembly and replaces every `fcvtzu` before assembling.
  - Signed conversions (`fcvtzs`) still differ from x86-64 for NaN and out-of-range values. The console
    differs from x86-64 there too.
  - Every game object depends on the launcher, so editing it rebuilds the library.
- The loader only accepts imports listed in its `IMPORT(...)` table. A new upstream game that fails with
  "unlisted strong import" needs that import added there.
- The game library is built with Doubles and Squad Strike on, as in the Linux release, so the host must
  be built with `build_full.py --squad-strike --doubles`. `build_all.sh` does both.
- Upstream builds the Linux game with GCC 14 (Ubuntu 14.2), and this fork uses GCC 15.2.
  `port/mac/release_parity.py` loads upstream's own Linux release library in the x86-64 host. On
  1.0.14 it matched both Mac builds exactly on every `parity_*` script that reaches a match.
  Not yet verified: the Windows build (MinGW GCC 15.3), and real matchmaking against other platforms.
- The GameCube adapter (Wii U mode) is read through libusb when `../deps/libusb` exists. This is the
  `MELEE_MAC_GC_ADAPTER` path in `port/runtime/host/gc_adapter.cpp`. Like Dolphin, it needs
  GCAdapterDriver installed and never detaches the kernel driver on macOS. Only one program can hold
  the adapter, so Slippi Dolphin must be closed.
- `spike/shader-seed/` (the bundled shader cache) is generated locally and is not in git.
  `package_full.py` copies it into the app when it is present.
- Opened from Finder (no arguments), the app uses the disc from `~/.local/share/melee-unlocked/last-iso`
  or asks for one in an open panel (`port/mac/disc_choice.h`). Any argument skips this, so tests are
  unaffected.
  - Test a Finder start with `open -n --env HOME=... --env XDG_DATA_HOME=...` (port/mac/README.md,
    "Opening the app from Finder"). Keep the test disc out of Downloads, Documents and Desktop: a
    LaunchServices start reading it there waits on a macOS privacy prompt on the user's screen.
  - Don't take window screenshots with `screencapture`: they make macOS ask the user to let the
    terminal record the screen.

## Testing

- Never run test builds against the real profile. The game reads the Slippi login from the home folder
  and signs in. Give every automated run throwaway `HOME`, `XDG_DATA_HOME`, `XDG_CONFIG_HOME`,
  `XDG_CACHE_HOME` and `XDG_STATE_HOME` folders, and pass `--user-dir`.
- Smoke test: `SDL_AUDIODRIVER=dummy <app>/Contents/MacOS/MeleeUnlocked --iso <iso> --headless --frames 600
  --volume 0 --user-dir <tmp>` must exit 0 and log `canary sites 0`.
- `--hidden`, `--headless` and `MELEE_NO_GC_ADAPTER=1` runs never open the adapter, so test the adapter
  by playing. Its log lines start with `gc adapter:` in `~/.local/share/melee-unlocked/User/Slippi/melee_port.log`.
- Determinism between the two architectures: run both apps with `--headless --fast --vanilla-game
  --vanilla-only --frames 9000 --time-base 0 --rng-seed 0x12345678 --script port/scripts/parity_vs.txt
  --state-digest <csv>` and compare with `compare_digest.compare` (port/mac/compare_digest.py). Match
  frames must show 0 differing values. Menu frames are excluded; their RNG also differs run to run.
- `port/mac/elf_loader_test.cpp` with `port/mac/elf_abi_probe.c` checks the loader, including the
  printf-family bridge between the Linux and Apple AArch64 calling conventions.
- Online sync between two builds:
  - Run `tools/p2p_pair.py --exe <x86 app binary> --exe-p2 <arm64 app binary> --chars 14/0:4/0
    --stage 3`, with throwaway HOME/XDG and `--extra-p1 "--user-dir A" --extra-p2 "--user-dir B"`.
    It plays a rollback match over loopback and needs 0 mismatched checksums. Add `--lag-ms 30
    --jitter-ms 10 --loss 2` for a bad connection.
  - The parity scripts have no Ice Climbers and no Pokemon Stadium, so run this pairing too. It
    caught the `fcvtzu` desync. They have no Yoshi's Story either: `--chars 3/0:13/0 --stage 8`
    caught the Randall desync.
  - `--games` above 1 never starts game 2 on any build, upstream's release library included:
    `p2p_bot.txt` presses nothing on the post-game screen (scene 0E:03).
- Against upstream's release: `python3 port/mac/release_parity.py --iso <iso>` downloads the Linux
  release named by VERSION. It runs every `parity_*.txt` on that release's library and on both Mac apps,
  and compares the results. Run it after merging a new upstream version.
- To find the first divergence, play the `.slp` a pair test saved (under each `--user-dir`'s `Replays/`)
  on both apps with `--replay <slp> --headless --fast --state-digest <csv>`. `MELEE_TRACE_NATIVE_RNG=a:b`
  logs every random draw in retraces a to b. A fighter the digest does not cover, such as Nana, shows up
  when you compare the two recordings' pre-frame and post-frame events.
