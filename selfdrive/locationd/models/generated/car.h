#pragma once
#include "rednose/helpers/ekf.h"
extern "C" {
void car_update_25(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_24(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_30(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_26(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_27(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_29(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_28(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_update_31(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea);
void car_err_fun(double *nom_x, double *delta_x, double *out_3737406110502016648);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2403136137136565417);
void car_H_mod_fun(double *state, double *out_9188041439089480832);
void car_f_fun(double *state, double dt, double *out_8940466509319208784);
void car_F_fun(double *state, double dt, double *out_4596333403153979532);
void car_h_25(double *state, double *unused, double *out_8177395392958100983);
void car_H_25(double *state, double *unused, double *out_2404778044889366002);
void car_h_24(double *state, double *unused, double *out_3033720568854893948);
void car_H_24(double *state, double *unused, double *out_227563621282216029);
void car_h_30(double *state, double *unused, double *out_6631899152497428283);
void car_H_30(double *state, double *unused, double *out_113554913617882625);
void car_h_26(double *state, double *unused, double *out_4509070587126112289);
void car_H_26(double *state, double *unused, double *out_899747924871434599);
void car_h_27(double *state, double *unused, double *out_7009651591881959246);
void car_H_27(double *state, double *unused, double *out_2061208398182542286);
void car_h_29(double *state, double *unused, double *out_1630225884836582638);
void car_H_29(double *state, double *unused, double *out_623786257932274809);
void car_h_28(double *state, double *unused, double *out_4197099488612105863);
void car_H_28(double *state, double *unused, double *out_4458612759137255765);
void car_h_31(double *state, double *unused, double *out_296122742204051798);
void car_H_31(double *state, double *unused, double *out_273539822638083123);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}