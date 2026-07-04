#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
}

#include <atomic>
#include <string>
#include <thread>

#include "common/queue.h"
#include "msgq/visionipc/visionbuf.h"

class ScreenEncoder {
public:
  ScreenEncoder(int width, int height, int fps, int bitrate);
  ~ScreenEncoder();

  bool open(const std::string &path);

  void close();
  void encode_frame(const uint8_t *rgb32, int stride, uint64_t ts_ns);

  bool ok() const { return is_open.load() && !stream_error.load(); }

  std::atomic<bool> is_open{false};

private:
  bool setup_device();
  bool setup_muxer(const std::string &path);

  void dequeue_loop();
  void mux_write_header(const uint8_t *data, int len);
  void mux_write_packet(const uint8_t *data, int len, uint64_t ts_us, bool keyframe);
  void teardown_device();
  void teardown_muxer();

  static constexpr int BUF_IN_COUNT = 6;
  static constexpr int BUF_OUT_COUNT = 6;

  const int bitrate;
  const int fps;
  const int height;
  const int width;

  bool base_set = false;
  bool header_written = false;
  bool warned_no_header = false;

  int fd = -1;
  int in_buf_size = 0;
  int uv_stride = 0;
  int y_scanlines = 0;
  int y_stride = 0;

  uint64_t base_ts_us = 0;

  int64_t last_dts = -1;

  std::atomic<bool> stream_error{false};

  std::atomic<uint64_t> drain_deadline_ns{0};

  std::thread dequeue_thread;

  VisionBuf buf_in[BUF_IN_COUNT];
  VisionBuf buf_out[BUF_OUT_COUNT];

  SafeQueue<unsigned int> free_buf_in;

  AVCodecContext *codec_ctx = nullptr;
  AVFormatContext *ofmt_ctx = nullptr;
  AVStream *out_stream = nullptr;

  std::string lock_path;
  std::string vid_path;
};
