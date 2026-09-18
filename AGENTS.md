# Agent Instructions

## Documentation

- If a change affects functionality (new features, changed behavior, altered
  UI flows, etc.), update `docs/functional-spec.md` to match in the same
  change.

## UI Layout

- When laying out a screen as a grid of tiles (e.g. `main/ui_settings.c`'s
  settings/submenu screens), choose column and row counts N (columns) x M
  (rows) such that `N = M` or `N = M + 1`. Avoid a single-row `Nx1` grid for
  more than a couple of tiles — with only one row, each tile stretches to the
  screen's full height while getting only a narrow fraction of its width,
  leaving tiles tall and narrow, so button text doesn't fit or wraps
  awkwardly. Prefer a landscape-biased near-square grid (slightly more
  columns than rows) instead, leaving any leftover cells empty rather than
  stretching remaining tiles to fill them.
