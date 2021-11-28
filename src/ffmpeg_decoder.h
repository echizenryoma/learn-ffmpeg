#pragma once

#include <memory>
#include <string>
#include <queue>
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

  int Init();

  void SaveVideoStream(const string& target_path);
  void SaveAudioStream(const string& target_path);
  void ExportYuv420(const string& target_path);
  void DecimatedFrame(const string& target_dir);

  int GetNextFrame(AVFrame*& frame);

  AVCodecContext* GetVideoCodecCtx();
  AVCodecContext* GetAudioCodecCtx();

  void ResetAvStream();

 private:
  int InitAvCtx();

  int InitAvCodecCtx();
  int InitVideoCodecCtx();
  int InitAudioCodecCtx();

  int InitAvFrame();

  void SaveVideoPixel(const string& target_dir, int image_width, int image_height,
                      const vector<uint8_t>& rgb_pixel);

 private:
  string av_path_;

  shared_ptr<AVFormatContext> av_ctx_;
  shared_ptr<AVFrame> video_frame_;
  shared_ptr<AVCodecContext> video_codec_ctx_;
  AVStream* video_stream_ = nullptr;
  size_t video_frame_num_ = 0;

  shared_ptr<AVFrame> audio_frame_;
  shared_ptr<AVCodecContext> audio_codec_ctx_;
  AVStream* audio_stream_ = nullptr;
  size_t audio_frame_num_ = 0;

  static constexpr size_t kMaxAudioFrameBufferSize = 192000;
  vector<uint8_t> audio_frame_buff_;
};

}  // namespace ryoma
