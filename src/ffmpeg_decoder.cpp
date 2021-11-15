#include "ffmpeg_decoder.h"

#include <fstream>
#include <thread>

#include "fmt/printf.h"
#include "spdlog/spdlog.h"
#include "video_frame_convert.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

namespace ryoma {

FFmpegDecoder::FFmpegDecoder(const string& av_path) : av_path_(av_path) {}

int FFmpegDecoder::Init() {
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

void FFmpegDecoder::SaveVideoStream(const string& target_path) {
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

void FFmpegDecoder::ExportYuv420(const string& prefix_path) {
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

void FFmpegDecoder::DecimatedFrame(const string& target_dir) {
  ResetAvStream();
  ryoma::VideoFrameConvert video_frame_convert(video_codec_ctx_.get(), AV_PIX_FMT_RGB24);

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
        SaveVideoPixel(target_dir, video_frame_->width, video_frame_->height,
                       video_frame_convert.ConvertToBytes(video_frame_.get()));
        video_frame_num_++;
      }
    }
  }
}

int FFmpegDecoder::GetNextFrame(AVFrame*& frame) {
  frame = nullptr;

  AVPacket av_packet;
  while (av_read_frame(av_ctx_.get(), &av_packet) == 0) {
    if (av_packet.stream_index == video_stream_->index) {
      int ret = avcodec_send_packet(video_codec_ctx_.get(), &av_packet);
      if (ret < 0) {
        spdlog::error("avcodec_send_packet failed, ret {}", ret);
        continue;
      }
      ret = avcodec_receive_frame(video_codec_ctx_.get(), video_frame_.get());
      if (ret < 0) {
        spdlog::error("avcodec_receive_frame failed, ret {}", ret);
        continue;
      }
      video_frame_num_++;
      frame = video_frame_.get();
      break;
    }
  }
  return 0;
}

const AVCodecContext* FFmpegDecoder::GetVideoCodecCtx() const { return video_codec_ctx_.get(); }

int FFmpegDecoder::InitAvCtx() {
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

int FFmpegDecoder::InitAvCodecCtx() {
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

int FFmpegDecoder::InitAvFrame() {
  video_frame_.reset(av_frame_alloc(), [](AVFrame*& ptr) { av_frame_free(&ptr); });
  return 0;
}

void FFmpegDecoder::ResetAvStream() {
  avio_seek(av_ctx_->pb, 0, SEEK_SET);
  int ret = avformat_seek_file(av_ctx_.get(), video_stream_->index, 0, 0, INT64_MAX,
                               AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    spdlog::error("avformat_seek_file failed, ret {}", ret);
    return;
  }
}

void FFmpegDecoder::SaveVideoPixel(const string& target_dir, int image_width, int image_height,
                                   const vector<uint8_t>& rgb_pixel) {
  if (video_frame_num_ % 100 == 0) {
    string image_path = fmt::format("{}/demo_{}.jpg", target_dir, video_frame_num_);
    int ret =
        stbi_write_jpg(image_path.c_str(), image_width, image_height, 3, rgb_pixel.data(), 80);
    if (ret < 0) {
      spdlog::error("stbi_write_jpg {} failed, ret {}", image_path, ret);
    }
  }
}

}  // namespace ryoma