# Frontier verification — 2026-10-20

Source baseline: `fe85192ac7e87500377aea05f555eec9d37aa7e4` on `main`.
Desktop checkout: `/home/yemre/Desktop/onehour`.
Remote: `git@github.com:yunusemrejr/onehour.git`.

The desktop checkout was clean when discovered. Implementation and testing used an isolated clone, with another clean-state check before desktop integration. No saved brain was used for training or overwritten during verification.

## Changes and their contracts

- A 128×128 battlefield adds 51.5% ground over 104×104. Routes, resource distribution, shore, forests and rotated landmarks scale together. The connected lake has 1,561 water tiles; the island has room beside its watchtower. Twenty supply piles and all mainland building entrances remain reachable from all starts.
- Four cottages and four workshops add small, destructible garrison positions. Their footprints participate in collision and connectivity checks. Castle art now has a slate keep, ashlar walls, round crenellated towers, an arched gate and recessed courtyard. Shelter roofs, watchtower roofs, house tiles and civilian apartment roof details follow the existing procedural style. Access tracks are baked into traversable ground.
- Unit efficiency learns from completed individual combat trials. Local visible enemy mix is captured at first contact; damage credit uses actual health removed. Idle/travelling units are censored. Match survivors contribute once at completion. The regression remains a production-choice model, not a learned steering controller.
- Attack-wave labels include only wave members and reinforcements. Small reserve groups can claim nearby reachable landmarks. Stowed/boarding infantry are excluded from the field army. Computer walls must preserve connectivity; human wall-building rules remain available.
- Ground escape candidates require a route, increase separation and avoid radiation destinations. Replanning preserves progress monitoring. Invalid/nonfinite ground path requests fail safely. Brain v14 files are saved atomically, and older v2–v14 files still load.

## Checks executed

`make -j4` succeeds with the existing portable C++17/O2 flags and no compiler warnings introduced. `git diff --check` passes.

All 39 regression commands pass:

```
braintest learningtest frontiermaptest placementtest navytest forttest
movetest civictest autotest recoverytest radtest walltest scenario
econtest hqtest droptest striketest haultest allytest aidtest supporttest
turrettest upgradetest helitest rulestest stealthtest jeeptest dronetest
strikejettest antinuketest dozertest supersonictest jettest nuketest
bombtest areatest airmatrix groundmatrix uitest
```

Run any as `./build/onehour --<name>`. Map-geometry fixtures were repaired to stage actors at their asserted ranges: the far nuke survivor stays outside the blast; the flak screen gets actual dry ground; jeep mechanics run without neutral buildings inside the fixture; projectile-screen ships spawn away from the larger island; retaliation tests flatten their range arena; interception-rate test power plants stand outside repeated blasts. Assertions were retained, including coverage, damage and interception-rate bounds. Baseline `navytest` and `jeeptest` already failed due to landmark/fixture conflicts.

- `--placementtest`: 3,000 randomized obstacle/footprint cases match an independent two-pass flood-fill oracle, including map edges, resources and large airfields.
- `--frontiermaptest`: all starts, 20 supplies and every mainland entrance connected after real structures are spawned; island landing area retained; commander claims a cottage with four reserve infantry and retains its garrison; lateral and boxed escape cases; blocked/out-of-world and NaN path inputs.
- `--learningtest`: no label for an idle vehicle after 90 seconds; delayed death credit; survivor deduplication; friendly fire; overkill; separate reserve/wave credit; bunker-floor damage; invalid samples; save/load round trip; full-layout NaN/negative-count corruption; a child process with a 128-byte file-size limit proves a failed save preserves the last complete file.
- Controlled unit-model interpolation: held-out absolute error **1.100 → 0.019** after 800 synthetic observations. This is a model/data-path check, not evidence of improved battle win rate.
- `--seed 12345 --determinismtest 600`: two complete runs match digest `0be998ba2d6d4123`, peak 858 entity slots.
- `--seed 54321 --determinismtest 600`: repeated digest `88c87e0eb29133d0`, peak 811 slots.
- `--seed 6789 --swap --selftest 1800`: 36,000 ticks, 45,404 ms, 1.261 ms/tick, peak 974 live entities; all simulation invariants pass. The free-for-all was unresolved at the limit.
- `--selftest 1200`, seed 12345: **24,000 ticks**, **27,090 ms**, **1.129 ms/tick**, peak **882 live entities**, no invariant failures. With `ONEHOUR_STUCK=1`, zero stuck reports after fixing progress tracking and computer wall exits; the earlier expanded-map build reported 14 in this scenario. The match was still unresolved at 20 simulated minutes.
- `--crowdtest`: **2,000 starting land units**, 1,200 ticks, **4,253 ms / 3.545 ms per tick**, 2,046 slots. Finite state and world/terrain bounds pass. Queued production crosses a forced vector-capacity boundary.
- ASan+UBSan: the same crowd test passes (28.487 ms/tick under instrumentation); six fuzz seeds 20261020–20261025 pass 300 simulated seconds and 1,000 random commands each. Leak detection was disabled; address/undefined-behavior failures halt immediately. Learning checks also pass under instrumentation.

## Performance evidence

The placement workload is 240 factory/airfield searches with the same seed, goals and initial state. Five optimized O2 runs on the **same 104×104 map** retained the exact position checksum `7212370199596290935`:

| Workload | Before | After |
| --- | ---: | ---: |
| Placement median | 145.448 ms | 38.068 ms |
| Placement range | 144.717–145.853 ms | 37.904–38.232 ms |

This is **3.82×** faster for that placement workload. The changes reject candidates whose nonnegative lower-bound score cannot beat the best and bypass connectivity flood-fills only when the footprint has a completely open perimeter. A 900-second match produced identical results before/after after excluding timing; whole-match time was 20,269 vs 20,730 ms, so there is no demonstrated whole-match speedup from this optimization.

On the final **128×128 map**, `--placementbench` takes 51.679 ms for 240 searches. `--bench 300` reports **4.55 ms average / 15.29 ms worst** for software-rendered frames, with 521 entity slots in its scene. The baseline render scene reported 7.54/35.02 ms over 180 frames, but the map, battle and particle counts differ; these are playability measurements, not a controlled rendering speedup claim. Timings are machine/workload-specific and were gathered alongside other task activity.

## Interactive and visual inspection

The native SDL game ran on a fresh Xvfb display with software rendering and a temporary brain path. Inspection covered menu/start, dozer selection, hotkey building placement, completed construction with the $800 charge and 10 power output, time/speed changes, F1 help, minimap camera movement, Home, and resizing from 1280×800 to 1024×640. Screenshots were inspected at both sizes. The test did not use or focus the shared desktop. Headless world screenshots and building sheets also verify the new castle, shelter, tower, cottage, workshop and civilian designs.

An overflowing F1 controls panel was found during real gameplay. Its content now wraps inside a window-sized panel and uses the actual anti-nuke percentages. It was rechecked with the UI event test and visual inspection.

## Limits and release workflow

No statistical match win-rate improvement is claimed. Long free-for-all matches can remain unresolved; no claim is made that every movement edge case is eliminated. Testing covers Linux SDL2, the supplied compiler/runtime and software rendering; no GPU/Windows/macOS certification or leak audit was performed.

The repository has a Makefile, `run.sh`, source and vendored SDL headers. No established production hosting, release job or packaged-release workflow was found. Delivery therefore updates the existing GitHub `main` workflow and the desktop source/executable, without inventing a public release process.
