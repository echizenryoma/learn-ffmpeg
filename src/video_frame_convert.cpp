#include "video_frame_convert.h"

extern "C" {
#include "libavutil/imgutils.h"
}

namespace ryoma {

VideoFrameConvert::VideoFrameConvert(const AVCodecContext* video_codec_ctx,
                                     AVPixelFormat target_pixel_format)
    : video_codec_ctx_(video_codec_ctx), target_pixel_format_(target_pixel_format) {
  Init();
}

void VideoFrameConvert::Init() {
  sws_ctx_ =
      sws_getContext(video_codec_ctx_->width, video_codec_ctx_->height, video_codec_ctx_->pix_fmt,
                     video_codec_ctx_->width, video_codec_ctx_->height, target_pixel_format_,
                     SWS_BICUBIC, nullptr, nullptr, nullptr);

  int video_target_pixel_size = av_image_get_buffer_size(
      target_pixel_format_, video_codec_ctx_->width, video_codec_ctx_->height, 1);
  target_frame_buff_.resize(video_target_pixel_size);
  target_frame_.reset(av_frame_alloc(), [](AVFrame*& ptr) { av_frame_free(&ptr); });
  av_image_fill_arrays(target_frame_->data, target_frame_->linesize, target_frame_buff_.data(),
                       target_pixel_format_, video_codec_ctx_->width, video_codec_ctx_->height, 1);
}

AVFrame* VideoFrameConvert::Convert(AVFrame* src) {
  sws_scale(sws_ctx_, src->data, src->linesize, 0, src->height, target_frame_->data,
            target_frame_->linesize);
  return target_frame_.get();
}

const vector<uint8_t>& VideoFrameConvert::ConvertToBytes(AVFrame* src) {
  Convert(src);
  return target_frame_buff_;
}

}  // namespace ryoma