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
void car_err_fun(double *nom_x, double *delta_x, double *out_5279200563100949282);
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2794898786486738948);
void car_H_mod_fun(double *state, double *out_3002480874372569413);
void car_f_fun(double *state, double dt, double *out_8611914425259886085);
void car_F_fun(double *state, double dt, double *out_8493233740844108942);
void car_h_25(double *state, double *unused, double *out_3148022997593239894);
void car_H_25(double *state, double *unused, double *out_745662918452737419);
void car_h_24(double *state, double *unused, double *out_5661741839916811149);
void car_H_24(double *state, double *unused, double *out_5076586435370679900);
void car_h_30(double *state, double *unused, double *out_1104814561170473263);
void car_H_30(double *state, double *unused, double *out_875001865595977489);
void car_h_26(double *state, double *unused, double *out_161304708268810828);
void car_H_26(double *state, double *unused, double *out_4487166237326793643);
void car_h_27(double *state, double *unused, double *out_3751338882269717411);
void car_H_27(double *state, double *unused, double *out_3049765177396402400);
void car_h_29(double *state, double *unused, double *out_3709219881979170914);
void car_H_29(double *state, double *unused, double *out_4763127904265953433);
void car_h_28(double *state, double *unused, double *out_4654317937374509242);
void car_H_28(double *state, double *unused, double *out_8601217152374067609);
void car_h_31(double *state, double *unused, double *out_7535455819295639281);
void car_H_31(double *state, double *unused, double *out_715016956575776991);
void car_predict(double *in_x, double *in_P, double *in_Q, double dt);
void car_set_mass(double x);
void car_set_rotational_inertia(double x);
void car_set_center_to_front(double x);
void car_set_center_to_rear(double x);
void car_set_stiffness_front(double x);
void car_set_stiffness_rear(double x);
}