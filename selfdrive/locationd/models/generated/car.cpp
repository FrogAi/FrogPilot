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
void err_fun(double *nom_x, double *delta_x, double *out_1174255670832059551) {
   out_1174255670832059551[0] = delta_x[0] + nom_x[0];
   out_1174255670832059551[1] = delta_x[1] + nom_x[1];
   out_1174255670832059551[2] = delta_x[2] + nom_x[2];
   out_1174255670832059551[3] = delta_x[3] + nom_x[3];
   out_1174255670832059551[4] = delta_x[4] + nom_x[4];
   out_1174255670832059551[5] = delta_x[5] + nom_x[5];
   out_1174255670832059551[6] = delta_x[6] + nom_x[6];
   out_1174255670832059551[7] = delta_x[7] + nom_x[7];
   out_1174255670832059551[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_4619408999502106690) {
   out_4619408999502106690[0] = -nom_x[0] + true_x[0];
   out_4619408999502106690[1] = -nom_x[1] + true_x[1];
   out_4619408999502106690[2] = -nom_x[2] + true_x[2];
   out_4619408999502106690[3] = -nom_x[3] + true_x[3];
   out_4619408999502106690[4] = -nom_x[4] + true_x[4];
   out_4619408999502106690[5] = -nom_x[5] + true_x[5];
   out_4619408999502106690[6] = -nom_x[6] + true_x[6];
   out_4619408999502106690[7] = -nom_x[7] + true_x[7];
   out_4619408999502106690[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_6508123924648701622) {
   out_6508123924648701622[0] = 1.0;
   out_6508123924648701622[1] = 0;
   out_6508123924648701622[2] = 0;
   out_6508123924648701622[3] = 0;
   out_6508123924648701622[4] = 0;
   out_6508123924648701622[5] = 0;
   out_6508123924648701622[6] = 0;
   out_6508123924648701622[7] = 0;
   out_6508123924648701622[8] = 0;
   out_6508123924648701622[9] = 0;
   out_6508123924648701622[10] = 1.0;
   out_6508123924648701622[11] = 0;
   out_6508123924648701622[12] = 0;
   out_6508123924648701622[13] = 0;
   out_6508123924648701622[14] = 0;
   out_6508123924648701622[15] = 0;
   out_6508123924648701622[16] = 0;
   out_6508123924648701622[17] = 0;
   out_6508123924648701622[18] = 0;
   out_6508123924648701622[19] = 0;
   out_6508123924648701622[20] = 1.0;
   out_6508123924648701622[21] = 0;
   out_6508123924648701622[22] = 0;
   out_6508123924648701622[23] = 0;
   out_6508123924648701622[24] = 0;
   out_6508123924648701622[25] = 0;
   out_6508123924648701622[26] = 0;
   out_6508123924648701622[27] = 0;
   out_6508123924648701622[28] = 0;
   out_6508123924648701622[29] = 0;
   out_6508123924648701622[30] = 1.0;
   out_6508123924648701622[31] = 0;
   out_6508123924648701622[32] = 0;
   out_6508123924648701622[33] = 0;
   out_6508123924648701622[34] = 0;
   out_6508123924648701622[35] = 0;
   out_6508123924648701622[36] = 0;
   out_6508123924648701622[37] = 0;
   out_6508123924648701622[38] = 0;
   out_6508123924648701622[39] = 0;
   out_6508123924648701622[40] = 1.0;
   out_6508123924648701622[41] = 0;
   out_6508123924648701622[42] = 0;
   out_6508123924648701622[43] = 0;
   out_6508123924648701622[44] = 0;
   out_6508123924648701622[45] = 0;
   out_6508123924648701622[46] = 0;
   out_6508123924648701622[47] = 0;
   out_6508123924648701622[48] = 0;
   out_6508123924648701622[49] = 0;
   out_6508123924648701622[50] = 1.0;
   out_6508123924648701622[51] = 0;
   out_6508123924648701622[52] = 0;
   out_6508123924648701622[53] = 0;
   out_6508123924648701622[54] = 0;
   out_6508123924648701622[55] = 0;
   out_6508123924648701622[56] = 0;
   out_6508123924648701622[57] = 0;
   out_6508123924648701622[58] = 0;
   out_6508123924648701622[59] = 0;
   out_6508123924648701622[60] = 1.0;
   out_6508123924648701622[61] = 0;
   out_6508123924648701622[62] = 0;
   out_6508123924648701622[63] = 0;
   out_6508123924648701622[64] = 0;
   out_6508123924648701622[65] = 0;
   out_6508123924648701622[66] = 0;
   out_6508123924648701622[67] = 0;
   out_6508123924648701622[68] = 0;
   out_6508123924648701622[69] = 0;
   out_6508123924648701622[70] = 1.0;
   out_6508123924648701622[71] = 0;
   out_6508123924648701622[72] = 0;
   out_6508123924648701622[73] = 0;
   out_6508123924648701622[74] = 0;
   out_6508123924648701622[75] = 0;
   out_6508123924648701622[76] = 0;
   out_6508123924648701622[77] = 0;
   out_6508123924648701622[78] = 0;
   out_6508123924648701622[79] = 0;
   out_6508123924648701622[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_8567017668350943053) {
   out_8567017668350943053[0] = state[0];
   out_8567017668350943053[1] = state[1];
   out_8567017668350943053[2] = state[2];
   out_8567017668350943053[3] = state[3];
   out_8567017668350943053[4] = state[4];
   out_8567017668350943053[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_8567017668350943053[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_8567017668350943053[7] = state[7];
   out_8567017668350943053[8] = state[8];
}
void F_fun(double *state, double dt, double *out_2679543167722733093) {
   out_2679543167722733093[0] = 1;
   out_2679543167722733093[1] = 0;
   out_2679543167722733093[2] = 0;
   out_2679543167722733093[3] = 0;
   out_2679543167722733093[4] = 0;
   out_2679543167722733093[5] = 0;
   out_2679543167722733093[6] = 0;
   out_2679543167722733093[7] = 0;
   out_2679543167722733093[8] = 0;
   out_2679543167722733093[9] = 0;
   out_2679543167722733093[10] = 1;
   out_2679543167722733093[11] = 0;
   out_2679543167722733093[12] = 0;
   out_2679543167722733093[13] = 0;
   out_2679543167722733093[14] = 0;
   out_2679543167722733093[15] = 0;
   out_2679543167722733093[16] = 0;
   out_2679543167722733093[17] = 0;
   out_2679543167722733093[18] = 0;
   out_2679543167722733093[19] = 0;
   out_2679543167722733093[20] = 1;
   out_2679543167722733093[21] = 0;
   out_2679543167722733093[22] = 0;
   out_2679543167722733093[23] = 0;
   out_2679543167722733093[24] = 0;
   out_2679543167722733093[25] = 0;
   out_2679543167722733093[26] = 0;
   out_2679543167722733093[27] = 0;
   out_2679543167722733093[28] = 0;
   out_2679543167722733093[29] = 0;
   out_2679543167722733093[30] = 1;
   out_2679543167722733093[31] = 0;
   out_2679543167722733093[32] = 0;
   out_2679543167722733093[33] = 0;
   out_2679543167722733093[34] = 0;
   out_2679543167722733093[35] = 0;
   out_2679543167722733093[36] = 0;
   out_2679543167722733093[37] = 0;
   out_2679543167722733093[38] = 0;
   out_2679543167722733093[39] = 0;
   out_2679543167722733093[40] = 1;
   out_2679543167722733093[41] = 0;
   out_2679543167722733093[42] = 0;
   out_2679543167722733093[43] = 0;
   out_2679543167722733093[44] = 0;
   out_2679543167722733093[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_2679543167722733093[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_2679543167722733093[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2679543167722733093[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2679543167722733093[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_2679543167722733093[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_2679543167722733093[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_2679543167722733093[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_2679543167722733093[53] = -9.8000000000000007*dt;
   out_2679543167722733093[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_2679543167722733093[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_2679543167722733093[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2679543167722733093[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2679543167722733093[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_2679543167722733093[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_2679543167722733093[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_2679543167722733093[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2679543167722733093[62] = 0;
   out_2679543167722733093[63] = 0;
   out_2679543167722733093[64] = 0;
   out_2679543167722733093[65] = 0;
   out_2679543167722733093[66] = 0;
   out_2679543167722733093[67] = 0;
   out_2679543167722733093[68] = 0;
   out_2679543167722733093[69] = 0;
   out_2679543167722733093[70] = 1;
   out_2679543167722733093[71] = 0;
   out_2679543167722733093[72] = 0;
   out_2679543167722733093[73] = 0;
   out_2679543167722733093[74] = 0;
   out_2679543167722733093[75] = 0;
   out_2679543167722733093[76] = 0;
   out_2679543167722733093[77] = 0;
   out_2679543167722733093[78] = 0;
   out_2679543167722733093[79] = 0;
   out_2679543167722733093[80] = 1;
}
void h_25(double *state, double *unused, double *out_2527539469091572327) {
   out_2527539469091572327[0] = state[6];
}
void H_25(double *state, double *unused, double *out_3385424745847202663) {
   out_3385424745847202663[0] = 0;
   out_3385424745847202663[1] = 0;
   out_3385424745847202663[2] = 0;
   out_3385424745847202663[3] = 0;
   out_3385424745847202663[4] = 0;
   out_3385424745847202663[5] = 0;
   out_3385424745847202663[6] = 1;
   out_3385424745847202663[7] = 0;
   out_3385424745847202663[8] = 0;
}
void h_24(double *state, double *unused, double *out_1306671744405780298) {
   out_1306671744405780298[0] = state[4];
   out_1306671744405780298[1] = state[5];
}
void H_24(double *state, double *unused, double *out_1483390119180504189) {
   out_1483390119180504189[0] = 0;
   out_1483390119180504189[1] = 0;
   out_1483390119180504189[2] = 0;
   out_1483390119180504189[3] = 0;
   out_1483390119180504189[4] = 1;
   out_1483390119180504189[5] = 0;
   out_1483390119180504189[6] = 0;
   out_1483390119180504189[7] = 0;
   out_1483390119180504189[8] = 0;
   out_1483390119180504189[9] = 0;
   out_1483390119180504189[10] = 0;
   out_1483390119180504189[11] = 0;
   out_1483390119180504189[12] = 0;
   out_1483390119180504189[13] = 0;
   out_1483390119180504189[14] = 1;
   out_1483390119180504189[15] = 0;
   out_1483390119180504189[16] = 0;
   out_1483390119180504189[17] = 0;
}
void h_30(double *state, double *unused, double *out_2673059293312227298) {
   out_2673059293312227298[0] = state[4];
}
void H_30(double *state, double *unused, double *out_1142271584280405535) {
   out_1142271584280405535[0] = 0;
   out_1142271584280405535[1] = 0;
   out_1142271584280405535[2] = 0;
   out_1142271584280405535[3] = 0;
   out_1142271584280405535[4] = 1;
   out_1142271584280405535[5] = 0;
   out_1142271584280405535[6] = 0;
   out_1142271584280405535[7] = 0;
   out_1142271584280405535[8] = 0;
}
void h_26(double *state, double *unused, double *out_7411205342819041228) {
   out_7411205342819041228[0] = state[7];
}
void H_26(double *state, double *unused, double *out_356078573026853561) {
   out_356078573026853561[0] = 0;
   out_356078573026853561[1] = 0;
   out_356078573026853561[2] = 0;
   out_356078573026853561[3] = 0;
   out_356078573026853561[4] = 0;
   out_356078573026853561[5] = 0;
   out_356078573026853561[6] = 0;
   out_356078573026853561[7] = 1;
   out_356078573026853561[8] = 0;
}
void h_27(double *state, double *unused, double *out_7529212736752417972) {
   out_7529212736752417972[0] = state[3];
}
void H_27(double *state, double *unused, double *out_3317034896080830446) {
   out_3317034896080830446[0] = 0;
   out_3317034896080830446[1] = 0;
   out_3317034896080830446[2] = 0;
   out_3317034896080830446[3] = 1;
   out_3317034896080830446[4] = 0;
   out_3317034896080830446[5] = 0;
   out_3317034896080830446[6] = 0;
   out_3317034896080830446[7] = 0;
   out_3317034896080830446[8] = 0;
}
void h_29(double *state, double *unused, double *out_68653972503529647) {
   out_68653972503529647[0] = state[1];
}
void H_29(double *state, double *unused, double *out_632040239966013351) {
   out_632040239966013351[0] = 0;
   out_632040239966013351[1] = 1;
   out_632040239966013351[2] = 0;
   out_632040239966013351[3] = 0;
   out_632040239966013351[4] = 0;
   out_632040239966013351[5] = 0;
   out_632040239966013351[6] = 0;
   out_632040239966013351[7] = 0;
   out_632040239966013351[8] = 0;
}
void h_28(double *state, double *unused, double *out_5915858069296287999) {
   out_5915858069296287999[0] = state[0];
}
void H_28(double *state, double *unused, double *out_5714439257035543925) {
   out_5714439257035543925[0] = 1;
   out_5714439257035543925[1] = 0;
   out_5714439257035543925[2] = 0;
   out_5714439257035543925[3] = 0;
   out_5714439257035543925[4] = 0;
   out_5714439257035543925[5] = 0;
   out_5714439257035543925[6] = 0;
   out_5714439257035543925[7] = 0;
   out_5714439257035543925[8] = 0;
}
void h_31(double *state, double *unused, double *out_8155939347797306848) {
   out_8155939347797306848[0] = state[8];
}
void H_31(double *state, double *unused, double *out_982286675260205037) {
   out_982286675260205037[0] = 0;
   out_982286675260205037[1] = 0;
   out_982286675260205037[2] = 0;
   out_982286675260205037[3] = 0;
   out_982286675260205037[4] = 0;
   out_982286675260205037[5] = 0;
   out_982286675260205037[6] = 0;
   out_982286675260205037[7] = 0;
   out_982286675260205037[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_1174255670832059551) {
  err_fun(nom_x, delta_x, out_1174255670832059551);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_4619408999502106690) {
  inv_err_fun(nom_x, true_x, out_4619408999502106690);
}
void car_H_mod_fun(double *state, double *out_6508123924648701622) {
  H_mod_fun(state, out_6508123924648701622);
}
void car_f_fun(double *state, double dt, double *out_8567017668350943053) {
  f_fun(state,  dt, out_8567017668350943053);
}
void car_F_fun(double *state, double dt, double *out_2679543167722733093) {
  F_fun(state,  dt, out_2679543167722733093);
}
void car_h_25(double *state, double *unused, double *out_2527539469091572327) {
  h_25(state, unused, out_2527539469091572327);
}
void car_H_25(double *state, double *unused, double *out_3385424745847202663) {
  H_25(state, unused, out_3385424745847202663);
}
void car_h_24(double *state, double *unused, double *out_1306671744405780298) {
  h_24(state, unused, out_1306671744405780298);
}
void car_H_24(double *state, double *unused, double *out_1483390119180504189) {
  H_24(state, unused, out_1483390119180504189);
}
void car_h_30(double *state, double *unused, double *out_2673059293312227298) {
  h_30(state, unused, out_2673059293312227298);
}
void car_H_30(double *state, double *unused, double *out_1142271584280405535) {
  H_30(state, unused, out_1142271584280405535);
}
void car_h_26(double *state, double *unused, double *out_7411205342819041228) {
  h_26(state, unused, out_7411205342819041228);
}
void car_H_26(double *state, double *unused, double *out_356078573026853561) {
  H_26(state, unused, out_356078573026853561);
}
void car_h_27(double *state, double *unused, double *out_7529212736752417972) {
  h_27(state, unused, out_7529212736752417972);
}
void car_H_27(double *state, double *unused, double *out_3317034896080830446) {
  H_27(state, unused, out_3317034896080830446);
}
void car_h_29(double *state, double *unused, double *out_68653972503529647) {
  h_29(state, unused, out_68653972503529647);
}
void car_H_29(double *state, double *unused, double *out_632040239966013351) {
  H_29(state, unused, out_632040239966013351);
}
void car_h_28(double *state, double *unused, double *out_5915858069296287999) {
  h_28(state, unused, out_5915858069296287999);
}
void car_H_28(double *state, double *unused, double *out_5714439257035543925) {
  H_28(state, unused, out_5714439257035543925);
}
void car_h_31(double *state, double *unused, double *out_8155939347797306848) {
  h_31(state, unused, out_8155939347797306848);
}
void car_H_31(double *state, double *unused, double *out_982286675260205037) {
  H_31(state, unused, out_982286675260205037);
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
