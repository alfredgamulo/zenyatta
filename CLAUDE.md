# CLAUDE.md — Operating rules for zenyatta

This file governs how **any** contributor — human or AI agent — works in this
repository. Read it before touching code. These rules win ties against personal
preference. When a rule here conflicts with a subproject's spec or `README`,
**this file wins** — fix the spec. When two rules here appear to conflict, the
lower-numbered one wins.

zenyatta is a monorepo of small, self-contained projects for handheld and
embedded hardware — ESP32 firmware (PlatformIO) and atomic Linux images for
Linux-capable handhelds (BlueBuild/bootc). See [`README.md`](./README.md) for
the map and the project archetypes. Each subproject is independent; these rules
are the one thing they share.

---

## 1. Core principles (non-negotiable)

This is the project's constitution. It applies to every subproject regardless of
archetype.

### 1.1 Security first
These are network-connected devices and, in the image projects, security tools.
Treat every input that crosses a trust boundary as hostile.

- **All external data is untrusted** — HTTP responses, JSON payloads, sensor and
  bus reads, USB bytes, RF frames, user keystrokes. Validate length, type,
  range, and ownership *before* it touches state. Reject; never assume.
- A parser must **not crash on malformed input** — check the error path (a bad
  HTTP code, a `DeserializationError`, a short buffer) and degrade, don't panic.
- Apply least privilege everywhere it exists: IAM roles, image package sets,
  service accounts, capabilities. Ship the smallest surface that works.
- Active/offensive capabilities in the image projects (capture, injection,
  replay) require an explicit, user-initiated action — never a background or
  remotely-triggerable path. A received frame must never start an operation.

### 1.2 No secrets in code, config, or logs — ever
- No Wi-Fi passwords, API keys, tokens, signing keys, or cloud credentials in any
  tracked file. The secret-bearing file is **gitignored**; a `.example` template
  with placeholders is committed in its place. The established patterns here are
  `include/credentials.h` + `credentials.h.example` (firmware) and `.env` +
  `.env.example` (everything else).
- Signing keys (cosign private halves, keystores) live in the OS keychain or a CI
  secret store, never in the repo. Only the **public** half (`cosign.pub`) is
  committed.
- **Never log secrets.** No keys, tokens, or passwords on the serial console,
  in `print()`, or in CI logs at any level.
- Public IDs (image refs, NTP servers, ticker symbols, package names) are
  *configuration*, not secrets — track them freely, but still keep them in named
  constants, not scattered literals (§1.5).
- A leaked secret is a **compromised** secret: rotate it, purge it, report it —
  do not just delete the line.

### 1.3 Fix what you find
If you spot a security hole, a swallowed error, a hardcoded pin used three ways,
a lint failure, or a bad pattern — **fix it**, whether or not your current change
caused it. Leave every file better than you found it. Do not route around a known
defect to ship a feature.

### 1.4 The device must never wedge
Embedded firmware has no user to restart it and an image on a 512 MB handheld has
no headroom to waste. Every long-running path must be able to recover on its own.

- Handle every error path. A failed network call, a malformed response, a missing
  peripheral, or a dropped Wi-Fi link must degrade gracefully — retry with a
  bounded timeout, fall back, or show a clear on-device message (a scrolled
  `"WiFi Error"`, a console line) — never hang silently or hard-fault.
- Every blocking wait has a **bounded timeout** and a defined outcome when it
  expires. No unbounded `while` on a hardware or network condition.
- Long-running firmware feeds the watchdog and reconnects rather than blocking
  forever; image services fail closed and restart cleanly.

### 1.5 Document everything; no magic numbers
- Every non-trivial function gets a comment: what it does, its inputs, what it
  returns, and any side effects or failure modes.
- **No magic numbers.** Every pin, register value, brightness level, timeout,
  baud rate, GMT offset, or buffer size is a named constant (`#define` /
  `constexpr`) or carries an inline comment explaining the value and *why* it was
  chosen. (WOPR's `gmtOffset_sec = -18000; // EST, -5h` and `attempts < 40 // 20s
  timeout` are the bar.) Named constants win when the value is reused or
  hardware-specific.
- When you change behavior, update the comment and any affected spec in the same
  change. Code and docs never disagree for long.
- Capture **non-obvious gotchas** in a "Maintainer notes" section of the
  subproject `README` — the things that aren't visible from reading the recipe or
  the source, for whoever returns after a long time (e.g. "don't rename the
  published image `name:` — it breaks rebases for existing installs").

### 1.6 Declarative and reproducible
The build is a description you can read, not a sequence you memorize.

- Firmware: the toolchain, board, and dependencies live in `platformio.ini`.
  Pin library versions (`@^x.y.z`).
- Images: the whole image is a BlueBuild `recipe.yml` — an ordered list of
  modules. **No hand-written `Containerfile`.** Almost every change is a one-liner
  in the recipe.
- A clean checkout plus the documented `just` target reproduces the artifact.
  Build outputs (`.pio/`, `out/`, `.raw`) are gitignored, never committed.

### 1.7 One operator interface: `just`
- If a human runs it in a terminal, it is a `just` target with a clear name and a
  `# comment`. No bare multi-line snippets in docs as the "how to run it" path.
- Never instruct the user to call `pio`, `bluebuild`, `podman`, `dd`, or `gh`
  directly — wrap it in a recipe first. If you copy-paste a command more than
  twice, make it a recipe. Don't make recipes for genuine one-offs.
- Targets are idempotent where possible and fail loudly with a useful message.
  Secrets a target needs come from the environment / keychain, never inline.

### 1.8 Hardware lives behind a seam
Retargeting a board must be a localized change, not a rewrite. This repo spans
ESP32-C3 today and ESP32-S3 / Pi-class silicon next.

- Pin maps, board selection, chipset constants, and panel/keyboard config live in
  **one** place per project — a `[env:<board>]` block, a board header, a
  device-tree overlay directory — never branched on inline throughout the logic.
- Add a new target board by adding a config, not by editing the application
  logic. If a feature has to ask "which board am I?", reshape it to query a
  capability instead.

---

## 2. How we work

### 2.1 Self-contained subprojects
Each top-level directory is an island: its own toolchain, build, dependencies,
and `README`. No cross-subproject imports or shared build state. Patterns may be
*copied* between projects (the cardputer-zero plan lifts its CI hygiene from
quantumqat) — but copied, not coupled.

### 2.2 Spec before non-trivial work
- Anything beyond a bug fix or a small single-concern change (≳50 lines) gets a
  short spec under the subproject's `specs/` first: problem statement, proposed
  approach, open questions. The implementation references its spec.
- A spec is a checklist worked top-to-bottom; respect stated prerequisites. If
  reality diverges from the spec, update the spec in the same change.
- Record significant or hard-to-reverse decisions as an ADR under `docs/adr/NNN-<kebab-title>.md`
  (which option, why, what was rejected). Justify any abstraction deeper than
  ~3 layers — default to YAGNI and solo-maintainable.

### 2.3 Definition of Done (every change)
1. The work does what its spec/checklist said.
2. New operator actions are exposed as `just` targets (§1.7).
3. New functions documented; no new magic numbers (§1.5).
4. Every error path handled; no new way for the device to wedge (§1.4).
5. No secret added; nothing sensitive logged (§1.2).
6. Lint/format/static-analysis clean; issues you touched are fixed (§1.3).
7. For anything that crosses a trust boundary or ships an active capability, a
   security review was performed and findings resolved (§1.1).
8. It was **built and flashed/booted on real or stand-in hardware** — "it
   compiles" is not "it works." Use the documented dev stand-in where the target
   isn't on the bench (a Pi Zero 2 W for the CardputerZero — identical silicon).

### 2.4 Research scratch space
Cloning a reference repo or downloading a datasheet/BSP for research goes in
`/tmp`. Extract what you need, cite the source in a comment or spec, delete the
clone. Never commit scratch clones or downloads — keep the working tree clean.

### 2.5 Subagents
Spawn subagents to **isolate context**, **parallelize independent work**, or
**offload bulk mechanical tasks**. Do **not** spawn when the parent needs the
reasoning in-context, when synthesis requires holding the pieces together, or
when spawn overhead dominates. Pick the cheapest model that does the subtask
well:

| Model | Use for |
|---|---|
| **Haiku** | Bulk mechanical work, no judgment (formatting, grep sweeps, boilerplate, renames) |
| **Sonnet** | Scoped research, code/datasheet exploration, in-scope synthesis |
| **Opus** | Subtasks needing real planning or genuine trade-offs |

If a subagent finds it needs a higher tier, it returns to the parent rather than
guessing. **The parent owns the final output and all cross-spawn synthesis.**

---

## 3. Per-archetype conventions

### 3.1 Firmware (PlatformIO / ESP32)
- PlatformIO is the build system. One `[env:<board>]` per target board; pin every
  `lib_deps` version. Never hand-invoke `arduino-cli`/`esptool` in docs — wrap in
  `just`.
- Pin maps and hardware constants are named `#define`/`constexpr`, grouped at the
  top of the file or in a board header — never bare literals in `loop()` (§1.5,
  §1.8).
- Treat the network and the bus as hostile (§1.1): check the HTTP status, the
  JSON parse result, and value sanity (`price != 0`) before display; bound every
  connect/read with a timeout (§1.4).
- Secrets (`include/credentials.h`) are gitignored with a committed `.example`;
  public config (the ticker list in `tickers.h`) can be tracked.
- Serial debug sits behind a compile flag (`#define DEBUG`), default-off for a
  release build.

### 3.2 Atomic images (BlueBuild / bootc)
- The `recipe.yml` is the source of truth — declarative modules, applied
  top-to-bottom. No `Containerfile`. Files are baked via the `files` module;
  packages via `dnf`; services via `systemd`.
- Keep package sets **lean** — these are RAM-bound handhelds (512 MB rules out a
  desktop or Flatpaks; prefer a headless console profile with zram).
- Pin the base image and every GitHub Action to a commit SHA with a `# vX`
  comment. Cross-arch builds prefer a native arm64 runner over QEMU.
- **Cosign-sign** images intended for OTA or public distribution; the public key
  is committed, the private half is a CI secret. Enable signing only when you
  actually distribute.
- **Don't rename a published image `name:`** — it's the ref users have rebased
  onto; changing it silently breaks their updates. Record this and other
  non-obvious traps in Maintainer notes (§1.5).

### 3.3 Supply-chain hygiene (any project with CI)
- SHA-pin Actions; enable **Dependabot** for `github-actions`.
- For versions pinned shell-style in a script (which Dependabot can't see), add a
  **release-watch** entry so a newer upstream opens an issue.
- Third-party artifacts that are unsigned and have no published checksum get an
  exact version **and** a vetted SHA-256, bumped together — a mismatch fails the
  build by design.

---

## 4. Commits & branches
- **Conventional Commits** (`feat:`, `fix:`, `docs:`, `chore:`, `refactor:`,
  `perf:`, `test:`). Subject ≤ 50 chars, imperative mood. Body explains *why*
  when it isn't obvious.
- Branch for non-trivial work. Never commit secrets or build artifacts. Keep the
  working tree clean.
- Commit or push **only when the human asks**.

## 5. Communication style
Default to **`/caveman full`** in chat — terse, fragments fine, no filler, no
pleasantries, exact technical terms. **Code, commits, PRs, specs, and security
or destructive-action warnings are written normally** — full sentences, no
compression. Off only on "stop caveman" / "normal mode".

---

## 6. Quick checklist before you say "done"
- [ ] Untrusted input validated; parser can't crash on garbage. (§1.1)
- [ ] No secret in code, config, or logs. (§1.2)
- [ ] Fixed the issues I found along the way. (§1.3)
- [ ] No new way for the device to wedge; waits are bounded. (§1.4)
- [ ] New functions documented; no magic numbers. (§1.5)
- [ ] Build is declarative and reproducible from a clean checkout. (§1.6)
- [ ] Operator steps are `just` targets. (§1.7)
- [ ] Board-specific bits stay behind the seam. (§1.8)
- [ ] Built and flashed/booted on real or stand-in hardware. (§2.3)
- [ ] Security review done where a trust boundary or active capability changed. (§1.1)
