#pragma once

#include <atomic>
#define SDL_MAIN_HANDLED

#include <memory>
#include <string>

#include "ffmpeg_decoder.h"

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_render.h"
#include "libavutil/frame.h"
}

using namespace std;

namespace ryoma {

enum SdlPlayerEventType {
  SDL_PALYER_EVENT_REFRESH = 1,
  SDL_PALYER_EVENT_STOP,
};

class SdlPlayer {
 public:
  ~SdlPlayer();

  int Play(ryoma::FFmpegDecoder& ffmpeg_decoder);

 private:
  static int Refresh(void* data);

  int Init(int width, int height, const string& title);
  void RendererFrame(AVFrame* frame, uint32_t delay);

 private:
  shared_ptr<SDL_Window> window_;
  shared_ptr<SDL_Renderer> renderer_;
  shared_ptr<SDL_Texture> texture_;
  SDL_Rect rect_;

  static atomic<bool> exit_;
  static atomic<bool> pause_;
};

}  // namespace ryoma
