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
void car_err_fun(double *nom_x, double *delta_x, double *out_4627553483824810991);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_3034444928256465443);
void car_H_mod_fun(double *state, double *out_1571749858522309971);
void car_f_fun(double *state, double dt, double *out_6067441169159859193);
void car_F_fun(double *state, double dt, double *out_7765987793843153008);
void car_h_25(double *state, double *unused, double *out_3070103496777372817);
void car_H_25(double *state, double *unused, double *out_7817550884622297004);
void car_h_24(double *state, double *unused, double *out_732461673771864771);
void car_H_24(double *state, double *unused, double *out_567694474361489429);
void car_h_30(double *state, double *unused, double *out_3345297559061878706);
void car_H_30(double *state, double *unused, double *out_5299217926115048377);
void car_h_26(double *state, double *unused, double *out_1880609656773446241);
void car_H_26(double *state, double *unused, double *out_6887689870213198388);
void car_h_27(double *state, double *unused, double *out_8948388984336087905);
void car_H_27(double *state, double *unused, double *out_7473981237915473288);
void car_h_29(double *state, double *unused, double *out_8346970826722724351);
void car_H_29(double *state, double *unused, double *out_4788986581800656193);
void car_h_28(double *state, double *unused, double *out_8895039204696986194);
void car_H_28(double *state, double *unused, double *out_8575358474839364849);
void car_h_31(double *state, double *unused, double *out_7483701082047655440);
void car_H_31(double *state, double *unused, double *out_6261481767979846912);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}