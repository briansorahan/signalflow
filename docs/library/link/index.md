[Reference library](../index.md) > [Link](index.md)

# Link

- **[AbletonLinkClock](abletonlinkclock/index.md)**: Emits a single-sample impulse (value 1) at each beat subdivision of the Ableton Link session, and 0 at all other times. With `ticks_per_beat` = 1 this ticks once per beat; higher values subdivide the beat. Behaves like Impulse, so it can drive ClockDivider, Euclidean, Counter, ASREnvelope(clock=...), BufferPlayer(clock=...), etc.
- **[AbletonLinkPhase](abletonlinkphase/index.md)**: Outputs the phase of the Ableton Link session as a ramp from 0 to 1 over each bar of `quantum` beats, wrapping back to 0 at the start of each bar.
- **[AbletonLinkBeat](abletonlinkbeat/index.md)**: Outputs the current beat number of the Ableton Link session as a held integer value that increments at each beat.
- **[AbletonLinkTempo](abletonlinktempo/index.md)**: Outputs the current tempo of the Ableton Link session, in beats per minute.
