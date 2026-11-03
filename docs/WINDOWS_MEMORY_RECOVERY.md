# Windows contiguous-memory recovery (2026-11-03)

The reported Exhibition → Three in a Row loading freeze exhausted the emulated
graphics pool: repeated allocation failures showed 64384 of 65408 KiB in use.
The remaining low-address reserve was unavailable to those requests. The
runtime ignored graphics-memory frees unless `RECOMP_HEAP_RECLAIM` was enabled.
The supplied archive contained an automatic hang dump and an earlier slowdown
mark with its dump and screenshots; the hang detector did operate.

Ported NHL Hitz Pro 1.0.8's deferred-reclaim policy into the project override:
remember title-freed contiguous blocks, keep their contents intact while space
remains, then release those blocks and retry once if allocation would fail.
Live allocations are never reclaimed. Alignment and physical-range limits
apply on both attempts. Immediate reclaim remains an explicit diagnostic option;
the general heap is unchanged. This policy postpones reuse; it adds no GPU fence.
Consecutive gameplay still needs validation for asynchronous lifetime issues.

`RECOMP_CONTIG_CAP_KB` can shrink the arena for developer reproduction. It is
unset in ordinary play. `[CONTIG] arena full: reused ...` records recovery;
`arena exhausted` still means a request could not be satisfied after retry.

The actual-runtime `hitz_contiguous_reclaim_test` uses a 64 KiB cap to verify
preserved bytes before pressure, recovery, duplicate frees, live-block protection,
alignment/range limits, honest failure and 200 reuse cycles. Deferred and
explicit immediate modes pass; the NIS lifetime test passes. Normal and packaged
debug builds compile. Full Exhibition → mini-game and repeated matches on the
reporting laptop have not yet been replayed with the fix.

Video logs now record `[VIDEO]` snapshots at startup, device readiness,
settings changes, fullscreen changes and render-target rebuilds. They include
local time, monotonic ticks, frame counts, requested/applied scale and MSAA,
supported MSAA, AF, fullscreen, stretch, VSync, cap/override and pack state.
Change records show old/new values. Logging runs only on events. An isolated
debug UI test confirmed 1x → 2x, later render-target application and F11 logging.

The original report's slowdown mark was at 5x with 8x MSAA on a GTX 660M;
the log confirms 1x was applied later, before the loading hang. The graphics-pool
leak is separate from that earlier high-resolution performance observation.
Original dumps and matching executable/PDB were preserved locally before rebuild.
