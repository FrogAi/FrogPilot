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
void car_err_fun(double *nom_x, double *delta_x, double *out_1762034731848154322);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1923774774921120974);
void car_H_mod_fun(double *state, double *out_8208381335871607819);
void car_f_fun(double *state, double dt, double *out_1983193947830437688);
void car_F_fun(double *state, double dt, double *out_8589051623902649482);
void car_h_25(double *state, double *unused, double *out_2444284713406751542);
void car_H_25(double *state, double *unused, double *out_866685846547765582);
void car_h_24(double *state, double *unused, double *out_2334447044872400550);
void car_H_24(double *state, double *unused, double *out_6140180129460778176);
void car_h_30(double *state, double *unused, double *out_1193164814847270808);
void car_H_30(double *state, double *unused, double *out_3385018805055014209);
void car_h_26(double *state, double *unused, double *out_6523714585703761970);
void car_H_26(double *state, double *unused, double *out_2874817472326290642);
void car_h_27(double *state, double *unused, double *out_3198445157674205293);
void car_H_27(double *state, double *unused, double *out_5608612876238957426);
void car_h_29(double *state, double *unused, double *out_1796480699523748325);
void car_H_29(double *state, double *unused, double *out_3895250149369406393);
void car_h_28(double *state, double *unused, double *out_7623806072972436826);
void car_H_28(double *state, double *unused, double *out_1187148867700124181);
void car_h_31(double *state, double *unused, double *out_2719478775691257431);
void car_H_31(double *state, double *unused, double *out_897331808424726010);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}