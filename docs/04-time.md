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
ANCHOR = 2018-07-13 00:00   (a Friday, start of the cached week)
WEEK   = 604 800 000 ms
t' = ANCHOR + ((t − ANCHOR) mod WEEK)
```

The ARM11 has no hardware divide, so the modulo is done by repeated subtraction: at most a few hundred iterations for current dates.

The anchor is hard-coded for the week found on the author's console. If your `boss.sarc` covers a different week, the anchor has to change: read `DateFrom` in your `Schedule.xml`.

### Side effects

- Every Thursday → Friday midnight the clock jumps back six days. Any "once per day" logic that checks *later than last time* may misbehave once a week.
- Timestamps written to save data are 2018 dates.
