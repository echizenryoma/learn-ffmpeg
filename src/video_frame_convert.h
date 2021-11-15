#pragma once

#include <cstdint>
#include <memory>
#include <vector>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

using namespace std;

namespace ryoma {

class VideoFrameConvert {
 public:
  explicit VideoFrameConvert(const AVCodecContext* video_codec_ctx,
                             AVPixelFormat target_pixel_format = AV_PIX_FMT_YUV420P);

  AVFrame* Convert(AVFrame* src);

  const vector<uint8_t>& ConvertToBytes(AVFrame* src);

 private:
  void Init();

 private:
  const AVCodecContext* video_codec_ctx_;
  AVPixelFormat target_pixel_format_;

  SwsContext* sws_ctx_ = nullptr;

  vector<uint8_t> target_frame_buff_;
  shared_ptr<AVFrame> target_frame_;
};

}  // namespace ryoma