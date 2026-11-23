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
  rebuild.

## Building the Mac app

```sh
port/mac/build_deps.sh                    # once: fills ../deps beside the repo
port/mac/build_all.sh path/to/melee.iso   # about 3 minutes clean; later runs rebuild only what changed
./play-mac.command                        # runs build-mac-full-squad/MeleeUnlocked.app
```

`port/mac/README.md` ("Building from a clone") documents the steps. Things that are not obvious:

- The host is x86-64 and runs under Rosetta. It loads the game as a Linux ELF (`melee_game.so`) through
  `port/mac/elf_loader.cpp`. The game is built with a GCC cross compiler that targets Linux
  (`port/mac/linux-x86_64-cross.cmake`), with `-fno-stack-protector`, because the loader refuses
  stack-canary code.
- The loader only accepts imports listed in its `IMPORT(...)` table. A new upstream game that fails with
  "unlisted strong import" needs that import added there.
- The game library is built with Doubles and Squad Strike on, as in the Linux release, so the host must
  be built with `build_full.py --squad-strike --doubles`. `build_all.sh` does both.
- Upstream builds the Linux game with GCC 14, and this fork uses GCC 15.2. Offline play is fine. Online
  play against other platforms is not yet verified to stay in sync.
- The GameCube adapter (Wii U mode) is read through libusb when `../deps/libusb` exists. This is the
  `MELEE_MAC_GC_ADAPTER` path in `port/runtime/host/gc_adapter.cpp`. Like Dolphin, it needs
  GCAdapterDriver installed and never detaches the kernel driver on macOS. Only one program can hold
  the adapter, so Slippi Dolphin must be closed.
- `spike/shader-seed/` (the bundled shader cache) is generated locally and is not in git.
  `package_full.py` copies it into the app when it is present.

## Testing

- Never run test builds against the real profile. The game reads the Slippi login from the home folder
  and signs in. Give every automated run throwaway `HOME`, `XDG_DATA_HOME`, `XDG_CONFIG_HOME`,
  `XDG_CACHE_HOME` and `XDG_STATE_HOME` folders, and pass `--user-dir`.
- Smoke test: `SDL_AUDIODRIVER=dummy <app>/Contents/MacOS/MeleeUnlocked --iso <iso> --headless --frames 600
  --volume 0 --user-dir <tmp>` must exit 0 and log `canary sites 0`.
- `--hidden`, `--headless` and `MELEE_NO_GC_ADAPTER=1` runs never open the adapter, so test the adapter
  by playing. Its log lines start with `gc adapter:` in `~/.local/share/melee-unlocked/User/Slippi/melee_port.log`.
