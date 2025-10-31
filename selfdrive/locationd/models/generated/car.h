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
void car_err_fun(double *nom_x, double *delta_x, double *out_1677816508566937490);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1895284364952760231);
void car_H_mod_fun(double *state, double *out_2734817720835474974);
void car_f_fun(double *state, double dt, double *out_2384083579918943231);
void car_F_fun(double *state, double dt, double *out_6808241585095737233);
void car_h_25(double *state, double *unused, double *out_5529562536143676140);
void car_H_25(double *state, double *unused, double *out_4185841419278708954);
void car_h_24(double *state, double *unused, double *out_3020037646968069856);
void car_H_24(double *state, double *unused, double *out_7685330847839324561);
void car_h_30(double *state, double *unused, double *out_9167012064397698490);
void car_H_30(double *state, double *unused, double *out_7344212312939225907);
void car_h_26(double *state, double *unused, double *out_4165338796736852845);
void car_H_26(double *state, double *unused, double *out_7490367389039509555);
void car_h_27(double *state, double *unused, double *out_3452406635863164625);
void car_H_27(double *state, double *unused, double *out_8927768448969900798);
void car_h_29(double *state, double *unused, double *out_8223502211496036055);
void car_H_29(double *state, double *unused, double *out_6833980968624833723);
void car_h_28(double *state, double *unused, double *out_1549266631797681020);
void car_H_28(double *state, double *unused, double *out_6530364088015187319);
void car_h_31(double *state, double *unused, double *out_915048594877817131);
void car_H_31(double *state, double *unused, double *out_7184227403919025409);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}