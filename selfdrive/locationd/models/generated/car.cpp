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
void err_fun(double *nom_x, double *delta_x, double *out_1448822998998473964) {
   out_1448822998998473964[0] = delta_x[0] + nom_x[0];
   out_1448822998998473964[1] = delta_x[1] + nom_x[1];
   out_1448822998998473964[2] = delta_x[2] + nom_x[2];
   out_1448822998998473964[3] = delta_x[3] + nom_x[3];
   out_1448822998998473964[4] = delta_x[4] + nom_x[4];
   out_1448822998998473964[5] = delta_x[5] + nom_x[5];
   out_1448822998998473964[6] = delta_x[6] + nom_x[6];
   out_1448822998998473964[7] = delta_x[7] + nom_x[7];
   out_1448822998998473964[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_2803394903321302341) {
   out_2803394903321302341[0] = -nom_x[0] + true_x[0];
   out_2803394903321302341[1] = -nom_x[1] + true_x[1];
   out_2803394903321302341[2] = -nom_x[2] + true_x[2];
   out_2803394903321302341[3] = -nom_x[3] + true_x[3];
   out_2803394903321302341[4] = -nom_x[4] + true_x[4];
   out_2803394903321302341[5] = -nom_x[5] + true_x[5];
   out_2803394903321302341[6] = -nom_x[6] + true_x[6];
   out_2803394903321302341[7] = -nom_x[7] + true_x[7];
   out_2803394903321302341[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_8083424101222547813) {
   out_8083424101222547813[0] = 1.0;
   out_8083424101222547813[1] = 0;
   out_8083424101222547813[2] = 0;
   out_8083424101222547813[3] = 0;
   out_8083424101222547813[4] = 0;
   out_8083424101222547813[5] = 0;
   out_8083424101222547813[6] = 0;
   out_8083424101222547813[7] = 0;
   out_8083424101222547813[8] = 0;
   out_8083424101222547813[9] = 0;
   out_8083424101222547813[10] = 1.0;
   out_8083424101222547813[11] = 0;
   out_8083424101222547813[12] = 0;
   out_8083424101222547813[13] = 0;
   out_8083424101222547813[14] = 0;
   out_8083424101222547813[15] = 0;
   out_8083424101222547813[16] = 0;
   out_8083424101222547813[17] = 0;
   out_8083424101222547813[18] = 0;
   out_8083424101222547813[19] = 0;
   out_8083424101222547813[20] = 1.0;
   out_8083424101222547813[21] = 0;
   out_8083424101222547813[22] = 0;
   out_8083424101222547813[23] = 0;
   out_8083424101222547813[24] = 0;
   out_8083424101222547813[25] = 0;
   out_8083424101222547813[26] = 0;
   out_8083424101222547813[27] = 0;
   out_8083424101222547813[28] = 0;
   out_8083424101222547813[29] = 0;
   out_8083424101222547813[30] = 1.0;
   out_8083424101222547813[31] = 0;
   out_8083424101222547813[32] = 0;
   out_8083424101222547813[33] = 0;
   out_8083424101222547813[34] = 0;
   out_8083424101222547813[35] = 0;
   out_8083424101222547813[36] = 0;
   out_8083424101222547813[37] = 0;
   out_8083424101222547813[38] = 0;
   out_8083424101222547813[39] = 0;
   out_8083424101222547813[40] = 1.0;
   out_8083424101222547813[41] = 0;
   out_8083424101222547813[42] = 0;
   out_8083424101222547813[43] = 0;
   out_8083424101222547813[44] = 0;
   out_8083424101222547813[45] = 0;
   out_8083424101222547813[46] = 0;
   out_8083424101222547813[47] = 0;
   out_8083424101222547813[48] = 0;
   out_8083424101222547813[49] = 0;
   out_8083424101222547813[50] = 1.0;
   out_8083424101222547813[51] = 0;
   out_8083424101222547813[52] = 0;
   out_8083424101222547813[53] = 0;
   out_8083424101222547813[54] = 0;
   out_8083424101222547813[55] = 0;
   out_8083424101222547813[56] = 0;
   out_8083424101222547813[57] = 0;
   out_8083424101222547813[58] = 0;
   out_8083424101222547813[59] = 0;
   out_8083424101222547813[60] = 1.0;
   out_8083424101222547813[61] = 0;
   out_8083424101222547813[62] = 0;
   out_8083424101222547813[63] = 0;
   out_8083424101222547813[64] = 0;
   out_8083424101222547813[65] = 0;
   out_8083424101222547813[66] = 0;
   out_8083424101222547813[67] = 0;
   out_8083424101222547813[68] = 0;
   out_8083424101222547813[69] = 0;
   out_8083424101222547813[70] = 1.0;
   out_8083424101222547813[71] = 0;
   out_8083424101222547813[72] = 0;
   out_8083424101222547813[73] = 0;
   out_8083424101222547813[74] = 0;
   out_8083424101222547813[75] = 0;
   out_8083424101222547813[76] = 0;
   out_8083424101222547813[77] = 0;
   out_8083424101222547813[78] = 0;
   out_8083424101222547813[79] = 0;
   out_8083424101222547813[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2859685047189495985) {
   out_2859685047189495985[0] = state[0];
   out_2859685047189495985[1] = state[1];
   out_2859685047189495985[2] = state[2];
   out_2859685047189495985[3] = state[3];
   out_2859685047189495985[4] = state[4];
   out_2859685047189495985[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2859685047189495985[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2859685047189495985[7] = state[7];
   out_2859685047189495985[8] = state[8];
}
void F_fun(double *state, double dt, double *out_2361948659728611371) {
   out_2361948659728611371[0] = 1;
   out_2361948659728611371[1] = 0;
   out_2361948659728611371[2] = 0;
   out_2361948659728611371[3] = 0;
   out_2361948659728611371[4] = 0;
   out_2361948659728611371[5] = 0;
   out_2361948659728611371[6] = 0;
   out_2361948659728611371[7] = 0;
   out_2361948659728611371[8] = 0;
   out_2361948659728611371[9] = 0;
   out_2361948659728611371[10] = 1;
   out_2361948659728611371[11] = 0;
   out_2361948659728611371[12] = 0;
   out_2361948659728611371[13] = 0;
   out_2361948659728611371[14] = 0;
   out_2361948659728611371[15] = 0;
   out_2361948659728611371[16] = 0;
   out_2361948659728611371[17] = 0;
   out_2361948659728611371[18] = 0;
   out_2361948659728611371[19] = 0;
   out_2361948659728611371[20] = 1;
   out_2361948659728611371[21] = 0;
   out_2361948659728611371[22] = 0;
   out_2361948659728611371[23] = 0;
   out_2361948659728611371[24] = 0;
   out_2361948659728611371[25] = 0;
   out_2361948659728611371[26] = 0;
   out_2361948659728611371[27] = 0;
   out_2361948659728611371[28] = 0;
   out_2361948659728611371[29] = 0;
   out_2361948659728611371[30] = 1;
   out_2361948659728611371[31] = 0;
   out_2361948659728611371[32] = 0;
   out_2361948659728611371[33] = 0;
   out_2361948659728611371[34] = 0;
   out_2361948659728611371[35] = 0;
   out_2361948659728611371[36] = 0;
   out_2361948659728611371[37] = 0;
   out_2361948659728611371[38] = 0;
   out_2361948659728611371[39] = 0;
   out_2361948659728611371[40] = 1;
   out_2361948659728611371[41] = 0;
   out_2361948659728611371[42] = 0;
   out_2361948659728611371[43] = 0;
   out_2361948659728611371[44] = 0;
   out_2361948659728611371[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_2361948659728611371[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_2361948659728611371[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2361948659728611371[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2361948659728611371[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_2361948659728611371[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_2361948659728611371[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_2361948659728611371[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_2361948659728611371[53] = -9.8000000000000007*dt;
   out_2361948659728611371[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_2361948659728611371[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_2361948659728611371[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2361948659728611371[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2361948659728611371[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_2361948659728611371[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_2361948659728611371[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_2361948659728611371[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2361948659728611371[62] = 0;
   out_2361948659728611371[63] = 0;
   out_2361948659728611371[64] = 0;
   out_2361948659728611371[65] = 0;
   out_2361948659728611371[66] = 0;
   out_2361948659728611371[67] = 0;
   out_2361948659728611371[68] = 0;
   out_2361948659728611371[69] = 0;
   out_2361948659728611371[70] = 1;
   out_2361948659728611371[71] = 0;
   out_2361948659728611371[72] = 0;
   out_2361948659728611371[73] = 0;
   out_2361948659728611371[74] = 0;
   out_2361948659728611371[75] = 0;
   out_2361948659728611371[76] = 0;
   out_2361948659728611371[77] = 0;
   out_2361948659728611371[78] = 0;
   out_2361948659728611371[79] = 0;
   out_2361948659728611371[80] = 1;
}
void h_25(double *state, double *unused, double *out_4094106748208449073) {
   out_4094106748208449073[0] = state[6];
}
void H_25(double *state, double *unused, double *out_4174050980296916764) {
   out_4174050980296916764[0] = 0;
   out_4174050980296916764[1] = 0;
   out_4174050980296916764[2] = 0;
   out_4174050980296916764[3] = 0;
   out_4174050980296916764[4] = 0;
   out_4174050980296916764[5] = 0;
   out_4174050980296916764[6] = 1;
   out_4174050980296916764[7] = 0;
   out_4174050980296916764[8] = 0;
}
void h_24(double *state, double *unused, double *out_3530061735652668723) {
   out_3530061735652668723[0] = state[4];
   out_3530061735652668723[1] = state[5];
}
void H_24(double *state, double *unused, double *out_8994372484952905027) {
   out_8994372484952905027[0] = 0;
   out_8994372484952905027[1] = 0;
   out_8994372484952905027[2] = 0;
   out_8994372484952905027[3] = 0;
   out_8994372484952905027[4] = 1;
   out_8994372484952905027[5] = 0;
   out_8994372484952905027[6] = 0;
   out_8994372484952905027[7] = 0;
   out_8994372484952905027[8] = 0;
   out_8994372484952905027[9] = 0;
   out_8994372484952905027[10] = 0;
   out_8994372484952905027[11] = 0;
   out_8994372484952905027[12] = 0;
   out_8994372484952905027[13] = 0;
   out_8994372484952905027[14] = 1;
   out_8994372484952905027[15] = 0;
   out_8994372484952905027[16] = 0;
   out_8994372484952905027[17] = 0;
}
void h_30(double *state, double *unused, double *out_5544738140837332240) {
   out_5544738140837332240[0] = state[4];
}
void H_30(double *state, double *unused, double *out_4303389927440156834) {
   out_4303389927440156834[0] = 0;
   out_4303389927440156834[1] = 0;
   out_4303389927440156834[2] = 0;
   out_4303389927440156834[3] = 0;
   out_4303389927440156834[4] = 1;
   out_4303389927440156834[5] = 0;
   out_4303389927440156834[6] = 0;
   out_4303389927440156834[7] = 0;
   out_4303389927440156834[8] = 0;
}
void h_26(double *state, double *unused, double *out_2732186037567478857) {
   out_2732186037567478857[0] = state[7];
}
void H_26(double *state, double *unused, double *out_7915554299170972988) {
   out_7915554299170972988[0] = 0;
   out_7915554299170972988[1] = 0;
   out_7915554299170972988[2] = 0;
   out_7915554299170972988[3] = 0;
   out_7915554299170972988[4] = 0;
   out_7915554299170972988[5] = 0;
   out_7915554299170972988[6] = 0;
   out_7915554299170972988[7] = 1;
   out_7915554299170972988[8] = 0;
}
void h_27(double *state, double *unused, double *out_7349874621708036764) {
   out_7349874621708036764[0] = state[3];
}
void H_27(double *state, double *unused, double *out_6478153239240581745) {
   out_6478153239240581745[0] = 0;
   out_6478153239240581745[1] = 0;
   out_6478153239240581745[2] = 0;
   out_6478153239240581745[3] = 1;
   out_6478153239240581745[4] = 0;
   out_6478153239240581745[5] = 0;
   out_6478153239240581745[6] = 0;
   out_6478153239240581745[7] = 0;
   out_6478153239240581745[8] = 0;
}
void h_29(double *state, double *unused, double *out_543064873176486595) {
   out_543064873176486595[0] = state[1];
}
void H_29(double *state, double *unused, double *out_3793158583125764650) {
   out_3793158583125764650[0] = 0;
   out_3793158583125764650[1] = 1;
   out_3793158583125764650[2] = 0;
   out_3793158583125764650[3] = 0;
   out_3793158583125764650[4] = 0;
   out_3793158583125764650[5] = 0;
   out_3793158583125764650[6] = 0;
   out_3793158583125764650[7] = 0;
   out_3793158583125764650[8] = 0;
}
void h_28(double *state, double *unused, double *out_4313088570263704079) {
   out_4313088570263704079[0] = state[0];
}
void H_28(double *state, double *unused, double *out_6227885694544806527) {
   out_6227885694544806527[0] = 1;
   out_6227885694544806527[1] = 0;
   out_6227885694544806527[2] = 0;
   out_6227885694544806527[3] = 0;
   out_6227885694544806527[4] = 0;
   out_6227885694544806527[5] = 0;
   out_6227885694544806527[6] = 0;
   out_6227885694544806527[7] = 0;
   out_6227885694544806527[8] = 0;
}
void h_31(double *state, double *unused, double *out_2073007291762685230) {
   out_2073007291762685230[0] = state[8];
}
void H_31(double *state, double *unused, double *out_4143405018419956336) {
   out_4143405018419956336[0] = 0;
   out_4143405018419956336[1] = 0;
   out_4143405018419956336[2] = 0;
   out_4143405018419956336[3] = 0;
   out_4143405018419956336[4] = 0;
   out_4143405018419956336[5] = 0;
   out_4143405018419956336[6] = 0;
   out_4143405018419956336[7] = 0;
   out_4143405018419956336[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_1448822998998473964) {
  err_fun(nom_x, delta_x, out_1448822998998473964);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2803394903321302341) {
  inv_err_fun(nom_x, true_x, out_2803394903321302341);
}
void car_H_mod_fun(double *state, double *out_8083424101222547813) {
  H_mod_fun(state, out_8083424101222547813);
}
void car_f_fun(double *state, double dt, double *out_2859685047189495985) {
  f_fun(state,  dt, out_2859685047189495985);
}
void car_F_fun(double *state, double dt, double *out_2361948659728611371) {
  F_fun(state,  dt, out_2361948659728611371);
}
void car_h_25(double *state, double *unused, double *out_4094106748208449073) {
  h_25(state, unused, out_4094106748208449073);
}
void car_H_25(double *state, double *unused, double *out_4174050980296916764) {
  H_25(state, unused, out_4174050980296916764);
}
void car_h_24(double *state, double *unused, double *out_3530061735652668723) {
  h_24(state, unused, out_3530061735652668723);
}
void car_H_24(double *state, double *unused, double *out_8994372484952905027) {
  H_24(state, unused, out_8994372484952905027);
}
void car_h_30(double *state, double *unused, double *out_5544738140837332240) {
  h_30(state, unused, out_5544738140837332240);
}
void car_H_30(double *state, double *unused, double *out_4303389927440156834) {
  H_30(state, unused, out_4303389927440156834);
}
void car_h_26(double *state, double *unused, double *out_2732186037567478857) {
  h_26(state, unused, out_2732186037567478857);
}
void car_H_26(double *state, double *unused, double *out_7915554299170972988) {
  H_26(state, unused, out_7915554299170972988);
}
void car_h_27(double *state, double *unused, double *out_7349874621708036764) {
  h_27(state, unused, out_7349874621708036764);
}
void car_H_27(double *state, double *unused, double *out_6478153239240581745) {
  H_27(state, unused, out_6478153239240581745);
}
void car_h_29(double *state, double *unused, double *out_543064873176486595) {
  h_29(state, unused, out_543064873176486595);
}
void car_H_29(double *state, double *unused, double *out_3793158583125764650) {
  H_29(state, unused, out_3793158583125764650);
}
void car_h_28(double *state, double *unused, double *out_4313088570263704079) {
  h_28(state, unused, out_4313088570263704079);
}
void car_H_28(double *state, double *unused, double *out_6227885694544806527) {
  H_28(state, unused, out_6227885694544806527);
}
void car_h_31(double *state, double *unused, double *out_2073007291762685230) {
  h_31(state, unused, out_2073007291762685230);
}
void car_H_31(double *state, double *unused, double *out_4143405018419956336) {
  H_31(state, unused, out_4143405018419956336);
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
