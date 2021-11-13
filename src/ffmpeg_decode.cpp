#include "ffmpeg_decode.h"

#include <fstream>
#include <thread>

#include "fmt/printf.h"
#include "sdl_player.h"
#include "spdlog/spdlog.h"

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
    spdlog::error("InitAvCtx failed, ret {}", ret);
    return ret;
  }

  ret = InitAvCodecCtx();
  if (ret != 0) {
    spdlog::error("InitAvCodecCtx failed, ret {}", ret);
    return ret;
  }

  ret = InitAvFrame();
  if (ret != 0) {
    spdlog::error("InitAvFrame failed, ret {}", ret);
    return ret;
  }
  return 0;
}

void FFmpegDecode::SaveVideoStream(const string& target_path) {
  ResetAvStream();
  AVFormatContext* target_ctx_ptr = nullptr;
  int ret = avformat_alloc_output_context2(&target_ctx_ptr, NULL, NULL, target_path.c_str());
  if (ret != 0) {
    spdlog::error("avformat_alloc_output_context2 failed, path {} ret {}", target_path, ret);
    return;
  }
  shared_ptr<AVFormatContext> target_ctx(target_ctx_ptr,
                                         [](AVFormatContext*& ptr) { avformat_close_input(&ptr); });
  AVStream* video_target_stream = avformat_new_stream(target_ctx.get(), nullptr);
  avio_open(&target_ctx->pb, target_ctx->url, AVIO_FLAG_WRITE);

  video_target_stream->time_base = video_stream_->time_base;
  avcodec_parameters_copy(video_target_stream->codecpar, video_stream_->codecpar);

  ret = avformat_write_header(target_ctx.get(), nullptr);
  if (ret < 0) {
    spdlog::error("avformat_write_header failed, ret {}", ret);
    return;
  }
  av_dump_format(target_ctx.get(), 0, target_ctx->url, 1);

  AVPacket av_packet;
  while (av_read_frame(av_ctx_.get(), &av_packet) == 0) {
    if (av_packet.stream_index == video_stream_->index) {
      av_interleaved_write_frame(target_ctx.get(), &av_packet);
    }
  }
  av_write_trailer(target_ctx.get());
}

void FFmpegDecode::ExportYuv420(const string& prefix_path) {
  ResetAvStream();
  ofstream fout(prefix_path, ios::out | ios::trunc | ios::binary);

  int video_width = video_codec_ctx_->width;
  int video_height = video_codec_ctx_->height;
  int pixel_size = video_width * video_height;

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
      ret = avcodec_receive_frame(video_codec_ctx_.get(), video_frame_.get());
      if (ret == 0) {
        // yuv420: yyyyyyyyuuvv|yyyyyyyyuuvv
        fout.write(reinterpret_cast<char*>(video_frame_->data[0]), pixel_size);
        fout.write(reinterpret_cast<char*>(video_frame_->data[1]), pixel_size / 4);
        fout.write(reinterpret_cast<char*>(video_frame_->data[2]), pixel_size / 4);
        video_frame_num_++;
      }
    }
  }
  fout.close();
}

void FFmpegDecode::DecimatedFrame(const string& target_dir) {
  ResetAvStream();
  int ret = InitSwsCtx();
  if (ret != 0) {
    spdlog::error("InitSwsCtx failed, ret {}", ret);
    return;
  }

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
      ret = avcodec_receive_frame(video_codec_ctx_.get(), video_frame_.get());
      if (ret == 0) {
        SaveVideoPixel(target_dir, video_frame_.get());
        video_frame_num_++;
      }
    }
  }
}

void FFmpegDecode::Play() {
  ryoma::SdlPlayer player;


  ResetAvStream();
  int video_width = video_codec_ctx_->width;
  int video_height = video_codec_ctx_->height;
  int pixel_size = video_width * video_height;

  player.Init(video_width, video_height, "Simple Video Player");

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
      ret = avcodec_receive_frame(video_codec_ctx_.get(), video_frame_.get());
      if (ret == 0) {
        player.RendererFrame(video_frame_.get(), 41);
        video_frame_num_++;
      }
    }
  }
}

int FFmpegDecode::InitAvCtx() {
  const auto& path = av_path_;

  AVFormatContext* av_ctx_ptr = nullptr;
  int ret = avformat_open_input(&av_ctx_ptr, path.c_str(), nullptr, nullptr);
  if (ret < 0) {
    spdlog::error("avformat open {} failed, ret {}", path, ret);
    return ret;
  }
  av_ctx_.reset(av_ctx_ptr, [](AVFormatContext*& ptr) { avformat_close_input(&ptr); });
  return 0;
}

int FFmpegDecode::InitAvCodecCtx() {
  av_dump_format(av_ctx_.get(), 0, av_ctx_->url, 0);
  int ret = avformat_find_stream_info(av_ctx_.get(), nullptr);
  if (ret < 0) {
    spdlog::error("avformat_find_stream_info failed, ret {}", ret);
    return ret;
  }

  int video_stream_index = av_find_best_stream(av_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    spdlog::error("not found video stream");
    return AVERROR_STREAM_NOT_FOUND;
  }
  video_stream_ = av_ctx_->streams[video_stream_index];
  auto* video_codec = avcodec_find_decoder(video_stream_->codecpar->codec_id);
  if (video_codec == nullptr) {
    spdlog::error("not found video codec, codec_id {}", video_stream_->codecpar->codec_id);
    return -1;
  }
  video_codec_ctx_.reset(avcodec_alloc_context3(video_codec),
                         [](AVCodecContext*& ptr) { avcodec_free_context(&ptr); });
  if (video_codec_ctx_ == nullptr) {
    spdlog::error("not found video decodec context");
    return -1;
  }
  ret = avcodec_parameters_to_context(video_codec_ctx_.get(), video_stream_->codecpar);
  video_codec_ctx_->thread_count = std::thread::hardware_concurrency();
  if (ret < 0) {
    spdlog::error("avcodec_parameters_to_context failed, ret {}", ret);
    return ret;
  }
  ret = avcodec_open2(video_codec_ctx_.get(), video_codec, nullptr);
  if (ret < 0) {
    spdlog::error("avcodec_open2 failed, ret {}", ret);
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

void FFmpegDecode::ResetAvStream() {
  avio_seek(av_ctx_->pb, 0, SEEK_SET);
  int ret = avformat_seek_file(av_ctx_.get(), video_stream_->index, 0, 0, INT64_MAX,
                               AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    spdlog::error("avformat_seek_file failed, ret {}", ret);
    return;
  }
}

void FFmpegDecode::SaveVideoPixel(const string& target_dir, AVFrame* frame) {
  uint8_t* video_target_pixel_ptr[] = {video_target_pixel_.data()};
  int video_target_pixel_line_sizes[] = {video_codec_ctx_->width * 3};
  if (video_frame_num_ % 100 == 0) {
    sws_scale(sws_ctx_, frame->data, frame->linesize, 0, frame->height, video_target_pixel_ptr,
              video_target_pixel_line_sizes);
    string image_path = fmt::format("{}/demo_{}.jpg", target_dir, video_frame_num_);
    int ret = stbi_write_jpg(image_path.c_str(), frame->width, frame->height, 3,
                             video_target_pixel_.data(), 80);
    if (ret < 0) {
      spdlog::error("stbi_write_jpg {} failed, ret {}", image_path, ret);
    }
  }
}

}  // namespace ryoma