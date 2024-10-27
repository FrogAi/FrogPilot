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
void err_fun(double *nom_x, double *delta_x, double *out_356713989538978140) {
   out_356713989538978140[0] = delta_x[0] + nom_x[0];
   out_356713989538978140[1] = delta_x[1] + nom_x[1];
   out_356713989538978140[2] = delta_x[2] + nom_x[2];
   out_356713989538978140[3] = delta_x[3] + nom_x[3];
   out_356713989538978140[4] = delta_x[4] + nom_x[4];
   out_356713989538978140[5] = delta_x[5] + nom_x[5];
   out_356713989538978140[6] = delta_x[6] + nom_x[6];
   out_356713989538978140[7] = delta_x[7] + nom_x[7];
   out_356713989538978140[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_1921624891084835294) {
   out_1921624891084835294[0] = -nom_x[0] + true_x[0];
   out_1921624891084835294[1] = -nom_x[1] + true_x[1];
   out_1921624891084835294[2] = -nom_x[2] + true_x[2];
   out_1921624891084835294[3] = -nom_x[3] + true_x[3];
   out_1921624891084835294[4] = -nom_x[4] + true_x[4];
   out_1921624891084835294[5] = -nom_x[5] + true_x[5];
   out_1921624891084835294[6] = -nom_x[6] + true_x[6];
   out_1921624891084835294[7] = -nom_x[7] + true_x[7];
   out_1921624891084835294[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_131422163375330076) {
   out_131422163375330076[0] = 1.0;
   out_131422163375330076[1] = 0;
   out_131422163375330076[2] = 0;
   out_131422163375330076[3] = 0;
   out_131422163375330076[4] = 0;
   out_131422163375330076[5] = 0;
   out_131422163375330076[6] = 0;
   out_131422163375330076[7] = 0;
   out_131422163375330076[8] = 0;
   out_131422163375330076[9] = 0;
   out_131422163375330076[10] = 1.0;
   out_131422163375330076[11] = 0;
   out_131422163375330076[12] = 0;
   out_131422163375330076[13] = 0;
   out_131422163375330076[14] = 0;
   out_131422163375330076[15] = 0;
   out_131422163375330076[16] = 0;
   out_131422163375330076[17] = 0;
   out_131422163375330076[18] = 0;
   out_131422163375330076[19] = 0;
   out_131422163375330076[20] = 1.0;
   out_131422163375330076[21] = 0;
   out_131422163375330076[22] = 0;
   out_131422163375330076[23] = 0;
   out_131422163375330076[24] = 0;
   out_131422163375330076[25] = 0;
   out_131422163375330076[26] = 0;
   out_131422163375330076[27] = 0;
   out_131422163375330076[28] = 0;
   out_131422163375330076[29] = 0;
   out_131422163375330076[30] = 1.0;
   out_131422163375330076[31] = 0;
   out_131422163375330076[32] = 0;
   out_131422163375330076[33] = 0;
   out_131422163375330076[34] = 0;
   out_131422163375330076[35] = 0;
   out_131422163375330076[36] = 0;
   out_131422163375330076[37] = 0;
   out_131422163375330076[38] = 0;
   out_131422163375330076[39] = 0;
   out_131422163375330076[40] = 1.0;
   out_131422163375330076[41] = 0;
   out_131422163375330076[42] = 0;
   out_131422163375330076[43] = 0;
   out_131422163375330076[44] = 0;
   out_131422163375330076[45] = 0;
   out_131422163375330076[46] = 0;
   out_131422163375330076[47] = 0;
   out_131422163375330076[48] = 0;
   out_131422163375330076[49] = 0;
   out_131422163375330076[50] = 1.0;
   out_131422163375330076[51] = 0;
   out_131422163375330076[52] = 0;
   out_131422163375330076[53] = 0;
   out_131422163375330076[54] = 0;
   out_131422163375330076[55] = 0;
   out_131422163375330076[56] = 0;
   out_131422163375330076[57] = 0;
   out_131422163375330076[58] = 0;
   out_131422163375330076[59] = 0;
   out_131422163375330076[60] = 1.0;
   out_131422163375330076[61] = 0;
   out_131422163375330076[62] = 0;
   out_131422163375330076[63] = 0;
   out_131422163375330076[64] = 0;
   out_131422163375330076[65] = 0;
   out_131422163375330076[66] = 0;
   out_131422163375330076[67] = 0;
   out_131422163375330076[68] = 0;
   out_131422163375330076[69] = 0;
   out_131422163375330076[70] = 1.0;
   out_131422163375330076[71] = 0;
   out_131422163375330076[72] = 0;
   out_131422163375330076[73] = 0;
   out_131422163375330076[74] = 0;
   out_131422163375330076[75] = 0;
   out_131422163375330076[76] = 0;
   out_131422163375330076[77] = 0;
   out_131422163375330076[78] = 0;
   out_131422163375330076[79] = 0;
   out_131422163375330076[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_989521806814443679) {
   out_989521806814443679[0] = state[0];
   out_989521806814443679[1] = state[1];
   out_989521806814443679[2] = state[2];
   out_989521806814443679[3] = state[3];
   out_989521806814443679[4] = state[4];
   out_989521806814443679[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_989521806814443679[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_989521806814443679[7] = state[7];
   out_989521806814443679[8] = state[8];
}
void F_fun(double *state, double dt, double *out_8780304336411573603) {
   out_8780304336411573603[0] = 1;
   out_8780304336411573603[1] = 0;
   out_8780304336411573603[2] = 0;
   out_8780304336411573603[3] = 0;
   out_8780304336411573603[4] = 0;
   out_8780304336411573603[5] = 0;
   out_8780304336411573603[6] = 0;
   out_8780304336411573603[7] = 0;
   out_8780304336411573603[8] = 0;
   out_8780304336411573603[9] = 0;
   out_8780304336411573603[10] = 1;
   out_8780304336411573603[11] = 0;
   out_8780304336411573603[12] = 0;
   out_8780304336411573603[13] = 0;
   out_8780304336411573603[14] = 0;
   out_8780304336411573603[15] = 0;
   out_8780304336411573603[16] = 0;
   out_8780304336411573603[17] = 0;
   out_8780304336411573603[18] = 0;
   out_8780304336411573603[19] = 0;
   out_8780304336411573603[20] = 1;
   out_8780304336411573603[21] = 0;
   out_8780304336411573603[22] = 0;
   out_8780304336411573603[23] = 0;
   out_8780304336411573603[24] = 0;
   out_8780304336411573603[25] = 0;
   out_8780304336411573603[26] = 0;
   out_8780304336411573603[27] = 0;
   out_8780304336411573603[28] = 0;
   out_8780304336411573603[29] = 0;
   out_8780304336411573603[30] = 1;
   out_8780304336411573603[31] = 0;
   out_8780304336411573603[32] = 0;
   out_8780304336411573603[33] = 0;
   out_8780304336411573603[34] = 0;
   out_8780304336411573603[35] = 0;
   out_8780304336411573603[36] = 0;
   out_8780304336411573603[37] = 0;
   out_8780304336411573603[38] = 0;
   out_8780304336411573603[39] = 0;
   out_8780304336411573603[40] = 1;
   out_8780304336411573603[41] = 0;
   out_8780304336411573603[42] = 0;
   out_8780304336411573603[43] = 0;
   out_8780304336411573603[44] = 0;
   out_8780304336411573603[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_8780304336411573603[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_8780304336411573603[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8780304336411573603[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8780304336411573603[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_8780304336411573603[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_8780304336411573603[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_8780304336411573603[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_8780304336411573603[53] = -9.8000000000000007*dt;
   out_8780304336411573603[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_8780304336411573603[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_8780304336411573603[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8780304336411573603[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8780304336411573603[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_8780304336411573603[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_8780304336411573603[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_8780304336411573603[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8780304336411573603[62] = 0;
   out_8780304336411573603[63] = 0;
   out_8780304336411573603[64] = 0;
   out_8780304336411573603[65] = 0;
   out_8780304336411573603[66] = 0;
   out_8780304336411573603[67] = 0;
   out_8780304336411573603[68] = 0;
   out_8780304336411573603[69] = 0;
   out_8780304336411573603[70] = 1;
   out_8780304336411573603[71] = 0;
   out_8780304336411573603[72] = 0;
   out_8780304336411573603[73] = 0;
   out_8780304336411573603[74] = 0;
   out_8780304336411573603[75] = 0;
   out_8780304336411573603[76] = 0;
   out_8780304336411573603[77] = 0;
   out_8780304336411573603[78] = 0;
   out_8780304336411573603[79] = 0;
   out_8780304336411573603[80] = 1;
}
void h_25(double *state, double *unused, double *out_3136114798976930003) {
   out_3136114798976930003[0] = state[6];
}
void H_25(double *state, double *unused, double *out_6350009131572229968) {
   out_6350009131572229968[0] = 0;
   out_6350009131572229968[1] = 0;
   out_6350009131572229968[2] = 0;
   out_6350009131572229968[3] = 0;
   out_6350009131572229968[4] = 0;
   out_6350009131572229968[5] = 0;
   out_6350009131572229968[6] = 1;
   out_6350009131572229968[7] = 0;
   out_6350009131572229968[8] = 0;
}
void h_24(double *state, double *unused, double *out_6724622281289421581) {
   out_6724622281289421581[0] = state[4];
   out_6724622281289421581[1] = state[5];
}
void H_24(double *state, double *unused, double *out_8682443429994267188) {
   out_8682443429994267188[0] = 0;
   out_8682443429994267188[1] = 0;
   out_8682443429994267188[2] = 0;
   out_8682443429994267188[3] = 0;
   out_8682443429994267188[4] = 1;
   out_8682443429994267188[5] = 0;
   out_8682443429994267188[6] = 0;
   out_8682443429994267188[7] = 0;
   out_8682443429994267188[8] = 0;
   out_8682443429994267188[9] = 0;
   out_8682443429994267188[10] = 0;
   out_8682443429994267188[11] = 0;
   out_8682443429994267188[12] = 0;
   out_8682443429994267188[13] = 0;
   out_8682443429994267188[14] = 1;
   out_8682443429994267188[15] = 0;
   out_8682443429994267188[16] = 0;
   out_8682443429994267188[17] = 0;
}
void h_30(double *state, double *unused, double *out_2978928130625800858) {
   out_2978928130625800858[0] = state[4];
}
void H_30(double *state, double *unused, double *out_7569038612009713450) {
   out_7569038612009713450[0] = 0;
   out_7569038612009713450[1] = 0;
   out_7569038612009713450[2] = 0;
   out_7569038612009713450[3] = 0;
   out_7569038612009713450[4] = 1;
   out_7569038612009713450[5] = 0;
   out_7569038612009713450[6] = 0;
   out_7569038612009713450[7] = 0;
   out_7569038612009713450[8] = 0;
}
void h_26(double *state, double *unused, double *out_9068774425669901612) {
   out_9068774425669901612[0] = state[7];
}
void H_26(double *state, double *unused, double *out_8355231623263265424) {
   out_8355231623263265424[0] = 0;
   out_8355231623263265424[1] = 0;
   out_8355231623263265424[2] = 0;
   out_8355231623263265424[3] = 0;
   out_8355231623263265424[4] = 0;
   out_8355231623263265424[5] = 0;
   out_8355231623263265424[6] = 0;
   out_8355231623263265424[7] = 1;
   out_8355231623263265424[8] = 0;
}
void h_27(double *state, double *unused, double *out_99328747532597318) {
   out_99328747532597318[0] = state[3];
}
void H_27(double *state, double *unused, double *out_8654111390515894949) {
   out_8654111390515894949[0] = 0;
   out_8654111390515894949[1] = 0;
   out_8654111390515894949[2] = 0;
   out_8654111390515894949[3] = 1;
   out_8654111390515894949[4] = 0;
   out_8654111390515894949[5] = 0;
   out_8654111390515894949[6] = 0;
   out_8654111390515894949[7] = 0;
   out_8654111390515894949[8] = 0;
}
void h_29(double *state, double *unused, double *out_7980601398286646503) {
   out_7980601398286646503[0] = state[1];
}
void H_29(double *state, double *unused, double *out_8079269956324105634) {
   out_8079269956324105634[0] = 0;
   out_8079269956324105634[1] = 1;
   out_8079269956324105634[2] = 0;
   out_8079269956324105634[3] = 0;
   out_8079269956324105634[4] = 0;
   out_8079269956324105634[5] = 0;
   out_8079269956324105634[6] = 0;
   out_8079269956324105634[7] = 0;
   out_8079269956324105634[8] = 0;
}
void h_28(double *state, double *unused, double *out_844181105369065117) {
   out_844181105369065117[0] = state[0];
}
void H_28(double *state, double *unused, double *out_8403843845820119731) {
   out_8403843845820119731[0] = 1;
   out_8403843845820119731[1] = 0;
   out_8403843845820119731[2] = 0;
   out_8403843845820119731[3] = 0;
   out_8403843845820119731[4] = 0;
   out_8403843845820119731[5] = 0;
   out_8403843845820119731[6] = 0;
   out_8403843845820119731[7] = 0;
   out_8403843845820119731[8] = 0;
}
void h_31(double *state, double *unused, double *out_3451713127499365160) {
   out_3451713127499365160[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7729023521029913948) {
   out_7729023521029913948[0] = 0;
   out_7729023521029913948[1] = 0;
   out_7729023521029913948[2] = 0;
   out_7729023521029913948[3] = 0;
   out_7729023521029913948[4] = 0;
   out_7729023521029913948[5] = 0;
   out_7729023521029913948[6] = 0;
   out_7729023521029913948[7] = 0;
   out_7729023521029913948[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_356713989538978140) {
  err_fun(nom_x, delta_x, out_356713989538978140);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1921624891084835294) {
  inv_err_fun(nom_x, true_x, out_1921624891084835294);
}
void car_H_mod_fun(double *state, double *out_131422163375330076) {
  H_mod_fun(state, out_131422163375330076);
}
void car_f_fun(double *state, double dt, double *out_989521806814443679) {
  f_fun(state,  dt, out_989521806814443679);
}
void car_F_fun(double *state, double dt, double *out_8780304336411573603) {
  F_fun(state,  dt, out_8780304336411573603);
}
void car_h_25(double *state, double *unused, double *out_3136114798976930003) {
  h_25(state, unused, out_3136114798976930003);
}
void car_H_25(double *state, double *unused, double *out_6350009131572229968) {
  H_25(state, unused, out_6350009131572229968);
}
void car_h_24(double *state, double *unused, double *out_6724622281289421581) {
  h_24(state, unused, out_6724622281289421581);
}
void car_H_24(double *state, double *unused, double *out_8682443429994267188) {
  H_24(state, unused, out_8682443429994267188);
}
void car_h_30(double *state, double *unused, double *out_2978928130625800858) {
  h_30(state, unused, out_2978928130625800858);
}
void car_H_30(double *state, double *unused, double *out_7569038612009713450) {
  H_30(state, unused, out_7569038612009713450);
}
void car_h_26(double *state, double *unused, double *out_9068774425669901612) {
  h_26(state, unused, out_9068774425669901612);
}
void car_H_26(double *state, double *unused, double *out_8355231623263265424) {
  H_26(state, unused, out_8355231623263265424);
}
void car_h_27(double *state, double *unused, double *out_99328747532597318) {
  h_27(state, unused, out_99328747532597318);
}
void car_H_27(double *state, double *unused, double *out_8654111390515894949) {
  H_27(state, unused, out_8654111390515894949);
}
void car_h_29(double *state, double *unused, double *out_7980601398286646503) {
  h_29(state, unused, out_7980601398286646503);
}
void car_H_29(double *state, double *unused, double *out_8079269956324105634) {
  H_29(state, unused, out_8079269956324105634);
}
void car_h_28(double *state, double *unused, double *out_844181105369065117) {
  h_28(state, unused, out_844181105369065117);
}
void car_H_28(double *state, double *unused, double *out_8403843845820119731) {
  H_28(state, unused, out_8403843845820119731);
}
void car_h_31(double *state, double *unused, double *out_3451713127499365160) {
  h_31(state, unused, out_3451713127499365160);
}
void car_H_31(double *state, double *unused, double *out_7729023521029913948) {
  H_31(state, unused, out_7729023521029913948);
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
