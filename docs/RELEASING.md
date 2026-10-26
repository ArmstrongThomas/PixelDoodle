# Releasing Pixel Doodle

Releases are built by `.github/workflows/release.yml` on GitHub Actions.
`release.json` holds the current version, release title and notes file. Pushing
`main` with a change to `release.json`, `core/version.h` or `docs/releases/**`
publishes that version once; other pushes do not run the release workflow.

## Prepare the version

1. Update `core/version.h`:

   ```c
   #define PD_VERSION "0.24.0"
   ```

2. Update `release.json`:

   ```json
   {
     "version": "0.24.0",
     "title": "Timeline moves and Menu cleanup",
     "notes": "docs/releases/0.24.0.md"
   }
   ```

3. Add user-facing notes at the referenced path. The workflow uses that file as
   the release description and appends `SHA256SUMS.txt`.
4. Run the normal local checks (`scripts/build.ps1` private then standard) and
   verify the packages on hardware before publishing.
5. Commit and push to `main`. The workflow builds and publishes if
   `v<version>` has no GitHub release yet; otherwise it reports that the
   version is already released and skips the builds.

Tag pushes are also accepted. A tag must match `release.json` exactly
(`v0.24.0` for version `0.24.0`). Manual runs from the Actions tab use the
current `release.json`.

The repository must allow Actions to write releases (Settings > Actions >
General > Workflow permissions, or the equivalent organization policy); the
workflow requests `contents: write` for the release job only.

## What the workflow does

- validates `release.json`, `core/version.h` and the notes file;
- skips everything when that version already has a release;
- runs the Windows host suite (`scripts/test-host.ps1`);
- builds the standard 3DSX and CIA in the pinned `devkitpro/devkitarm` image,
  then packages the CIA with pinned makerom 0.18.4 and bannertool 1.2.0;
- builds the standard NDS in the pinned BlocksDS v1.24.0 image;
- verifies CIA title/version, NDS headers/DLDI and embedded icons;
- creates `v<version>` at the pushed commit if needed, and publishes
  `PixelDoodle.3dsx`, `PixelDoodle.cia`, `PixelDoodle.nds` and
  `SHA256SUMS.txt` with the notes as the release body.

Only standard builds are published. Private builds embed the webhook and TLS
state and must never be uploaded by this workflow.

## Version rules

- `release.json` and `core/version.h` must agree, and the version must be
  `MAJOR.MINOR.PATCH`.
- CIA metadata packs `major << 10 | minor << 4 | patch`, so major/minor must be
  0-63 and patch 0-15. `scripts/build-cia-unix.sh` enforces this.
- Never replace assets under an existing tag. Bump the version and push again.
- A tag that exists on another commit, or a version that already has a release,
  fails rather than republishing.

## Pinned build inputs

- 3DS toolchain: `devkitpro/devkitarm` image digest
  `sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5`
- DS toolchain: `skylyrac/blocksds` v1.24.0 digest
  `sha256:779f6193d1030c62e60d1f0f39c3e32cf4045c0bd224a45c0ceadf9d9ab01a51`
- makerom 0.18.4 Ubuntu x86_64, SHA-256
  `dd596854718c195c6e3229286be485b122921715555af8ae5cf8e9a465d9f970`
- bannertool 1.2.0 Linux x86_64, SHA-256
  `69768596f836acb3e3aeaa66e47c6ba560dde813c6dfcd33c8afc25fe29b7524`

Changing any of these is a release-affecting change; rebuild and re-verify both
artifacts before pushing a new version.

## Linux build equivalents

The workflow uses the same source lists and flags as the Windows scripts:

```bash
# inside devkitpro/devkitarm
bash scripts/build-3ds-unix.sh

# on a Linux host with bannertool and makerom in PATH, after
# python3 scripts/banner.py build/3ds
bash scripts/build-cia-unix.sh

# NDS, from the repository root
docker run --rm --network none \
  --mount "type=bind,source=$PWD,target=/work" --workdir /work \
  skylyrac/blocksds@sha256:779f6193d1030c62e60d1f0f39c3e32cf4045c0bd224a45c0ceadf9d9ab01a51 \
  make -B -f Makefile.nds -j2 PD_PRIVATE_BUILD=0
```
