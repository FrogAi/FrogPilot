#include "car.h"

namespace {
#define DIM 9
#define EDIM 9
#define MEDIM 9
typedef void (*Hfun)(double *, double *, double *);

double mass;

void set_mass(double x){ mass = x;}

double rotational_inertia;

void set_rotational_inertia(double x){ rotational_inertia = x;}

double center_to_front;

void set_center_to_front(double x){ center_to_front = x;}

double center_to_rear;

void set_center_to_rear(double x){ center_to_rear = x;}

double stiffness_front;

void set_stiffness_front(double x){ stiffness_front = x;}

double stiffness_rear;

void set_stiffness_rear(double x){ stiffness_rear = x;}
const static double MAHA_THRESH_25 = 3.8414588206941227;
const static double MAHA_THRESH_24 = 5.991464547107981;
const static double MAHA_THRESH_30 = 3.8414588206941227;
const static double MAHA_THRESH_26 = 3.8414588206941227;
const static double MAHA_THRESH_27 = 3.8414588206941227;
const static double MAHA_THRESH_29 = 3.8414588206941227;
const static double MAHA_THRESH_28 = 3.8414588206941227;
const static double MAHA_THRESH_31 = 3.8414588206941227;

/******************************************************************************
 *                       Code generated with SymPy 1.12                       *
 *                                                                            *
 *              See http://www.sympy.org/ for more information.               *
 *                                                                            *
 *                         This file is part of 'ekf'                         *
 ******************************************************************************/
void err_fun(double *nom_x, double *delta_x, double *out_7718808222430479447) {
   out_7718808222430479447[0] = delta_x[0] + nom_x[0];
   out_7718808222430479447[1] = delta_x[1] + nom_x[1];
   out_7718808222430479447[2] = delta_x[2] + nom_x[2];
   out_7718808222430479447[3] = delta_x[3] + nom_x[3];
   out_7718808222430479447[4] = delta_x[4] + nom_x[4];
   out_7718808222430479447[5] = delta_x[5] + nom_x[5];
   out_7718808222430479447[6] = delta_x[6] + nom_x[6];
   out_7718808222430479447[7] = delta_x[7] + nom_x[7];
   out_7718808222430479447[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_1034114423937537934) {
   out_1034114423937537934[0] = -nom_x[0] + true_x[0];
   out_1034114423937537934[1] = -nom_x[1] + true_x[1];
   out_1034114423937537934[2] = -nom_x[2] + true_x[2];
   out_1034114423937537934[3] = -nom_x[3] + true_x[3];
   out_1034114423937537934[4] = -nom_x[4] + true_x[4];
   out_1034114423937537934[5] = -nom_x[5] + true_x[5];
   out_1034114423937537934[6] = -nom_x[6] + true_x[6];
   out_1034114423937537934[7] = -nom_x[7] + true_x[7];
   out_1034114423937537934[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_7225960904852606817) {
   out_7225960904852606817[0] = 1.0;
   out_7225960904852606817[1] = 0;
   out_7225960904852606817[2] = 0;
   out_7225960904852606817[3] = 0;
   out_7225960904852606817[4] = 0;
   out_7225960904852606817[5] = 0;
   out_7225960904852606817[6] = 0;
   out_7225960904852606817[7] = 0;
   out_7225960904852606817[8] = 0;
   out_7225960904852606817[9] = 0;
   out_7225960904852606817[10] = 1.0;
   out_7225960904852606817[11] = 0;
   out_7225960904852606817[12] = 0;
   out_7225960904852606817[13] = 0;
   out_7225960904852606817[14] = 0;
   out_7225960904852606817[15] = 0;
   out_7225960904852606817[16] = 0;
   out_7225960904852606817[17] = 0;
   out_7225960904852606817[18] = 0;
   out_7225960904852606817[19] = 0;
   out_7225960904852606817[20] = 1.0;
   out_7225960904852606817[21] = 0;
   out_7225960904852606817[22] = 0;
   out_7225960904852606817[23] = 0;
   out_7225960904852606817[24] = 0;
   out_7225960904852606817[25] = 0;
   out_7225960904852606817[26] = 0;
   out_7225960904852606817[27] = 0;
   out_7225960904852606817[28] = 0;
   out_7225960904852606817[29] = 0;
   out_7225960904852606817[30] = 1.0;
   out_7225960904852606817[31] = 0;
   out_7225960904852606817[32] = 0;
   out_7225960904852606817[33] = 0;
   out_7225960904852606817[34] = 0;
   out_7225960904852606817[35] = 0;
   out_7225960904852606817[36] = 0;
   out_7225960904852606817[37] = 0;
   out_7225960904852606817[38] = 0;
   out_7225960904852606817[39] = 0;
   out_7225960904852606817[40] = 1.0;
   out_7225960904852606817[41] = 0;
   out_7225960904852606817[42] = 0;
   out_7225960904852606817[43] = 0;
   out_7225960904852606817[44] = 0;
   out_7225960904852606817[45] = 0;
   out_7225960904852606817[46] = 0;
   out_7225960904852606817[47] = 0;
   out_7225960904852606817[48] = 0;
   out_7225960904852606817[49] = 0;
   out_7225960904852606817[50] = 1.0;
   out_7225960904852606817[51] = 0;
   out_7225960904852606817[52] = 0;
   out_7225960904852606817[53] = 0;
   out_7225960904852606817[54] = 0;
   out_7225960904852606817[55] = 0;
   out_7225960904852606817[56] = 0;
   out_7225960904852606817[57] = 0;
   out_7225960904852606817[58] = 0;
   out_7225960904852606817[59] = 0;
   out_7225960904852606817[60] = 1.0;
   out_7225960904852606817[61] = 0;
   out_7225960904852606817[62] = 0;
   out_7225960904852606817[63] = 0;
   out_7225960904852606817[64] = 0;
   out_7225960904852606817[65] = 0;
   out_7225960904852606817[66] = 0;
   out_7225960904852606817[67] = 0;
   out_7225960904852606817[68] = 0;
   out_7225960904852606817[69] = 0;
   out_7225960904852606817[70] = 1.0;
   out_7225960904852606817[71] = 0;
   out_7225960904852606817[72] = 0;
   out_7225960904852606817[73] = 0;
   out_7225960904852606817[74] = 0;
   out_7225960904852606817[75] = 0;
   out_7225960904852606817[76] = 0;
   out_7225960904852606817[77] = 0;
   out_7225960904852606817[78] = 0;
   out_7225960904852606817[79] = 0;
   out_7225960904852606817[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_5925834804962007644) {
   out_5925834804962007644[0] = state[0];
   out_5925834804962007644[1] = state[1];
   out_5925834804962007644[2] = state[2];
   out_5925834804962007644[3] = state[3];
   out_5925834804962007644[4] = state[4];
   out_5925834804962007644[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_5925834804962007644[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_5925834804962007644[7] = state[7];
   out_5925834804962007644[8] = state[8];
}
void F_fun(double *state, double dt, double *out_1594563891292900416) {
   out_1594563891292900416[0] = 1;
   out_1594563891292900416[1] = 0;
   out_1594563891292900416[2] = 0;
   out_1594563891292900416[3] = 0;
   out_1594563891292900416[4] = 0;
   out_1594563891292900416[5] = 0;
   out_1594563891292900416[6] = 0;
   out_1594563891292900416[7] = 0;
   out_1594563891292900416[8] = 0;
   out_1594563891292900416[9] = 0;
   out_1594563891292900416[10] = 1;
   out_1594563891292900416[11] = 0;
   out_1594563891292900416[12] = 0;
   out_1594563891292900416[13] = 0;
   out_1594563891292900416[14] = 0;
   out_1594563891292900416[15] = 0;
   out_1594563891292900416[16] = 0;
   out_1594563891292900416[17] = 0;
   out_1594563891292900416[18] = 0;
   out_1594563891292900416[19] = 0;
   out_1594563891292900416[20] = 1;
   out_1594563891292900416[21] = 0;
   out_1594563891292900416[22] = 0;
   out_1594563891292900416[23] = 0;
   out_1594563891292900416[24] = 0;
   out_1594563891292900416[25] = 0;
   out_1594563891292900416[26] = 0;
   out_1594563891292900416[27] = 0;
   out_1594563891292900416[28] = 0;
   out_1594563891292900416[29] = 0;
   out_1594563891292900416[30] = 1;
   out_1594563891292900416[31] = 0;
   out_1594563891292900416[32] = 0;
   out_1594563891292900416[33] = 0;
   out_1594563891292900416[34] = 0;
   out_1594563891292900416[35] = 0;
   out_1594563891292900416[36] = 0;
   out_1594563891292900416[37] = 0;
   out_1594563891292900416[38] = 0;
   out_1594563891292900416[39] = 0;
   out_1594563891292900416[40] = 1;
   out_1594563891292900416[41] = 0;
   out_1594563891292900416[42] = 0;
   out_1594563891292900416[43] = 0;
   out_1594563891292900416[44] = 0;
   out_1594563891292900416[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_1594563891292900416[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_1594563891292900416[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_1594563891292900416[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_1594563891292900416[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_1594563891292900416[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_1594563891292900416[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_1594563891292900416[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_1594563891292900416[53] = -9.8000000000000007*dt;
   out_1594563891292900416[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_1594563891292900416[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_1594563891292900416[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1594563891292900416[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1594563891292900416[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_1594563891292900416[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_1594563891292900416[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_1594563891292900416[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1594563891292900416[62] = 0;
   out_1594563891292900416[63] = 0;
   out_1594563891292900416[64] = 0;
   out_1594563891292900416[65] = 0;
   out_1594563891292900416[66] = 0;
   out_1594563891292900416[67] = 0;
   out_1594563891292900416[68] = 0;
   out_1594563891292900416[69] = 0;
   out_1594563891292900416[70] = 1;
   out_1594563891292900416[71] = 0;
   out_1594563891292900416[72] = 0;
   out_1594563891292900416[73] = 0;
   out_1594563891292900416[74] = 0;
   out_1594563891292900416[75] = 0;
   out_1594563891292900416[76] = 0;
   out_1594563891292900416[77] = 0;
   out_1594563891292900416[78] = 0;
   out_1594563891292900416[79] = 0;
   out_1594563891292900416[80] = 1;
}
void h_25(double *state, double *unused, double *out_4446559038823907444) {
   out_4446559038823907444[0] = state[6];
}
void H_25(double *state, double *unused, double *out_2291888026643029333) {
   out_2291888026643029333[0] = 0;
   out_2291888026643029333[1] = 0;
   out_2291888026643029333[2] = 0;
   out_2291888026643029333[3] = 0;
   out_2291888026643029333[4] = 0;
   out_2291888026643029333[5] = 0;
   out_2291888026643029333[6] = 1;
   out_2291888026643029333[7] = 0;
   out_2291888026643029333[8] = 0;
}
void h_24(double *state, double *unused, double *out_5489680431960231161) {
   out_5489680431960231161[0] = state[4];
   out_5489680431960231161[1] = state[5];
}
void H_24(double *state, double *unused, double *out_1895242722289169838) {
   out_1895242722289169838[0] = 0;
   out_1895242722289169838[1] = 0;
   out_1895242722289169838[2] = 0;
   out_1895242722289169838[3] = 0;
   out_1895242722289169838[4] = 1;
   out_1895242722289169838[5] = 0;
   out_1895242722289169838[6] = 0;
   out_1895242722289169838[7] = 0;
   out_1895242722289169838[8] = 0;
   out_1895242722289169838[9] = 0;
   out_1895242722289169838[10] = 0;
   out_1895242722289169838[11] = 0;
   out_1895242722289169838[12] = 0;
   out_1895242722289169838[13] = 0;
   out_1895242722289169838[14] = 1;
   out_1895242722289169838[15] = 0;
   out_1895242722289169838[16] = 0;
   out_1895242722289169838[17] = 0;
}
void h_30(double *state, double *unused, double *out_8308242515252409096) {
   out_8308242515252409096[0] = state[4];
}
void H_30(double *state, double *unused, double *out_9208578368134646088) {
   out_9208578368134646088[0] = 0;
   out_9208578368134646088[1] = 0;
   out_9208578368134646088[2] = 0;
   out_9208578368134646088[3] = 0;
   out_9208578368134646088[4] = 1;
   out_9208578368134646088[5] = 0;
   out_9208578368134646088[6] = 0;
   out_9208578368134646088[7] = 0;
   out_9208578368134646088[8] = 0;
}
void h_26(double *state, double *unused, double *out_2392611761518546760) {
   out_2392611761518546760[0] = state[7];
}
void H_26(double *state, double *unused, double *out_1449615292231026891) {
   out_1449615292231026891[0] = 0;
   out_1449615292231026891[1] = 0;
   out_1449615292231026891[2] = 0;
   out_1449615292231026891[3] = 0;
   out_1449615292231026891[4] = 0;
   out_1449615292231026891[5] = 0;
   out_1449615292231026891[6] = 0;
   out_1449615292231026891[7] = 1;
   out_1449615292231026891[8] = 0;
}
void h_27(double *state, double *unused, double *out_5428643132159205556) {
   out_5428643132159205556[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7033815056334221177) {
   out_7033815056334221177[0] = 0;
   out_7033815056334221177[1] = 0;
   out_7033815056334221177[2] = 0;
   out_7033815056334221177[3] = 1;
   out_7033815056334221177[4] = 0;
   out_7033815056334221177[5] = 0;
   out_7033815056334221177[6] = 0;
   out_7033815056334221177[7] = 0;
   out_7033815056334221177[8] = 0;
}
void h_29(double *state, double *unused, double *out_5703837194443711445) {
   out_5703837194443711445[0] = state[1];
}
void H_29(double *state, double *unused, double *out_5320452329464670144) {
   out_5320452329464670144[0] = 0;
   out_5320452329464670144[1] = 1;
   out_5320452329464670144[2] = 0;
   out_5320452329464670144[3] = 0;
   out_5320452329464670144[4] = 0;
   out_5320452329464670144[5] = 0;
   out_5320452329464670144[6] = 0;
   out_5320452329464670144[7] = 0;
   out_5320452329464670144[8] = 0;
}
void h_28(double *state, double *unused, double *out_6908571236733552516) {
   out_6908571236733552516[0] = state[0];
}
void H_28(double *state, double *unused, double *out_238053312395139570) {
   out_238053312395139570[0] = 1;
   out_238053312395139570[1] = 0;
   out_238053312395139570[2] = 0;
   out_238053312395139570[3] = 0;
   out_238053312395139570[4] = 0;
   out_238053312395139570[5] = 0;
   out_238053312395139570[6] = 0;
   out_238053312395139570[7] = 0;
   out_238053312395139570[8] = 0;
}
void h_31(double *state, double *unused, double *out_3210479663755662439) {
   out_3210479663755662439[0] = state[8];
}
void H_31(double *state, double *unused, double *out_2322533988519989761) {
   out_2322533988519989761[0] = 0;
   out_2322533988519989761[1] = 0;
   out_2322533988519989761[2] = 0;
   out_2322533988519989761[3] = 0;
   out_2322533988519989761[4] = 0;
   out_2322533988519989761[5] = 0;
   out_2322533988519989761[6] = 0;
   out_2322533988519989761[7] = 0;
   out_2322533988519989761[8] = 1;
}
#include <eigen3/Eigen/Dense>
#include <iostream>

typedef Eigen::Matrix<double, DIM, DIM, Eigen::RowMajor> DDM;
typedef Eigen::Matrix<double, EDIM, EDIM, Eigen::RowMajor> EEM;
typedef Eigen::Matrix<double, DIM, EDIM, Eigen::RowMajor> DEM;

void predict(double *in_x, double *in_P, double *in_Q, double dt) {
  typedef Eigen::Matrix<double, MEDIM, MEDIM, Eigen::RowMajor> RRM;

  double nx[DIM] = {0};
  double in_F[EDIM*EDIM] = {0};

  // functions from sympy
  f_fun(in_x, dt, nx);
  F_fun(in_x, dt, in_F);


  EEM F(in_F);
  EEM P(in_P);
  EEM Q(in_Q);

  RRM F_main = F.topLeftCorner(MEDIM, MEDIM);
  P.topLeftCorner(MEDIM, MEDIM) = (F_main * P.topLeftCorner(MEDIM, MEDIM)) * F_main.transpose();
  P.topRightCorner(MEDIM, EDIM - MEDIM) = F_main * P.topRightCorner(MEDIM, EDIM - MEDIM);
  P.bottomLeftCorner(EDIM - MEDIM, MEDIM) = P.bottomLeftCorner(EDIM - MEDIM, MEDIM) * F_main.transpose();

  P = P + dt*Q;

  // copy out state
  memcpy(in_x, nx, DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
}

// note: extra_args dim only correct when null space projecting
// otherwise 1
template <int ZDIM, int EADIM, bool MAHA_TEST>
void update(double *in_x, double *in_P, Hfun h_fun, Hfun H_fun, Hfun Hea_fun, double *in_z, double *in_R, double *in_ea, double MAHA_THRESHOLD) {
  typedef Eigen::Matrix<double, ZDIM, ZDIM, Eigen::RowMajor> ZZM;
  typedef Eigen::Matrix<double, ZDIM, DIM, Eigen::RowMajor> ZDM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, EDIM, Eigen::RowMajor> XEM;
  //typedef Eigen::Matrix<double, EDIM, ZDIM, Eigen::RowMajor> EZM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, 1> X1M;
  typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> XXM;

  double in_hx[ZDIM] = {0};
  double in_H[ZDIM * DIM] = {0};
  double in_H_mod[EDIM * DIM] = {0};
  double delta_x[EDIM] = {0};
  double x_new[DIM] = {0};


  // state x, P
  Eigen::Matrix<double, ZDIM, 1> z(in_z);
  EEM P(in_P);
  ZZM pre_R(in_R);

  // functions from sympy
  h_fun(in_x, in_ea, in_hx);
  H_fun(in_x, in_ea, in_H);
  ZDM pre_H(in_H);

  // get y (y = z - hx)
  Eigen::Matrix<double, ZDIM, 1> pre_y(in_hx); pre_y = z - pre_y;
  X1M y; XXM H; XXM R;
  if (Hea_fun){
    typedef Eigen::Matrix<double, ZDIM, EADIM, Eigen::RowMajor> ZAM;
    double in_Hea[ZDIM * EADIM] = {0};
    Hea_fun(in_x, in_ea, in_Hea);
    ZAM Hea(in_Hea);
    XXM A = Hea.transpose().fullPivLu().kernel();


    y = A.transpose() * pre_y;
    H = A.transpose() * pre_H;
    R = A.transpose() * pre_R * A;
  } else {
    y = pre_y;
    H = pre_H;
    R = pre_R;
  }
  // get modified H
  H_mod_fun(in_x, in_H_mod);
  DEM H_mod(in_H_mod);
  XEM H_err = H * H_mod;

  // Do mahalobis distance test
  if (MAHA_TEST){
    XXM a = (H_err * P * H_err.transpose() + R).inverse();
    double maha_dist = y.transpose() * a * y;
    if (maha_dist > MAHA_THRESHOLD){
      R = 1.0e16 * R;
    }
  }

  // Outlier resilient weighting
  double weight = 1;//(1.5)/(1 + y.squaredNorm()/R.sum());

  // kalman gains and I_KH
  XXM S = ((H_err * P) * H_err.transpose()) + R/weight;
  XEM KT = S.fullPivLu().solve(H_err * P.transpose());
  //EZM K = KT.transpose(); TODO: WHY DOES THIS NOT COMPILE?
  //EZM K = S.fullPivLu().solve(H_err * P.transpose()).transpose();
  //std::cout << "Here is the matrix rot:\n" << K << std::endl;
  EEM I_KH = Eigen::Matrix<double, EDIM, EDIM>::Identity() - (KT.transpose() * H_err);

  // update state by injecting dx
  Eigen::Matrix<double, EDIM, 1> dx(delta_x);
  dx  = (KT.transpose() * y);
  memcpy(delta_x, dx.data(), EDIM * sizeof(double));
  err_fun(in_x, delta_x, x_new);
  Eigen::Matrix<double, DIM, 1> x(x_new);

  // update cov
  P = ((I_KH * P) * I_KH.transpose()) + ((KT.transpose() * R) * KT);

  // copy out state
  memcpy(in_x, x.data(), DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
  memcpy(in_z, y.data(), y.rows() * sizeof(double));
}




}
extern "C" {

void car_update_25(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_25, H_25, NULL, in_z, in_R, in_ea, MAHA_THRESH_25);
}
void car_update_24(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<2, 3, 0>(in_x, in_P, h_24, H_24, NULL, in_z, in_R, in_ea, MAHA_THRESH_24);
}
void car_update_30(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_30, H_30, NULL, in_z, in_R, in_ea, MAHA_THRESH_30);
}
void car_update_26(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_26, H_26, NULL, in_z, in_R, in_ea, MAHA_THRESH_26);
}
void car_update_27(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_27, H_27, NULL, in_z, in_R, in_ea, MAHA_THRESH_27);
}
void car_update_29(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_29, H_29, NULL, in_z, in_R, in_ea, MAHA_THRESH_29);
}
void car_update_28(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_28, H_28, NULL, in_z, in_R, in_ea, MAHA_THRESH_28);
}
void car_update_31(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_31, H_31, NULL, in_z, in_R, in_ea, MAHA_THRESH_31);
}
void car_err_fun(double *nom_x, double *delta_x, double *out_7718808222430479447) {
  err_fun(nom_x, delta_x, out_7718808222430479447);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1034114423937537934) {
  inv_err_fun(nom_x, true_x, out_1034114423937537934);
}
void car_H_mod_fun(double *state, double *out_7225960904852606817) {
  H_mod_fun(state, out_7225960904852606817);
}
void car_f_fun(double *state, double dt, double *out_5925834804962007644) {
  f_fun(state,  dt, out_5925834804962007644);
}
void car_F_fun(double *state, double dt, double *out_1594563891292900416) {
  F_fun(state,  dt, out_1594563891292900416);
}
void car_h_25(double *state, double *unused, double *out_4446559038823907444) {
  h_25(state, unused, out_4446559038823907444);
}
void car_H_25(double *state, double *unused, double *out_2291888026643029333) {
  H_25(state, unused, out_2291888026643029333);
}
void car_h_24(double *state, double *unused, double *out_5489680431960231161) {
  h_24(state, unused, out_5489680431960231161);
}
void car_H_24(double *state, double *unused, double *out_1895242722289169838) {
  H_24(state, unused, out_1895242722289169838);
}
void car_h_30(double *state, double *unused, double *out_8308242515252409096) {
  h_30(state, unused, out_8308242515252409096);
}
void car_H_30(double *state, double *unused, double *out_9208578368134646088) {
  H_30(state, unused, out_9208578368134646088);
}
void car_h_26(double *state, double *unused, double *out_2392611761518546760) {
  h_26(state, unused, out_2392611761518546760);
}
void car_H_26(double *state, double *unused, double *out_1449615292231026891) {
  H_26(state, unused, out_1449615292231026891);
}
void car_h_27(double *state, double *unused, double *out_5428643132159205556) {
  h_27(state, unused, out_5428643132159205556);
}
void car_H_27(double *state, double *unused, double *out_7033815056334221177) {
  H_27(state, unused, out_7033815056334221177);
}
void car_h_29(double *state, double *unused, double *out_5703837194443711445) {
  h_29(state, unused, out_5703837194443711445);
}
void car_H_29(double *state, double *unused, double *out_5320452329464670144) {
  H_29(state, unused, out_5320452329464670144);
}
void car_h_28(double *state, double *unused, double *out_6908571236733552516) {
  h_28(state, unused, out_6908571236733552516);
}
void car_H_28(double *state, double *unused, double *out_238053312395139570) {
  H_28(state, unused, out_238053312395139570);
}
void car_h_31(double *state, double *unused, double *out_3210479663755662439) {
  h_31(state, unused, out_3210479663755662439);
}
void car_H_31(double *state, double *unused, double *out_2322533988519989761) {
  H_31(state, unused, out_2322533988519989761);
}
void car_predict(double *in_x, double *in_P, double *in_Q, double dt) {
  predict(in_x, in_P, in_Q, dt);
}
void car_set_mass(double x) {
  set_mass(x);
}
void car_set_rotational_inertia(double x) {
  set_rotational_inertia(x);
}
void car_set_center_to_front(double x) {
  set_center_to_front(x);
}
void car_set_center_to_rear(double x) {
  set_center_to_rear(x);
}
void car_set_stiffness_front(double x) {
  set_stiffness_front(x);
}
void car_set_stiffness_rear(double x) {
  set_stiffness_rear(x);
}
}

const EKF car = {
  .name = "car",
  .kinds = { 25, 24, 30, 26, 27, 29, 28, 31 },
  .feature_kinds = {  },
  .f_fun = car_f_fun,
  .F_fun = car_F_fun,
  .err_fun = car_err_fun,
  .inv_err_fun = car_inv_err_fun,
  .H_mod_fun = car_H_mod_fun,
  .predict = car_predict,
  .hs = {
    { 25, car_h_25 },
    { 24, car_h_24 },
    { 30, car_h_30 },
    { 26, car_h_26 },
    { 27, car_h_27 },
    { 29, car_h_29 },
    { 28, car_h_28 },
    { 31, car_h_31 },
  },
  .Hs = {
    { 25, car_H_25 },
    { 24, car_H_24 },
    { 30, car_H_30 },
    { 26, car_H_26 },
    { 27, car_H_27 },
    { 29, car_H_29 },
    { 28, car_H_28 },
    { 31, car_H_31 },
  },
  .updates = {
    { 25, car_update_25 },
    { 24, car_update_24 },
    { 30, car_update_30 },
    { 26, car_update_26 },
    { 27, car_update_27 },
    { 29, car_update_29 },
    { 28, car_update_28 },
    { 31, car_update_31 },
  },
  .Hes = {
  },
  .sets = {
    { "mass", car_set_mass },
    { "rotational_inertia", car_set_rotational_inertia },
    { "center_to_front", car_set_center_to_front },
    { "center_to_rear", car_set_center_to_rear },
    { "stiffness_front", car_set_stiffness_front },
    { "stiffness_rear", car_set_stiffness_rear },
  },
  .extra_routines = {
  },
};

ekf_lib_init(car)
