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
void live_H(double *in_vec, double *out_5171794447343407321);
void live_err_fun(double *nom_x, double *delta_x, double *out_1317002562073362978);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_7636025450415368412);
void live_H_mod_fun(double *state, double *out_1500301335471955957);
void live_f_fun(double *state, double dt, double *out_4032995164320582650);
void live_F_fun(double *state, double dt, double *out_3947896294475931852);
void live_h_4(double *state, double *unused, double *out_6701028740274849865);
void live_H_4(double *state, double *unused, double *out_7070150886838624903);
void live_h_9(double *state, double *unused, double *out_4141582308850062639);
void live_H_9(double *state, double *unused, double *out_4089374251606479243);
void live_h_10(double *state, double *unused, double *out_4515977219178952433);
void live_H_10(double *state, double *unused, double *out_6410890604015588949);
void live_h_12(double *state, double *unused, double *out_5337922668901600339);
void live_H_12(double *state, double *unused, double *out_688892509795891907);
void live_h_35(double *state, double *unused, double *out_6914618105715301166);
void live_H_35(double *state, double *unused, double *out_3611573746513951209);
void live_h_32(double *state, double *unused, double *out_2537235844539281332);
void live_H_32(double *state, double *unused, double *out_3924195441401396237);
void live_h_13(double *state, double *unused, double *out_1306748253489036171);
void live_H_13(double *state, double *unused, double *out_8441583874601616301);
void live_h_14(double *state, double *unused, double *out_4141582308850062639);
void live_H_14(double *state, double *unused, double *out_4089374251606479243);
void live_h_33(double *state, double *unused, double *out_7163487222908004242);
void live_H_33(double *state, double *unused, double *out_461016741875093605);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}