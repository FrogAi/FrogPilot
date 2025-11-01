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
void err_fun(double *nom_x, double *delta_x, double *out_3737406110502016648) {
   out_3737406110502016648[0] = delta_x[0] + nom_x[0];
   out_3737406110502016648[1] = delta_x[1] + nom_x[1];
   out_3737406110502016648[2] = delta_x[2] + nom_x[2];
   out_3737406110502016648[3] = delta_x[3] + nom_x[3];
   out_3737406110502016648[4] = delta_x[4] + nom_x[4];
   out_3737406110502016648[5] = delta_x[5] + nom_x[5];
   out_3737406110502016648[6] = delta_x[6] + nom_x[6];
   out_3737406110502016648[7] = delta_x[7] + nom_x[7];
   out_3737406110502016648[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_2403136137136565417) {
   out_2403136137136565417[0] = -nom_x[0] + true_x[0];
   out_2403136137136565417[1] = -nom_x[1] + true_x[1];
   out_2403136137136565417[2] = -nom_x[2] + true_x[2];
   out_2403136137136565417[3] = -nom_x[3] + true_x[3];
   out_2403136137136565417[4] = -nom_x[4] + true_x[4];
   out_2403136137136565417[5] = -nom_x[5] + true_x[5];
   out_2403136137136565417[6] = -nom_x[6] + true_x[6];
   out_2403136137136565417[7] = -nom_x[7] + true_x[7];
   out_2403136137136565417[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_9188041439089480832) {
   out_9188041439089480832[0] = 1.0;
   out_9188041439089480832[1] = 0;
   out_9188041439089480832[2] = 0;
   out_9188041439089480832[3] = 0;
   out_9188041439089480832[4] = 0;
   out_9188041439089480832[5] = 0;
   out_9188041439089480832[6] = 0;
   out_9188041439089480832[7] = 0;
   out_9188041439089480832[8] = 0;
   out_9188041439089480832[9] = 0;
   out_9188041439089480832[10] = 1.0;
   out_9188041439089480832[11] = 0;
   out_9188041439089480832[12] = 0;
   out_9188041439089480832[13] = 0;
   out_9188041439089480832[14] = 0;
   out_9188041439089480832[15] = 0;
   out_9188041439089480832[16] = 0;
   out_9188041439089480832[17] = 0;
   out_9188041439089480832[18] = 0;
   out_9188041439089480832[19] = 0;
   out_9188041439089480832[20] = 1.0;
   out_9188041439089480832[21] = 0;
   out_9188041439089480832[22] = 0;
   out_9188041439089480832[23] = 0;
   out_9188041439089480832[24] = 0;
   out_9188041439089480832[25] = 0;
   out_9188041439089480832[26] = 0;
   out_9188041439089480832[27] = 0;
   out_9188041439089480832[28] = 0;
   out_9188041439089480832[29] = 0;
   out_9188041439089480832[30] = 1.0;
   out_9188041439089480832[31] = 0;
   out_9188041439089480832[32] = 0;
   out_9188041439089480832[33] = 0;
   out_9188041439089480832[34] = 0;
   out_9188041439089480832[35] = 0;
   out_9188041439089480832[36] = 0;
   out_9188041439089480832[37] = 0;
   out_9188041439089480832[38] = 0;
   out_9188041439089480832[39] = 0;
   out_9188041439089480832[40] = 1.0;
   out_9188041439089480832[41] = 0;
   out_9188041439089480832[42] = 0;
   out_9188041439089480832[43] = 0;
   out_9188041439089480832[44] = 0;
   out_9188041439089480832[45] = 0;
   out_9188041439089480832[46] = 0;
   out_9188041439089480832[47] = 0;
   out_9188041439089480832[48] = 0;
   out_9188041439089480832[49] = 0;
   out_9188041439089480832[50] = 1.0;
   out_9188041439089480832[51] = 0;
   out_9188041439089480832[52] = 0;
   out_9188041439089480832[53] = 0;
   out_9188041439089480832[54] = 0;
   out_9188041439089480832[55] = 0;
   out_9188041439089480832[56] = 0;
   out_9188041439089480832[57] = 0;
   out_9188041439089480832[58] = 0;
   out_9188041439089480832[59] = 0;
   out_9188041439089480832[60] = 1.0;
   out_9188041439089480832[61] = 0;
   out_9188041439089480832[62] = 0;
   out_9188041439089480832[63] = 0;
   out_9188041439089480832[64] = 0;
   out_9188041439089480832[65] = 0;
   out_9188041439089480832[66] = 0;
   out_9188041439089480832[67] = 0;
   out_9188041439089480832[68] = 0;
   out_9188041439089480832[69] = 0;
   out_9188041439089480832[70] = 1.0;
   out_9188041439089480832[71] = 0;
   out_9188041439089480832[72] = 0;
   out_9188041439089480832[73] = 0;
   out_9188041439089480832[74] = 0;
   out_9188041439089480832[75] = 0;
   out_9188041439089480832[76] = 0;
   out_9188041439089480832[77] = 0;
   out_9188041439089480832[78] = 0;
   out_9188041439089480832[79] = 0;
   out_9188041439089480832[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_8940466509319208784) {
   out_8940466509319208784[0] = state[0];
   out_8940466509319208784[1] = state[1];
   out_8940466509319208784[2] = state[2];
   out_8940466509319208784[3] = state[3];
   out_8940466509319208784[4] = state[4];
   out_8940466509319208784[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_8940466509319208784[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_8940466509319208784[7] = state[7];
   out_8940466509319208784[8] = state[8];
}
void F_fun(double *state, double dt, double *out_4596333403153979532) {
   out_4596333403153979532[0] = 1;
   out_4596333403153979532[1] = 0;
   out_4596333403153979532[2] = 0;
   out_4596333403153979532[3] = 0;
   out_4596333403153979532[4] = 0;
   out_4596333403153979532[5] = 0;
   out_4596333403153979532[6] = 0;
   out_4596333403153979532[7] = 0;
   out_4596333403153979532[8] = 0;
   out_4596333403153979532[9] = 0;
   out_4596333403153979532[10] = 1;
   out_4596333403153979532[11] = 0;
   out_4596333403153979532[12] = 0;
   out_4596333403153979532[13] = 0;
   out_4596333403153979532[14] = 0;
   out_4596333403153979532[15] = 0;
   out_4596333403153979532[16] = 0;
   out_4596333403153979532[17] = 0;
   out_4596333403153979532[18] = 0;
   out_4596333403153979532[19] = 0;
   out_4596333403153979532[20] = 1;
   out_4596333403153979532[21] = 0;
   out_4596333403153979532[22] = 0;
   out_4596333403153979532[23] = 0;
   out_4596333403153979532[24] = 0;
   out_4596333403153979532[25] = 0;
   out_4596333403153979532[26] = 0;
   out_4596333403153979532[27] = 0;
   out_4596333403153979532[28] = 0;
   out_4596333403153979532[29] = 0;
   out_4596333403153979532[30] = 1;
   out_4596333403153979532[31] = 0;
   out_4596333403153979532[32] = 0;
   out_4596333403153979532[33] = 0;
   out_4596333403153979532[34] = 0;
   out_4596333403153979532[35] = 0;
   out_4596333403153979532[36] = 0;
   out_4596333403153979532[37] = 0;
   out_4596333403153979532[38] = 0;
   out_4596333403153979532[39] = 0;
   out_4596333403153979532[40] = 1;
   out_4596333403153979532[41] = 0;
   out_4596333403153979532[42] = 0;
   out_4596333403153979532[43] = 0;
   out_4596333403153979532[44] = 0;
   out_4596333403153979532[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_4596333403153979532[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_4596333403153979532[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4596333403153979532[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4596333403153979532[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_4596333403153979532[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_4596333403153979532[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_4596333403153979532[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_4596333403153979532[53] = -9.8000000000000007*dt;
   out_4596333403153979532[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_4596333403153979532[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_4596333403153979532[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4596333403153979532[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4596333403153979532[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_4596333403153979532[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_4596333403153979532[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_4596333403153979532[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4596333403153979532[62] = 0;
   out_4596333403153979532[63] = 0;
   out_4596333403153979532[64] = 0;
   out_4596333403153979532[65] = 0;
   out_4596333403153979532[66] = 0;
   out_4596333403153979532[67] = 0;
   out_4596333403153979532[68] = 0;
   out_4596333403153979532[69] = 0;
   out_4596333403153979532[70] = 1;
   out_4596333403153979532[71] = 0;
   out_4596333403153979532[72] = 0;
   out_4596333403153979532[73] = 0;
   out_4596333403153979532[74] = 0;
   out_4596333403153979532[75] = 0;
   out_4596333403153979532[76] = 0;
   out_4596333403153979532[77] = 0;
   out_4596333403153979532[78] = 0;
   out_4596333403153979532[79] = 0;
   out_4596333403153979532[80] = 1;
}
void h_25(double *state, double *unused, double *out_8177395392958100983) {
   out_8177395392958100983[0] = state[6];
}
void H_25(double *state, double *unused, double *out_2404778044889366002) {
   out_2404778044889366002[0] = 0;
   out_2404778044889366002[1] = 0;
   out_2404778044889366002[2] = 0;
   out_2404778044889366002[3] = 0;
   out_2404778044889366002[4] = 0;
   out_2404778044889366002[5] = 0;
   out_2404778044889366002[6] = 1;
   out_2404778044889366002[7] = 0;
   out_2404778044889366002[8] = 0;
}
void h_24(double *state, double *unused, double *out_3033720568854893948) {
   out_3033720568854893948[0] = state[4];
   out_3033720568854893948[1] = state[5];
}
void H_24(double *state, double *unused, double *out_227563621282216029) {
   out_227563621282216029[0] = 0;
   out_227563621282216029[1] = 0;
   out_227563621282216029[2] = 0;
   out_227563621282216029[3] = 0;
   out_227563621282216029[4] = 1;
   out_227563621282216029[5] = 0;
   out_227563621282216029[6] = 0;
   out_227563621282216029[7] = 0;
   out_227563621282216029[8] = 0;
   out_227563621282216029[9] = 0;
   out_227563621282216029[10] = 0;
   out_227563621282216029[11] = 0;
   out_227563621282216029[12] = 0;
   out_227563621282216029[13] = 0;
   out_227563621282216029[14] = 1;
   out_227563621282216029[15] = 0;
   out_227563621282216029[16] = 0;
   out_227563621282216029[17] = 0;
}
void h_30(double *state, double *unused, double *out_6631899152497428283) {
   out_6631899152497428283[0] = state[4];
}
void H_30(double *state, double *unused, double *out_113554913617882625) {
   out_113554913617882625[0] = 0;
   out_113554913617882625[1] = 0;
   out_113554913617882625[2] = 0;
   out_113554913617882625[3] = 0;
   out_113554913617882625[4] = 1;
   out_113554913617882625[5] = 0;
   out_113554913617882625[6] = 0;
   out_113554913617882625[7] = 0;
   out_113554913617882625[8] = 0;
}
void h_26(double *state, double *unused, double *out_4509070587126112289) {
   out_4509070587126112289[0] = state[7];
}
void H_26(double *state, double *unused, double *out_899747924871434599) {
   out_899747924871434599[0] = 0;
   out_899747924871434599[1] = 0;
   out_899747924871434599[2] = 0;
   out_899747924871434599[3] = 0;
   out_899747924871434599[4] = 0;
   out_899747924871434599[5] = 0;
   out_899747924871434599[6] = 0;
   out_899747924871434599[7] = 1;
   out_899747924871434599[8] = 0;
}
void h_27(double *state, double *unused, double *out_7009651591881959246) {
   out_7009651591881959246[0] = state[3];
}
void H_27(double *state, double *unused, double *out_2061208398182542286) {
   out_2061208398182542286[0] = 0;
   out_2061208398182542286[1] = 0;
   out_2061208398182542286[2] = 0;
   out_2061208398182542286[3] = 1;
   out_2061208398182542286[4] = 0;
   out_2061208398182542286[5] = 0;
   out_2061208398182542286[6] = 0;
   out_2061208398182542286[7] = 0;
   out_2061208398182542286[8] = 0;
}
void h_29(double *state, double *unused, double *out_1630225884836582638) {
   out_1630225884836582638[0] = state[1];
}
void H_29(double *state, double *unused, double *out_623786257932274809) {
   out_623786257932274809[0] = 0;
   out_623786257932274809[1] = 1;
   out_623786257932274809[2] = 0;
   out_623786257932274809[3] = 0;
   out_623786257932274809[4] = 0;
   out_623786257932274809[5] = 0;
   out_623786257932274809[6] = 0;
   out_623786257932274809[7] = 0;
   out_623786257932274809[8] = 0;
}
void h_28(double *state, double *unused, double *out_4197099488612105863) {
   out_4197099488612105863[0] = state[0];
}
void H_28(double *state, double *unused, double *out_4458612759137255765) {
   out_4458612759137255765[0] = 1;
   out_4458612759137255765[1] = 0;
   out_4458612759137255765[2] = 0;
   out_4458612759137255765[3] = 0;
   out_4458612759137255765[4] = 0;
   out_4458612759137255765[5] = 0;
   out_4458612759137255765[6] = 0;
   out_4458612759137255765[7] = 0;
   out_4458612759137255765[8] = 0;
}
void h_31(double *state, double *unused, double *out_296122742204051798) {
   out_296122742204051798[0] = state[8];
}
void H_31(double *state, double *unused, double *out_273539822638083123) {
   out_273539822638083123[0] = 0;
   out_273539822638083123[1] = 0;
   out_273539822638083123[2] = 0;
   out_273539822638083123[3] = 0;
   out_273539822638083123[4] = 0;
   out_273539822638083123[5] = 0;
   out_273539822638083123[6] = 0;
   out_273539822638083123[7] = 0;
   out_273539822638083123[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_3737406110502016648) {
  err_fun(nom_x, delta_x, out_3737406110502016648);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2403136137136565417) {
  inv_err_fun(nom_x, true_x, out_2403136137136565417);
}
void car_H_mod_fun(double *state, double *out_9188041439089480832) {
  H_mod_fun(state, out_9188041439089480832);
}
void car_f_fun(double *state, double dt, double *out_8940466509319208784) {
  f_fun(state,  dt, out_8940466509319208784);
}
void car_F_fun(double *state, double dt, double *out_4596333403153979532) {
  F_fun(state,  dt, out_4596333403153979532);
}
void car_h_25(double *state, double *unused, double *out_8177395392958100983) {
  h_25(state, unused, out_8177395392958100983);
}
void car_H_25(double *state, double *unused, double *out_2404778044889366002) {
  H_25(state, unused, out_2404778044889366002);
}
void car_h_24(double *state, double *unused, double *out_3033720568854893948) {
  h_24(state, unused, out_3033720568854893948);
}
void car_H_24(double *state, double *unused, double *out_227563621282216029) {
  H_24(state, unused, out_227563621282216029);
}
void car_h_30(double *state, double *unused, double *out_6631899152497428283) {
  h_30(state, unused, out_6631899152497428283);
}
void car_H_30(double *state, double *unused, double *out_113554913617882625) {
  H_30(state, unused, out_113554913617882625);
}
void car_h_26(double *state, double *unused, double *out_4509070587126112289) {
  h_26(state, unused, out_4509070587126112289);
}
void car_H_26(double *state, double *unused, double *out_899747924871434599) {
  H_26(state, unused, out_899747924871434599);
}
void car_h_27(double *state, double *unused, double *out_7009651591881959246) {
  h_27(state, unused, out_7009651591881959246);
}
void car_H_27(double *state, double *unused, double *out_2061208398182542286) {
  H_27(state, unused, out_2061208398182542286);
}
void car_h_29(double *state, double *unused, double *out_1630225884836582638) {
  h_29(state, unused, out_1630225884836582638);
}
void car_H_29(double *state, double *unused, double *out_623786257932274809) {
  H_29(state, unused, out_623786257932274809);
}
void car_h_28(double *state, double *unused, double *out_4197099488612105863) {
  h_28(state, unused, out_4197099488612105863);
}
void car_H_28(double *state, double *unused, double *out_4458612759137255765) {
  H_28(state, unused, out_4458612759137255765);
}
void car_h_31(double *state, double *unused, double *out_296122742204051798) {
  h_31(state, unused, out_296122742204051798);
}
void car_H_31(double *state, double *unused, double *out_273539822638083123) {
  H_31(state, unused, out_273539822638083123);
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
