#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "frogpilot/ui/qt/widgets/frogpilot_controls.h"

bool modelFileMatches(const QString &file, const QString &modelKey);

namespace {
void writeFiles(const QDir &dir, const QStringList &names) {
  for (const QString &name : names) {
    QFile file(dir.filePath(name));
    REQUIRE(file.open(QIODevice::WriteOnly));
  }
}
}

TEST_CASE("downloaded tinygrad models") {
  QTemporaryDir modelsDir;
  QDir dir(modelsDir.path());
  writeFiles(dir, {
    "cd210-model_driving_policy_metadata.pkl",
    "cd210-model_driving_policy_tinygrad.pkl",
    "cd210-model_driving_vision_metadata.pkl",
    "cheddar-model_driving_metadata.pkl",
    "cheddar-model_driving_tinygrad.pkl",
    "partial-model_driving_tinygrad.pkl",
  });

  REQUIRE(hasAllTinygradFiles(dir, "cheddar-model"));
  REQUIRE_FALSE(hasAllTinygradFiles(dir, "cd210-model"));
  REQUIRE_FALSE(hasAllTinygradFiles(dir, "partial-model"));

  writeFiles(dir, {"cd210-model_driving_vision_tinygrad.pkl"});

  REQUIRE(hasAllTinygradFiles(dir, "cd210-model"));
}

TEST_CASE("model file matches") {
  REQUIRE(modelFileMatches("cd210-model_driving_vision_tinygrad.pkl", "cd210-model"));
  REQUIRE(modelFileMatches("cheddar-model_driving_metadata.pkl", "cheddar-model"));
  REQUIRE(modelFileMatches("cheddar-model_driving_tinygrad.pkl", "cheddar-model"));
  REQUIRE(modelFileMatches("wd-40.thneed", "wd-40"));
  REQUIRE_FALSE(modelFileMatches("cheddar-model_driving_tinygrad.pkl", "cheddar"));
}
