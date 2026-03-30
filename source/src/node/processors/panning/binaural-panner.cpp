#include "signalflow/node/processors/panning/binaural-panner.h"
#include "signalflow/core/graph.h"

namespace signalflow
{

const int N = 1024;

BinauralPanner::BinauralPanner(NodeRef input, NodeRef azimuth, NodeRef elevation)
    : input(input), azimuth(azimuth), elevation(elevation)
{
    this->name = "binaural-panner";
    this->set_channels(1, 2);

    this->create_input("input", this->input);
    this->create_input("azimuth", this->azimuth);
    this->create_input("elevation", this->elevation);

    int err = 0;
    int filter_length;
    hrtf = mysofa_open("P0099_Windowed_48kHz.sofa", this->get_graph()->get_sample_rate(), &filter_length, &err);
    // hrtf = mysofa_open("MIT_KEMAR_normal_pinna.sofa", this->get_graph()->get_sample_rate(), &filter_length, &err);

    if (!hrtf)
    {
        return;
    }

    // Zero-pad to 2048, to work around signalflow bug with convolution when filter length < block size
    // (would normally use filter_length)
    memset(left_ir, 0, sizeof(float) * N);
    memset(right_ir, 0, sizeof(float) * N);

    fft_l = new FFT(input, N, N / 2, N / 2, false);
    fft_r = new FFT(input, N, N / 2, N / 2, false);
    left_ir_buffer = new Buffer(1, N);
    right_ir_buffer = new Buffer(1, N);
    convolve_l = new FFTConvolve(fft_l, left_ir_buffer);
    convolve_r = new FFTConvolve(fft_r, right_ir_buffer);
    ifft_l = new IFFT(convolve_l);
    ifft_r = new IFFT(convolve_r);
}


void BinauralPanner::process(Buffer &out, int num_frames)
{
    // Convert from -1..+1 to mysofa's coordinate system, which is in degrees anticlockwise from the X-axis
    sample azimuth = (-1 * this->azimuth->out[0][0]) * 90;
    sample elevation = this->elevation->out[0][0] * 90;

    float coords[] = {azimuth, elevation, 1.0};
    mysofa_s2c(coords);
    mysofa_getfilter_float(hrtf, coords[0], coords[1], coords[2], left_ir, right_ir, &left_delay_seconds, &right_delay_seconds);
    memcpy(left_ir_buffer->data[0], left_ir, sizeof(float) * N);
    memcpy(right_ir_buffer->data[0], right_ir, sizeof(float) * N);

    convolve_l->set_buffer("buffer", left_ir_buffer);
    convolve_r->set_buffer("buffer", right_ir_buffer);

    fft_l->process(num_frames);
    fft_r->process(num_frames);
    convolve_l->process(num_frames);
    convolve_r->process(num_frames);
    ifft_l->process(num_frames);
    ifft_r->process(num_frames);

    memcpy(out[0], ifft_l->out[0], num_frames * sizeof(sample));
    memcpy(out[1], ifft_r->out[0], num_frames * sizeof(sample));
}

}
