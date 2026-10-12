# Technical documentation

How Nintendo Badge Arcade (EUR v1.3.1) works internally, and what the patch changes.

| Page | Topic |
|---|---|
| [01 – Overview](01-overview.md) | Method, tools, memory layout, list of patches |
| [02 – Boot sequence](02-boot-sequence.md) | The 61-state title-screen state machine |
| [03 – Hall and cranes](03-hall-and-cranes.md) | Hall network states, server requests, free plays |
| [04 – Time](04-time.md) | Date format, the schedule, clock remapping |
| [05 – HOME badges](05-home-badges.md) | Badge box file format, badge ownership, debug menu |

All addresses are **virtual addresses** in the decompressed `.code` of the v1.3.1 update (title `0004000E00153600`). IPS offsets are `address − 0x100000`.

These notes describe behaviour and give addresses. They intentionally do not reproduce the game's code.
