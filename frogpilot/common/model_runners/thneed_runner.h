#pragma once

#include <cassert>

#include "frogpilot/common/model_runners/thneed.h"

struct NativeModel;

extern "C" {

NativeModel *thneed_model_create(const char *path);
void thneed_model_run(NativeModel *model, float **inputs, float *output);

}
