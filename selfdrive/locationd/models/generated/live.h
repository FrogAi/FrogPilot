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
void live_H(double *in_vec, double *out_2114416079634207852);
void live_err_fun(double *nom_x, double *delta_x, double *out_372316230930825639);
void live_inv_err_fun(double *nom_x, double *true_x, double *out_1693261261373619934);
void live_H_mod_fun(double *state, double *out_8334019462529006929);
void live_f_fun(double *state, double dt, double *out_3758975137131001086);
void live_F_fun(double *state, double dt, double *out_1883468306993083301);
void live_h_4(double *state, double *unused, double *out_7357049428002528646);
void live_H_4(double *state, double *unused, double *out_3521429698098074330);
void live_h_9(double *state, double *unused, double *out_6230799881466344411);
void live_H_9(double *state, double *unused, double *out_3762619344727664975);
void live_h_10(double *state, double *unused, double *out_6725900980060908231);
void live_H_10(double *state, double *unused, double *out_1425692259011899697);
void live_h_12(double *state, double *unused, double *out_7836144418372183403);
void live_H_12(double *state, double *unused, double *out_8540886106130036125);
void live_h_35(double *state, double *unused, double *out_5515682694374441280);
void live_H_35(double *state, double *unused, double *out_6888091755470681706);
void live_h_32(double *state, double *unused, double *out_9112069325649196173);
void live_H_32(double *state, double *unused, double *out_4376636770836084189);
void live_h_13(double *state, double *unused, double *out_4651474403522186471);
void live_H_13(double *state, double *unused, double *out_4885981829847031971);
void live_h_14(double *state, double *unused, double *out_6230799881466344411);
void live_H_14(double *state, double *unused, double *out_3762619344727664975);
void live_h_33(double *state, double *unused, double *out_3359769451900324528);
void live_H_33(double *state, double *unused, double *out_8408095313600012306);
void live_predict(double *in_x, double *in_P, double *in_Q, double dt);
}