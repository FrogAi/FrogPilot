
#include "system/loggerd/zstd_writer.h"

#include <cassert>
#include <cerrno>
#include <cstring>

#include "common/swaglog.h"
#include "common/util.h"

// Constructor: Initializes compression stream and opens file
ZstdFileWriter::ZstdFileWriter(const std::string& filename, int compression_level) : filename_(filename) {
  // Create the compression stream
  cstream_ = ZSTD_createCStream();
  assert(cstream_);

  size_t initResult = ZSTD_initCStream(cstream_, compression_level);
  assert(!ZSTD_isError(initResult));

  input_cache_capacity_ = ZSTD_CStreamInSize();
  input_cache_.reserve(input_cache_capacity_);
  output_buffer_.resize(ZSTD_CStreamOutSize());

  // a failed open leaves the writer in a no-op state instead of aborting,
  // so loggerd survives log storage failure (e.g. an ext4 read-only remount)
  file_ = util::safe_fopen(filename.c_str(), "wb");
  if (file_ == nullptr) {
    LOGE("ZstdFileWriter: failed to open %s: %s", filename.c_str(), strerror(errno));
  }
}

// Destructor: Finalizes compression and closes file
ZstdFileWriter::~ZstdFileWriter() {
  if (file_ != nullptr) {
    if (!write_error_) {
      flushCache(true);
      util::safe_fflush(file_);
    }
    if (fclose(file_) != 0) {
      LOGE("ZstdFileWriter: failed to close %s: %s", filename_.c_str(), strerror(errno));
    }
    file_ = nullptr;
  }

  ZSTD_freeCStream(cstream_);
}

// Compresses and writes data to file
void ZstdFileWriter::write(void* data, size_t size) {
  // Drop data while the writer is broken so the cache can't grow unbounded
  if (!ok()) return;

  // Add data to the input cache
  input_cache_.insert(input_cache_.end(), (uint8_t*)data, (uint8_t*)data + size);

  // If the cache is full, compress and write to the file
  if (input_cache_.size() >= input_cache_capacity_) {
    flushCache(false);
  }
}

// Compress and flush the input cache to the file
void ZstdFileWriter::flushCache(bool last_chunk) {
  if (!ok()) {
    input_cache_.clear();
    return;
  }

  ZSTD_inBuffer input = {input_cache_.data(), input_cache_.size(), 0};
  ZSTD_EndDirective mode = !last_chunk ? ZSTD_e_continue : ZSTD_e_end;
  int finished = 0;

  do {
    ZSTD_outBuffer output = {output_buffer_.data(), output_buffer_.size(), 0};
    size_t remaining = ZSTD_compressStream2(cstream_, &output, &input, mode);
    assert(!ZSTD_isError(remaining));

    size_t written = util::safe_fwrite(output_buffer_.data(), 1, output.pos, file_);
    if (written != output.pos) {
      // storage failure (EIO/EROFS): latch the error and go no-op instead of aborting
      LOGE("ZstdFileWriter: failed to write %s: %s", filename_.c_str(), strerror(errno));
      write_error_ = true;
      input_cache_.clear();
      return;
    }

    finished = last_chunk ? (remaining == 0) : (input.pos == input.size);
  } while (!finished);

  input_cache_.clear();  // Clear cache after compression
}
