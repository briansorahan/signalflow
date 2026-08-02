#ifdef HAVE_ABLETON_LINK

#pragma once

#include "signalflow/node/node.h"

#include <cstdint>

namespace signalflow
{

/**--------------------------------------------------------------------------------*
 * Abstract base for nodes that follow an Ableton Link session.
 *
 * All AbletonLink* nodes read from a single, process-wide Link session (see
 * signalflow_shared_link()), which is enabled lazily on first construction and
 * synchronises tempo/beat/phase with other Link-enabled apps on the local network.
 * These nodes are follow-only: they never modify the shared session.
 *
 * `quantum` is the number of beats per bar/cycle (used for phase alignment).
 * `use_link_clock` selects the time source: 1 (default) follows Link's real-time
 * host clock; 0 advances beats deterministically by num_frames each buffer, which
 * is required for reproducible offline rendering / unit tests (Link's host clock
 * runs at wall-clock rate, so faster-than-real-time offline renders would otherwise
 * map every buffer to nearly the same beat).
 *---------------------------------------------------------------------------------*/
class AbletonLinkNode : public Node
{
protected:
    AbletonLinkNode(int quantum = 4);

    /*--------------------------------------------------------------------------------
     * Capture the session state once per buffer and compute the beat, phase and
     * tempo at frame 0, plus beats-per-sample (bps) for per-frame extrapolation.
     * In deterministic mode this also advances the internal beat accumulator by
     * num_frames. Call once at the top of each subclass's process().
     *-------------------------------------------------------------------------------*/
    void capture_link_frame(int num_frames, double &beat0, double &phase0, double &tempo, double &bps);

    /*--------------------------------------------------------------------------------
     * The quantum (beats per bar), clamped to be at least 1 to avoid division by
     * zero when mapping beats to phase.
     *-------------------------------------------------------------------------------*/
    double get_quantum();

    PropertyRef quantum;
    PropertyRef use_link_clock;

    // Beat position accumulated across buffers, used only in deterministic mode.
    double beat_accumulator;
};

/**--------------------------------------------------------------------------------*
 * Emits a single-sample impulse (value 1) at each beat subdivision of the Ableton
 * Link session, and 0 at all other times. With `ticks_per_beat` = 1 this ticks once
 * per beat; higher values subdivide the beat. Behaves like Impulse, so it can drive
 * ClockDivider, Euclidean, Counter, ASREnvelope(clock=...), BufferPlayer(clock=...), etc.
 *---------------------------------------------------------------------------------*/
class AbletonLinkClock : public AbletonLinkNode
{
public:
    AbletonLinkClock(int quantum = 4, int ticks_per_beat = 1);

    virtual void process(Buffer &out, int num_frames) override;

private:
    PropertyRef ticks_per_beat;

    // Index of the last subdivision tick emitted; ticks fire only when it increases.
    int64_t last_index;
    bool seeded;
};

/**--------------------------------------------------------------------------------*
 * Outputs the phase of the Ableton Link session as a ramp from 0 to 1 over each
 * bar of `quantum` beats, wrapping back to 0 at the start of each bar.
 *---------------------------------------------------------------------------------*/
class AbletonLinkPhase : public AbletonLinkNode
{
public:
    AbletonLinkPhase(int quantum = 4);

    virtual void process(Buffer &out, int num_frames) override;
};

/**--------------------------------------------------------------------------------*
 * Outputs the current beat number of the Ableton Link session as a held integer
 * value that increments at each beat.
 *---------------------------------------------------------------------------------*/
class AbletonLinkBeat : public AbletonLinkNode
{
public:
    AbletonLinkBeat(int quantum = 4);

    virtual void process(Buffer &out, int num_frames) override;
};

/**--------------------------------------------------------------------------------*
 * Outputs the current tempo of the Ableton Link session, in beats per minute.
 *---------------------------------------------------------------------------------*/
class AbletonLinkTempo : public AbletonLinkNode
{
public:
    AbletonLinkTempo(int quantum = 4);

    virtual void process(Buffer &out, int num_frames) override;
};

REGISTER(AbletonLinkClock, "ableton-link-clock")
REGISTER(AbletonLinkPhase, "ableton-link-phase")
REGISTER(AbletonLinkBeat, "ableton-link-beat")
REGISTER(AbletonLinkTempo, "ableton-link-tempo")

}

#endif
