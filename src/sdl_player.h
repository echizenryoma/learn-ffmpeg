#pragma once

#include <cstdint>

#define SDL_MAIN_HANDLED

#include <atomic>
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
  SDL_PALYER_EVENT_REFRESH = 10,
  SDL_PALYER_EVENT_STOP,
};

class SdlPlayer {
 public:
  struct RefreshData {
    atomic<bool> exit;
    atomic<bool> pause;
    atomic<uint32_t> delay_ms;
  };

 public:
  ~SdlPlayer();

  int Init(const string& title, ryoma::FFmpegDecoder* ffmpeg_decoder);

  int Play();

 private:
  static int Refresh(void* data);

  void RendererFrame(AVFrame* frame);

  void PlayAudioFrame(const vector<uint8_t>& audio_data);
  static void FillAudio(void* data, Uint8* stream, int len);

 private:
  shared_ptr<SDL_Window> window_;
  shared_ptr<SDL_Renderer> renderer_;
  shared_ptr<SDL_Texture> texture_;
  SDL_Rect rect_;

  SDL_AudioDeviceID audio_dev_;
  SDL_AudioSpec audio_wanted_spec_;

  static RefreshData refresh_data_;

  int video_target_pixel_size_ = 0;

  AVPixelFormat video_target_pixel_format_ = AV_PIX_FMT_YUV420P;

  ryoma::FFmpegDecoder* ffmpeg_decoder_ = nullptr;

  static atomic<int> audio_len_;
  static atomic<const Uint8*> audio_data_;
};

}  // namespace ryoma
