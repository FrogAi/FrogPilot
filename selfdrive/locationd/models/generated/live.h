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
void live_H(double *in_vec, double *out_4171763688590799061);
void live_err_fun(double *nom_x, double *delta_x, double *out_74794720923982406);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_7969314602708569285);
void live_H_mod_fun(double *state, double *out_3607177039271197524);
void live_f_fun(double *state, double dt, double *out_2027835697335042621);
void live_F_fun(double *state, double dt, double *out_7319548843794364945);
void live_h_4(double *state, double *unused, double *out_1386069147269809586);
void live_H_4(double *state, double *unused, double *out_4398745843108300130);
void live_h_9(double *state, double *unused, double *out_8327271951963491608);
void live_H_9(double *state, double *unused, double *out_241578106753522647);
void live_h_10(double *state, double *unused, double *out_4058930997587686025);
void live_H_10(double *state, double *unused, double *out_4039467529030858569);
void live_h_12(double *state, double *unused, double *out_621371140025306491);
void live_H_12(double *state, double *unused, double *out_5019844868155893797);
void live_h_35(double *state, double *unused, double *out_1739017043157635559);
void live_H_35(double *state, double *unused, double *out_7765407900480907506);
void live_h_32(double *state, double *unused, double *out_7616018766603410498);
void live_H_32(double *state, double *unused, double *out_851648720175369692);
void live_h_13(double *state, double *unused, double *out_825166878468725327);
void live_H_13(double *state, double *unused, double *out_250107028938897244);
void live_h_14(double *state, double *unused, double *out_8327271951963491608);
void live_H_14(double *state, double *unused, double *out_241578106753522647);
void live_h_33(double *state, double *unused, double *out_4196250099107922616);
void live_H_33(double *state, double *unused, double *out_7530779168589786506);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}