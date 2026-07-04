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
void err_fun(double *nom_x, double *delta_x, double *out_1762034731848154322) {
   out_1762034731848154322[0] = delta_x[0] + nom_x[0];
   out_1762034731848154322[1] = delta_x[1] + nom_x[1];
   out_1762034731848154322[2] = delta_x[2] + nom_x[2];
   out_1762034731848154322[3] = delta_x[3] + nom_x[3];
   out_1762034731848154322[4] = delta_x[4] + nom_x[4];
   out_1762034731848154322[5] = delta_x[5] + nom_x[5];
   out_1762034731848154322[6] = delta_x[6] + nom_x[6];
   out_1762034731848154322[7] = delta_x[7] + nom_x[7];
   out_1762034731848154322[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_1923774774921120974) {
   out_1923774774921120974[0] = -nom_x[0] + true_x[0];
   out_1923774774921120974[1] = -nom_x[1] + true_x[1];
   out_1923774774921120974[2] = -nom_x[2] + true_x[2];
   out_1923774774921120974[3] = -nom_x[3] + true_x[3];
   out_1923774774921120974[4] = -nom_x[4] + true_x[4];
   out_1923774774921120974[5] = -nom_x[5] + true_x[5];
   out_1923774774921120974[6] = -nom_x[6] + true_x[6];
   out_1923774774921120974[7] = -nom_x[7] + true_x[7];
   out_1923774774921120974[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_8208381335871607819) {
   out_8208381335871607819[0] = 1.0;
   out_8208381335871607819[1] = 0;
   out_8208381335871607819[2] = 0;
   out_8208381335871607819[3] = 0;
   out_8208381335871607819[4] = 0;
   out_8208381335871607819[5] = 0;
   out_8208381335871607819[6] = 0;
   out_8208381335871607819[7] = 0;
   out_8208381335871607819[8] = 0;
   out_8208381335871607819[9] = 0;
   out_8208381335871607819[10] = 1.0;
   out_8208381335871607819[11] = 0;
   out_8208381335871607819[12] = 0;
   out_8208381335871607819[13] = 0;
   out_8208381335871607819[14] = 0;
   out_8208381335871607819[15] = 0;
   out_8208381335871607819[16] = 0;
   out_8208381335871607819[17] = 0;
   out_8208381335871607819[18] = 0;
   out_8208381335871607819[19] = 0;
   out_8208381335871607819[20] = 1.0;
   out_8208381335871607819[21] = 0;
   out_8208381335871607819[22] = 0;
   out_8208381335871607819[23] = 0;
   out_8208381335871607819[24] = 0;
   out_8208381335871607819[25] = 0;
   out_8208381335871607819[26] = 0;
   out_8208381335871607819[27] = 0;
   out_8208381335871607819[28] = 0;
   out_8208381335871607819[29] = 0;
   out_8208381335871607819[30] = 1.0;
   out_8208381335871607819[31] = 0;
   out_8208381335871607819[32] = 0;
   out_8208381335871607819[33] = 0;
   out_8208381335871607819[34] = 0;
   out_8208381335871607819[35] = 0;
   out_8208381335871607819[36] = 0;
   out_8208381335871607819[37] = 0;
   out_8208381335871607819[38] = 0;
   out_8208381335871607819[39] = 0;
   out_8208381335871607819[40] = 1.0;
   out_8208381335871607819[41] = 0;
   out_8208381335871607819[42] = 0;
   out_8208381335871607819[43] = 0;
   out_8208381335871607819[44] = 0;
   out_8208381335871607819[45] = 0;
   out_8208381335871607819[46] = 0;
   out_8208381335871607819[47] = 0;
   out_8208381335871607819[48] = 0;
   out_8208381335871607819[49] = 0;
   out_8208381335871607819[50] = 1.0;
   out_8208381335871607819[51] = 0;
   out_8208381335871607819[52] = 0;
   out_8208381335871607819[53] = 0;
   out_8208381335871607819[54] = 0;
   out_8208381335871607819[55] = 0;
   out_8208381335871607819[56] = 0;
   out_8208381335871607819[57] = 0;
   out_8208381335871607819[58] = 0;
   out_8208381335871607819[59] = 0;
   out_8208381335871607819[60] = 1.0;
   out_8208381335871607819[61] = 0;
   out_8208381335871607819[62] = 0;
   out_8208381335871607819[63] = 0;
   out_8208381335871607819[64] = 0;
   out_8208381335871607819[65] = 0;
   out_8208381335871607819[66] = 0;
   out_8208381335871607819[67] = 0;
   out_8208381335871607819[68] = 0;
   out_8208381335871607819[69] = 0;
   out_8208381335871607819[70] = 1.0;
   out_8208381335871607819[71] = 0;
   out_8208381335871607819[72] = 0;
   out_8208381335871607819[73] = 0;
   out_8208381335871607819[74] = 0;
   out_8208381335871607819[75] = 0;
   out_8208381335871607819[76] = 0;
   out_8208381335871607819[77] = 0;
   out_8208381335871607819[78] = 0;
   out_8208381335871607819[79] = 0;
   out_8208381335871607819[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_1983193947830437688) {
   out_1983193947830437688[0] = state[0];
   out_1983193947830437688[1] = state[1];
   out_1983193947830437688[2] = state[2];
   out_1983193947830437688[3] = state[3];
   out_1983193947830437688[4] = state[4];
   out_1983193947830437688[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_1983193947830437688[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_1983193947830437688[7] = state[7];
   out_1983193947830437688[8] = state[8];
}
void F_fun(double *state, double dt, double *out_8589051623902649482) {
   out_8589051623902649482[0] = 1;
   out_8589051623902649482[1] = 0;
   out_8589051623902649482[2] = 0;
   out_8589051623902649482[3] = 0;
   out_8589051623902649482[4] = 0;
   out_8589051623902649482[5] = 0;
   out_8589051623902649482[6] = 0;
   out_8589051623902649482[7] = 0;
   out_8589051623902649482[8] = 0;
   out_8589051623902649482[9] = 0;
   out_8589051623902649482[10] = 1;
   out_8589051623902649482[11] = 0;
   out_8589051623902649482[12] = 0;
   out_8589051623902649482[13] = 0;
   out_8589051623902649482[14] = 0;
   out_8589051623902649482[15] = 0;
   out_8589051623902649482[16] = 0;
   out_8589051623902649482[17] = 0;
   out_8589051623902649482[18] = 0;
   out_8589051623902649482[19] = 0;
   out_8589051623902649482[20] = 1;
   out_8589051623902649482[21] = 0;
   out_8589051623902649482[22] = 0;
   out_8589051623902649482[23] = 0;
   out_8589051623902649482[24] = 0;
   out_8589051623902649482[25] = 0;
   out_8589051623902649482[26] = 0;
   out_8589051623902649482[27] = 0;
   out_8589051623902649482[28] = 0;
   out_8589051623902649482[29] = 0;
   out_8589051623902649482[30] = 1;
   out_8589051623902649482[31] = 0;
   out_8589051623902649482[32] = 0;
   out_8589051623902649482[33] = 0;
   out_8589051623902649482[34] = 0;
   out_8589051623902649482[35] = 0;
   out_8589051623902649482[36] = 0;
   out_8589051623902649482[37] = 0;
   out_8589051623902649482[38] = 0;
   out_8589051623902649482[39] = 0;
   out_8589051623902649482[40] = 1;
   out_8589051623902649482[41] = 0;
   out_8589051623902649482[42] = 0;
   out_8589051623902649482[43] = 0;
   out_8589051623902649482[44] = 0;
   out_8589051623902649482[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_8589051623902649482[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_8589051623902649482[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8589051623902649482[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8589051623902649482[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_8589051623902649482[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_8589051623902649482[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_8589051623902649482[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_8589051623902649482[53] = -9.8000000000000007*dt;
   out_8589051623902649482[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_8589051623902649482[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_8589051623902649482[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8589051623902649482[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8589051623902649482[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_8589051623902649482[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_8589051623902649482[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_8589051623902649482[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8589051623902649482[62] = 0;
   out_8589051623902649482[63] = 0;
   out_8589051623902649482[64] = 0;
   out_8589051623902649482[65] = 0;
   out_8589051623902649482[66] = 0;
   out_8589051623902649482[67] = 0;
   out_8589051623902649482[68] = 0;
   out_8589051623902649482[69] = 0;
   out_8589051623902649482[70] = 1;
   out_8589051623902649482[71] = 0;
   out_8589051623902649482[72] = 0;
   out_8589051623902649482[73] = 0;
   out_8589051623902649482[74] = 0;
   out_8589051623902649482[75] = 0;
   out_8589051623902649482[76] = 0;
   out_8589051623902649482[77] = 0;
   out_8589051623902649482[78] = 0;
   out_8589051623902649482[79] = 0;
   out_8589051623902649482[80] = 1;
}
void h_25(double *state, double *unused, double *out_2444284713406751542) {
   out_2444284713406751542[0] = state[6];
}
void H_25(double *state, double *unused, double *out_866685846547765582) {
   out_866685846547765582[0] = 0;
   out_866685846547765582[1] = 0;
   out_866685846547765582[2] = 0;
   out_866685846547765582[3] = 0;
   out_866685846547765582[4] = 0;
   out_866685846547765582[5] = 0;
   out_866685846547765582[6] = 1;
   out_866685846547765582[7] = 0;
   out_866685846547765582[8] = 0;
}
void h_24(double *state, double *unused, double *out_2334447044872400550) {
   out_2334447044872400550[0] = state[4];
   out_2334447044872400550[1] = state[5];
}
void H_24(double *state, double *unused, double *out_6140180129460778176) {
   out_6140180129460778176[0] = 0;
   out_6140180129460778176[1] = 0;
   out_6140180129460778176[2] = 0;
   out_6140180129460778176[3] = 0;
   out_6140180129460778176[4] = 1;
   out_6140180129460778176[5] = 0;
   out_6140180129460778176[6] = 0;
   out_6140180129460778176[7] = 0;
   out_6140180129460778176[8] = 0;
   out_6140180129460778176[9] = 0;
   out_6140180129460778176[10] = 0;
   out_6140180129460778176[11] = 0;
   out_6140180129460778176[12] = 0;
   out_6140180129460778176[13] = 0;
   out_6140180129460778176[14] = 1;
   out_6140180129460778176[15] = 0;
   out_6140180129460778176[16] = 0;
   out_6140180129460778176[17] = 0;
}
void h_30(double *state, double *unused, double *out_1193164814847270808) {
   out_1193164814847270808[0] = state[4];
}
void H_30(double *state, double *unused, double *out_3385018805055014209) {
   out_3385018805055014209[0] = 0;
   out_3385018805055014209[1] = 0;
   out_3385018805055014209[2] = 0;
   out_3385018805055014209[3] = 0;
   out_3385018805055014209[4] = 1;
   out_3385018805055014209[5] = 0;
   out_3385018805055014209[6] = 0;
   out_3385018805055014209[7] = 0;
   out_3385018805055014209[8] = 0;
}
void h_26(double *state, double *unused, double *out_6523714585703761970) {
   out_6523714585703761970[0] = state[7];
}
void H_26(double *state, double *unused, double *out_2874817472326290642) {
   out_2874817472326290642[0] = 0;
   out_2874817472326290642[1] = 0;
   out_2874817472326290642[2] = 0;
   out_2874817472326290642[3] = 0;
   out_2874817472326290642[4] = 0;
   out_2874817472326290642[5] = 0;
   out_2874817472326290642[6] = 0;
   out_2874817472326290642[7] = 1;
   out_2874817472326290642[8] = 0;
}
void h_27(double *state, double *unused, double *out_3198445157674205293) {
   out_3198445157674205293[0] = state[3];
}
void H_27(double *state, double *unused, double *out_5608612876238957426) {
   out_5608612876238957426[0] = 0;
   out_5608612876238957426[1] = 0;
   out_5608612876238957426[2] = 0;
   out_5608612876238957426[3] = 1;
   out_5608612876238957426[4] = 0;
   out_5608612876238957426[5] = 0;
   out_5608612876238957426[6] = 0;
   out_5608612876238957426[7] = 0;
   out_5608612876238957426[8] = 0;
}
void h_29(double *state, double *unused, double *out_1796480699523748325) {
   out_1796480699523748325[0] = state[1];
}
void H_29(double *state, double *unused, double *out_3895250149369406393) {
   out_3895250149369406393[0] = 0;
   out_3895250149369406393[1] = 1;
   out_3895250149369406393[2] = 0;
   out_3895250149369406393[3] = 0;
   out_3895250149369406393[4] = 0;
   out_3895250149369406393[5] = 0;
   out_3895250149369406393[6] = 0;
   out_3895250149369406393[7] = 0;
   out_3895250149369406393[8] = 0;
}
void h_28(double *state, double *unused, double *out_7623806072972436826) {
   out_7623806072972436826[0] = state[0];
}
void H_28(double *state, double *unused, double *out_1187148867700124181) {
   out_1187148867700124181[0] = 1;
   out_1187148867700124181[1] = 0;
   out_1187148867700124181[2] = 0;
   out_1187148867700124181[3] = 0;
   out_1187148867700124181[4] = 0;
   out_1187148867700124181[5] = 0;
   out_1187148867700124181[6] = 0;
   out_1187148867700124181[7] = 0;
   out_1187148867700124181[8] = 0;
}
void h_31(double *state, double *unused, double *out_2719478775691257431) {
   out_2719478775691257431[0] = state[8];
}
void H_31(double *state, double *unused, double *out_897331808424726010) {
   out_897331808424726010[0] = 0;
   out_897331808424726010[1] = 0;
   out_897331808424726010[2] = 0;
   out_897331808424726010[3] = 0;
   out_897331808424726010[4] = 0;
   out_897331808424726010[5] = 0;
   out_897331808424726010[6] = 0;
   out_897331808424726010[7] = 0;
   out_897331808424726010[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_1762034731848154322) {
  err_fun(nom_x, delta_x, out_1762034731848154322);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1923774774921120974) {
  inv_err_fun(nom_x, true_x, out_1923774774921120974);
}
void car_H_mod_fun(double *state, double *out_8208381335871607819) {
  H_mod_fun(state, out_8208381335871607819);
}
void car_f_fun(double *state, double dt, double *out_1983193947830437688) {
  f_fun(state,  dt, out_1983193947830437688);
}
void car_F_fun(double *state, double dt, double *out_8589051623902649482) {
  F_fun(state,  dt, out_8589051623902649482);
}
void car_h_25(double *state, double *unused, double *out_2444284713406751542) {
  h_25(state, unused, out_2444284713406751542);
}
void car_H_25(double *state, double *unused, double *out_866685846547765582) {
  H_25(state, unused, out_866685846547765582);
}
void car_h_24(double *state, double *unused, double *out_2334447044872400550) {
  h_24(state, unused, out_2334447044872400550);
}
void car_H_24(double *state, double *unused, double *out_6140180129460778176) {
  H_24(state, unused, out_6140180129460778176);
}
void car_h_30(double *state, double *unused, double *out_1193164814847270808) {
  h_30(state, unused, out_1193164814847270808);
}
void car_H_30(double *state, double *unused, double *out_3385018805055014209) {
  H_30(state, unused, out_3385018805055014209);
}
void car_h_26(double *state, double *unused, double *out_6523714585703761970) {
  h_26(state, unused, out_6523714585703761970);
}
void car_H_26(double *state, double *unused, double *out_2874817472326290642) {
  H_26(state, unused, out_2874817472326290642);
}
void car_h_27(double *state, double *unused, double *out_3198445157674205293) {
  h_27(state, unused, out_3198445157674205293);
}
void car_H_27(double *state, double *unused, double *out_5608612876238957426) {
  H_27(state, unused, out_5608612876238957426);
}
void car_h_29(double *state, double *unused, double *out_1796480699523748325) {
  h_29(state, unused, out_1796480699523748325);
}
void car_H_29(double *state, double *unused, double *out_3895250149369406393) {
  H_29(state, unused, out_3895250149369406393);
}
void car_h_28(double *state, double *unused, double *out_7623806072972436826) {
  h_28(state, unused, out_7623806072972436826);
}
void car_H_28(double *state, double *unused, double *out_1187148867700124181) {
  H_28(state, unused, out_1187148867700124181);
}
void car_h_31(double *state, double *unused, double *out_2719478775691257431) {
  h_31(state, unused, out_2719478775691257431);
}
void car_H_31(double *state, double *unused, double *out_897331808424726010) {
  H_31(state, unused, out_897331808424726010);
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
