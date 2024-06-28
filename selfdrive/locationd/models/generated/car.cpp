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
void err_fun(double *nom_x, double *delta_x, double *out_3670970001479568934) {
   out_3670970001479568934[0] = delta_x[0] + nom_x[0];
   out_3670970001479568934[1] = delta_x[1] + nom_x[1];
   out_3670970001479568934[2] = delta_x[2] + nom_x[2];
   out_3670970001479568934[3] = delta_x[3] + nom_x[3];
   out_3670970001479568934[4] = delta_x[4] + nom_x[4];
   out_3670970001479568934[5] = delta_x[5] + nom_x[5];
   out_3670970001479568934[6] = delta_x[6] + nom_x[6];
   out_3670970001479568934[7] = delta_x[7] + nom_x[7];
   out_3670970001479568934[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_5195485033129772925) {
   out_5195485033129772925[0] = -nom_x[0] + true_x[0];
   out_5195485033129772925[1] = -nom_x[1] + true_x[1];
   out_5195485033129772925[2] = -nom_x[2] + true_x[2];
   out_5195485033129772925[3] = -nom_x[3] + true_x[3];
   out_5195485033129772925[4] = -nom_x[4] + true_x[4];
   out_5195485033129772925[5] = -nom_x[5] + true_x[5];
   out_5195485033129772925[6] = -nom_x[6] + true_x[6];
   out_5195485033129772925[7] = -nom_x[7] + true_x[7];
   out_5195485033129772925[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_6483417260839774991) {
   out_6483417260839774991[0] = 1.0;
   out_6483417260839774991[1] = 0;
   out_6483417260839774991[2] = 0;
   out_6483417260839774991[3] = 0;
   out_6483417260839774991[4] = 0;
   out_6483417260839774991[5] = 0;
   out_6483417260839774991[6] = 0;
   out_6483417260839774991[7] = 0;
   out_6483417260839774991[8] = 0;
   out_6483417260839774991[9] = 0;
   out_6483417260839774991[10] = 1.0;
   out_6483417260839774991[11] = 0;
   out_6483417260839774991[12] = 0;
   out_6483417260839774991[13] = 0;
   out_6483417260839774991[14] = 0;
   out_6483417260839774991[15] = 0;
   out_6483417260839774991[16] = 0;
   out_6483417260839774991[17] = 0;
   out_6483417260839774991[18] = 0;
   out_6483417260839774991[19] = 0;
   out_6483417260839774991[20] = 1.0;
   out_6483417260839774991[21] = 0;
   out_6483417260839774991[22] = 0;
   out_6483417260839774991[23] = 0;
   out_6483417260839774991[24] = 0;
   out_6483417260839774991[25] = 0;
   out_6483417260839774991[26] = 0;
   out_6483417260839774991[27] = 0;
   out_6483417260839774991[28] = 0;
   out_6483417260839774991[29] = 0;
   out_6483417260839774991[30] = 1.0;
   out_6483417260839774991[31] = 0;
   out_6483417260839774991[32] = 0;
   out_6483417260839774991[33] = 0;
   out_6483417260839774991[34] = 0;
   out_6483417260839774991[35] = 0;
   out_6483417260839774991[36] = 0;
   out_6483417260839774991[37] = 0;
   out_6483417260839774991[38] = 0;
   out_6483417260839774991[39] = 0;
   out_6483417260839774991[40] = 1.0;
   out_6483417260839774991[41] = 0;
   out_6483417260839774991[42] = 0;
   out_6483417260839774991[43] = 0;
   out_6483417260839774991[44] = 0;
   out_6483417260839774991[45] = 0;
   out_6483417260839774991[46] = 0;
   out_6483417260839774991[47] = 0;
   out_6483417260839774991[48] = 0;
   out_6483417260839774991[49] = 0;
   out_6483417260839774991[50] = 1.0;
   out_6483417260839774991[51] = 0;
   out_6483417260839774991[52] = 0;
   out_6483417260839774991[53] = 0;
   out_6483417260839774991[54] = 0;
   out_6483417260839774991[55] = 0;
   out_6483417260839774991[56] = 0;
   out_6483417260839774991[57] = 0;
   out_6483417260839774991[58] = 0;
   out_6483417260839774991[59] = 0;
   out_6483417260839774991[60] = 1.0;
   out_6483417260839774991[61] = 0;
   out_6483417260839774991[62] = 0;
   out_6483417260839774991[63] = 0;
   out_6483417260839774991[64] = 0;
   out_6483417260839774991[65] = 0;
   out_6483417260839774991[66] = 0;
   out_6483417260839774991[67] = 0;
   out_6483417260839774991[68] = 0;
   out_6483417260839774991[69] = 0;
   out_6483417260839774991[70] = 1.0;
   out_6483417260839774991[71] = 0;
   out_6483417260839774991[72] = 0;
   out_6483417260839774991[73] = 0;
   out_6483417260839774991[74] = 0;
   out_6483417260839774991[75] = 0;
   out_6483417260839774991[76] = 0;
   out_6483417260839774991[77] = 0;
   out_6483417260839774991[78] = 0;
   out_6483417260839774991[79] = 0;
   out_6483417260839774991[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2977687138516011449) {
   out_2977687138516011449[0] = state[0];
   out_2977687138516011449[1] = state[1];
   out_2977687138516011449[2] = state[2];
   out_2977687138516011449[3] = state[3];
   out_2977687138516011449[4] = state[4];
   out_2977687138516011449[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2977687138516011449[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2977687138516011449[7] = state[7];
   out_2977687138516011449[8] = state[8];
}
void F_fun(double *state, double dt, double *out_8278273557355961566) {
   out_8278273557355961566[0] = 1;
   out_8278273557355961566[1] = 0;
   out_8278273557355961566[2] = 0;
   out_8278273557355961566[3] = 0;
   out_8278273557355961566[4] = 0;
   out_8278273557355961566[5] = 0;
   out_8278273557355961566[6] = 0;
   out_8278273557355961566[7] = 0;
   out_8278273557355961566[8] = 0;
   out_8278273557355961566[9] = 0;
   out_8278273557355961566[10] = 1;
   out_8278273557355961566[11] = 0;
   out_8278273557355961566[12] = 0;
   out_8278273557355961566[13] = 0;
   out_8278273557355961566[14] = 0;
   out_8278273557355961566[15] = 0;
   out_8278273557355961566[16] = 0;
   out_8278273557355961566[17] = 0;
   out_8278273557355961566[18] = 0;
   out_8278273557355961566[19] = 0;
   out_8278273557355961566[20] = 1;
   out_8278273557355961566[21] = 0;
   out_8278273557355961566[22] = 0;
   out_8278273557355961566[23] = 0;
   out_8278273557355961566[24] = 0;
   out_8278273557355961566[25] = 0;
   out_8278273557355961566[26] = 0;
   out_8278273557355961566[27] = 0;
   out_8278273557355961566[28] = 0;
   out_8278273557355961566[29] = 0;
   out_8278273557355961566[30] = 1;
   out_8278273557355961566[31] = 0;
   out_8278273557355961566[32] = 0;
   out_8278273557355961566[33] = 0;
   out_8278273557355961566[34] = 0;
   out_8278273557355961566[35] = 0;
   out_8278273557355961566[36] = 0;
   out_8278273557355961566[37] = 0;
   out_8278273557355961566[38] = 0;
   out_8278273557355961566[39] = 0;
   out_8278273557355961566[40] = 1;
   out_8278273557355961566[41] = 0;
   out_8278273557355961566[42] = 0;
   out_8278273557355961566[43] = 0;
   out_8278273557355961566[44] = 0;
   out_8278273557355961566[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_8278273557355961566[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_8278273557355961566[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8278273557355961566[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8278273557355961566[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_8278273557355961566[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_8278273557355961566[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_8278273557355961566[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_8278273557355961566[53] = -9.8000000000000007*dt;
   out_8278273557355961566[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_8278273557355961566[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_8278273557355961566[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8278273557355961566[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8278273557355961566[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_8278273557355961566[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_8278273557355961566[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_8278273557355961566[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8278273557355961566[62] = 0;
   out_8278273557355961566[63] = 0;
   out_8278273557355961566[64] = 0;
   out_8278273557355961566[65] = 0;
   out_8278273557355961566[66] = 0;
   out_8278273557355961566[67] = 0;
   out_8278273557355961566[68] = 0;
   out_8278273557355961566[69] = 0;
   out_8278273557355961566[70] = 1;
   out_8278273557355961566[71] = 0;
   out_8278273557355961566[72] = 0;
   out_8278273557355961566[73] = 0;
   out_8278273557355961566[74] = 0;
   out_8278273557355961566[75] = 0;
   out_8278273557355961566[76] = 0;
   out_8278273557355961566[77] = 0;
   out_8278273557355961566[78] = 0;
   out_8278273557355961566[79] = 0;
   out_8278273557355961566[80] = 1;
}
void h_25(double *state, double *unused, double *out_1926313835253732662) {
   out_1926313835253732662[0] = state[6];
}
void H_25(double *state, double *unused, double *out_8185001041261982072) {
   out_8185001041261982072[0] = 0;
   out_8185001041261982072[1] = 0;
   out_8185001041261982072[2] = 0;
   out_8185001041261982072[3] = 0;
   out_8185001041261982072[4] = 0;
   out_8185001041261982072[5] = 0;
   out_8185001041261982072[6] = 1;
   out_8185001041261982072[7] = 0;
   out_8185001041261982072[8] = 0;
}
void h_24(double *state, double *unused, double *out_7840106857941678322) {
   out_7840106857941678322[0] = state[4];
   out_7840106857941678322[1] = state[5];
}
void H_24(double *state, double *unused, double *out_5959293257283113510) {
   out_5959293257283113510[0] = 0;
   out_5959293257283113510[1] = 0;
   out_5959293257283113510[2] = 0;
   out_5959293257283113510[3] = 0;
   out_5959293257283113510[4] = 1;
   out_5959293257283113510[5] = 0;
   out_5959293257283113510[6] = 0;
   out_5959293257283113510[7] = 0;
   out_5959293257283113510[8] = 0;
   out_5959293257283113510[9] = 0;
   out_5959293257283113510[10] = 0;
   out_5959293257283113510[11] = 0;
   out_5959293257283113510[12] = 0;
   out_5959293257283113510[13] = 0;
   out_5959293257283113510[14] = 1;
   out_5959293257283113510[15] = 0;
   out_5959293257283113510[16] = 0;
   out_5959293257283113510[17] = 0;
}
void h_30(double *state, double *unused, double *out_7736621140010297992) {
   out_7736621140010297992[0] = state[4];
}
void H_30(double *state, double *unused, double *out_8314339988405222142) {
   out_8314339988405222142[0] = 0;
   out_8314339988405222142[1] = 0;
   out_8314339988405222142[2] = 0;
   out_8314339988405222142[3] = 0;
   out_8314339988405222142[4] = 1;
   out_8314339988405222142[5] = 0;
   out_8314339988405222142[6] = 0;
   out_8314339988405222142[7] = 0;
   out_8314339988405222142[8] = 0;
}
void h_26(double *state, double *unused, double *out_6652793040630072418) {
   out_6652793040630072418[0] = state[7];
}
void H_26(double *state, double *unused, double *out_6520239713573513320) {
   out_6520239713573513320[0] = 0;
   out_6520239713573513320[1] = 0;
   out_6520239713573513320[2] = 0;
   out_6520239713573513320[3] = 0;
   out_6520239713573513320[4] = 0;
   out_6520239713573513320[5] = 0;
   out_6520239713573513320[6] = 0;
   out_6520239713573513320[7] = 1;
   out_6520239713573513320[8] = 0;
}
void h_27(double *state, double *unused, double *out_3616761669989413622) {
   out_3616761669989413622[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7957640773503904563) {
   out_7957640773503904563[0] = 0;
   out_7957640773503904563[1] = 0;
   out_7957640773503904563[2] = 0;
   out_7957640773503904563[3] = 1;
   out_7957640773503904563[4] = 0;
   out_7957640773503904563[5] = 0;
   out_7957640773503904563[6] = 0;
   out_7957640773503904563[7] = 0;
   out_7957640773503904563[8] = 0;
}
void h_29(double *state, double *unused, double *out_6937042908835614635) {
   out_6937042908835614635[0] = state[1];
}
void H_29(double *state, double *unused, double *out_7804108644090829958) {
   out_7804108644090829958[0] = 0;
   out_7804108644090829958[1] = 1;
   out_7804108644090829958[2] = 0;
   out_7804108644090829958[3] = 0;
   out_7804108644090829958[4] = 0;
   out_7804108644090829958[5] = 0;
   out_7804108644090829958[6] = 0;
   out_7804108644090829958[7] = 0;
   out_7804108644090829958[8] = 0;
}
void h_28(double *state, double *unused, double *out_105358240764198780) {
   out_105358240764198780[0] = state[0];
}
void H_28(double *state, double *unused, double *out_5840478372525503707) {
   out_5840478372525503707[0] = 1;
   out_5840478372525503707[1] = 0;
   out_5840478372525503707[2] = 0;
   out_5840478372525503707[3] = 0;
   out_5840478372525503707[4] = 0;
   out_5840478372525503707[5] = 0;
   out_5840478372525503707[6] = 0;
   out_5840478372525503707[7] = 0;
   out_5840478372525503707[8] = 0;
}
void h_31(double *state, double *unused, double *out_8912058470354675270) {
   out_8912058470354675270[0] = state[8];
}
void H_31(double *state, double *unused, double *out_8154355079385021644) {
   out_8154355079385021644[0] = 0;
   out_8154355079385021644[1] = 0;
   out_8154355079385021644[2] = 0;
   out_8154355079385021644[3] = 0;
   out_8154355079385021644[4] = 0;
   out_8154355079385021644[5] = 0;
   out_8154355079385021644[6] = 0;
   out_8154355079385021644[7] = 0;
   out_8154355079385021644[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_3670970001479568934) {
  err_fun(nom_x, delta_x, out_3670970001479568934);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_5195485033129772925) {
  inv_err_fun(nom_x, true_x, out_5195485033129772925);
}
void car_H_mod_fun(double *state, double *out_6483417260839774991) {
  H_mod_fun(state, out_6483417260839774991);
}
void car_f_fun(double *state, double dt, double *out_2977687138516011449) {
  f_fun(state,  dt, out_2977687138516011449);
}
void car_F_fun(double *state, double dt, double *out_8278273557355961566) {
  F_fun(state,  dt, out_8278273557355961566);
}
void car_h_25(double *state, double *unused, double *out_1926313835253732662) {
  h_25(state, unused, out_1926313835253732662);
}
void car_H_25(double *state, double *unused, double *out_8185001041261982072) {
  H_25(state, unused, out_8185001041261982072);
}
void car_h_24(double *state, double *unused, double *out_7840106857941678322) {
  h_24(state, unused, out_7840106857941678322);
}
void car_H_24(double *state, double *unused, double *out_5959293257283113510) {
  H_24(state, unused, out_5959293257283113510);
}
void car_h_30(double *state, double *unused, double *out_7736621140010297992) {
  h_30(state, unused, out_7736621140010297992);
}
void car_H_30(double *state, double *unused, double *out_8314339988405222142) {
  H_30(state, unused, out_8314339988405222142);
}
void car_h_26(double *state, double *unused, double *out_6652793040630072418) {
  h_26(state, unused, out_6652793040630072418);
}
void car_H_26(double *state, double *unused, double *out_6520239713573513320) {
  H_26(state, unused, out_6520239713573513320);
}
void car_h_27(double *state, double *unused, double *out_3616761669989413622) {
  h_27(state, unused, out_3616761669989413622);
}
void car_H_27(double *state, double *unused, double *out_7957640773503904563) {
  H_27(state, unused, out_7957640773503904563);
}
void car_h_29(double *state, double *unused, double *out_6937042908835614635) {
  h_29(state, unused, out_6937042908835614635);
}
void car_H_29(double *state, double *unused, double *out_7804108644090829958) {
  H_29(state, unused, out_7804108644090829958);
}
void car_h_28(double *state, double *unused, double *out_105358240764198780) {
  h_28(state, unused, out_105358240764198780);
}
void car_H_28(double *state, double *unused, double *out_5840478372525503707) {
  H_28(state, unused, out_5840478372525503707);
}
void car_h_31(double *state, double *unused, double *out_8912058470354675270) {
  h_31(state, unused, out_8912058470354675270);
}
void car_H_31(double *state, double *unused, double *out_8154355079385021644) {
  H_31(state, unused, out_8154355079385021644);
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
