#include "sdl_player.h"

#include "spdlog/spdlog.h"

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
}


namespace ryoma {

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
  return 0;
}

int SdlPlayer::RendererFrame(AVFrame* frame, uint32_t delay) {
  SDL_UpdateYUVTexture(texture_.get(), &rect_, frame->data[0], frame->linesize[0], frame->data[1],
                       frame->linesize[1], frame->data[2], frame->linesize[2]);
  SDL_RenderClear(renderer_.get());
  SDL_RenderCopy(renderer_.get(), texture_.get(), NULL, &rect_);
  SDL_RenderPresent(renderer_.get());
  SDL_Delay(delay);
  return 0;
}

}  // namespace ryoma