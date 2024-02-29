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
void live_H(double *in_vec, double *out_3201977696260257895);
void live_err_fun(double *nom_x, double *delta_x, double *out_997666715504881949);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_1105659959287770713);
void live_H_mod_fun(double *state, double *out_3225137284270052937);
void live_f_fun(double *state, double dt, double *out_6676648528904182614);
void live_F_fun(double *state, double dt, double *out_2422814851340655634);
void live_h_4(double *state, double *unused, double *out_7905629914609901013);
void live_H_4(double *state, double *unused, double *out_6417935588060894208);
void live_h_9(double *state, double *unused, double *out_6011904027858753050);
void live_H_9(double *state, double *unused, double *out_7871640749293879925);
void live_h_10(double *state, double *unused, double *out_4787295524011749156);
void live_H_10(double *state, double *unused, double *out_2791411260120262675);
void live_h_12(double *state, double *unused, double *out_1064618350480509991);
void live_H_12(double *state, double *unused, double *out_5796836563013300541);
void live_h_35(double *state, double *unused, double *out_6961984814518061679);
void live_H_35(double *state, double *unused, double *out_3051273530688286832);
void live_h_32(double *state, double *unused, double *out_6892861438456163734);
void live_H_32(double *state, double *unused, double *out_267615366269869239);
void live_h_13(double *state, double *unused, double *out_6939087373795999189);
void live_H_13(double *state, double *unused, double *out_2344787966839707023);
void live_h_14(double *state, double *unused, double *out_6011904027858753050);
void live_H_14(double *state, double *unused, double *out_7871640749293879925);
void live_h_33(double *state, double *unused, double *out_5996726837204422069);
void live_H_33(double *state, double *unused, double *out_99283473950570772);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}