# Arena free-for-all (feat/arena-ffa)

Branch `feat/arena-ffa` in worktree `C:\Users\Chandler\NEW project\melee-ffa`, from next994 3ffe0cb5.
Plan: `C:\Users\Chandler\.claude\plans\streamed-wondering-token.md`. Not merged, not published.

## What players get

One room setting, **Match: Singles | Doubles | Free-for-all**, in the Arena panel:
- Rules page (Y): the Match row, left/right proposes the next kind; a kind that can't be played says why
  under the row ("Doubles needs 4 people", "Free-for-all needs 3 people",
  "Everyone needs the latest Melee Unlocked").
- R in the right panel proposes the next kind the room can play.
- Same vote as Doubles: everyone who would be in the new ring says yes; the switch happens between games.
- Offered by head count: Free-for-all at 3+, Doubles at 4+. One hint line when the room reaches 4.
- Leaving: Doubles with 3 left becomes Free-for-all (when every build plays it), else Singles;
  Free-for-all under 3 becomes Singles. The line and records are kept.
- Next game in Free-for-all, Winner stays: the winner keeps the spot, the losers go to the back of the
  line, the people who were waiting come in (longest waiting first). Take turns: the one in longest
  steps out. Ask: as before.
- Stages: the Doubles list (no Fountain of Dreams), drawn from the seed every player agrees on.
- Panel: gold "FREE-FOR-ALL" corners (no red/blue teams), each fighter's own record beside the name;
  Ring look shows all fighters; caption "FFA  WINNER STAYS".
- The Teams character select in a free-for-all hides the team toggle; Start locks in (random stage).

## How it works

- Rules: `port/runtime/hle/arena_rules.{h,cpp}` `Ring` kind, room wire 3 (wire 2 still sent to older
  members, who also turn Free-for-all off: `ffa_allowed`). Tests `port/tests/arena_rules_test.cpp`.
- Match: a Teams search on the host's code with `MatchSearchSettings::arena_ffa`; `slippi_online.cpp`
  builds the game with teams off, the doubles stage list, and salts the setup hash (a peer that isn't
  playing a free-for-all refuses before frame 1). `build_verdict` refuses a peer without
  `MU_BUILD_CAPS3_ARENA_FFA` (0x80): plain Slippi Dolphin or an older Melee Unlocked can't join one.
- Results: the end report carries a result byte; in a free-for-all the host waits for the winner's own
  report (8 s), else no winner.
- YouGame: the arena's room carries 3 or 4 fighters (`yougame_link.cpp` accepts 3).
- Slippi, 4 fighters: Slippi's Teams search with the host's code.
- Slippi, 3 fighters: Slippi's server only matches Teams at 4, so the three pair up by Direct searches
  on one port each (`Mesh` in `slippi_arena.cpp`), report ready to the host, and on the host's go make
  the match from the addresses the pairs found.
- Select: `sourceport/game/shim/mu_slippi_css.c` reads bit 4 of the arena reply (cmd 0xDD).

## Proven (hidden, loopback, this PC)

Rules tests 72/72, feed test, Linux syntax check of the three arena files. Loopback rooms
(`tools/arena_room_check.py`, real menus): see the final results in the commit message.

## Not proven

- YouGame production with real accounts (needs signed-in accounts).
- Slippi with real players over the internet (4-player FFA and the 3-player pairing).
- The 3-player pairing's match: it pairs, syncs and starts on loopback through the matchmaking
  stand-in, but on this line the stand-in harness can't finish even a plain singles game on the
  shipped build, so it can't confirm the match itself.

## Build and test

- `C:\Users\Chandler\mu-ffa\build_ffa.py [cfg-game game cfg-host host tests]`
- `C:\Users\Chandler\mu-ffa\run_room.py <name> --players N [--ffa|--doubles] [--watch k]`
- `C:\Users\Chandler\mu-ffa\fixture_shots.py ffa,ffavote 0,1,2,3,4` (panel pictures)
- Junctions in this worktree: `sourceport\extern\melee` and `port\third_party\{ngx\include,ngx\lib,
  ngx\dlssnr,onnxruntime\bin}`. Delete them before removing the worktree.
