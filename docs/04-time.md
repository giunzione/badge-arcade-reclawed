# 04 – Time

## Date format

The game uses the SDK `DateTime` type: **milliseconds since 2000-01-01 00:00**, as a signed 64-bit value.

- `0x2ABAC4(out, y, m, d, h, min, s, ms)` builds one: `days × 86 400 000 + h × 3 600 000 + …`
- `0x25B840(out, YYYYMMDD)` converts the integer dates found in `Schedule.xml`
- `0x2B0300(out)` reads the **local RTC** and is the root clock
- `0x25C430(out, mgr)` is the "current time" used by game logic. It uses server time when available and falls back to `0x2B0300`, then adds a per-manager offset

## The schedule

SpotPass delivered one week of content at a time. It is cached in extdata as `boss.sarc`, a SARC archive containing `Schedule.xml`, which has entries such as:

```
DefaultStage  20180713 → 20180714   DefaultStageName000 … 029 = machine IDs
BonusStage    20180713 → 20180714   Practice Catcher machine
PrizeCollection 20180713 → 20180720 sharc/180713_180720.sarc
```

Every entry has a date range. Outside it, nothing shows up. With the clock set to today, the hall is empty.

## Clock remap (patch 11)

The final store of `0x2B0300` is redirected to a cave that maps any time into the cached week, keeping the weekday and the time of day:

```
ANCHOR = first day of the cached week, 00:00   (e.g. 2018-07-13, a Friday)
WEEK   = 604 800 000 ms
t' = ANCHOR + ((t − ANCHOR) mod WEEK)
```

The ARM11 has no hardware divide, so the modulo is done by repeated subtraction: at most a few hundred iterations for current dates.

### Which week: set per console

Every console has the week it last downloaded, so the anchor can't be a constant. The cave reads it from the patch's **settings block** (patch 16), and the installer app writes the console's own week there before it saves `code.ips`:

1. It opens the game's SD extdata (EUR `0x1536`, USA `0x1535`) and reads `boss.sarc`'s header and its `Schedule.xml`, not the whole 25 MB archive.
2. The week's first day is the `<DateStartText>` of the `PrizeCollection` entry. Without one, it's the earliest `DefaultStage` start.
3. It counts the `DefaultStage` entries in the 7 days from there. The two dumps checked so far both have 30 machines a day, every day: EUR `2018-07-13`, USA `2019-01-10`.

There is no date in the TOMLs or the release files. As built, the block holds anchor 0 and catalog mode 1, the game's built-in machines ([02 – Boot sequence](02-boot-sequence.md#loadingdata-and-the-catalog-mode)): the one setting that works on any console. A patch copied by hand from a release zip runs like that. The installer switches it to mode 0 with the console's week whenever `boss.sarc` is usable, and leaves mode 1 otherwise.

The built-in schedule runs from 2000 to 2030. In mode 1 the clock is still folded into the week from the anchor (2000-01-01 when there is none), so it always falls inside that range.

### Side effects

- Every Thursday → Friday midnight the clock jumps back six days. Any "once per day" logic that checks *later than last time* may misbehave once a week.
- Timestamps written to save data are 2018 dates.
