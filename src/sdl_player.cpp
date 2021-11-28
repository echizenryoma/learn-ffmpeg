#include "sdl_player.h"

#include <memory>

#include "audio_frame_resample.h"
#include "spdlog/spdlog.h"
#include "video_frame_convert.h"

extern "C" {
#include "SDL2/SDL.h"
#include "SDL2/SDL_main.h"
#include "SDL2/SDL_timer.h"
}

namespace ryoma {

SdlPlayer::RefreshData SdlPlayer::refresh_data_;

atomic<int> SdlPlayer::audio_len_{0};
atomic<const Uint8*> SdlPlayer::audio_data_{nullptr};

SdlPlayer::~SdlPlayer() {
  SDL_CloseAudio();
  SDL_Quit();
}

int SdlPlayer::Init(const string& title, ryoma::FFmpegDecoder* ffmpeg_decoder) {
  if (ffmpeg_decoder == nullptr) {
    spdlog::error("ffmpeg_decoder is nullptr");
    return -1;
  }
  ffmpeg_decoder_ = ffmpeg_decoder;

  int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret < 0) {
    spdlog::error("Could not initialize SDL, ret {}", SDL_GetError());
    return -1;
  }

  auto* video_codec_ctx = ffmpeg_decoder->GetVideoCodecCtx();
  auto* audio_codec_ctx = ffmpeg_decoder->GetAudioCodecCtx();

  int width = video_codec_ctx->width;
  int height = video_codec_ctx->height;

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

  audio_wanted_spec_.freq = audio_codec_ctx->sample_rate;
  audio_wanted_spec_.channels = audio_codec_ctx->channels;
  audio_wanted_spec_.format = AUDIO_S16SYS;
  audio_wanted_spec_.silence = 0;
  audio_wanted_spec_.samples = audio_codec_ctx->frame_size;
  audio_wanted_spec_.callback = FillAudio;
  audio_wanted_spec_.userdata = reinterpret_cast<void*>(audio_codec_ctx);
  ret = SDL_OpenAudio(&audio_wanted_spec_, nullptr);
  if (ret < 0) {
    spdlog::error("SDL_OpenAudio: {}", SDL_GetError());
    return -1;
  }

  return 0;
}

int SdlPlayer::Play() {
  auto* video_codec_ctx = ffmpeg_decoder_->GetVideoCodecCtx();
  auto* audio_codec_ctx = ffmpeg_decoder_->GetAudioCodecCtx();
  refresh_data_.delay_ms.store(video_codec_ctx->delay);

  ffmpeg_decoder_->ResetAvStream();
  ryoma::VideoFrameConvert video_frame_convert(video_codec_ctx, AV_PIX_FMT_YUV420P);

  ryoma::AudioFrameResample audio_frame_resample(audio_codec_ctx, 44100, AV_SAMPLE_FMT_S16);

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
        int ret = ffmpeg_decoder_->GetNextFrame(frame);
        if (ret < 0) {
          break;
        }
        if (frame == nullptr) {
          break;
        }
         refresh_data_.delay_ms.store(frame->pkt_duration);
        // RendererFrame(video_frame_convert.Convert(frame));
        PlayAudioFrame(audio_frame_resample.Resample(frame));
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

void SdlPlayer::PlayAudioFrame(const vector<uint8_t>& audio_data) {
  while (audio_len_ > 0) {
    SDL_Delay(1);
  }
  audio_data_ = audio_data.data();
  audio_len_ = audio_data.size();
  SDL_PauseAudio(0);
}

void SdlPlayer::FillAudio(void* userdata, Uint8* stream, int len) {
  SDL_memset(stream, 0, len);
  if (audio_len_ == 0) {
    return;
  }
  len = min(len, audio_len_.load());
  SDL_MixAudio(stream, audio_data_, len, SDL_MIX_MAXVOLUME);
  audio_data_ += len;
  audio_len_ -= len;
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