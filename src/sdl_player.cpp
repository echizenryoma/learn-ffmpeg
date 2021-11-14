#include "sdl_player.h"

#include "spdlog/spdlog.h"

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
}

namespace ryoma {

atomic<bool> SdlPlayer::exit_{false};
atomic<bool> SdlPlayer::pause_{false};

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

  ffmpeg_decoder.ResetAvStream();

  SDL_Event event;
  bool is_loop = true;
  AVFrame* frame = nullptr;
  while (is_loop) {
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_SPACE) {
          pause_.store(!pause_);
        }
        break;
      case SDL_QUIT:
        exit_.store(true);
        is_loop = false;
        break;

      case SDL_PALYER_EVENT_REFRESH: {
        int ret = ffmpeg_decoder.GetNextFrame(frame);
        if (ret < 0) {
          exit_.store(true);
          is_loop = false;
          break;
        }

        if (frame == nullptr) {
          break;
        }

        RendererFrame(frame, frame->pkt_duration);
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

void SdlPlayer::RendererFrame(AVFrame* frame, uint32_t delay) {
  SDL_UpdateYUVTexture(texture_.get(), &rect_, frame->data[0], frame->linesize[0], frame->data[1],
                       frame->linesize[1], frame->data[2], frame->linesize[2]);
  SDL_RenderClear(renderer_.get());
  SDL_RenderCopy(renderer_.get(), texture_.get(), nullptr, &rect_);
  SDL_RenderPresent(renderer_.get());
  SDL_Delay(delay);
}

int SdlPlayer::Refresh(void* data) {
  SDL_Event event;
  while (!exit_) {
    if (!pause_) {
      event.type = SDL_PALYER_EVENT_REFRESH;
      SDL_PushEvent(&event);
    }
  }
  event.type = SDL_PALYER_EVENT_STOP;
  SDL_PushEvent(&event);
  return 0;
}

}  // namespace ryoma