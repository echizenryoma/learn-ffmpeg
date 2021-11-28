#include "audio_frame_resample.h"

namespace ryoma {

ryoma::AudioFrameResample::AudioFrameResample(const AVCodecContext* audio_codec_ctx,
                                              int sample_rate, AVSampleFormat sample_fmt)
    : audio_codec_ctx_(audio_codec_ctx), sample_rate_(sample_rate), sample_fmt_(sample_fmt) {
  Init();
}

void AudioFrameResample::Init() {
  sws_ctx_.reset(
      swr_alloc_set_opts(nullptr, audio_codec_ctx_->channel_layout, sample_fmt_, sample_rate_,
                         audio_codec_ctx_->channel_layout, audio_codec_ctx_->sample_fmt,
                         audio_codec_ctx_->sample_rate, 0, nullptr),
      [](SwrContext*& ptr) { swr_free(&ptr); });
  swr_init(sws_ctx_.get());
  int target_frame_buff_size = av_samples_get_buffer_size(
      nullptr, audio_codec_ctx_->channels, audio_codec_ctx_->frame_size, sample_fmt_, 1);
  target_frame_buff_.resize(target_frame_buff_size);
}

const vector<uint8_t>& AudioFrameResample::Resample(AVFrame* frame) {
  uint8_t* audio_buff = target_frame_buff_.data();
  int ret = swr_convert(sws_ctx_.get(), &audio_buff, target_frame_buff_.size(),
                        (const uint8_t**)frame->data, frame->nb_samples);
  if (ret < 0) {
    return {};
  }
  return target_frame_buff_;
}

}  // namespace ryoma