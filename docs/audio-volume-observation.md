# Audio volume observation

`AudioVolumeObserver` is a hook-free owner-thread observer. Start resolves the engine
pointer slot in executable game code using a unique masked signature:

```text
48 8B 05 ?? ?? ?? ?? C5 FB 10 B0 C8 00 00 00 8B 98 D0 00 00 00 48 8B CE
```

The signed RIP displacement at +3 is relative to the end of the seven-byte MOV.
The resolved slot must be aligned and inside the game image. Missing or ambiguous
matches leave volume unknown; there is no fallback to a fixed RVA or heap address.

## Current layout

From the engine, +0x1070 resolves the settings manager. Manager fields +0x50 and
+0x68 point to inner option objects (native base +0x28). Each option's +0x70 points
to its inner audio object (native base +0x28).

| Audio native field | Meaning |
| --- | --- |
| +0x10 | Native option owner |
| +0x78 / +0xD8 | Integer-property vtables |
| +0x88 / +0xE8 | Native audio owner |
| +0xA0 / +0x100 | Master/effects name string objects |
| +0xD0 / +0x130 | Master/effects signed integer percentages |

String objects point to exact NUL-terminated names
`UI_GameSetting_Sound_MasterVolume` and `UI_GameSetting_Sound_SFXVolume`.
Percentages must be in [0, 100]. All traversed object vtables must belong to the
image; both integer properties must share their vtable. Every pointer is reread.
The nearby +0xCC / +0x12C fields are upper bounds, not current values.

The two option sets update at different times. Sampling requires their volumes to
agree and two complete snapshots, including object identities, to match. Any failed
read or validation produces unknown. Sampling runs at most once every 50 ms.
This reduces exposure to intermediate updates; it is not an atomic game snapshot.

## Playback and validation

The application applies linear gain `master_percent * effects_percent / 10000` to
fresh PCM bytes for every notification, additionally multiplied by the configured
`[sound] volume_percent / 100` (default 100; zero mutes notifications). No native audio function is called and no
mixer setting is changed. Wwise's exact slider gain curve is not established.
Unknown or muted samples stop sound; a skipped notification is consumed.
Nonzero changes affect subsequent sounds. WinMM playback remains process-wide.

Cheat Engine probing verified independent changes of both sliders, menu exits and
resolution after a game restart with different heap addresses, including 50/25.
These observations do not establish compatibility with later game versions; the
signature and field layout remain game-specific. Tests cover missing reads, ownership,
property names, percentage bounds, torn snapshots, signature uniqueness, PCM scaling
and notification suppression. The compiled integration still needs in-game playback
verification at 100/100, reduced master/effects, and either slider at zero, including
an optional custom WAV.
