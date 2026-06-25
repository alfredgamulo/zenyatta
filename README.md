# zenyatta 🧘

> A workshop for handheld and embedded device projects — a tinkerer's
> playground held to production discipline.

zenyatta is a monorepo of small, self-contained projects for handheld and
embedded hardware: ESP32 firmware, atomic Linux images for Linux-capable
handhelds, and whatever else the bench turns up next. Each subproject stands on
its own — its own toolchain, its own build, its own `README` — but they all
answer to one set of rules in [`CLAUDE.md`](./CLAUDE.md).

The throughline is **formalized, reproducible craft**: hobby *scope*, not hobby
*rigor*. Declarative builds over hand-rolled steps, one operator interface per
project, no secrets in the tree, and a clean working directory. PlatformIO for
firmware and BlueBuild/bootc for images are both chosen for the same reason —
they make a build a *description you can read*, not a ritual you memorize.

## Project archetypes

Two shapes recur. A new project should fit one of them, or declare in its
`README` why it doesn't.

| Archetype | Stack | Deliverable | Example |
|---|---|---|---|
| **Firmware** | PlatformIO + Arduino / ESP-IDF, ESP32 family | Flashable firmware | [`WOPR/`](./WOPR) |
| **Atomic image** | BlueBuild + bootc / image-builder | Flashable `.raw` / OCI image | [`cardputer-zero/`](./cardputer-zero) |

## Subprojects

| Path | What it is | Target hardware | Status |
|---|---|---|---|
| [`WOPR/`](./WOPR) | Scrolling LED-matrix display — clock + live stock tickers over Wi-Fi | ESP32-C3 + MAX72xx 8×8 matrices | Working firmware |
| [`cardputer-zero/`](./cardputer-zero) | Immutable Fedora `bootc` handheld image with a security toolset | M5Stack CardputerZero (Pi CM0, aarch64) | [Implementation plan](./cardputer-zero/cardputer-atomic-plan.md) |
| *m5stackchan* *(planned)* | Animated desktop companion (stack-chan) | ESP32-S3 | Idea |

## Adding a project

1. Create a top-level directory named for the project.
2. Pick an archetype and scaffold its standard toolchain — a PlatformIO project
   or a BlueBuild recipe.
3. Give it a `README.md` (what it is, the hardware, how to build and flash) and
   a `justfile` exposing every operator action.
4. Keep secrets out of the tree: gitignore them and commit a `.example`
   template (see [`WOPR/include/credentials.h.example`](./WOPR/include/credentials.h.example)).
5. Follow [`CLAUDE.md`](./CLAUDE.md) — it governs every subproject in this repo.

## Conventions (the short version)

- **One operator interface.** Everything a human runs is a `just` target — never
  a bare multi-line snippet pasted into docs.
- **No secrets, ever.** Credentials live in gitignored files (`.env`,
  `include/credentials.h`) with committed `.example` templates. Public config
  (a ticker list, a package list) is *configuration*, not a secret, and can be
  tracked.
- **Declarative beats imperative.** A build should be a file you can read top to
  bottom — a `platformio.ini`, a `recipe.yml` — not steps you have to remember.
- **Self-contained subprojects.** No cross-subproject coupling; each builds from
  its own directory with its own toolchain.
- **Hardware lives behind a seam.** Pin maps, board configs, and chipset
  constants live in *one* place per project so that retargeting a board is a
  config change, not a rewrite.
- **Leave it better.** Fix what you find; keep the working tree clean — research
  clones and scratch go to `/tmp`.

The full rules — security, error handling, docs, specs, supply-chain hygiene,
commits, and per-archetype conventions — live in [`CLAUDE.md`](./CLAUDE.md).
