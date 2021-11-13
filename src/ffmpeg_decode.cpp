#include "ffmpeg_decode.h"

#include <thread>

#include "fmt/core.h"
#include "fmt/printf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

namespace ryoma {

FFmpegDecode::FFmpegDecode(const string& av_path) : av_path_(av_path) {}

void FFmpegDecode::SetVideoTargetPixelFormat(AVPixelFormat pixel_format) {
  video_target_pixel_format_ = pixel_format;
}

int FFmpegDecode::Init() {
  int ret = InitAvCtx();
  if (ret != 0) {
    fmt::print(stderr, "InitAvCtx failed, ret {}\n", ret);
    return ret;
  }

  ret = InitAvCodecCtx();
  if (ret != 0) {
    fmt::print(stderr, "InitAvCodecCtx failed, ret {}\n", ret);
    return ret;
  }

  ret = InitAvFrame();
  if (ret != 0) {
    fmt::print(stderr, "InitAvFrame failed, ret {}\n", ret);
    return ret;
  }

  ret = InitSwsCtx();
  if (ret != 0) {
    fmt::print(stderr, "InitSwsCtx failed, ret {}\n", ret);
    return ret;
  }

  return 0;
}

void FFmpegDecode::DecodeAv() {
  AVPacket av_packet;
  while (av_read_frame(av_ctx_.get(), &av_packet) == 0) {
    if (av_packet.stream_index == video_stream_->index) {
      int ret = avcodec_send_packet(video_codec_ctx_.get(), &av_packet);
      if (ret < 0) {
        continue;
      }
      if (av_packet.size <= 0) {
        continue;
      }
      DecadeVideoFrame(video_frame_.get());
    }
  }
}

int FFmpegDecode::InitAvCtx() {
  const auto& path = av_path_;

  AVFormatContext* av_ctx_ptr = nullptr;
  int ret = avformat_open_input(&av_ctx_ptr, path.c_str(), nullptr, nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avformat open {} failed, ret {}\n", path, ret);
    return ret;
  }
  av_ctx_.reset(av_ctx_ptr, [](AVFormatContext*& ptr) { avformat_close_input(&ptr); });
  return 0;
}

int FFmpegDecode::InitAvCodecCtx() {
  av_dump_format(av_ctx_.get(), 0, av_ctx_->url, 0);
  int ret = avformat_find_stream_info(av_ctx_.get(), nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avformat_find_stream_info failed, ret {}\n", ret);
    return ret;
  }

  int video_stream_index = av_find_best_stream(av_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    fmt::print(stderr, "not found video stream\n");
    return AVERROR_STREAM_NOT_FOUND;
  }
  video_stream_ = av_ctx_->streams[video_stream_index];
  auto* video_codec = avcodec_find_decoder(video_stream_->codecpar->codec_id);
  if (video_codec == nullptr) {
    fmt::print(stderr, "not found video codec, codec_id {}\n", video_stream_->codecpar->codec_id);
    return -1;
  }
  video_codec_ctx_.reset(avcodec_alloc_context3(video_codec),
                         [](AVCodecContext*& ptr) { avcodec_free_context(&ptr); });
  if (video_codec_ctx_ == nullptr) {
    fmt::print(stderr, "not found video decodec context\n");
    return -1;
  }
  ret = avcodec_parameters_to_context(video_codec_ctx_.get(), video_stream_->codecpar);
  video_codec_ctx_->thread_count = std::thread::hardware_concurrency();
  if (ret < 0) {
    fmt::print(stderr, "avcodec_parameters_to_context failed, ret {}\n", ret);
    return ret;
  }
  ret = avcodec_open2(video_codec_ctx_.get(), video_codec, nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avcodec_open2 failed, ret {}\n", ret);
    return ret;
  }
  return 0;
}

int FFmpegDecode::InitAvFrame() {
  video_frame_.reset(av_frame_alloc(), [](AVFrame*& ptr) { av_frame_free(&ptr); });
  video_target_pixel_size_ =
      av_image_get_buffer_size(video_target_pixel_format_, video_stream_->codecpar->width,
                               video_stream_->codecpar->height, 1);
  video_target_pixel_.resize(video_target_pixel_size_);
  return 0;
}

int FFmpegDecode::InitSwsCtx() {
  sws_ctx_ = sws_getCachedContext(nullptr, video_stream_->codecpar->width,
                                  video_stream_->codecpar->height, video_codec_ctx_->pix_fmt,
                                  video_stream_->codecpar->width, video_stream_->codecpar->height,
                                  video_target_pixel_format_, 0, nullptr, nullptr, nullptr);
  return 0;
}

void FFmpegDecode::DecadeVideoFrame(AVFrame* frame) {
  uint8_t* video_target_pixel_ptr[] = {video_target_pixel_.data()};
  int video_target_pixel_line_sizes[] = {video_codec_ctx_->width * 3};
  while (avcodec_receive_frame(video_codec_ctx_.get(), frame) == 0) {
    if (video_frame_num_ % 1000 == 0) {
      sws_scale(sws_ctx_, video_frame_->data, video_frame_->linesize, 0, video_frame_->height,
                video_target_pixel_ptr, video_target_pixel_line_sizes);
      string image_path = fmt::format("../static/demo_{}.jpg", video_frame_num_);
      int ret = stbi_write_jpg(image_path.c_str(), frame->width, frame->height, 3,
                               video_target_pixel_.data(), 80);
      if (ret != 1) {
        fmt::print(stderr, "stbi_write_jpg {} failed, ret {}\n", image_path, ret);
      }
    }
    video_frame_num_++;
  }
}

}  // namespace ryoma