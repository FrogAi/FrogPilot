#pragma once

#include <future>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "common/queue.h"

enum ParamKeyFlag {
  PERSISTENT = 0x02,
  CLEAR_ON_MANAGER_START = 0x04,
  CLEAR_ON_ONROAD_TRANSITION = 0x08,
  CLEAR_ON_OFFROAD_TRANSITION = 0x10,
  DONT_LOG = 0x20,
  DEVELOPMENT_ONLY = 0x40,
  CLEAR_ON_IGNITION_ON = 0x80,
  ALL = 0xFFFFFFFF
};

enum ParamKeyType {
  STRING = 0, // must be utf-8 decodable
  BOOL = 1,
  INT = 2,
  FLOAT = 3,
  TIME = 4, // ISO 8601
  JSON = 5,
  BYTES = 6
};

struct ParamKeyAttributes {
  uint32_t flags;
  ParamKeyType type;
  std::optional<std::string> default_value = std::nullopt;

  // FrogPilot variables
  std::optional<std::string> stock_value = std::nullopt;

  int tuning_level = 0;
};

class Params {
public:
  explicit Params(const std::string &path = {}, bool cache = false, bool memory = false);
  ~Params();
  // Not copyable.
  Params(const Params&) = delete;
  Params& operator=(const Params&) = delete;

  std::vector<std::string> allKeys() const;
  bool checkKey(const std::string &key);
  ParamKeyFlag getKeyFlag(const std::string &key);
  ParamKeyType getKeyType(const std::string &key);
  std::optional<std::string> getKeyDefaultValue(const std::string &key);
  inline std::string getParamPath(const std::string &key = {}) {
    return params_path + params_prefix + (key.empty() ? "" : "/" + key);
  }

  // Delete a value
  int remove(const std::string &key);
  void clearAll(ParamKeyFlag flag);

  // helpers for reading values
  std::string get(const std::string &key, bool block = false);
  inline bool getBool(const std::string &key, bool block = false) {
    return get(key, block) == "1";
  }
  std::map<std::string, std::string> readAll();

  // helpers for writing values
  int put(const char *key, const char *val, size_t value_size);
  inline int put(const std::string &key, const std::string &val) {
    return put(key.c_str(), val.data(), val.size());
  }
  inline int putBool(const std::string &key, bool val) {
    return put(key.c_str(), val ? "1" : "0", 1);
  }
  void putNonBlocking(const std::string &key, const std::string &val);
  inline void putBoolNonBlocking(const std::string &key, bool val) {
    putNonBlocking(key, val ? "1" : "0");
  }

  // FrogPilot variables
  inline int getInt(const std::string &key, bool block = false) {
    std::string value = get(key, block);
    return value.empty() ? 0 : std::stoi(value);
  }
  inline float getFloat(const std::string &key, bool block = false) {
    std::string value = get(key, block);
    return value.empty() ? 0.0 : std::stof(value);
  }

  inline int putInt(const std::string &key, int val) {
    return put(key.c_str(), std::to_string(val).c_str(), std::to_string(val).size());
  }
  inline int putFloat(const std::string &key, float val) {
    return put(key.c_str(), std::to_string(val).c_str(), std::to_string(val).size());
  }

  inline void putIntNonBlocking(const std::string &key, int val) {
    putNonBlocking(key, std::to_string(val));
  }
  inline void putFloatNonBlocking(const std::string &key, float val) {
    putNonBlocking(key, std::to_string(val));
  }

  int getTuningLevel(const std::string &key);

  std::optional<std::string> getStockValue(const std::string &key);

private:
  void asyncWriteThread();

  std::string params_path;
  std::string params_prefix;

  // for nonblocking write
  std::future<void> future;
  SafeQueue<std::pair<std::string, std::string>> queue;
};
