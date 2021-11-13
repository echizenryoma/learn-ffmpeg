#include <string>

#include "ffmpeg_decode.h"
#include "spdlog/spdlog.h"

using namespace std;

int main() {
  string av_path = "../static/demo.mkv";
  ryoma::FFmpegDecode ffmpeg_decode(av_path);
  int ret = ffmpeg_decode.Init();
  if (ret != 0) {
    spdlog::error("FFmpegDecode::Init failed, ret {}", ret);
    return ret;
  }
  ffmpeg_decode.DecimatedFrame("../static");
  string video_path = "../static/demo.h264";
  ffmpeg_decode.SaveVideoStream(video_path);
  return 0;
}
