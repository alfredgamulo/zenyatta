# M5Stack CardputerZero — Atomic Fedora Image: Implementation Plan

Build a minimal, immutable **Fedora bootc** arm64 image for the **M5Stack
CardputerZero** (a Raspberry Pi CM0 handheld), with a Fedora-packaged security
toolset, produced with **BlueBuild** and delivered as a flashable **`.raw`** via
**bootc-image-builder**. Pipeline patterns are carried over from the `quantumqat`
project (BlueBuild recipe → GHCR → image).

---

## 1. Target hardware (reference)

| Part | Spec |
|---|---|
| SoC | **Raspberry Pi CM0**, RP3A0 (**BCM2837**), quad-core Cortex-A53 @ 1 GHz, ARMv8-A (**aarch64**) |
| RAM | **512 MB LPDDR2** (the binding constraint) |
| Storage | microSD (32 GB bundled on full; none on Lite) |
| Display | 1.9" LCD **ST7789v3** (SPI) + **HDMI 1080p30** out |
| Input | **46-key matrix keyboard** (GPIO) |
| Camera | Sony **IMX219** 8 MP CSI (full only; Pi Camera Module 2 sensor → mainline) |
| Wireless / net | 2.4 GHz Wi-Fi b/g/n, BT 4.2/BLE, 10/100 Ethernet |
| Ports / bus | 2× USB-C (host/device switch), USB-A host, GPIO/SPI/UART/I²C header, HY2.0 |
| IR | Infrared **TX + RX** |
| Sensors | **BMI270** IMU + **RX8130CE** RTC (full only) |
| Power | 3.7 V 1500 mAh Li-Po, **BQ27220** fuel gauge |
| Variants | Lite $59 (no camera/IMU/SD) · Full $89 — *specs preliminary* |

Same silicon as a **Raspberry Pi Zero 2 W**, which is the development stand-in.

---

## 2. Design decisions (reference)

- **Base image:** a Pi-capable arm64 Fedora bootc base (e.g. `fedora-bootc-raspi`
  lineage). Generic `quay.io/fedora/fedora-bootc` arm64 assumes UEFI and will not
  boot a Pi without the Pi kernel + firmware + device tree.
- **Profile:** minimal / headless. 512 MB RAM rules out an atomic desktop or
  Flatpaks; target a console image with zram/swap configured.
- **Toolset:** security tools from Fedora repos, layered with the `dnf` module.
  Add RPM Fusion through the module if a tool is not in base Fedora.
- **Image format:** `.raw` via bootc-image-builder. No ISO / Anaconda (that is an
  x86 installer path).
- **Updates:** `bootc upgrade`. Cosign signing is optional — enable it only for
  verified OTA or public distribution (Step 8).
- **Drivers:** camera (IMX219), IMU (BMI270), RTC (RX8130CE), and fuel gauge
  (BQ27220) have in-tree Linux drivers. Only the **ST7789v3 panel** and the
  **46-key matrix keyboard** need custom device-tree overlays (Step 7).

---

## 3. Architecture (reference)

```
recipes/recipe.yml
   │  BlueBuild CLI (platforms: linux/arm64)
   ▼
arm64 bootc image ──► GHCR  (cosign-signed only if Step 8 enabled)
   │
   ▼  bootc-image-builder (--type raw --target-arch arm64)
disk.raw ──► flash to microSD ──► boot ──► bootc upgrade (OTA)
```

---

## 4. Repository layout (reference)

```
.
├── recipes/recipe.yml
├── files/
│   ├── system/            # /etc, /usr, and /boot overlays (device tree, config.txt)
│   ├── scripts/           # build-time scripts (zram/swap, tweaks)
│   └── justfiles/         # optional ujust helpers
├── image-builder/config.toml
├── .github/workflows/build.yml
├── cosign.pub             # Step 8 only
└── README.md
```

---

## Step 1 — Scaffold the repository

1. Create the directory tree from §4.
2. Copy the supply-chain hygiene from `quantumqat`: SHA-pinned actions, Dependabot
   (`github-actions`), and the `release-watch` notifier if you pin any third-party
   artifacts.
3. Commit a placeholder `README.md`.

## Step 2 — Write the recipe (`recipes/recipe.yml`)

```yaml
---
version: 1
name: cardputerzero
description: Atomic Fedora handheld image for the M5Stack CardputerZero (arm64)
base-image: ghcr.io/<pi-fedora-bootc-base>   # set to a Pi-capable arm64 bootc base
image-version: "41"                          # pin to a real version
platforms:
  - linux/arm64

modules:
  # Device config: device-tree overlays, /boot/config.txt, modprobe, zram
  - type: files
    files:
      - source: system
        destination: /

  # Headless console kernel args (adjust UART to the board)
  - type: kargs
    kargs:
      - console=ttyS0,115200
      - console=tty1

  # Fedora-packaged security toolset (verify availability; add RPM Fusion repo
  # here for anything not in base Fedora)
  - type: dnf
    install:
      packages:
        - nmap
        - tcpdump
        - wireshark-cli
        - aircrack-ng
        - iw
        - hydra
        - john
        - hashcat
        - bettercap
        - lirc            # IR TX/RX capture & replay
        - zram-generator

  # Enable required units
  - type: systemd
    system:
      enabled:
        - systemd-zram-setup@zram0.service

  # Step 8 only:
  # - type: signing
```

Set `base-image` to a Pi-capable arm64 bootc base (see §2). Keep the package list
lean for 512 MB; expand per use case.

## Step 3 — CI: build the arm64 image (`.github/workflows/build.yml`, job 1)

```yaml
name: build
on:
  schedule: [{ cron: '0 6 * * *' }]
  push: { branches: [main], paths-ignore: ['**.md'] }
  pull_request:
  workflow_dispatch:

jobs:
  build_image:
    runs-on: ubuntu-24.04            # use an arm64 runner if available (skips QEMU)
    permissions: { contents: read, packages: write, id-token: write }
    steps:
      - uses: docker/setup-qemu-action@<sha>   # only needed for arm64-on-x86 builds
      - uses: blue-build/github-action@<sha>
        with:
          recipe: recipe.yml
          registry_token: ${{ github.token }}
          pr_event_number: ${{ github.event.number }}
          # cosign_private_key: ${{ secrets.SIGNING_SECRET }}   # Step 8 only
```

Pin every action to a commit SHA with a `# vX` comment. Cross-arch builds under
QEMU are slow; prefer arm64 runners.

## Step 4 — CI: build the `.raw` (job 2)

`image-builder/config.toml`:

```toml
[[customizations.user]]
name = "operator"
groups = ["wheel"]
# set an SSH key or password

[customizations.kernel]
append = "console=ttyS0,115200"

[[customizations.filesystem]]
mountpoint = "/"
minsize = "6 GiB"
```

Workflow job:

```yaml
  build_raw:
    needs: build_image
    if: github.event_name != 'pull_request'   # image must be published first
    runs-on: ubuntu-24.04
    permissions: { contents: read, packages: read }
    steps:
      - uses: actions/checkout@<sha>
      - name: bootc-image-builder
        run: |
          sudo podman run --rm --privileged --security-opt label=disable \
            -v ./image-builder/config.toml:/config.toml:ro \
            -v ./out:/output \
            -v /var/lib/containers/storage:/var/lib/containers/storage \
            quay.io/centos-bootc/bootc-image-builder:latest \
            --type raw --rootfs ext4 --target-arch arm64 \
            --config /config.toml \
            ghcr.io/<you>/cardputerzero:latest
      - uses: actions/upload-artifact@<sha>
        with: { name: cardputerzero-raw, path: out/**/*.raw }
```

## Step 5 — Build locally

```bash
bluebuild build ./recipes/recipe.yml      # builds ghcr.io/<you>/cardputerzero

sudo podman run --rm --privileged --security-opt label=disable \
  -v $PWD/image-builder/config.toml:/config.toml:ro \
  -v $PWD/out:/output \
  -v /var/lib/containers/storage:/var/lib/containers/storage \
  quay.io/centos-bootc/bootc-image-builder:latest \
  --type raw --rootfs ext4 --target-arch arm64 --config /config.toml \
  ghcr.io/<you>/cardputerzero:latest
```

## Step 6 — Flash and first boot (headless)

```bash
sudo arm-image-installer --image=out/disk.raw --target=rpi3 --media=/dev/sdX
# or: sudo dd if=out/disk.raw of=/dev/sdX bs=4M status=progress oflag=direct
```

Boot first over **HDMI + USB keyboard** (or serial/SSH). Validate: it boots, the
toolset runs, `bootc upgrade` works, and 512 MB headroom is acceptable. Do this on
a **Raspberry Pi Zero 2 W** before the hardware ships — identical silicon.

## Step 7 — Enable the built-in panel and keyboard

The ST7789v3 SPI panel and 46-key GPIO matrix keyboard need device-tree overlays;
everything else is mainline.

1. Obtain M5Stack's CardputerZero image/BSP; extract its device-tree overlays and
   `config.txt` for the panel and keyboard.
2. Place the overlays and `config.txt` settings under `files/system/boot/` so they
   bake into the image.
3. Confirm drivers are present/enabled: **ST7789 DRM panel** (or `fbtft`
   `fb_st7789v`) for the display; **`matrix-keypad`** for the keyboard, with a
   46-key keymap.
4. Rebuild, reflash, verify the on-board screen and keyboard.

## Step 8 — (Optional) signing and verified updates

Enable only for verified OTA or public distribution:

1. Generate a cosign keypair; add the private key as the `SIGNING_SECRET` repo
   secret; commit `cosign.pub`.
2. Add `- type: signing` to the recipe.
3. Pass `cosign_private_key: ${{ secrets.SIGNING_SECRET }}` to the BlueBuild action.
4. Rebase/flash; thereafter `bootc upgrade` verifies signatures.

---

## Reference A — Carried over / dropped vs `quantumqat`

| Item | Decision | Reason |
|---|---|---|
| `dnf` module, SHA-pinned actions, Dependabot, release-watch | **Keep** | arch-independent hygiene |
| cosign / `signing` | **Optional** (Step 8) | only for OTA / distribution |
| `kargs`, headless profile, zram | **Add** | 512 MB RAM + serial console |
| bootc-image-builder `.raw` | **Add** | the device deliverable |
| ISO / Anaconda build | **Drop** | x86 installer path |
| Secure Boot enrollment | **Drop** | not how a Pi boots |
| Winboat / `/opt` GitHub-RPM | **Drop** | x86 + Windows VM |
| `default-flatpaks` | **Drop** | headless, RAM-bound |

## Reference B — Board firmware / bootloader

A `.raw` is the whole disk: boot partition (firmware, bootloader, kernel, device
trees) + root. The Pi boot partition must carry the board firmware and overlays.
`bootupd` needs board awareness to place firmware under `/boot/efi`. Reuse existing
Pi bootc work: `osbuild/bootc-image-builder`, `ondrejbudai/fedora-bootc-raspi`,
`AlmaLinux/bootc-images-rpi`.

## Reference C — Development order

1. Steps 1–5 → produce a `.raw`.
2. Step 6 on a **Pi Zero 2 W**, headless (HDMI + USB) — proves the full pipeline on
   identical silicon.
3. Step 7 on the CardputerZero — the only device-specific work (panel + keyboard
   overlays).
4. Step 8 if/when verified updates or distribution are needed.
