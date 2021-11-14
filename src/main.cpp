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
  /*
  string yuv_path = "../static/demo_1280x720.yuv";
  ffmpeg_decode.ExportYuv420(yuv_path);

  ffmpeg_decode.DecimatedFrame("../static");

  string video_path = "../static/demo.h264";
  ffmpeg_decode.SaveVideoStream(video_path);
  */

  ryoma::SdlPlayer player;
  player.Play(ffmpeg_decoder);
  return 0;
}
