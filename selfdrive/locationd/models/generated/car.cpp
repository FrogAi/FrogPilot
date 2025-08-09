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
void err_fun(double *nom_x, double *delta_x, double *out_3630519523719827664) {
   out_3630519523719827664[0] = delta_x[0] + nom_x[0];
   out_3630519523719827664[1] = delta_x[1] + nom_x[1];
   out_3630519523719827664[2] = delta_x[2] + nom_x[2];
   out_3630519523719827664[3] = delta_x[3] + nom_x[3];
   out_3630519523719827664[4] = delta_x[4] + nom_x[4];
   out_3630519523719827664[5] = delta_x[5] + nom_x[5];
   out_3630519523719827664[6] = delta_x[6] + nom_x[6];
   out_3630519523719827664[7] = delta_x[7] + nom_x[7];
   out_3630519523719827664[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_634137438963888633) {
   out_634137438963888633[0] = -nom_x[0] + true_x[0];
   out_634137438963888633[1] = -nom_x[1] + true_x[1];
   out_634137438963888633[2] = -nom_x[2] + true_x[2];
   out_634137438963888633[3] = -nom_x[3] + true_x[3];
   out_634137438963888633[4] = -nom_x[4] + true_x[4];
   out_634137438963888633[5] = -nom_x[5] + true_x[5];
   out_634137438963888633[6] = -nom_x[6] + true_x[6];
   out_634137438963888633[7] = -nom_x[7] + true_x[7];
   out_634137438963888633[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_4950664487485516056) {
   out_4950664487485516056[0] = 1.0;
   out_4950664487485516056[1] = 0;
   out_4950664487485516056[2] = 0;
   out_4950664487485516056[3] = 0;
   out_4950664487485516056[4] = 0;
   out_4950664487485516056[5] = 0;
   out_4950664487485516056[6] = 0;
   out_4950664487485516056[7] = 0;
   out_4950664487485516056[8] = 0;
   out_4950664487485516056[9] = 0;
   out_4950664487485516056[10] = 1.0;
   out_4950664487485516056[11] = 0;
   out_4950664487485516056[12] = 0;
   out_4950664487485516056[13] = 0;
   out_4950664487485516056[14] = 0;
   out_4950664487485516056[15] = 0;
   out_4950664487485516056[16] = 0;
   out_4950664487485516056[17] = 0;
   out_4950664487485516056[18] = 0;
   out_4950664487485516056[19] = 0;
   out_4950664487485516056[20] = 1.0;
   out_4950664487485516056[21] = 0;
   out_4950664487485516056[22] = 0;
   out_4950664487485516056[23] = 0;
   out_4950664487485516056[24] = 0;
   out_4950664487485516056[25] = 0;
   out_4950664487485516056[26] = 0;
   out_4950664487485516056[27] = 0;
   out_4950664487485516056[28] = 0;
   out_4950664487485516056[29] = 0;
   out_4950664487485516056[30] = 1.0;
   out_4950664487485516056[31] = 0;
   out_4950664487485516056[32] = 0;
   out_4950664487485516056[33] = 0;
   out_4950664487485516056[34] = 0;
   out_4950664487485516056[35] = 0;
   out_4950664487485516056[36] = 0;
   out_4950664487485516056[37] = 0;
   out_4950664487485516056[38] = 0;
   out_4950664487485516056[39] = 0;
   out_4950664487485516056[40] = 1.0;
   out_4950664487485516056[41] = 0;
   out_4950664487485516056[42] = 0;
   out_4950664487485516056[43] = 0;
   out_4950664487485516056[44] = 0;
   out_4950664487485516056[45] = 0;
   out_4950664487485516056[46] = 0;
   out_4950664487485516056[47] = 0;
   out_4950664487485516056[48] = 0;
   out_4950664487485516056[49] = 0;
   out_4950664487485516056[50] = 1.0;
   out_4950664487485516056[51] = 0;
   out_4950664487485516056[52] = 0;
   out_4950664487485516056[53] = 0;
   out_4950664487485516056[54] = 0;
   out_4950664487485516056[55] = 0;
   out_4950664487485516056[56] = 0;
   out_4950664487485516056[57] = 0;
   out_4950664487485516056[58] = 0;
   out_4950664487485516056[59] = 0;
   out_4950664487485516056[60] = 1.0;
   out_4950664487485516056[61] = 0;
   out_4950664487485516056[62] = 0;
   out_4950664487485516056[63] = 0;
   out_4950664487485516056[64] = 0;
   out_4950664487485516056[65] = 0;
   out_4950664487485516056[66] = 0;
   out_4950664487485516056[67] = 0;
   out_4950664487485516056[68] = 0;
   out_4950664487485516056[69] = 0;
   out_4950664487485516056[70] = 1.0;
   out_4950664487485516056[71] = 0;
   out_4950664487485516056[72] = 0;
   out_4950664487485516056[73] = 0;
   out_4950664487485516056[74] = 0;
   out_4950664487485516056[75] = 0;
   out_4950664487485516056[76] = 0;
   out_4950664487485516056[77] = 0;
   out_4950664487485516056[78] = 0;
   out_4950664487485516056[79] = 0;
   out_4950664487485516056[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_3645824172833169214) {
   out_3645824172833169214[0] = state[0];
   out_3645824172833169214[1] = state[1];
   out_3645824172833169214[2] = state[2];
   out_3645824172833169214[3] = state[3];
   out_3645824172833169214[4] = state[4];
   out_3645824172833169214[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_3645824172833169214[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_3645824172833169214[7] = state[7];
   out_3645824172833169214[8] = state[8];
}
void F_fun(double *state, double dt, double *out_7534620374241633819) {
   out_7534620374241633819[0] = 1;
   out_7534620374241633819[1] = 0;
   out_7534620374241633819[2] = 0;
   out_7534620374241633819[3] = 0;
   out_7534620374241633819[4] = 0;
   out_7534620374241633819[5] = 0;
   out_7534620374241633819[6] = 0;
   out_7534620374241633819[7] = 0;
   out_7534620374241633819[8] = 0;
   out_7534620374241633819[9] = 0;
   out_7534620374241633819[10] = 1;
   out_7534620374241633819[11] = 0;
   out_7534620374241633819[12] = 0;
   out_7534620374241633819[13] = 0;
   out_7534620374241633819[14] = 0;
   out_7534620374241633819[15] = 0;
   out_7534620374241633819[16] = 0;
   out_7534620374241633819[17] = 0;
   out_7534620374241633819[18] = 0;
   out_7534620374241633819[19] = 0;
   out_7534620374241633819[20] = 1;
   out_7534620374241633819[21] = 0;
   out_7534620374241633819[22] = 0;
   out_7534620374241633819[23] = 0;
   out_7534620374241633819[24] = 0;
   out_7534620374241633819[25] = 0;
   out_7534620374241633819[26] = 0;
   out_7534620374241633819[27] = 0;
   out_7534620374241633819[28] = 0;
   out_7534620374241633819[29] = 0;
   out_7534620374241633819[30] = 1;
   out_7534620374241633819[31] = 0;
   out_7534620374241633819[32] = 0;
   out_7534620374241633819[33] = 0;
   out_7534620374241633819[34] = 0;
   out_7534620374241633819[35] = 0;
   out_7534620374241633819[36] = 0;
   out_7534620374241633819[37] = 0;
   out_7534620374241633819[38] = 0;
   out_7534620374241633819[39] = 0;
   out_7534620374241633819[40] = 1;
   out_7534620374241633819[41] = 0;
   out_7534620374241633819[42] = 0;
   out_7534620374241633819[43] = 0;
   out_7534620374241633819[44] = 0;
   out_7534620374241633819[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_7534620374241633819[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_7534620374241633819[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7534620374241633819[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7534620374241633819[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_7534620374241633819[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_7534620374241633819[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_7534620374241633819[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_7534620374241633819[53] = -9.8000000000000007*dt;
   out_7534620374241633819[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_7534620374241633819[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_7534620374241633819[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7534620374241633819[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7534620374241633819[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_7534620374241633819[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_7534620374241633819[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_7534620374241633819[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7534620374241633819[62] = 0;
   out_7534620374241633819[63] = 0;
   out_7534620374241633819[64] = 0;
   out_7534620374241633819[65] = 0;
   out_7534620374241633819[66] = 0;
   out_7534620374241633819[67] = 0;
   out_7534620374241633819[68] = 0;
   out_7534620374241633819[69] = 0;
   out_7534620374241633819[70] = 1;
   out_7534620374241633819[71] = 0;
   out_7534620374241633819[72] = 0;
   out_7534620374241633819[73] = 0;
   out_7534620374241633819[74] = 0;
   out_7534620374241633819[75] = 0;
   out_7534620374241633819[76] = 0;
   out_7534620374241633819[77] = 0;
   out_7534620374241633819[78] = 0;
   out_7534620374241633819[79] = 0;
   out_7534620374241633819[80] = 1;
}
void h_25(double *state, double *unused, double *out_2057774082201549556) {
   out_2057774082201549556[0] = state[6];
}
void H_25(double *state, double *unused, double *out_8831157109305284721) {
   out_8831157109305284721[0] = 0;
   out_8831157109305284721[1] = 0;
   out_8831157109305284721[2] = 0;
   out_8831157109305284721[3] = 0;
   out_8831157109305284721[4] = 0;
   out_8831157109305284721[5] = 0;
   out_8831157109305284721[6] = 1;
   out_8831157109305284721[7] = 0;
   out_8831157109305284721[8] = 0;
}
void h_24(double *state, double *unused, double *out_336340693360392124) {
   out_336340693360392124[0] = state[4];
   out_336340693360392124[1] = state[5];
}
void H_24(double *state, double *unused, double *out_2255585302713766620) {
   out_2255585302713766620[0] = 0;
   out_2255585302713766620[1] = 0;
   out_2255585302713766620[2] = 0;
   out_2255585302713766620[3] = 0;
   out_2255585302713766620[4] = 1;
   out_2255585302713766620[5] = 0;
   out_2255585302713766620[6] = 0;
   out_2255585302713766620[7] = 0;
   out_2255585302713766620[8] = 0;
   out_2255585302713766620[9] = 0;
   out_2255585302713766620[10] = 0;
   out_2255585302713766620[11] = 0;
   out_2255585302713766620[12] = 0;
   out_2255585302713766620[13] = 0;
   out_2255585302713766620[14] = 1;
   out_2255585302713766620[15] = 0;
   out_2255585302713766620[16] = 0;
   out_2255585302713766620[17] = 0;
}
void h_30(double *state, double *unused, double *out_5823498568552499629) {
   out_5823498568552499629[0] = state[4];
}
void H_30(double *state, double *unused, double *out_1914466767813667966) {
   out_1914466767813667966[0] = 0;
   out_1914466767813667966[1] = 0;
   out_1914466767813667966[2] = 0;
   out_1914466767813667966[3] = 0;
   out_1914466767813667966[4] = 1;
   out_1914466767813667966[5] = 0;
   out_1914466767813667966[6] = 0;
   out_1914466767813667966[7] = 0;
   out_1914466767813667966[8] = 0;
}
void h_26(double *state, double *unused, double *out_821825300891653984) {
   out_821825300891653984[0] = state[7];
}
void H_26(double *state, double *unused, double *out_5874083645530210671) {
   out_5874083645530210671[0] = 0;
   out_5874083645530210671[1] = 0;
   out_5874083645530210671[2] = 0;
   out_5874083645530210671[3] = 0;
   out_5874083645530210671[4] = 0;
   out_5874083645530210671[5] = 0;
   out_5874083645530210671[6] = 0;
   out_5874083645530210671[7] = 1;
   out_5874083645530210671[8] = 0;
}
void h_27(double *state, double *unused, double *out_2943899185459296089) {
   out_2943899185459296089[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7311484705460601914) {
   out_7311484705460601914[0] = 0;
   out_7311484705460601914[1] = 0;
   out_7311484705460601914[2] = 0;
   out_7311484705460601914[3] = 1;
   out_7311484705460601914[4] = 0;
   out_7311484705460601914[5] = 0;
   out_7311484705460601914[6] = 0;
   out_7311484705460601914[7] = 0;
   out_7311484705460601914[8] = 0;
}
void h_29(double *state, double *unused, double *out_3219093247743801978) {
   out_3219093247743801978[0] = state[1];
}
void H_29(double *state, double *unused, double *out_5598121978591050881) {
   out_5598121978591050881[0] = 0;
   out_5598121978591050881[1] = 1;
   out_5598121978591050881[2] = 0;
   out_5598121978591050881[3] = 0;
   out_5598121978591050881[4] = 0;
   out_5598121978591050881[5] = 0;
   out_5598121978591050881[6] = 0;
   out_5598121978591050881[7] = 0;
   out_5598121978591050881[8] = 0;
}
void h_28(double *state, double *unused, double *out_7945572453120141734) {
   out_7945572453120141734[0] = state[0];
}
void H_28(double *state, double *unused, double *out_7561752250156377132) {
   out_7561752250156377132[0] = 1;
   out_7561752250156377132[1] = 0;
   out_7561752250156377132[2] = 0;
   out_7561752250156377132[3] = 0;
   out_7561752250156377132[4] = 0;
   out_7561752250156377132[5] = 0;
   out_7561752250156377132[6] = 0;
   out_7561752250156377132[7] = 0;
   out_7561752250156377132[8] = 0;
}
void h_31(double *state, double *unused, double *out_5005500072557034517) {
   out_5005500072557034517[0] = state[8];
}
void H_31(double *state, double *unused, double *out_8800511147428324293) {
   out_8800511147428324293[0] = 0;
   out_8800511147428324293[1] = 0;
   out_8800511147428324293[2] = 0;
   out_8800511147428324293[3] = 0;
   out_8800511147428324293[4] = 0;
   out_8800511147428324293[5] = 0;
   out_8800511147428324293[6] = 0;
   out_8800511147428324293[7] = 0;
   out_8800511147428324293[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_3630519523719827664) {
  err_fun(nom_x, delta_x, out_3630519523719827664);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_634137438963888633) {
  inv_err_fun(nom_x, true_x, out_634137438963888633);
}
void car_H_mod_fun(double *state, double *out_4950664487485516056) {
  H_mod_fun(state, out_4950664487485516056);
}
void car_f_fun(double *state, double dt, double *out_3645824172833169214) {
  f_fun(state,  dt, out_3645824172833169214);
}
void car_F_fun(double *state, double dt, double *out_7534620374241633819) {
  F_fun(state,  dt, out_7534620374241633819);
}
void car_h_25(double *state, double *unused, double *out_2057774082201549556) {
  h_25(state, unused, out_2057774082201549556);
}
void car_H_25(double *state, double *unused, double *out_8831157109305284721) {
  H_25(state, unused, out_8831157109305284721);
}
void car_h_24(double *state, double *unused, double *out_336340693360392124) {
  h_24(state, unused, out_336340693360392124);
}
void car_H_24(double *state, double *unused, double *out_2255585302713766620) {
  H_24(state, unused, out_2255585302713766620);
}
void car_h_30(double *state, double *unused, double *out_5823498568552499629) {
  h_30(state, unused, out_5823498568552499629);
}
void car_H_30(double *state, double *unused, double *out_1914466767813667966) {
  H_30(state, unused, out_1914466767813667966);
}
void car_h_26(double *state, double *unused, double *out_821825300891653984) {
  h_26(state, unused, out_821825300891653984);
}
void car_H_26(double *state, double *unused, double *out_5874083645530210671) {
  H_26(state, unused, out_5874083645530210671);
}
void car_h_27(double *state, double *unused, double *out_2943899185459296089) {
  h_27(state, unused, out_2943899185459296089);
}
void car_H_27(double *state, double *unused, double *out_7311484705460601914) {
  H_27(state, unused, out_7311484705460601914);
}
void car_h_29(double *state, double *unused, double *out_3219093247743801978) {
  h_29(state, unused, out_3219093247743801978);
}
void car_H_29(double *state, double *unused, double *out_5598121978591050881) {
  H_29(state, unused, out_5598121978591050881);
}
void car_h_28(double *state, double *unused, double *out_7945572453120141734) {
  h_28(state, unused, out_7945572453120141734);
}
void car_H_28(double *state, double *unused, double *out_7561752250156377132) {
  H_28(state, unused, out_7561752250156377132);
}
void car_h_31(double *state, double *unused, double *out_5005500072557034517) {
  h_31(state, unused, out_5005500072557034517);
}
void car_H_31(double *state, double *unused, double *out_8800511147428324293) {
  H_31(state, unused, out_8800511147428324293);
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
