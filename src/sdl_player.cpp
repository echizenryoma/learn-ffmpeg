#include "sdl_player.h"

#include <memory>

#include "spdlog/spdlog.h"
#include "video_frame_convert.h"

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_timer.h"
}

namespace ryoma {

SdlPlayer::RefreshData SdlPlayer::refresh_data_;

SdlPlayer::~SdlPlayer() { SDL_Quit(); }

int SdlPlayer::Init(int width, int height, const string& title) {
  int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret < 0) {
    spdlog::error("Could not initialize SDL, ret {}", SDL_GetError());
    return -1;
  }

  window_.reset(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 width, height, SDL_WINDOW_OPENGL),
                SDL_DestroyWindow);
  if (window_ == nullptr) {
    spdlog::error("SDL_CreateWindow: {}", SDL_GetError());
    return -1;
  }
  renderer_.reset(SDL_CreateRenderer(window_.get(), -1, 0), SDL_DestroyRenderer);
  if (renderer_ == nullptr) {
    spdlog::error("SDL_CreateRenderer: {}", SDL_GetError());
    return -1;
  }

  texture_.reset(SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_IYUV,
                                   SDL_TEXTUREACCESS_STREAMING, width, height),
                 SDL_DestroyTexture);
  if (texture_ == nullptr) {
    spdlog::error("SDL_CreateTexture: {}", SDL_GetError());
    return -1;
  }

  rect_.x = 0;
  rect_.y = 0;

  rect_.h = height;
  rect_.w = width;

  auto* tid = SDL_CreateThread(Refresh, nullptr, nullptr);
  if (tid == nullptr) {
    spdlog::error("SDL_CreateThread: {}", SDL_GetError());
    return -1;
  }
  return 0;
}

int SdlPlayer::Play(ryoma::FFmpegDecoder& ffmpeg_decoder) {
  auto* video_codec_ctx = ffmpeg_decoder.GetVideoCodecCtx();
  Init(video_codec_ctx->width, video_codec_ctx->height, "Simple Video Player");
  refresh_data_.delay_ms.store(video_codec_ctx->delay);

  ffmpeg_decoder.ResetAvStream();
  ryoma::VideoFrameConvert video_frame_convert(video_codec_ctx, AV_PIX_FMT_YUV420P);

  SDL_Event event;
  bool is_loop = true;
  AVFrame* frame = nullptr;
  while (is_loop) {
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_SPACE) {
          refresh_data_.pause.store(!refresh_data_.pause);
        }
        break;
      case SDL_QUIT:
        refresh_data_.exit.store(true);
        is_loop = false;
        break;

      case SDL_PALYER_EVENT_REFRESH: {
        int ret = ffmpeg_decoder.GetNextFrame(frame);
        if (ret < 0) {
          break;
        }
        if (frame == nullptr) {
          break;
        }
        refresh_data_.delay_ms.store(frame->pkt_duration);
        RendererFrame(video_frame_convert.Convert(frame));
        break;
      }
      case SDL_PALYER_EVENT_STOP:
        is_loop = false;
        break;
      default:
        break;
    }
  }

  return 0;
}

void SdlPlayer::RendererFrame(AVFrame* frame) {
  SDL_UpdateYUVTexture(texture_.get(), &rect_, frame->data[0], frame->linesize[0], frame->data[1],
                       frame->linesize[1], frame->data[2], frame->linesize[2]);
  SDL_RenderClear(renderer_.get());
  SDL_RenderCopy(renderer_.get(), texture_.get(), nullptr, &rect_);
  SDL_RenderPresent(renderer_.get());
}

int SdlPlayer::Refresh(void* data) {
  SDL_Event event;
  while (!refresh_data_.exit) {
    if (!refresh_data_.pause) {
      event.type = SDL_PALYER_EVENT_REFRESH;
      SDL_PushEvent(&event);
      SDL_Delay(refresh_data_.delay_ms);
    }
  }
  event.type = SDL_PALYER_EVENT_STOP;
  SDL_PushEvent(&event);
  return 0;
}

}  // namespace ryoma