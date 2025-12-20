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
void car_err_fun(double *nom_x, double *delta_x, double *out_1174255670832059551);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_4619408999502106690);
void car_H_mod_fun(double *state, double *out_6508123924648701622);
void car_f_fun(double *state, double dt, double *out_8567017668350943053);
void car_F_fun(double *state, double dt, double *out_2679543167722733093);
void car_h_25(double *state, double *unused, double *out_2527539469091572327);
void car_H_25(double *state, double *unused, double *out_3385424745847202663);
void car_h_24(double *state, double *unused, double *out_1306671744405780298);
void car_H_24(double *state, double *unused, double *out_1483390119180504189);
void car_h_30(double *state, double *unused, double *out_2673059293312227298);
void car_H_30(double *state, double *unused, double *out_1142271584280405535);
void car_h_26(double *state, double *unused, double *out_7411205342819041228);
void car_H_26(double *state, double *unused, double *out_356078573026853561);
void car_h_27(double *state, double *unused, double *out_7529212736752417972);
void car_H_27(double *state, double *unused, double *out_3317034896080830446);
void car_h_29(double *state, double *unused, double *out_68653972503529647);
void car_H_29(double *state, double *unused, double *out_632040239966013351);
void car_h_28(double *state, double *unused, double *out_5915858069296287999);
void car_H_28(double *state, double *unused, double *out_5714439257035543925);
void car_h_31(double *state, double *unused, double *out_8155939347797306848);
void car_H_31(double *state, double *unused, double *out_982286675260205037);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}