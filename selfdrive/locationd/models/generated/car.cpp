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
void err_fun(double *nom_x, double *delta_x, double *out_3296532285088142988) {
   out_3296532285088142988[0] = delta_x[0] + nom_x[0];
   out_3296532285088142988[1] = delta_x[1] + nom_x[1];
   out_3296532285088142988[2] = delta_x[2] + nom_x[2];
   out_3296532285088142988[3] = delta_x[3] + nom_x[3];
   out_3296532285088142988[4] = delta_x[4] + nom_x[4];
   out_3296532285088142988[5] = delta_x[5] + nom_x[5];
   out_3296532285088142988[6] = delta_x[6] + nom_x[6];
   out_3296532285088142988[7] = delta_x[7] + nom_x[7];
   out_3296532285088142988[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_3648214217490293601) {
   out_3648214217490293601[0] = -nom_x[0] + true_x[0];
   out_3648214217490293601[1] = -nom_x[1] + true_x[1];
   out_3648214217490293601[2] = -nom_x[2] + true_x[2];
   out_3648214217490293601[3] = -nom_x[3] + true_x[3];
   out_3648214217490293601[4] = -nom_x[4] + true_x[4];
   out_3648214217490293601[5] = -nom_x[5] + true_x[5];
   out_3648214217490293601[6] = -nom_x[6] + true_x[6];
   out_3648214217490293601[7] = -nom_x[7] + true_x[7];
   out_3648214217490293601[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_3259352301902337161) {
   out_3259352301902337161[0] = 1.0;
   out_3259352301902337161[1] = 0;
   out_3259352301902337161[2] = 0;
   out_3259352301902337161[3] = 0;
   out_3259352301902337161[4] = 0;
   out_3259352301902337161[5] = 0;
   out_3259352301902337161[6] = 0;
   out_3259352301902337161[7] = 0;
   out_3259352301902337161[8] = 0;
   out_3259352301902337161[9] = 0;
   out_3259352301902337161[10] = 1.0;
   out_3259352301902337161[11] = 0;
   out_3259352301902337161[12] = 0;
   out_3259352301902337161[13] = 0;
   out_3259352301902337161[14] = 0;
   out_3259352301902337161[15] = 0;
   out_3259352301902337161[16] = 0;
   out_3259352301902337161[17] = 0;
   out_3259352301902337161[18] = 0;
   out_3259352301902337161[19] = 0;
   out_3259352301902337161[20] = 1.0;
   out_3259352301902337161[21] = 0;
   out_3259352301902337161[22] = 0;
   out_3259352301902337161[23] = 0;
   out_3259352301902337161[24] = 0;
   out_3259352301902337161[25] = 0;
   out_3259352301902337161[26] = 0;
   out_3259352301902337161[27] = 0;
   out_3259352301902337161[28] = 0;
   out_3259352301902337161[29] = 0;
   out_3259352301902337161[30] = 1.0;
   out_3259352301902337161[31] = 0;
   out_3259352301902337161[32] = 0;
   out_3259352301902337161[33] = 0;
   out_3259352301902337161[34] = 0;
   out_3259352301902337161[35] = 0;
   out_3259352301902337161[36] = 0;
   out_3259352301902337161[37] = 0;
   out_3259352301902337161[38] = 0;
   out_3259352301902337161[39] = 0;
   out_3259352301902337161[40] = 1.0;
   out_3259352301902337161[41] = 0;
   out_3259352301902337161[42] = 0;
   out_3259352301902337161[43] = 0;
   out_3259352301902337161[44] = 0;
   out_3259352301902337161[45] = 0;
   out_3259352301902337161[46] = 0;
   out_3259352301902337161[47] = 0;
   out_3259352301902337161[48] = 0;
   out_3259352301902337161[49] = 0;
   out_3259352301902337161[50] = 1.0;
   out_3259352301902337161[51] = 0;
   out_3259352301902337161[52] = 0;
   out_3259352301902337161[53] = 0;
   out_3259352301902337161[54] = 0;
   out_3259352301902337161[55] = 0;
   out_3259352301902337161[56] = 0;
   out_3259352301902337161[57] = 0;
   out_3259352301902337161[58] = 0;
   out_3259352301902337161[59] = 0;
   out_3259352301902337161[60] = 1.0;
   out_3259352301902337161[61] = 0;
   out_3259352301902337161[62] = 0;
   out_3259352301902337161[63] = 0;
   out_3259352301902337161[64] = 0;
   out_3259352301902337161[65] = 0;
   out_3259352301902337161[66] = 0;
   out_3259352301902337161[67] = 0;
   out_3259352301902337161[68] = 0;
   out_3259352301902337161[69] = 0;
   out_3259352301902337161[70] = 1.0;
   out_3259352301902337161[71] = 0;
   out_3259352301902337161[72] = 0;
   out_3259352301902337161[73] = 0;
   out_3259352301902337161[74] = 0;
   out_3259352301902337161[75] = 0;
   out_3259352301902337161[76] = 0;
   out_3259352301902337161[77] = 0;
   out_3259352301902337161[78] = 0;
   out_3259352301902337161[79] = 0;
   out_3259352301902337161[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_7755882963801286876) {
   out_7755882963801286876[0] = state[0];
   out_7755882963801286876[1] = state[1];
   out_7755882963801286876[2] = state[2];
   out_7755882963801286876[3] = state[3];
   out_7755882963801286876[4] = state[4];
   out_7755882963801286876[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_7755882963801286876[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_7755882963801286876[7] = state[7];
   out_7755882963801286876[8] = state[8];
}
void F_fun(double *state, double dt, double *out_1074076454550104479) {
   out_1074076454550104479[0] = 1;
   out_1074076454550104479[1] = 0;
   out_1074076454550104479[2] = 0;
   out_1074076454550104479[3] = 0;
   out_1074076454550104479[4] = 0;
   out_1074076454550104479[5] = 0;
   out_1074076454550104479[6] = 0;
   out_1074076454550104479[7] = 0;
   out_1074076454550104479[8] = 0;
   out_1074076454550104479[9] = 0;
   out_1074076454550104479[10] = 1;
   out_1074076454550104479[11] = 0;
   out_1074076454550104479[12] = 0;
   out_1074076454550104479[13] = 0;
   out_1074076454550104479[14] = 0;
   out_1074076454550104479[15] = 0;
   out_1074076454550104479[16] = 0;
   out_1074076454550104479[17] = 0;
   out_1074076454550104479[18] = 0;
   out_1074076454550104479[19] = 0;
   out_1074076454550104479[20] = 1;
   out_1074076454550104479[21] = 0;
   out_1074076454550104479[22] = 0;
   out_1074076454550104479[23] = 0;
   out_1074076454550104479[24] = 0;
   out_1074076454550104479[25] = 0;
   out_1074076454550104479[26] = 0;
   out_1074076454550104479[27] = 0;
   out_1074076454550104479[28] = 0;
   out_1074076454550104479[29] = 0;
   out_1074076454550104479[30] = 1;
   out_1074076454550104479[31] = 0;
   out_1074076454550104479[32] = 0;
   out_1074076454550104479[33] = 0;
   out_1074076454550104479[34] = 0;
   out_1074076454550104479[35] = 0;
   out_1074076454550104479[36] = 0;
   out_1074076454550104479[37] = 0;
   out_1074076454550104479[38] = 0;
   out_1074076454550104479[39] = 0;
   out_1074076454550104479[40] = 1;
   out_1074076454550104479[41] = 0;
   out_1074076454550104479[42] = 0;
   out_1074076454550104479[43] = 0;
   out_1074076454550104479[44] = 0;
   out_1074076454550104479[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_1074076454550104479[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_1074076454550104479[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_1074076454550104479[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_1074076454550104479[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_1074076454550104479[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_1074076454550104479[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_1074076454550104479[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_1074076454550104479[53] = -9.8000000000000007*dt;
   out_1074076454550104479[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_1074076454550104479[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_1074076454550104479[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1074076454550104479[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1074076454550104479[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_1074076454550104479[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_1074076454550104479[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_1074076454550104479[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_1074076454550104479[62] = 0;
   out_1074076454550104479[63] = 0;
   out_1074076454550104479[64] = 0;
   out_1074076454550104479[65] = 0;
   out_1074076454550104479[66] = 0;
   out_1074076454550104479[67] = 0;
   out_1074076454550104479[68] = 0;
   out_1074076454550104479[69] = 0;
   out_1074076454550104479[70] = 1;
   out_1074076454550104479[71] = 0;
   out_1074076454550104479[72] = 0;
   out_1074076454550104479[73] = 0;
   out_1074076454550104479[74] = 0;
   out_1074076454550104479[75] = 0;
   out_1074076454550104479[76] = 0;
   out_1074076454550104479[77] = 0;
   out_1074076454550104479[78] = 0;
   out_1074076454550104479[79] = 0;
   out_1074076454550104479[80] = 1;
}
void h_25(double *state, double *unused, double *out_7687865361544069403) {
   out_7687865361544069403[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7425868149578305114) {
   out_7425868149578305114[0] = 0;
   out_7425868149578305114[1] = 0;
   out_7425868149578305114[2] = 0;
   out_7425868149578305114[3] = 0;
   out_7425868149578305114[4] = 0;
   out_7425868149578305114[5] = 0;
   out_7425868149578305114[6] = 1;
   out_7425868149578305114[7] = 0;
   out_7425868149578305114[8] = 0;
}
void h_24(double *state, double *unused, double *out_6202868449489990231) {
   out_6202868449489990231[0] = state[4];
   out_6202868449489990231[1] = state[5];
}
void H_24(double *state, double *unused, double *out_4819843244280085076) {
   out_4819843244280085076[0] = 0;
   out_4819843244280085076[1] = 0;
   out_4819843244280085076[2] = 0;
   out_4819843244280085076[3] = 0;
   out_4819843244280085076[4] = 1;
   out_4819843244280085076[5] = 0;
   out_4819843244280085076[6] = 0;
   out_4819843244280085076[7] = 0;
   out_4819843244280085076[8] = 0;
   out_4819843244280085076[9] = 0;
   out_4819843244280085076[10] = 0;
   out_4819843244280085076[11] = 0;
   out_4819843244280085076[12] = 0;
   out_4819843244280085076[13] = 0;
   out_4819843244280085076[14] = 1;
   out_4819843244280085076[15] = 0;
   out_4819843244280085076[16] = 0;
   out_4819843244280085076[17] = 0;
}
void h_30(double *state, double *unused, double *out_7229663999385507741) {
   out_7229663999385507741[0] = state[4];
}
void H_30(double *state, double *unused, double *out_4907535191071056487) {
   out_4907535191071056487[0] = 0;
   out_4907535191071056487[1] = 0;
   out_4907535191071056487[2] = 0;
   out_4907535191071056487[3] = 0;
   out_4907535191071056487[4] = 1;
   out_4907535191071056487[5] = 0;
   out_4907535191071056487[6] = 0;
   out_4907535191071056487[7] = 0;
   out_4907535191071056487[8] = 0;
}
void h_26(double *state, double *unused, double *out_8173173852287170176) {
   out_8173173852287170176[0] = state[7];
}
void H_26(double *state, double *unused, double *out_7279372605257190278) {
   out_7279372605257190278[0] = 0;
   out_7279372605257190278[1] = 0;
   out_7279372605257190278[2] = 0;
   out_7279372605257190278[3] = 0;
   out_7279372605257190278[4] = 0;
   out_7279372605257190278[5] = 0;
   out_7279372605257190278[6] = 0;
   out_7279372605257190278[7] = 1;
   out_7279372605257190278[8] = 0;
}
void h_27(double *state, double *unused, double *out_6360926630883853201) {
   out_6360926630883853201[0] = state[3];
}
void H_27(double *state, double *unused, double *out_2683941119887113270) {
   out_2683941119887113270[0] = 0;
   out_2683941119887113270[1] = 0;
   out_2683941119887113270[2] = 0;
   out_2683941119887113270[3] = 1;
   out_2683941119887113270[4] = 0;
   out_2683941119887113270[5] = 0;
   out_2683941119887113270[6] = 0;
   out_2683941119887113270[7] = 0;
   out_2683941119887113270[8] = 0;
}
void h_29(double *state, double *unused, double *out_4192877947941175056) {
   out_4192877947941175056[0] = state[1];
}
void H_29(double *state, double *unused, double *out_4397303846756664303) {
   out_4397303846756664303[0] = 0;
   out_4397303846756664303[1] = 1;
   out_4397303846756664303[2] = 0;
   out_4397303846756664303[3] = 0;
   out_4397303846756664303[4] = 0;
   out_4397303846756664303[5] = 0;
   out_4397303846756664303[6] = 0;
   out_4397303846756664303[7] = 0;
   out_4397303846756664303[8] = 0;
}
void h_28(double *state, double *unused, double *out_9049031391381365730) {
   out_9049031391381365730[0] = state[0];
}
void H_28(double *state, double *unused, double *out_8967041209883356739) {
   out_8967041209883356739[0] = 1;
   out_8967041209883356739[1] = 0;
   out_8967041209883356739[2] = 0;
   out_8967041209883356739[3] = 0;
   out_8967041209883356739[4] = 0;
   out_8967041209883356739[5] = 0;
   out_8967041209883356739[6] = 0;
   out_8967041209883356739[7] = 0;
   out_8967041209883356739[8] = 0;
}
void h_31(double *state, double *unused, double *out_7845052029895198548) {
   out_7845052029895198548[0] = state[8];
}
void H_31(double *state, double *unused, double *out_6653164503023838802) {
   out_6653164503023838802[0] = 0;
   out_6653164503023838802[1] = 0;
   out_6653164503023838802[2] = 0;
   out_6653164503023838802[3] = 0;
   out_6653164503023838802[4] = 0;
   out_6653164503023838802[5] = 0;
   out_6653164503023838802[6] = 0;
   out_6653164503023838802[7] = 0;
   out_6653164503023838802[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_3296532285088142988) {
  err_fun(nom_x, delta_x, out_3296532285088142988);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_3648214217490293601) {
  inv_err_fun(nom_x, true_x, out_3648214217490293601);
}
void car_H_mod_fun(double *state, double *out_3259352301902337161) {
  H_mod_fun(state, out_3259352301902337161);
}
void car_f_fun(double *state, double dt, double *out_7755882963801286876) {
  f_fun(state,  dt, out_7755882963801286876);
}
void car_F_fun(double *state, double dt, double *out_1074076454550104479) {
  F_fun(state,  dt, out_1074076454550104479);
}
void car_h_25(double *state, double *unused, double *out_7687865361544069403) {
  h_25(state, unused, out_7687865361544069403);
}
void car_H_25(double *state, double *unused, double *out_7425868149578305114) {
  H_25(state, unused, out_7425868149578305114);
}
void car_h_24(double *state, double *unused, double *out_6202868449489990231) {
  h_24(state, unused, out_6202868449489990231);
}
void car_H_24(double *state, double *unused, double *out_4819843244280085076) {
  H_24(state, unused, out_4819843244280085076);
}
void car_h_30(double *state, double *unused, double *out_7229663999385507741) {
  h_30(state, unused, out_7229663999385507741);
}
void car_H_30(double *state, double *unused, double *out_4907535191071056487) {
  H_30(state, unused, out_4907535191071056487);
}
void car_h_26(double *state, double *unused, double *out_8173173852287170176) {
  h_26(state, unused, out_8173173852287170176);
}
void car_H_26(double *state, double *unused, double *out_7279372605257190278) {
  H_26(state, unused, out_7279372605257190278);
}
void car_h_27(double *state, double *unused, double *out_6360926630883853201) {
  h_27(state, unused, out_6360926630883853201);
}
void car_H_27(double *state, double *unused, double *out_2683941119887113270) {
  H_27(state, unused, out_2683941119887113270);
}
void car_h_29(double *state, double *unused, double *out_4192877947941175056) {
  h_29(state, unused, out_4192877947941175056);
}
void car_H_29(double *state, double *unused, double *out_4397303846756664303) {
  H_29(state, unused, out_4397303846756664303);
}
void car_h_28(double *state, double *unused, double *out_9049031391381365730) {
  h_28(state, unused, out_9049031391381365730);
}
void car_H_28(double *state, double *unused, double *out_8967041209883356739) {
  H_28(state, unused, out_8967041209883356739);
}
void car_h_31(double *state, double *unused, double *out_7845052029895198548) {
  h_31(state, unused, out_7845052029895198548);
}
void car_H_31(double *state, double *unused, double *out_6653164503023838802) {
  H_31(state, unused, out_6653164503023838802);
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
