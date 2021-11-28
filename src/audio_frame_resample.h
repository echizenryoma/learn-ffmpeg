#pragma once

#include <memory>
#include <vector>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

using namespace std;

namespace ryoma {

class AudioFrameResample {
 public:
  explicit AudioFrameResample(const AVCodecContext* audio_codec_ctx, int sample_rate = 44100,
                              AVSampleFormat sample_fmt = AV_SAMPLE_FMT_S16);

  const vector<uint8_t>& Resample(AVFrame* frame);

 private:
  void Init();

 private:
  const AVCodecContext* audio_codec_ctx_;
  int sample_rate_ = 0;
  AVSampleFormat sample_fmt_;

  shared_ptr<SwrContext> sws_ctx_;
  vector<uint8_t> target_frame_buff_;
};

}  // namespace ryoma