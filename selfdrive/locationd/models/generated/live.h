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
void live_H(double *in_vec, double *out_1452770364131172983);
void live_err_fun(double *nom_x, double *delta_x, double *out_6477708461690947783);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_5234845542127999715);
void live_H_mod_fun(double *state, double *out_928207725729189715);
void live_f_fun(double *state, double dt, double *out_3815188181926804977);
void live_F_fun(double *state, double dt, double *out_1484174514504545978);
void live_h_4(double *state, double *unused, double *out_458302490386935556);
void live_H_4(double *state, double *unused, double *out_8904350707421158349);
void live_h_9(double *state, double *unused, double *out_6581587843007508622);
void live_H_9(double *state, double *unused, double *out_8663161060791567704);
void live_h_10(double *state, double *unused, double *out_7750736964248749635);
void live_H_10(double *state, double *unused, double *out_5742515571475046617);
void live_h_12(double *state, double *unused, double *out_7393609720072682745);
void live_H_12(double *state, double *unused, double *out_3884894299389196554);
void live_h_35(double *state, double *unused, double *out_6707548545740942886);
void live_H_35(double *state, double *unused, double *out_1139331267064182845);
void live_h_32(double *state, double *unused, double *out_6342799956956973114);
void live_H_32(double *state, double *unused, double *out_5406523865107810504);
void live_h_13(double *state, double *unused, double *out_1386348835464089497);
void live_H_13(double *state, double *unused, double *out_2619083771954748826);
void live_h_14(double *state, double *unused, double *out_6581587843007508622);
void live_H_14(double *state, double *unused, double *out_8663161060791567704);
void live_h_33(double *state, double *unused, double *out_7779036136666018931);
void live_H_33(double *state, double *unused, double *out_2387131645409693369);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}