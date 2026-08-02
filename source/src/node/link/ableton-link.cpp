#ifdef HAVE_ABLETON_LINK

#include "signalflow/core/core.h"
#include "signalflow/core/graph.h"
#include "signalflow/node/link/ableton-link.h"

#include <ableton/Link.hpp>

#include <cmath>

namespace signalflow
{

namespace
{
/*--------------------------------------------------------------------------------
 * The single, process-wide Ableton Link session shared by all AbletonLink* nodes.
 *
 * Created and enabled lazily on first use via a function-local static, so that
 * merely importing/linking signalflow opens no sockets: the network thread starts
 * only when the first real-time (use_link_clock=1) AbletonLink* node runs. C++
 * guarantees the initialisation is thread-safe and happens exactly once; the
 * session lives for the remainder of the process and is torn down (its network
 * thread joined) at exit. Deterministic-mode nodes never call this, so offline
 * rendering and unit tests never touch the network.
 *-------------------------------------------------------------------------------*/
ableton::Link &signalflow_shared_link()
{
    static ableton::Link link(120.0);
    static const bool _enabled = (link.enable(true), true);
    (void) _enabled;
    return link;
}
}

AbletonLinkNode::AbletonLinkNode(int quantum)
    : quantum(quantum), use_link_clock(1), beat_accumulator(0.0)
{
    SIGNALFLOW_CHECK_GRAPH();

    /*--------------------------------------------------------------------------------
     * Driven by a global session rather than a signal input: no inputs, mono output.
     * The graph up-mixes to more channels downstream if needed.
     *-------------------------------------------------------------------------------*/
    this->set_channels(0, 1);

    this->create_property("quantum", this->quantum);
    this->create_property("use_link_clock", this->use_link_clock);
}

double AbletonLinkNode::get_quantum()
{
    double q = this->quantum->float_value();
    if (q < 1.0)
        q = 1.0;
    return q;
}

void AbletonLinkNode::capture_link_frame(int num_frames, double &beat0, double &phase0, double &tempo, double &bps)
{
    double sample_rate = this->graph->get_sample_rate();
    double q = this->get_quantum();
    bool follow_link = this->use_link_clock->int_value() != 0;

    if (follow_link)
    {
        /*----------------------------------------------------------------------------
         * Real time: read tempo/beat/phase from the shared Link session at the current
         * host time. Output latency is not compensated (a few ms), which is acceptable
         * for a follower; a future refinement could offset by output_buffer_size/Fs.
         *---------------------------------------------------------------------------*/
        ableton::Link &link = signalflow_shared_link();
        ableton::Link::SessionState state = link.captureAudioSessionState();
        std::chrono::microseconds now = link.clock().micros();
        tempo = state.tempo();
        beat0 = state.beatAtTime(now, q);
        phase0 = state.phaseAtTime(now, q);
        bps = tempo / 60.0 / sample_rate;
    }
    else
    {
        /*----------------------------------------------------------------------------
         * Deterministic: ignore Link entirely and advance beats by exactly num_frames
         * per buffer at a fixed 120 BPM, so offline renders are reproducible.
         *---------------------------------------------------------------------------*/
        tempo = 120.0;
        bps = tempo / 60.0 / sample_rate;
        beat0 = this->beat_accumulator;
        phase0 = std::fmod(this->beat_accumulator, q);
        if (phase0 < 0)
            phase0 += q;
        this->beat_accumulator += num_frames * bps;
    }
}

AbletonLinkClock::AbletonLinkClock(int quantum, int ticks_per_beat)
    : AbletonLinkNode(quantum), ticks_per_beat(ticks_per_beat), last_index(0), seeded(false)
{
    this->name = "ableton-link-clock";
    this->create_property("ticks_per_beat", this->ticks_per_beat);
}

void AbletonLinkClock::process(Buffer &out, int num_frames)
{
    double beat0, phase0, tempo, bps;
    this->capture_link_frame(num_frames, beat0, phase0, tempo, bps);

    int tpb = this->ticks_per_beat->int_value();
    if (tpb < 1)
        tpb = 1;

    for (int frame = 0; frame < num_frames; frame++)
    {
        double beat = beat0 + frame * bps;
        int64_t index = (int64_t) std::floor(beat * tpb);

        if (!this->seeded)
        {
            /*----------------------------------------------------------------------------
             * Seed the tick index on the first frame ever processed.
             *  - Real time: seed to the current index, so joining a session mid-beat does
             *    not emit a phantom tick at an arbitrary phase; the follower waits for the
             *    next subdivision boundary.
             *  - Deterministic: seed one below, so beat 0 emits a tick at sample 0,
             *    matching Impulse (whose first output sample is a tick).
             *---------------------------------------------------------------------------*/
            this->last_index = (this->use_link_clock->int_value() != 0) ? index : index - 1;
            this->seeded = true;
        }

        /*--------------------------------------------------------------------------------
         * Emit a tick only when the subdivision index strictly increases. Always store
         * the latest index, so a backward jump in the Link timeline (e.g. a peer joining
         * and re-anchoring the beat) resyncs silently rather than firing a spurious tick.
         *-------------------------------------------------------------------------------*/
        sample rv = 0;
        if (index > this->last_index)
            rv = 1;
        this->last_index = index;

        out[0][frame] = rv;
    }
}

AbletonLinkPhase::AbletonLinkPhase(int quantum)
    : AbletonLinkNode(quantum)
{
    this->name = "ableton-link-phase";
}

void AbletonLinkPhase::process(Buffer &out, int num_frames)
{
    double beat0, phase0, tempo, bps;
    this->capture_link_frame(num_frames, beat0, phase0, tempo, bps);
    double q = this->get_quantum();

    for (int frame = 0; frame < num_frames; frame++)
    {
        double p = std::fmod(phase0 + frame * bps, q);
        if (p < 0)
            p += q;
        out[0][frame] = (sample) (p / q);
    }
}

AbletonLinkBeat::AbletonLinkBeat(int quantum)
    : AbletonLinkNode(quantum)
{
    this->name = "ableton-link-beat";
}

void AbletonLinkBeat::process(Buffer &out, int num_frames)
{
    double beat0, phase0, tempo, bps;
    this->capture_link_frame(num_frames, beat0, phase0, tempo, bps);

    for (int frame = 0; frame < num_frames; frame++)
    {
        out[0][frame] = (sample) std::floor(beat0 + frame * bps);
    }
}

AbletonLinkTempo::AbletonLinkTempo(int quantum)
    : AbletonLinkNode(quantum)
{
    this->name = "ableton-link-tempo";
}

void AbletonLinkTempo::process(Buffer &out, int num_frames)
{
    double beat0, phase0, tempo, bps;
    this->capture_link_frame(num_frames, beat0, phase0, tempo, bps);

    for (int frame = 0; frame < num_frames; frame++)
    {
        out[0][frame] = (sample) tempo;
    }
}

}

#endif
