#include "frogpilot/common/model_runners/thneed_runner.h"

struct NativeModel {
  Thneed thneed;
  bool recorded = false;

  NativeModel(const char *path) : thneed(true, nullptr) {
    thneed.load(path);

    cl_int result = thneed.clexec();
    assert(result == CL_SUCCESS);
  }
};

extern "C" {

NativeModel *thneed_model_create(const char *path) {
  return new NativeModel(path);
}

void thneed_model_run(NativeModel *model, float **inputs, float *output) {
  if (!model->recorded) {
    model->thneed.record = true;
    model->thneed.copy_inputs(inputs);

    cl_int result = model->thneed.clexec();
    assert(result == CL_SUCCESS);

    model->thneed.copy_output(output);
    model->thneed.stop();
    model->recorded = true;
  } else {
    model->thneed.execute(inputs, output);
  }
}

}
