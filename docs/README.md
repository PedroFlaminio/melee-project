# Documentation

Everything in this folder is about the native PC port. Documentation is written in English.

## Planning and status

- [**Native PC port plan**](native_pc_port_plan.md) — the project plan: scope, architecture,
  technical decisions, verification strategy, roadmap and risks. Start here.
- [**Native port status**](native_port_status.md) — what is done, what is in progress, the next
  gates, and the current limitations. The limitations section is the one to read before
  assuming something works.
- [**MVP progress**](port-mvp-progress.md) — the MVP definition, a per-area estimate, the
  verified evidence behind it and the change log.
- [**Port progress (characters and stages)**](port-characters-stages.md) — per-character and
  per-stage table.

## Working on the port

- [**Native port development**](native_port_development.md) — how to build, run and debug: the
  presets, the diagnostic commands (`--run-modes`, `--view-*`, `--play`), and a long list of
  failure patterns with the symptom that identifies each one. The practical reference.
- [**Native fight flow**](fight_flow_port.md) — how the local match was reached, blocker by
  blocker, and what each translator unlocked.
- [**Glossary**](glossary.md) — the abbreviations used throughout the Melee source.

## Design notes and reviews

- [**Video and presentation plan**](video_enhancements_plan.md) — separating the 60 Hz
  simulation from presentation: resolution, aspect, filter, window mode and rate.
- [**Melee Unlocked research**](melee_unlocked_research.md) — what can be reused from that
  project as reference, tests and implementation examples, and what cannot.
- [**Technical review from `fa257ed`**](review-fa257ed.md) — a point-in-time review of a commit
  range, its findings and what was done about each one.
