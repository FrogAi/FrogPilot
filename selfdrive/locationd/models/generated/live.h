#pragma once
#include "rednose/helpers/ekf.h"
extern "C" {
void live_update_4(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_9(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_10(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_12(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_35(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_32(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_13(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_14(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_update_33(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void live_H(double *in_vec, double *out_1030892538088703046);
void live_err_fun(double *nom_x, double *delta_x, double *out_4555843542312345642);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_913499310372198985);
void live_H_mod_fun(double *state, double *out_1455121800340994207);
void live_f_fun(double *state, double dt, double *out_7760840852727347336);
void live_F_fun(double *state, double dt, double *out_5904092994272526580);
void live_h_4(double *state, double *unused, double *out_683673907898961342);
void live_H_4(double *state, double *unused, double *out_6985725737682539694);
void live_h_9(double *state, double *unused, double *out_3518367756339512630);
void live_H_9(double *state, double *unused, double *out_6744536091052949049);
void live_h_10(double *state, double *unused, double *out_1696460479858437053);
void live_H_10(double *state, double *unused, double *out_8475046284041961594);
void live_h_12(double *state, double *unused, double *out_2563524166673299033);
void live_H_12(double *state, double *unused, double *out_1966269329650577899);
void live_h_35(double *state, double *unused, double *out_5427861829338939000);
void live_H_35(double *state, double *unused, double *out_3619063680309932318);
void live_h_32(double *state, double *unused, double *out_4559568879405682368);
void live_H_32(double *state, double *unused, double *out_466672007786990782);
void live_h_13(double *state, double *unused, double *out_1821948546406560367);
void live_H_13(double *state, double *unused, double *out_2303605643795534437);
void live_h_14(double *state, double *unused, double *out_3518367756339512630);
void live_H_14(double *state, double *unused, double *out_6744536091052949049);
void live_h_33(double *state, double *unused, double *out_1198873119594982396);
void live_H_33(double *state, double *unused, double *out_468506675671074714);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}