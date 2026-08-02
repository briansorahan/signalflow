title: AbletonLinkClock node documentation
description: AbletonLinkClock: Emits a single-sample impulse (value 1) at each beat subdivision of the Ableton Link session, and 0 at all other times. With `ticks_per_beat` = 1 this ticks once per beat; higher values subdivide the beat. Behaves like Impulse, so it can drive ClockDivider, Euclidean, Counter, ASREnvelope(clock=...), BufferPlayer(clock=...), etc.

[Reference library](../../index.md) > [Link](../index.md) > [AbletonLinkClock](index.md)

# AbletonLinkClock

```python
AbletonLinkClock(quantum=4, ticks_per_beat=1)
```

Emits a single-sample impulse (value 1) at each beat subdivision of the Ableton Link session, and 0 at all other times. With `ticks_per_beat` = 1 this ticks once per beat; higher values subdivide the beat. Behaves like Impulse, so it can drive ClockDivider, Euclidean, Counter, ASREnvelope(clock=...), BufferPlayer(clock=...), etc.

