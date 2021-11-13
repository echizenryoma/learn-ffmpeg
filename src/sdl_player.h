#pragma once

#include <memory>
#include <string>

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_render.h"
#include "libavutil/frame.h"
}

using namespace std;

namespace ryoma {

class SdlPlayer {
 public:
  ~SdlPlayer();

  int Init(int width, int height, const string& title);

  int RendererFrame(AVFrame* frame, uint32_t delay);

 private:
  shared_ptr<SDL_Window> window_;
  shared_ptr<SDL_Renderer> renderer_;
  shared_ptr<SDL_Texture> texture_;
  SDL_Rect rect_;
};

}  // namespace ryoma
