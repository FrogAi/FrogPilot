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
void err_fun(double *nom_x, double *delta_x, double *out_727365234126363634) {
   out_727365234126363634[0] = delta_x[0] + nom_x[0];
   out_727365234126363634[1] = delta_x[1] + nom_x[1];
   out_727365234126363634[2] = delta_x[2] + nom_x[2];
   out_727365234126363634[3] = delta_x[3] + nom_x[3];
   out_727365234126363634[4] = delta_x[4] + nom_x[4];
   out_727365234126363634[5] = delta_x[5] + nom_x[5];
   out_727365234126363634[6] = delta_x[6] + nom_x[6];
   out_727365234126363634[7] = delta_x[7] + nom_x[7];
   out_727365234126363634[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_2091208360785955533) {
   out_2091208360785955533[0] = -nom_x[0] + true_x[0];
   out_2091208360785955533[1] = -nom_x[1] + true_x[1];
   out_2091208360785955533[2] = -nom_x[2] + true_x[2];
   out_2091208360785955533[3] = -nom_x[3] + true_x[3];
   out_2091208360785955533[4] = -nom_x[4] + true_x[4];
   out_2091208360785955533[5] = -nom_x[5] + true_x[5];
   out_2091208360785955533[6] = -nom_x[6] + true_x[6];
   out_2091208360785955533[7] = -nom_x[7] + true_x[7];
   out_2091208360785955533[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_7318059712744546467) {
   out_7318059712744546467[0] = 1.0;
   out_7318059712744546467[1] = 0;
   out_7318059712744546467[2] = 0;
   out_7318059712744546467[3] = 0;
   out_7318059712744546467[4] = 0;
   out_7318059712744546467[5] = 0;
   out_7318059712744546467[6] = 0;
   out_7318059712744546467[7] = 0;
   out_7318059712744546467[8] = 0;
   out_7318059712744546467[9] = 0;
   out_7318059712744546467[10] = 1.0;
   out_7318059712744546467[11] = 0;
   out_7318059712744546467[12] = 0;
   out_7318059712744546467[13] = 0;
   out_7318059712744546467[14] = 0;
   out_7318059712744546467[15] = 0;
   out_7318059712744546467[16] = 0;
   out_7318059712744546467[17] = 0;
   out_7318059712744546467[18] = 0;
   out_7318059712744546467[19] = 0;
   out_7318059712744546467[20] = 1.0;
   out_7318059712744546467[21] = 0;
   out_7318059712744546467[22] = 0;
   out_7318059712744546467[23] = 0;
   out_7318059712744546467[24] = 0;
   out_7318059712744546467[25] = 0;
   out_7318059712744546467[26] = 0;
   out_7318059712744546467[27] = 0;
   out_7318059712744546467[28] = 0;
   out_7318059712744546467[29] = 0;
   out_7318059712744546467[30] = 1.0;
   out_7318059712744546467[31] = 0;
   out_7318059712744546467[32] = 0;
   out_7318059712744546467[33] = 0;
   out_7318059712744546467[34] = 0;
   out_7318059712744546467[35] = 0;
   out_7318059712744546467[36] = 0;
   out_7318059712744546467[37] = 0;
   out_7318059712744546467[38] = 0;
   out_7318059712744546467[39] = 0;
   out_7318059712744546467[40] = 1.0;
   out_7318059712744546467[41] = 0;
   out_7318059712744546467[42] = 0;
   out_7318059712744546467[43] = 0;
   out_7318059712744546467[44] = 0;
   out_7318059712744546467[45] = 0;
   out_7318059712744546467[46] = 0;
   out_7318059712744546467[47] = 0;
   out_7318059712744546467[48] = 0;
   out_7318059712744546467[49] = 0;
   out_7318059712744546467[50] = 1.0;
   out_7318059712744546467[51] = 0;
   out_7318059712744546467[52] = 0;
   out_7318059712744546467[53] = 0;
   out_7318059712744546467[54] = 0;
   out_7318059712744546467[55] = 0;
   out_7318059712744546467[56] = 0;
   out_7318059712744546467[57] = 0;
   out_7318059712744546467[58] = 0;
   out_7318059712744546467[59] = 0;
   out_7318059712744546467[60] = 1.0;
   out_7318059712744546467[61] = 0;
   out_7318059712744546467[62] = 0;
   out_7318059712744546467[63] = 0;
   out_7318059712744546467[64] = 0;
   out_7318059712744546467[65] = 0;
   out_7318059712744546467[66] = 0;
   out_7318059712744546467[67] = 0;
   out_7318059712744546467[68] = 0;
   out_7318059712744546467[69] = 0;
   out_7318059712744546467[70] = 1.0;
   out_7318059712744546467[71] = 0;
   out_7318059712744546467[72] = 0;
   out_7318059712744546467[73] = 0;
   out_7318059712744546467[74] = 0;
   out_7318059712744546467[75] = 0;
   out_7318059712744546467[76] = 0;
   out_7318059712744546467[77] = 0;
   out_7318059712744546467[78] = 0;
   out_7318059712744546467[79] = 0;
   out_7318059712744546467[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2254883399827986092) {
   out_2254883399827986092[0] = state[0];
   out_2254883399827986092[1] = state[1];
   out_2254883399827986092[2] = state[2];
   out_2254883399827986092[3] = state[3];
   out_2254883399827986092[4] = state[4];
   out_2254883399827986092[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2254883399827986092[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2254883399827986092[7] = state[7];
   out_2254883399827986092[8] = state[8];
}
void F_fun(double *state, double dt, double *out_117785442133545594) {
   out_117785442133545594[0] = 1;
   out_117785442133545594[1] = 0;
   out_117785442133545594[2] = 0;
   out_117785442133545594[3] = 0;
   out_117785442133545594[4] = 0;
   out_117785442133545594[5] = 0;
   out_117785442133545594[6] = 0;
   out_117785442133545594[7] = 0;
   out_117785442133545594[8] = 0;
   out_117785442133545594[9] = 0;
   out_117785442133545594[10] = 1;
   out_117785442133545594[11] = 0;
   out_117785442133545594[12] = 0;
   out_117785442133545594[13] = 0;
   out_117785442133545594[14] = 0;
   out_117785442133545594[15] = 0;
   out_117785442133545594[16] = 0;
   out_117785442133545594[17] = 0;
   out_117785442133545594[18] = 0;
   out_117785442133545594[19] = 0;
   out_117785442133545594[20] = 1;
   out_117785442133545594[21] = 0;
   out_117785442133545594[22] = 0;
   out_117785442133545594[23] = 0;
   out_117785442133545594[24] = 0;
   out_117785442133545594[25] = 0;
   out_117785442133545594[26] = 0;
   out_117785442133545594[27] = 0;
   out_117785442133545594[28] = 0;
   out_117785442133545594[29] = 0;
   out_117785442133545594[30] = 1;
   out_117785442133545594[31] = 0;
   out_117785442133545594[32] = 0;
   out_117785442133545594[33] = 0;
   out_117785442133545594[34] = 0;
   out_117785442133545594[35] = 0;
   out_117785442133545594[36] = 0;
   out_117785442133545594[37] = 0;
   out_117785442133545594[38] = 0;
   out_117785442133545594[39] = 0;
   out_117785442133545594[40] = 1;
   out_117785442133545594[41] = 0;
   out_117785442133545594[42] = 0;
   out_117785442133545594[43] = 0;
   out_117785442133545594[44] = 0;
   out_117785442133545594[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_117785442133545594[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_117785442133545594[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_117785442133545594[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_117785442133545594[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_117785442133545594[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_117785442133545594[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_117785442133545594[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_117785442133545594[53] = -9.8000000000000007*dt;
   out_117785442133545594[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_117785442133545594[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_117785442133545594[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_117785442133545594[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_117785442133545594[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_117785442133545594[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_117785442133545594[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_117785442133545594[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_117785442133545594[62] = 0;
   out_117785442133545594[63] = 0;
   out_117785442133545594[64] = 0;
   out_117785442133545594[65] = 0;
   out_117785442133545594[66] = 0;
   out_117785442133545594[67] = 0;
   out_117785442133545594[68] = 0;
   out_117785442133545594[69] = 0;
   out_117785442133545594[70] = 1;
   out_117785442133545594[71] = 0;
   out_117785442133545594[72] = 0;
   out_117785442133545594[73] = 0;
   out_117785442133545594[74] = 0;
   out_117785442133545594[75] = 0;
   out_117785442133545594[76] = 0;
   out_117785442133545594[77] = 0;
   out_117785442133545594[78] = 0;
   out_117785442133545594[79] = 0;
   out_117785442133545594[80] = 1;
}
void h_25(double *state, double *unused, double *out_7943443423127701256) {
   out_7943443423127701256[0] = state[6];
}
void H_25(double *state, double *unused, double *out_346504727667476065) {
   out_346504727667476065[0] = 0;
   out_346504727667476065[1] = 0;
   out_346504727667476065[2] = 0;
   out_346504727667476065[3] = 0;
   out_346504727667476065[4] = 0;
   out_346504727667476065[5] = 0;
   out_346504727667476065[6] = 1;
   out_346504727667476065[7] = 0;
   out_346504727667476065[8] = 0;
}
void h_24(double *state, double *unused, double *out_4059367817167151369) {
   out_4059367817167151369[0] = state[4];
   out_4059367817167151369[1] = state[5];
}
void H_24(double *state, double *unused, double *out_2602352802791293694) {
   out_2602352802791293694[0] = 0;
   out_2602352802791293694[1] = 0;
   out_2602352802791293694[2] = 0;
   out_2602352802791293694[3] = 0;
   out_2602352802791293694[4] = 1;
   out_2602352802791293694[5] = 0;
   out_2602352802791293694[6] = 0;
   out_2602352802791293694[7] = 0;
   out_2602352802791293694[8] = 0;
   out_2602352802791293694[9] = 0;
   out_2602352802791293694[10] = 0;
   out_2602352802791293694[11] = 0;
   out_2602352802791293694[12] = 0;
   out_2602352802791293694[13] = 0;
   out_2602352802791293694[14] = 1;
   out_2602352802791293694[15] = 0;
   out_2602352802791293694[16] = 0;
   out_2602352802791293694[17] = 0;
}
void h_30(double *state, double *unused, double *out_4081759946699199604) {
   out_4081759946699199604[0] = state[4];
}
void H_30(double *state, double *unused, double *out_4181191602460132133) {
   out_4181191602460132133[0] = 0;
   out_4181191602460132133[1] = 0;
   out_4181191602460132133[2] = 0;
   out_4181191602460132133[3] = 0;
   out_4181191602460132133[4] = 1;
   out_4181191602460132133[5] = 0;
   out_4181191602460132133[6] = 0;
   out_4181191602460132133[7] = 0;
   out_4181191602460132133[8] = 0;
}
void h_26(double *state, double *unused, double *out_2391312111963518644) {
   out_2391312111963518644[0] = state[7];
}
void H_26(double *state, double *unused, double *out_3394998591206580159) {
   out_3394998591206580159[0] = 0;
   out_3394998591206580159[1] = 0;
   out_3394998591206580159[2] = 0;
   out_3394998591206580159[3] = 0;
   out_3394998591206580159[4] = 0;
   out_3394998591206580159[5] = 0;
   out_3394998591206580159[6] = 0;
   out_3394998591206580159[7] = 1;
   out_3394998591206580159[8] = 0;
}
void h_27(double *state, double *unused, double *out_1521331478575009595) {
   out_1521331478575009595[0] = state[3];
}
void H_27(double *state, double *unused, double *out_6355954914260557044) {
   out_6355954914260557044[0] = 0;
   out_6355954914260557044[1] = 0;
   out_6355954914260557044[2] = 0;
   out_6355954914260557044[3] = 1;
   out_6355954914260557044[4] = 0;
   out_6355954914260557044[5] = 0;
   out_6355954914260557044[6] = 0;
   out_6355954914260557044[7] = 0;
   out_6355954914260557044[8] = 0;
}
void h_29(double *state, double *unused, double *out_1666851302795664566) {
   out_1666851302795664566[0] = state[1];
}
void H_29(double *state, double *unused, double *out_3670960258145739949) {
   out_3670960258145739949[0] = 0;
   out_3670960258145739949[1] = 1;
   out_3670960258145739949[2] = 0;
   out_3670960258145739949[3] = 0;
   out_3670960258145739949[4] = 0;
   out_3670960258145739949[5] = 0;
   out_3670960258145739949[6] = 0;
   out_3670960258145739949[7] = 0;
   out_3670960258145739949[8] = 0;
}
void h_28(double *state, double *unused, double *out_6523004746235855240) {
   out_6523004746235855240[0] = state[0];
}
void H_28(double *state, double *unused, double *out_1707329986580413698) {
   out_1707329986580413698[0] = 1;
   out_1707329986580413698[1] = 0;
   out_1707329986580413698[2] = 0;
   out_1707329986580413698[3] = 0;
   out_1707329986580413698[4] = 0;
   out_1707329986580413698[5] = 0;
   out_1707329986580413698[6] = 0;
   out_1707329986580413698[7] = 0;
   out_1707329986580413698[8] = 0;
}
void h_31(double *state, double *unused, double *out_7117791317339858400) {
   out_7117791317339858400[0] = state[8];
}
void H_31(double *state, double *unused, double *out_4021206693439931635) {
   out_4021206693439931635[0] = 0;
   out_4021206693439931635[1] = 0;
   out_4021206693439931635[2] = 0;
   out_4021206693439931635[3] = 0;
   out_4021206693439931635[4] = 0;
   out_4021206693439931635[5] = 0;
   out_4021206693439931635[6] = 0;
   out_4021206693439931635[7] = 0;
   out_4021206693439931635[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_727365234126363634) {
  err_fun(nom_x, delta_x, out_727365234126363634);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2091208360785955533) {
  inv_err_fun(nom_x, true_x, out_2091208360785955533);
}
void car_H_mod_fun(double *state, double *out_7318059712744546467) {
  H_mod_fun(state, out_7318059712744546467);
}
void car_f_fun(double *state, double dt, double *out_2254883399827986092) {
  f_fun(state,  dt, out_2254883399827986092);
}
void car_F_fun(double *state, double dt, double *out_117785442133545594) {
  F_fun(state,  dt, out_117785442133545594);
}
void car_h_25(double *state, double *unused, double *out_7943443423127701256) {
  h_25(state, unused, out_7943443423127701256);
}
void car_H_25(double *state, double *unused, double *out_346504727667476065) {
  H_25(state, unused, out_346504727667476065);
}
void car_h_24(double *state, double *unused, double *out_4059367817167151369) {
  h_24(state, unused, out_4059367817167151369);
}
void car_H_24(double *state, double *unused, double *out_2602352802791293694) {
  H_24(state, unused, out_2602352802791293694);
}
void car_h_30(double *state, double *unused, double *out_4081759946699199604) {
  h_30(state, unused, out_4081759946699199604);
}
void car_H_30(double *state, double *unused, double *out_4181191602460132133) {
  H_30(state, unused, out_4181191602460132133);
}
void car_h_26(double *state, double *unused, double *out_2391312111963518644) {
  h_26(state, unused, out_2391312111963518644);
}
void car_H_26(double *state, double *unused, double *out_3394998591206580159) {
  H_26(state, unused, out_3394998591206580159);
}
void car_h_27(double *state, double *unused, double *out_1521331478575009595) {
  h_27(state, unused, out_1521331478575009595);
}
void car_H_27(double *state, double *unused, double *out_6355954914260557044) {
  H_27(state, unused, out_6355954914260557044);
}
void car_h_29(double *state, double *unused, double *out_1666851302795664566) {
  h_29(state, unused, out_1666851302795664566);
}
void car_H_29(double *state, double *unused, double *out_3670960258145739949) {
  H_29(state, unused, out_3670960258145739949);
}
void car_h_28(double *state, double *unused, double *out_6523004746235855240) {
  h_28(state, unused, out_6523004746235855240);
}
void car_H_28(double *state, double *unused, double *out_1707329986580413698) {
  H_28(state, unused, out_1707329986580413698);
}
void car_h_31(double *state, double *unused, double *out_7117791317339858400) {
  h_31(state, unused, out_7117791317339858400);
}
void car_H_31(double *state, double *unused, double *out_4021206693439931635) {
  H_31(state, unused, out_4021206693439931635);
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
