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
void car_err_fun(double *nom_x, double *delta_x, double *out_9029221129050068959);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_6139230198769637759);
void car_H_mod_fun(double *state, double *out_4005708931311870903);
void car_f_fun(double *state, double dt, double *out_708450726625971407);
void car_F_fun(double *state, double dt, double *out_4447378851817122637);
void car_h_25(double *state, double *unused, double *out_4251623014853923117);
void car_H_25(double *state, double *unused, double *out_7019278493219463555);
void car_h_24(double *state, double *unused, double *out_632683078013282463);
void car_H_24(double *state, double *unused, double *out_7869539981687764925);
void car_h_30(double *state, double *unused, double *out_4200662900743896530);
void car_H_30(double *state, double *unused, double *out_4510775238998471306);
void car_h_26(double *state, double *unused, double *out_8367795685406939998);
void car_H_26(double *state, double *unused, double *out_3277775174345407331);
void car_h_27(double *state, double *unused, double *out_3648307130177445600);
void car_H_27(double *state, double *unused, double *out_6685538550798896217);
void car_h_29(double *state, double *unused, double *out_8027601717181755080);
void car_H_29(double *state, double *unused, double *out_8398901277668447250);
void car_h_28(double *state, double *unused, double *out_6252712450986143251);
void car_H_28(double *state, double *unused, double *out_4965443778971573792);
void car_h_31(double *state, double *unused, double *out_8113306491282424769);
void car_H_31(double *state, double *unused, double *out_7049924455096423983);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}