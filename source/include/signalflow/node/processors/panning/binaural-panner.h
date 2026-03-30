#pragma once

#include "signalflow/node/node.h"

#include "signalflow/node/fft/fft.h"
#include "signalflow/node/fft/ifft.h"
#include "signalflow/node/fft/processors/fft-convolve.h"

#include <mysofa.h>


namespace signalflow
{
/**--------------------------------------------------------------------------------*
 * Binaural panner.
 *---------------------------------------------------------------------------------*/
class BinauralPanner : public Node
{
public:
    BinauralPanner(NodeRef input = 0, NodeRef azimuth = 0.0, NodeRef elevation = 0.0);

    virtual void process(Buffer &out, int num_frames) override;

    NodeRef input;
    NodeRef azimuth;
    NodeRef elevation;

private:
    struct MYSOFA_EASY *hrtf = NULL;

    NodeRef fft_l, fft_r;
    NodeRef convolve_l, convolve_r;
    NodeRef ifft_l, ifft_r;
    float left_ir[2048];
    float right_ir[2048];
    BufferRef left_ir_buffer;
    BufferRef right_ir_buffer;
    float left_delay_seconds;
    float right_delay_seconds;


};

REGISTER(BinauralPanner, "binaural-panner")
}
