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
void live_H(double *in_vec, double *out_1775695409103746766);
void live_err_fun(double *nom_x, double *delta_x, double *out_4080072497468539864);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_8455093254969418897);
void live_H_mod_fun(double *state, double *out_6942381718179375186);
void live_f_fun(double *state, double dt, double *out_869332632954176924);
void live_F_fun(double *state, double dt, double *out_7462977281681762749);
void live_h_4(double *state, double *unused, double *out_3447343603239487397);
void live_H_4(double *state, double *unused, double *out_3422419312934850626);
void live_h_9(double *state, double *unused, double *out_1566617830585758854);
void live_H_9(double *state, double *unused, double *out_7737105825510253520);
void live_h_10(double *state, double *unused, double *out_1214253740792945755);
void live_H_10(double *state, double *unused, double *out_3794911453710658286);
void live_h_12(double *state, double *unused, double *out_3488710232913772919);
void live_H_12(double *state, double *unused, double *out_8441875720966812421);
void live_h_35(double *state, double *unused, double *out_3145348211035984820);
void live_H_35(double *state, double *unused, double *out_6789081370307458002);
void live_h_32(double *state, double *unused, double *out_1759377177639706396);
void live_H_32(double *state, double *unused, double *out_8952638190500343677);
void live_h_13(double *state, double *unused, double *out_5751607250102481191);
void live_H_13(double *state, double *unused, double *out_3391200598047122102);
void live_h_14(double *state, double *unused, double *out_1566617830585758854);
void live_H_14(double *state, double *unused, double *out_7737105825510253520);
void live_h_33(double *state, double *unused, double *out_8157701097653920707);
void live_H_33(double *state, double *unused, double *out_8507105698763236010);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}