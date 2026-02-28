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
void car_err_fun(double *nom_x, double *delta_x, double *out_5588406888365626113);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_8022320061418085482);
void car_H_mod_fun(double *state, double *out_7995506957028497918);
void car_f_fun(double *state, double dt, double *out_2360361574345086061);
void car_F_fun(double *state, double dt, double *out_7631457160567166404);
void car_h_25(double *state, double *unused, double *out_6987712039313090741);
void car_H_25(double *state, double *unused, double *out_4445037697698347388);
void car_h_24(double *state, double *unused, double *out_8502122595903451231);
void car_H_24(double *state, double *unused, double *out_887112098896024252);
void car_h_30(double *state, double *unused, double *out_4039986048957605780);
void car_H_30(double *state, double *unused, double *out_2471652643793269367);
void car_h_26(double *state, double *unused, double *out_290244726764480927);
void car_H_26(double *state, double *unused, double *out_8186541016572403612);
void car_h_27(double *state, double *unused, double *out_885192603579896351);
void car_H_27(double *state, double *unused, double *out_296889331992844456);
void car_h_29(double *state, double *unused, double *out_2752256924674125999);
void car_H_29(double *state, double *unused, double *out_2981883988107661551);
void car_h_28(double *state, double *unused, double *out_3921978655024229036);
void car_H_28(double *state, double *unused, double *out_6498872411946237151);
void car_h_31(double *state, double *unused, double *out_6162059933525247885);
void car_H_31(double *state, double *unused, double *out_4414391735821386960);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}