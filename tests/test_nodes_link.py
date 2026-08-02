from signalflow import *
from . import graph

import signalflow
import numpy as np
import pytest

#--------------------------------------------------------------------------------
# The Ableton Link nodes are only compiled when signalflow is built with
# -DHAVE_ABLETON_LINK=ON, so skip the whole module otherwise.
#--------------------------------------------------------------------------------
pytestmark = pytest.mark.skipif(not hasattr(signalflow, "AbletonLinkClock"),
                                reason="Ableton Link support not built (requires -DHAVE_ABLETON_LINK=ON)")

#--------------------------------------------------------------------------------
# All tests run in deterministic mode (use_link_clock=0), which ignores the
# network Link session and advances the beat by exactly num_frames per buffer at
# a fixed 120 BPM. This makes offline renders reproducible.
#
# A sample rate of 64 makes the maths exact: beats-per-sample = 120/60/64 = 1/32
# (a power of two), so one beat is exactly 32 samples and one bar (quantum=4) is
# exactly 128 samples. render_subgraph() renders output_buffer_size (2048) frames,
# i.e. exactly 16 bars.
#--------------------------------------------------------------------------------
Fs = 64
BPS = 120.0 / 60.0 / Fs        # beats per sample = 1/32
BEAT_SAMPLES = int(Fs * 60 / 120)  # 32 samples per beat


def _deterministic(node):
    node.set_property("use_link_clock", 0)
    return node


def test_nodes_link_clock(graph):
    graph.sample_rate = Fs
    clock = _deterministic(AbletonLinkClock(quantum=4, ticks_per_beat=1))
    graph.render_subgraph(clock)
    out = clock.output_buffer[0]
    N = len(out)

    #--------------------------------------------------------------------------------
    # One tick per beat, starting at sample 0 (like Impulse's first sample).
    #--------------------------------------------------------------------------------
    indices = np.where(out == 1)[0]
    expected = np.arange(0, N, BEAT_SAMPLES)
    assert np.array_equal(indices, expected)
    # Ticks are single-sample impulses: everything else is zero.
    assert np.count_nonzero(out) == len(expected)


def test_nodes_link_clock_ticks_per_beat(graph):
    graph.sample_rate = Fs
    clock1 = _deterministic(AbletonLinkClock(quantum=4, ticks_per_beat=1))
    clock2 = _deterministic(AbletonLinkClock(quantum=4, ticks_per_beat=2))
    graph.render_subgraph(clock1)
    graph.render_subgraph(clock2)

    indices1 = np.where(clock1.output_buffer[0] == 1)[0]
    indices2 = np.where(clock2.output_buffer[0] == 1)[0]

    #--------------------------------------------------------------------------------
    # ticks_per_beat=2 subdivides each beat: twice as many ticks, and the
    # once-per-beat ticks are exactly every other subdivision.
    #--------------------------------------------------------------------------------
    assert len(indices2) == 2 * len(indices1)
    assert np.array_equal(indices2[::2], indices1)
    assert np.array_equal(np.diff(indices2), np.full(len(indices2) - 1, BEAT_SAMPLES // 2))


def test_nodes_link_beat(graph):
    graph.sample_rate = Fs
    beat = _deterministic(AbletonLinkBeat(quantum=4))
    graph.render_subgraph(beat)
    out = beat.output_buffer[0]
    N = len(out)

    #--------------------------------------------------------------------------------
    # Held integer beat number, incrementing every BEAT_SAMPLES samples: 0,0,..,1,1,..
    #--------------------------------------------------------------------------------
    expected = np.floor(np.arange(N) * BPS)
    assert np.array_equal(out, expected)
    # Sanity: starts at beat 0, integer-valued, monotonically non-decreasing.
    assert out[0] == 0
    assert np.array_equal(out, np.floor(out))
    assert np.all(np.diff(out) >= 0)


def test_nodes_link_phase(graph):
    graph.sample_rate = Fs
    quantum = 4
    phase = _deterministic(AbletonLinkPhase(quantum=quantum))
    graph.render_subgraph(phase)
    out = phase.output_buffer[0]
    N = len(out)
    bar_samples = quantum * BEAT_SAMPLES  # 128

    #--------------------------------------------------------------------------------
    # Ramp from 0 up towards 1 over each bar, wrapping back to 0 at the bar boundary.
    #--------------------------------------------------------------------------------
    expected = np.fmod(np.arange(N) * BPS, quantum) / quantum
    assert np.allclose(out, expected)

    # Always within [0, 1).
    assert np.all(out >= 0.0)
    assert np.all(out < 1.0)

    # The ramp decreases only at bar boundaries (a wrap from ~1 back to 0).
    neg_diffs = np.where(np.diff(out) < 0)[0]
    expected_wraps = np.arange(bar_samples - 1, N - 1, bar_samples)
    assert np.array_equal(neg_diffs, expected_wraps)


def test_nodes_link_tempo(graph):
    graph.sample_rate = Fs
    tempo = _deterministic(AbletonLinkTempo(quantum=4))
    graph.render_subgraph(tempo)
    out = tempo.output_buffer[0]

    #--------------------------------------------------------------------------------
    # Deterministic mode reports a constant 120 BPM.
    #--------------------------------------------------------------------------------
    assert np.all(out == 120.0)


def test_nodes_link_composability(graph):
    graph.sample_rate = Fs

    #--------------------------------------------------------------------------------
    # The clock is a drop-in Impulse-like trigger, so it composes directly with the
    # existing sequencing ecosystem: ClockDivider thins it, Euclidean patterns it.
    #--------------------------------------------------------------------------------
    clock = _deterministic(AbletonLinkClock(quantum=4, ticks_per_beat=1))
    divider = ClockDivider(clock, 2)
    euclidean = Euclidean(divider, 16, 5)
    graph.render_subgraph(euclidean)

    clock_ticks = np.where(clock.output_buffer[0] == 1)[0]
    divider_ticks = np.where(divider.output_buffer[0] == 1)[0]
    euclidean_onsets = np.where(euclidean.output_buffer[0] > 0)[0]

    # ClockDivider(clock, 2) keeps every second clock tick.
    assert np.array_equal(divider_ticks, clock_ticks[::2])

    # Euclidean fires only on divided-clock ticks (its onsets align to the clock).
    assert len(euclidean_onsets) > 0
    assert set(euclidean_onsets.tolist()).issubset(set(divider_ticks.tolist()))
