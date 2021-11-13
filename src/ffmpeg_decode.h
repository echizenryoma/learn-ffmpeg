#pragma once

#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

using namespace std;

namespace ryoma {

class FFmpegDecode {
 public:
  explicit FFmpegDecode(const string& av_path);

  void SetVideoTargetPixelFormat(AVPixelFormat pixel_format);

  int Init();
  
  void SaveVideoStream(const string& target_path);

  void DecodeAv();

 private:
  int InitAvCtx();
  int InitAvCodecCtx();
  int InitAvFrame();
  int InitSwsCtx();

  void DecodeVideoFrame(AVFrame* frame);
  void SaveVideoPixel(AVFrame* frame);

 private:
  string av_path_;

  shared_ptr<AVFormatContext> av_ctx_;
  shared_ptr<AVFrame> video_frame_;
  shared_ptr<AVCodecContext> video_codec_ctx_;
  AVStream* video_stream_ = nullptr;
  size_t video_frame_num_ = 0;
  int video_target_pixel_size_ = 0;
  vector<uint8_t> video_target_pixel_;

  AVPixelFormat video_target_pixel_format_ = AV_PIX_FMT_RGB24;

  SwsContext* sws_ctx_ = nullptr;
};

}  // namespace ryoma
