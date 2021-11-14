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

class FFmpegDecoder {
 public:
  explicit FFmpegDecoder(const string& av_path);

  void SetVideoTargetPixelFormat(AVPixelFormat pixel_format);

  int Init();

  void SaveVideoStream(const string& target_path);
  void ExportYuv420(const string& target_path);
  void DecimatedFrame(const string& target_dir);

  int GetNextFrame(AVFrame*& frame);

  const AVCodecContext* GetVideoCodecCtx() const;

  void ResetAvStream();

 private:
  int InitAvCtx();
  int InitAvCodecCtx();
  int InitAvFrame();
  int InitSwsCtx();

  void SaveVideoPixel(const string& target_dir, AVFrame* frame);

 private:
  string av_path_;

  shared_ptr<AVFormatContext> av_ctx_;
  shared_ptr<AVFrame> av_frame_;
  shared_ptr<AVCodecContext> video_codec_ctx_;
  AVStream* video_stream_ = nullptr;
  size_t video_frame_num_ = 0;
  int video_target_pixel_size_ = 0;
  vector<uint8_t> video_target_pixel_;

  AVPixelFormat video_target_pixel_format_ = AV_PIX_FMT_RGB24;

  SwsContext* sws_ctx_ = nullptr;
};

}  // namespace ryoma
