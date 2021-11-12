#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "fmt/printf.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"

using defer = std::shared_ptr<void>;

using namespace std;

static size_t frame_num = 0;

void DecodeVideoFrame(AVCodecContext* codec, SwsContext* swsctx, AVFrame* frame) {
  cv::Mat image(codec->height, codec->width, CV_8UC3);
  int image_line_sizes[] = {static_cast<int>(image.step1())};

  while (avcodec_receive_frame(codec, frame) == 0) {
    sws_scale(swsctx, frame->data, frame->linesize, 0, frame->height, &image.data,
              image_line_sizes);
    imwrite("../static/demo_" + to_string(frame_num) + ".jpg", image);
    frame_num++;
  }
}

int main() {
  int ret = 0;
  fmt::print("{}\n", avcodec_configuration());
  std::string path = "../static/demo.mkv";
  AVFormatContext* avctx_ptr = nullptr;
  ret = avformat_open_input(&avctx_ptr, path.c_str(), nullptr, nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avformat open {} failed, ret {}\n", path, ret);
    return ret;
  }
  shared_ptr<AVFormatContext> avctx(avctx_ptr,
                                    [](AVFormatContext*& ptr) { avformat_close_input(&ptr); });
  av_dump_format(avctx.get(), 0, avctx->url, 0);
  ret = avformat_find_stream_info(avctx.get(), nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avformat_find_stream_info failed, ret {}\n", ret);
    return ret;
  }

  int video_stream_index = av_find_best_stream(avctx.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (video_stream_index == AVERROR_STREAM_NOT_FOUND) {
    fmt::print(stderr, "not found video stream\n");
    return -1;
  }
  auto* video_stream = avctx->streams[video_stream_index];
  auto* video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
  if (video_codec == nullptr) {
    fmt::print(stderr, "not found video codec, codec_id {}\n", video_stream->codecpar->codec_id);
    return -1;
  }

  shared_ptr<AVCodecContext> video_dec_ctx(
      avcodec_alloc_context3(video_codec),
      [](AVCodecContext*& ptr) { avcodec_free_context(&ptr); });
  if (video_dec_ctx == nullptr) {
    fmt::print(stderr, "not found video decodec context\n");
    return -1;
  }
  ret = avcodec_parameters_to_context(video_dec_ctx.get(), video_stream->codecpar);
  if (ret < 0) {
    fmt::print(stderr, "avcodec_parameters_to_context failed, ret {}\n", ret);
    return -1;
  }
  ret = avcodec_open2(video_dec_ctx.get(), video_codec, nullptr);
  if (ret < 0) {
    fmt::print(stderr, "avcodec_open2 failed, ret {}\n", ret);
    return -1;
  }

  shared_ptr<AVFrame> video_frame(av_frame_alloc(), [](AVFrame*& ptr) { av_frame_free(&ptr); });
  AVPacket av_packet;

  SwsContext* swsctx = sws_getCachedContext(
      nullptr, video_stream->codecpar->width, video_stream->codecpar->height,
      video_dec_ctx->pix_fmt, video_stream->codecpar->width, video_stream->codecpar->height,
      AV_PIX_FMT_BGR24, 0, nullptr, nullptr, nullptr);
  size_t frame_num = 0;
  while (true) {
    ret = av_read_frame(avctx.get(), &av_packet);
    if (ret != 0) {
      break;
    }

    if (av_packet.stream_index == video_stream->index) {
      ret = avcodec_send_packet(video_dec_ctx.get(), &av_packet);
      if (ret < 0) {
        continue;
      }
      if (av_packet.size <= 0) {
        continue;
      }
      DecodeVideoFrame(video_dec_ctx.get(), swsctx, video_frame.get());
    }
  }

  return 0;
}
