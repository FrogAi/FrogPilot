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
void live_H(double *in_vec, double *out_2551152945433930967);
void live_err_fun(double *nom_x, double *delta_x, double *out_6065003216777607013);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_8637710388403354025);
void live_H_mod_fun(double *state, double *out_2528101214593118882);
void live_f_fun(double *state, double dt, double *out_8954241495774098044);
void live_F_fun(double *state, double dt, double *out_1099085337224138825);
void live_h_4(double *state, double *unused, double *out_4104064553155268601);
void live_H_4(double *state, double *unused, double *out_4731541239389966221);
void live_h_9(double *state, double *unused, double *out_3243196974677797653);
void live_H_9(double *state, double *unused, double *out_6427983899055137925);
void live_h_10(double *state, double *unused, double *out_2167761159853206631);
void live_H_10(double *state, double *unused, double *out_3020789051413255206);
void live_h_12(double *state, double *unused, double *out_5219913024747697854);
void live_H_12(double *state, double *unused, double *out_5352640264437559888);
void live_h_35(double *state, double *unused, double *out_1875702029625223179);
void live_H_35(double *state, double *unused, double *out_8098203296762573597);
void live_h_32(double *state, double *unused, double *out_5317356958784015429);
void live_H_32(double *state, double *unused, double *out_7643516264045228082);
void live_h_13(double *state, double *unused, double *out_5773064861780981417);
void live_H_13(double *state, double *unused, double *out_8943953577674599694);
void live_h_14(double *state, double *unused, double *out_3243196974677797653);
void live_H_14(double *state, double *unused, double *out_6427983899055137925);
void live_h_33(double *state, double *unused, double *out_3955100895678966380);
void live_H_33(double *state, double *unused, double *out_7197983772308120415);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}