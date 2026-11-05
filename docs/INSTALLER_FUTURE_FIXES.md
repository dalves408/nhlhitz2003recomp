# Installed texture pack detection

The game installer includes this detection starting with 1.0.3. The
standalone add-on source also has the detection logic. The published 1.0.1
texture asset remains the shared payload.

## Recognize the currently installed texture pack

Game Setup checks the selected game directory before building its optional-task
list. If the current texture pack is already installed, the download checkbox is
omitted and the Ready page says the existing pack will be kept. Changing the
selected game folder causes that folder to be checked rather than relying on a
cached result from another installation.

The standalone addon also checks the destination before touching a BIN or the
download cache. A matching installation shows "Texture pack already installed"
and finishes without downloading or extracting texture data. Its configuration
utility can still be updated, and normal legacy-registration migration remains
available. It does not ask to remove a cache that this run did not use.

Release identity is anchored to the expected SHA-256 of `texture_pack/manifest.txt`
and its file count, pinned alongside the payload URL/hash in `texture_payload.iss`.
Every listed DDS must also exist at its expected size. A folder name, editable
version label or leftover metadata alone is insufficient. Extra custom files
are allowed. Older manifests, missing files and truncated files still permit
installation and repair.

This is a fast release/completeness check, not a rehash of every installed DDS.
Same-size content modifications are intentionally not inspected. For an explicit
repair even when the metadata and sizes match, launch the updated installer with
`/REPAIRTEXTURES=1`. Game Setup additionally requires selecting its texture task
(or `/TASKS=hires`); the standalone addon performs the requested repair directly.
Repair reinstalls from the size/SHA-256-verified BIN and preserves unrelated files.

Before publishing a changed pack, update `TextureManifestSHA256` and
`TextureFileCount` as well as the payload URL, SHA-256 and byte sizes. The payload
manifest JSON already records both identity fields.

## Validation

Private installer tests passed current-pack task suppression, standalone
no-download/no-cache completion, older manifests, missing/truncated files,
explicit repair, cross-installer resume, cache retention/deletion, failure
preservation and settings/custom-file protection. The real 6,198-file pack was
recognized read-only without a BIN in approximately 704 ms on this host. Another
game directory with no installed pack was rejected. This is a host observation,
not a general timing guarantee.

Evidence: `analysis/texture-payload-upload/installers-890d7eb6` and
`analysis/texture-payload-upload/current-check-64120a0d`.
