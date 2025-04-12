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
void err_fun(double *nom_x, double *delta_x, double *out_1269080487968059847) {
   out_1269080487968059847[0] = delta_x[0] + nom_x[0];
   out_1269080487968059847[1] = delta_x[1] + nom_x[1];
   out_1269080487968059847[2] = delta_x[2] + nom_x[2];
   out_1269080487968059847[3] = delta_x[3] + nom_x[3];
   out_1269080487968059847[4] = delta_x[4] + nom_x[4];
   out_1269080487968059847[5] = delta_x[5] + nom_x[5];
   out_1269080487968059847[6] = delta_x[6] + nom_x[6];
   out_1269080487968059847[7] = delta_x[7] + nom_x[7];
   out_1269080487968059847[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_7789893795323574636) {
   out_7789893795323574636[0] = -nom_x[0] + true_x[0];
   out_7789893795323574636[1] = -nom_x[1] + true_x[1];
   out_7789893795323574636[2] = -nom_x[2] + true_x[2];
   out_7789893795323574636[3] = -nom_x[3] + true_x[3];
   out_7789893795323574636[4] = -nom_x[4] + true_x[4];
   out_7789893795323574636[5] = -nom_x[5] + true_x[5];
   out_7789893795323574636[6] = -nom_x[6] + true_x[6];
   out_7789893795323574636[7] = -nom_x[7] + true_x[7];
   out_7789893795323574636[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_7128067499669851600) {
   out_7128067499669851600[0] = 1.0;
   out_7128067499669851600[1] = 0;
   out_7128067499669851600[2] = 0;
   out_7128067499669851600[3] = 0;
   out_7128067499669851600[4] = 0;
   out_7128067499669851600[5] = 0;
   out_7128067499669851600[6] = 0;
   out_7128067499669851600[7] = 0;
   out_7128067499669851600[8] = 0;
   out_7128067499669851600[9] = 0;
   out_7128067499669851600[10] = 1.0;
   out_7128067499669851600[11] = 0;
   out_7128067499669851600[12] = 0;
   out_7128067499669851600[13] = 0;
   out_7128067499669851600[14] = 0;
   out_7128067499669851600[15] = 0;
   out_7128067499669851600[16] = 0;
   out_7128067499669851600[17] = 0;
   out_7128067499669851600[18] = 0;
   out_7128067499669851600[19] = 0;
   out_7128067499669851600[20] = 1.0;
   out_7128067499669851600[21] = 0;
   out_7128067499669851600[22] = 0;
   out_7128067499669851600[23] = 0;
   out_7128067499669851600[24] = 0;
   out_7128067499669851600[25] = 0;
   out_7128067499669851600[26] = 0;
   out_7128067499669851600[27] = 0;
   out_7128067499669851600[28] = 0;
   out_7128067499669851600[29] = 0;
   out_7128067499669851600[30] = 1.0;
   out_7128067499669851600[31] = 0;
   out_7128067499669851600[32] = 0;
   out_7128067499669851600[33] = 0;
   out_7128067499669851600[34] = 0;
   out_7128067499669851600[35] = 0;
   out_7128067499669851600[36] = 0;
   out_7128067499669851600[37] = 0;
   out_7128067499669851600[38] = 0;
   out_7128067499669851600[39] = 0;
   out_7128067499669851600[40] = 1.0;
   out_7128067499669851600[41] = 0;
   out_7128067499669851600[42] = 0;
   out_7128067499669851600[43] = 0;
   out_7128067499669851600[44] = 0;
   out_7128067499669851600[45] = 0;
   out_7128067499669851600[46] = 0;
   out_7128067499669851600[47] = 0;
   out_7128067499669851600[48] = 0;
   out_7128067499669851600[49] = 0;
   out_7128067499669851600[50] = 1.0;
   out_7128067499669851600[51] = 0;
   out_7128067499669851600[52] = 0;
   out_7128067499669851600[53] = 0;
   out_7128067499669851600[54] = 0;
   out_7128067499669851600[55] = 0;
   out_7128067499669851600[56] = 0;
   out_7128067499669851600[57] = 0;
   out_7128067499669851600[58] = 0;
   out_7128067499669851600[59] = 0;
   out_7128067499669851600[60] = 1.0;
   out_7128067499669851600[61] = 0;
   out_7128067499669851600[62] = 0;
   out_7128067499669851600[63] = 0;
   out_7128067499669851600[64] = 0;
   out_7128067499669851600[65] = 0;
   out_7128067499669851600[66] = 0;
   out_7128067499669851600[67] = 0;
   out_7128067499669851600[68] = 0;
   out_7128067499669851600[69] = 0;
   out_7128067499669851600[70] = 1.0;
   out_7128067499669851600[71] = 0;
   out_7128067499669851600[72] = 0;
   out_7128067499669851600[73] = 0;
   out_7128067499669851600[74] = 0;
   out_7128067499669851600[75] = 0;
   out_7128067499669851600[76] = 0;
   out_7128067499669851600[77] = 0;
   out_7128067499669851600[78] = 0;
   out_7128067499669851600[79] = 0;
   out_7128067499669851600[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_5940995647752763897) {
   out_5940995647752763897[0] = state[0];
   out_5940995647752763897[1] = state[1];
   out_5940995647752763897[2] = state[2];
   out_5940995647752763897[3] = state[3];
   out_5940995647752763897[4] = state[4];
   out_5940995647752763897[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_5940995647752763897[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_5940995647752763897[7] = state[7];
   out_5940995647752763897[8] = state[8];
}
void F_fun(double *state, double dt, double *out_5161594095091833010) {
   out_5161594095091833010[0] = 1;
   out_5161594095091833010[1] = 0;
   out_5161594095091833010[2] = 0;
   out_5161594095091833010[3] = 0;
   out_5161594095091833010[4] = 0;
   out_5161594095091833010[5] = 0;
   out_5161594095091833010[6] = 0;
   out_5161594095091833010[7] = 0;
   out_5161594095091833010[8] = 0;
   out_5161594095091833010[9] = 0;
   out_5161594095091833010[10] = 1;
   out_5161594095091833010[11] = 0;
   out_5161594095091833010[12] = 0;
   out_5161594095091833010[13] = 0;
   out_5161594095091833010[14] = 0;
   out_5161594095091833010[15] = 0;
   out_5161594095091833010[16] = 0;
   out_5161594095091833010[17] = 0;
   out_5161594095091833010[18] = 0;
   out_5161594095091833010[19] = 0;
   out_5161594095091833010[20] = 1;
   out_5161594095091833010[21] = 0;
   out_5161594095091833010[22] = 0;
   out_5161594095091833010[23] = 0;
   out_5161594095091833010[24] = 0;
   out_5161594095091833010[25] = 0;
   out_5161594095091833010[26] = 0;
   out_5161594095091833010[27] = 0;
   out_5161594095091833010[28] = 0;
   out_5161594095091833010[29] = 0;
   out_5161594095091833010[30] = 1;
   out_5161594095091833010[31] = 0;
   out_5161594095091833010[32] = 0;
   out_5161594095091833010[33] = 0;
   out_5161594095091833010[34] = 0;
   out_5161594095091833010[35] = 0;
   out_5161594095091833010[36] = 0;
   out_5161594095091833010[37] = 0;
   out_5161594095091833010[38] = 0;
   out_5161594095091833010[39] = 0;
   out_5161594095091833010[40] = 1;
   out_5161594095091833010[41] = 0;
   out_5161594095091833010[42] = 0;
   out_5161594095091833010[43] = 0;
   out_5161594095091833010[44] = 0;
   out_5161594095091833010[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_5161594095091833010[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_5161594095091833010[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5161594095091833010[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5161594095091833010[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_5161594095091833010[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_5161594095091833010[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_5161594095091833010[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_5161594095091833010[53] = -9.8000000000000007*dt;
   out_5161594095091833010[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_5161594095091833010[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_5161594095091833010[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5161594095091833010[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5161594095091833010[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_5161594095091833010[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_5161594095091833010[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_5161594095091833010[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5161594095091833010[62] = 0;
   out_5161594095091833010[63] = 0;
   out_5161594095091833010[64] = 0;
   out_5161594095091833010[65] = 0;
   out_5161594095091833010[66] = 0;
   out_5161594095091833010[67] = 0;
   out_5161594095091833010[68] = 0;
   out_5161594095091833010[69] = 0;
   out_5161594095091833010[70] = 1;
   out_5161594095091833010[71] = 0;
   out_5161594095091833010[72] = 0;
   out_5161594095091833010[73] = 0;
   out_5161594095091833010[74] = 0;
   out_5161594095091833010[75] = 0;
   out_5161594095091833010[76] = 0;
   out_5161594095091833010[77] = 0;
   out_5161594095091833010[78] = 0;
   out_5161594095091833010[79] = 0;
   out_5161594095091833010[80] = 1;
}
void h_25(double *state, double *unused, double *out_4050981913391279772) {
   out_4050981913391279772[0] = state[6];
}
void H_25(double *state, double *unused, double *out_5281435846039489278) {
   out_5281435846039489278[0] = 0;
   out_5281435846039489278[1] = 0;
   out_5281435846039489278[2] = 0;
   out_5281435846039489278[3] = 0;
   out_5281435846039489278[4] = 0;
   out_5281435846039489278[5] = 0;
   out_5281435846039489278[6] = 1;
   out_5281435846039489278[7] = 0;
   out_5281435846039489278[8] = 0;
}
void h_24(double *state, double *unused, double *out_6921905372779659650) {
   out_6921905372779659650[0] = state[4];
   out_6921905372779659650[1] = state[5];
}
void H_24(double *state, double *unused, double *out_1723510247237166142) {
   out_1723510247237166142[0] = 0;
   out_1723510247237166142[1] = 0;
   out_1723510247237166142[2] = 0;
   out_1723510247237166142[3] = 0;
   out_1723510247237166142[4] = 1;
   out_1723510247237166142[5] = 0;
   out_1723510247237166142[6] = 0;
   out_1723510247237166142[7] = 0;
   out_1723510247237166142[8] = 0;
   out_1723510247237166142[9] = 0;
   out_1723510247237166142[10] = 0;
   out_1723510247237166142[11] = 0;
   out_1723510247237166142[12] = 0;
   out_1723510247237166142[13] = 0;
   out_1723510247237166142[14] = 1;
   out_1723510247237166142[15] = 0;
   out_1723510247237166142[16] = 0;
   out_1723510247237166142[17] = 0;
}
void h_30(double *state, double *unused, double *out_413532385137257422) {
   out_413532385137257422[0] = state[4];
}
void H_30(double *state, double *unused, double *out_5410774793182729348) {
   out_5410774793182729348[0] = 0;
   out_5410774793182729348[1] = 0;
   out_5410774793182729348[2] = 0;
   out_5410774793182729348[3] = 0;
   out_5410774793182729348[4] = 1;
   out_5410774793182729348[5] = 0;
   out_5410774793182729348[6] = 0;
   out_5410774793182729348[7] = 0;
   out_5410774793182729348[8] = 0;
}
void h_26(double *state, double *unused, double *out_7992124434506969231) {
   out_7992124434506969231[0] = state[7];
}
void H_26(double *state, double *unused, double *out_9022939164913545502) {
   out_9022939164913545502[0] = 0;
   out_9022939164913545502[1] = 0;
   out_9022939164913545502[2] = 0;
   out_9022939164913545502[3] = 0;
   out_9022939164913545502[4] = 0;
   out_9022939164913545502[5] = 0;
   out_9022939164913545502[6] = 0;
   out_9022939164913545502[7] = 1;
   out_9022939164913545502[8] = 0;
}
void h_27(double *state, double *unused, double *out_6960618725889571031) {
   out_6960618725889571031[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7585538104983154259) {
   out_7585538104983154259[0] = 0;
   out_7585538104983154259[1] = 0;
   out_7585538104983154259[2] = 0;
   out_7585538104983154259[3] = 1;
   out_7585538104983154259[4] = 0;
   out_7585538104983154259[5] = 0;
   out_7585538104983154259[6] = 0;
   out_7585538104983154259[7] = 0;
   out_7585538104983154259[8] = 0;
}
void h_29(double *state, double *unused, double *out_189783499539220095) {
   out_189783499539220095[0] = state[1];
}
void H_29(double *state, double *unused, double *out_9147843241856846324) {
   out_9147843241856846324[0] = 0;
   out_9147843241856846324[1] = 1;
   out_9147843241856846324[2] = 0;
   out_9147843241856846324[3] = 0;
   out_9147843241856846324[4] = 0;
   out_9147843241856846324[5] = 0;
   out_9147843241856846324[6] = 0;
   out_9147843241856846324[7] = 0;
   out_9147843241856846324[8] = 0;
}
void h_28(double *state, double *unused, double *out_5376052269567797406) {
   out_5376052269567797406[0] = state[0];
}
void H_28(double *state, double *unused, double *out_7335270560287379041) {
   out_7335270560287379041[0] = 1;
   out_7335270560287379041[1] = 0;
   out_7335270560287379041[2] = 0;
   out_7335270560287379041[3] = 0;
   out_7335270560287379041[4] = 0;
   out_7335270560287379041[5] = 0;
   out_7335270560287379041[6] = 0;
   out_7335270560287379041[7] = 0;
   out_7335270560287379041[8] = 0;
}
void h_31(double *state, double *unused, double *out_3409576850857608553) {
   out_3409576850857608553[0] = state[8];
}
void H_31(double *state, double *unused, double *out_5250789884162528850) {
   out_5250789884162528850[0] = 0;
   out_5250789884162528850[1] = 0;
   out_5250789884162528850[2] = 0;
   out_5250789884162528850[3] = 0;
   out_5250789884162528850[4] = 0;
   out_5250789884162528850[5] = 0;
   out_5250789884162528850[6] = 0;
   out_5250789884162528850[7] = 0;
   out_5250789884162528850[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_1269080487968059847) {
  err_fun(nom_x, delta_x, out_1269080487968059847);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_7789893795323574636) {
  inv_err_fun(nom_x, true_x, out_7789893795323574636);
}
void car_H_mod_fun(double *state, double *out_7128067499669851600) {
  H_mod_fun(state, out_7128067499669851600);
}
void car_f_fun(double *state, double dt, double *out_5940995647752763897) {
  f_fun(state,  dt, out_5940995647752763897);
}
void car_F_fun(double *state, double dt, double *out_5161594095091833010) {
  F_fun(state,  dt, out_5161594095091833010);
}
void car_h_25(double *state, double *unused, double *out_4050981913391279772) {
  h_25(state, unused, out_4050981913391279772);
}
void car_H_25(double *state, double *unused, double *out_5281435846039489278) {
  H_25(state, unused, out_5281435846039489278);
}
void car_h_24(double *state, double *unused, double *out_6921905372779659650) {
  h_24(state, unused, out_6921905372779659650);
}
void car_H_24(double *state, double *unused, double *out_1723510247237166142) {
  H_24(state, unused, out_1723510247237166142);
}
void car_h_30(double *state, double *unused, double *out_413532385137257422) {
  h_30(state, unused, out_413532385137257422);
}
void car_H_30(double *state, double *unused, double *out_5410774793182729348) {
  H_30(state, unused, out_5410774793182729348);
}
void car_h_26(double *state, double *unused, double *out_7992124434506969231) {
  h_26(state, unused, out_7992124434506969231);
}
void car_H_26(double *state, double *unused, double *out_9022939164913545502) {
  H_26(state, unused, out_9022939164913545502);
}
void car_h_27(double *state, double *unused, double *out_6960618725889571031) {
  h_27(state, unused, out_6960618725889571031);
}
void car_H_27(double *state, double *unused, double *out_7585538104983154259) {
  H_27(state, unused, out_7585538104983154259);
}
void car_h_29(double *state, double *unused, double *out_189783499539220095) {
  h_29(state, unused, out_189783499539220095);
}
void car_H_29(double *state, double *unused, double *out_9147843241856846324) {
  H_29(state, unused, out_9147843241856846324);
}
void car_h_28(double *state, double *unused, double *out_5376052269567797406) {
  h_28(state, unused, out_5376052269567797406);
}
void car_H_28(double *state, double *unused, double *out_7335270560287379041) {
  H_28(state, unused, out_7335270560287379041);
}
void car_h_31(double *state, double *unused, double *out_3409576850857608553) {
  h_31(state, unused, out_3409576850857608553);
}
void car_H_31(double *state, double *unused, double *out_5250789884162528850) {
  H_31(state, unused, out_5250789884162528850);
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
