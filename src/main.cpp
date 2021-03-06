#include <string>

#include "ffmpeg_decoder.h"
#include "sdl_player.h"
#include "spdlog/spdlog.h"

using namespace std;

int main() {
  ios_base::sync_with_stdio(false);

  string av_path = "../static/demo.mkv";
  ryoma::FFmpegDecoder ffmpeg_decoder(av_path);
  int ret = ffmpeg_decoder.Init();
  if (ret != 0) {
    spdlog::error("FFmpegDecoder::Init failed, ret {}", ret);
    return ret;
  }
  // string yuv_path = "../static/demo_1280x720.yuv";
  // ffmpeg_decoder.ExportYuv420(yuv_path);

  // ffmpeg_decoder.DecimatedFrame("../static");

  // string video_path = "../static/demo.h264";
  // ffmpeg_decoder.SaveVideoStream(video_path);

  // string audio_path = "../static/dem/*o.aac";
  // ffmpeg_decoder.SaveAudioStream(audio_path);

  ryoma::SdlPlayer player;
  player.Init("Simple video player", &ffmpeg_decoder);
  player.Play();
  return 0;
}
