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
void err_fun(double *nom_x, double *delta_x, double *out_9139594623004300603) {
   out_9139594623004300603[0] = delta_x[0] + nom_x[0];
   out_9139594623004300603[1] = delta_x[1] + nom_x[1];
   out_9139594623004300603[2] = delta_x[2] + nom_x[2];
   out_9139594623004300603[3] = delta_x[3] + nom_x[3];
   out_9139594623004300603[4] = delta_x[4] + nom_x[4];
   out_9139594623004300603[5] = delta_x[5] + nom_x[5];
   out_9139594623004300603[6] = delta_x[6] + nom_x[6];
   out_9139594623004300603[7] = delta_x[7] + nom_x[7];
   out_9139594623004300603[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_7441847263776914647) {
   out_7441847263776914647[0] = -nom_x[0] + true_x[0];
   out_7441847263776914647[1] = -nom_x[1] + true_x[1];
   out_7441847263776914647[2] = -nom_x[2] + true_x[2];
   out_7441847263776914647[3] = -nom_x[3] + true_x[3];
   out_7441847263776914647[4] = -nom_x[4] + true_x[4];
   out_7441847263776914647[5] = -nom_x[5] + true_x[5];
   out_7441847263776914647[6] = -nom_x[6] + true_x[6];
   out_7441847263776914647[7] = -nom_x[7] + true_x[7];
   out_7441847263776914647[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_1307752065838593594) {
   out_1307752065838593594[0] = 1.0;
   out_1307752065838593594[1] = 0;
   out_1307752065838593594[2] = 0;
   out_1307752065838593594[3] = 0;
   out_1307752065838593594[4] = 0;
   out_1307752065838593594[5] = 0;
   out_1307752065838593594[6] = 0;
   out_1307752065838593594[7] = 0;
   out_1307752065838593594[8] = 0;
   out_1307752065838593594[9] = 0;
   out_1307752065838593594[10] = 1.0;
   out_1307752065838593594[11] = 0;
   out_1307752065838593594[12] = 0;
   out_1307752065838593594[13] = 0;
   out_1307752065838593594[14] = 0;
   out_1307752065838593594[15] = 0;
   out_1307752065838593594[16] = 0;
   out_1307752065838593594[17] = 0;
   out_1307752065838593594[18] = 0;
   out_1307752065838593594[19] = 0;
   out_1307752065838593594[20] = 1.0;
   out_1307752065838593594[21] = 0;
   out_1307752065838593594[22] = 0;
   out_1307752065838593594[23] = 0;
   out_1307752065838593594[24] = 0;
   out_1307752065838593594[25] = 0;
   out_1307752065838593594[26] = 0;
   out_1307752065838593594[27] = 0;
   out_1307752065838593594[28] = 0;
   out_1307752065838593594[29] = 0;
   out_1307752065838593594[30] = 1.0;
   out_1307752065838593594[31] = 0;
   out_1307752065838593594[32] = 0;
   out_1307752065838593594[33] = 0;
   out_1307752065838593594[34] = 0;
   out_1307752065838593594[35] = 0;
   out_1307752065838593594[36] = 0;
   out_1307752065838593594[37] = 0;
   out_1307752065838593594[38] = 0;
   out_1307752065838593594[39] = 0;
   out_1307752065838593594[40] = 1.0;
   out_1307752065838593594[41] = 0;
   out_1307752065838593594[42] = 0;
   out_1307752065838593594[43] = 0;
   out_1307752065838593594[44] = 0;
   out_1307752065838593594[45] = 0;
   out_1307752065838593594[46] = 0;
   out_1307752065838593594[47] = 0;
   out_1307752065838593594[48] = 0;
   out_1307752065838593594[49] = 0;
   out_1307752065838593594[50] = 1.0;
   out_1307752065838593594[51] = 0;
   out_1307752065838593594[52] = 0;
   out_1307752065838593594[53] = 0;
   out_1307752065838593594[54] = 0;
   out_1307752065838593594[55] = 0;
   out_1307752065838593594[56] = 0;
   out_1307752065838593594[57] = 0;
   out_1307752065838593594[58] = 0;
   out_1307752065838593594[59] = 0;
   out_1307752065838593594[60] = 1.0;
   out_1307752065838593594[61] = 0;
   out_1307752065838593594[62] = 0;
   out_1307752065838593594[63] = 0;
   out_1307752065838593594[64] = 0;
   out_1307752065838593594[65] = 0;
   out_1307752065838593594[66] = 0;
   out_1307752065838593594[67] = 0;
   out_1307752065838593594[68] = 0;
   out_1307752065838593594[69] = 0;
   out_1307752065838593594[70] = 1.0;
   out_1307752065838593594[71] = 0;
   out_1307752065838593594[72] = 0;
   out_1307752065838593594[73] = 0;
   out_1307752065838593594[74] = 0;
   out_1307752065838593594[75] = 0;
   out_1307752065838593594[76] = 0;
   out_1307752065838593594[77] = 0;
   out_1307752065838593594[78] = 0;
   out_1307752065838593594[79] = 0;
   out_1307752065838593594[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_3671525320198024362) {
   out_3671525320198024362[0] = state[0];
   out_3671525320198024362[1] = state[1];
   out_3671525320198024362[2] = state[2];
   out_3671525320198024362[3] = state[3];
   out_3671525320198024362[4] = state[4];
   out_3671525320198024362[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_3671525320198024362[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_3671525320198024362[7] = state[7];
   out_3671525320198024362[8] = state[8];
}
void F_fun(double *state, double dt, double *out_5753557811453604860) {
   out_5753557811453604860[0] = 1;
   out_5753557811453604860[1] = 0;
   out_5753557811453604860[2] = 0;
   out_5753557811453604860[3] = 0;
   out_5753557811453604860[4] = 0;
   out_5753557811453604860[5] = 0;
   out_5753557811453604860[6] = 0;
   out_5753557811453604860[7] = 0;
   out_5753557811453604860[8] = 0;
   out_5753557811453604860[9] = 0;
   out_5753557811453604860[10] = 1;
   out_5753557811453604860[11] = 0;
   out_5753557811453604860[12] = 0;
   out_5753557811453604860[13] = 0;
   out_5753557811453604860[14] = 0;
   out_5753557811453604860[15] = 0;
   out_5753557811453604860[16] = 0;
   out_5753557811453604860[17] = 0;
   out_5753557811453604860[18] = 0;
   out_5753557811453604860[19] = 0;
   out_5753557811453604860[20] = 1;
   out_5753557811453604860[21] = 0;
   out_5753557811453604860[22] = 0;
   out_5753557811453604860[23] = 0;
   out_5753557811453604860[24] = 0;
   out_5753557811453604860[25] = 0;
   out_5753557811453604860[26] = 0;
   out_5753557811453604860[27] = 0;
   out_5753557811453604860[28] = 0;
   out_5753557811453604860[29] = 0;
   out_5753557811453604860[30] = 1;
   out_5753557811453604860[31] = 0;
   out_5753557811453604860[32] = 0;
   out_5753557811453604860[33] = 0;
   out_5753557811453604860[34] = 0;
   out_5753557811453604860[35] = 0;
   out_5753557811453604860[36] = 0;
   out_5753557811453604860[37] = 0;
   out_5753557811453604860[38] = 0;
   out_5753557811453604860[39] = 0;
   out_5753557811453604860[40] = 1;
   out_5753557811453604860[41] = 0;
   out_5753557811453604860[42] = 0;
   out_5753557811453604860[43] = 0;
   out_5753557811453604860[44] = 0;
   out_5753557811453604860[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_5753557811453604860[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_5753557811453604860[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5753557811453604860[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5753557811453604860[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_5753557811453604860[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_5753557811453604860[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_5753557811453604860[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_5753557811453604860[53] = -9.8000000000000007*dt;
   out_5753557811453604860[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_5753557811453604860[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_5753557811453604860[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5753557811453604860[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5753557811453604860[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_5753557811453604860[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_5753557811453604860[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_5753557811453604860[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5753557811453604860[62] = 0;
   out_5753557811453604860[63] = 0;
   out_5753557811453604860[64] = 0;
   out_5753557811453604860[65] = 0;
   out_5753557811453604860[66] = 0;
   out_5753557811453604860[67] = 0;
   out_5753557811453604860[68] = 0;
   out_5753557811453604860[69] = 0;
   out_5753557811453604860[70] = 1;
   out_5753557811453604860[71] = 0;
   out_5753557811453604860[72] = 0;
   out_5753557811453604860[73] = 0;
   out_5753557811453604860[74] = 0;
   out_5753557811453604860[75] = 0;
   out_5753557811453604860[76] = 0;
   out_5753557811453604860[77] = 0;
   out_5753557811453604860[78] = 0;
   out_5753557811453604860[79] = 0;
   out_5753557811453604860[80] = 1;
}
void h_25(double *state, double *unused, double *out_8725750883400130937) {
   out_8725750883400130937[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7910028787137180931) {
   out_7910028787137180931[0] = 0;
   out_7910028787137180931[1] = 0;
   out_7910028787137180931[2] = 0;
   out_7910028787137180931[3] = 0;
   out_7910028787137180931[4] = 0;
   out_7910028787137180931[5] = 0;
   out_7910028787137180931[6] = 1;
   out_7910028787137180931[7] = 0;
   out_7910028787137180931[8] = 0;
}
void h_24(double *state, double *unused, double *out_7978321134886029324) {
   out_7978321134886029324[0] = state[4];
   out_7978321134886029324[1] = state[5];
}
void H_24(double *state, double *unused, double *out_1318036398932014294) {
   out_1318036398932014294[0] = 0;
   out_1318036398932014294[1] = 0;
   out_1318036398932014294[2] = 0;
   out_1318036398932014294[3] = 0;
   out_1318036398932014294[4] = 1;
   out_1318036398932014294[5] = 0;
   out_1318036398932014294[6] = 0;
   out_1318036398932014294[7] = 0;
   out_1318036398932014294[8] = 0;
   out_1318036398932014294[9] = 0;
   out_1318036398932014294[10] = 0;
   out_1318036398932014294[11] = 0;
   out_1318036398932014294[12] = 0;
   out_1318036398932014294[13] = 0;
   out_1318036398932014294[14] = 1;
   out_1318036398932014294[15] = 0;
   out_1318036398932014294[16] = 0;
   out_1318036398932014294[17] = 0;
}
void h_30(double *state, double *unused, double *out_8450556821115625048) {
   out_8450556821115625048[0] = state[4];
}
void H_30(double *state, double *unused, double *out_6009018956444762487) {
   out_6009018956444762487[0] = 0;
   out_6009018956444762487[1] = 0;
   out_6009018956444762487[2] = 0;
   out_6009018956444762487[3] = 0;
   out_6009018956444762487[4] = 1;
   out_6009018956444762487[5] = 0;
   out_6009018956444762487[6] = 0;
   out_6009018956444762487[7] = 0;
   out_6009018956444762487[8] = 0;
}
void h_26(double *state, double *unused, double *out_6262269178414634790) {
   out_6262269178414634790[0] = state[7];
}
void H_26(double *state, double *unused, double *out_6795211967698314461) {
   out_6795211967698314461[0] = 0;
   out_6795211967698314461[1] = 0;
   out_6795211967698314461[2] = 0;
   out_6795211967698314461[3] = 0;
   out_6795211967698314461[4] = 0;
   out_6795211967698314461[5] = 0;
   out_6795211967698314461[6] = 0;
   out_6795211967698314461[7] = 1;
   out_6795211967698314461[8] = 0;
}
void h_27(double *state, double *unused, double *out_1076405710088796595) {
   out_1076405710088796595[0] = state[3];
}
void H_27(double *state, double *unused, double *out_8232613027628705704) {
   out_8232613027628705704[0] = 0;
   out_8232613027628705704[1] = 0;
   out_8232613027628705704[2] = 0;
   out_8232613027628705704[3] = 1;
   out_8232613027628705704[4] = 0;
   out_8232613027628705704[5] = 0;
   out_8232613027628705704[6] = 0;
   out_8232613027628705704[7] = 0;
   out_8232613027628705704[8] = 0;
}
void h_29(double *state, double *unused, double *out_2561043818165225755) {
   out_2561043818165225755[0] = state[1];
}
void H_29(double *state, double *unused, double *out_6519250300759154671) {
   out_6519250300759154671[0] = 0;
   out_6519250300759154671[1] = 1;
   out_6519250300759154671[2] = 0;
   out_6519250300759154671[3] = 0;
   out_6519250300759154671[4] = 0;
   out_6519250300759154671[5] = 0;
   out_6519250300759154671[6] = 0;
   out_6519250300759154671[7] = 0;
   out_6519250300759154671[8] = 0;
}
void h_28(double *state, double *unused, double *out_8964681543976528368) {
   out_8964681543976528368[0] = state[0];
}
void H_28(double *state, double *unused, double *out_1436851283689624097) {
   out_1436851283689624097[0] = 1;
   out_1436851283689624097[1] = 0;
   out_1436851283689624097[2] = 0;
   out_1436851283689624097[3] = 0;
   out_1436851283689624097[4] = 0;
   out_1436851283689624097[5] = 0;
   out_1436851283689624097[6] = 0;
   out_1436851283689624097[7] = 0;
   out_1436851283689624097[8] = 0;
}
void h_31(double *state, double *unused, double *out_6083543662055398329) {
   out_6083543662055398329[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7879382825260220503) {
   out_7879382825260220503[0] = 0;
   out_7879382825260220503[1] = 0;
   out_7879382825260220503[2] = 0;
   out_7879382825260220503[3] = 0;
   out_7879382825260220503[4] = 0;
   out_7879382825260220503[5] = 0;
   out_7879382825260220503[6] = 0;
   out_7879382825260220503[7] = 0;
   out_7879382825260220503[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_9139594623004300603) {
  err_fun(nom_x, delta_x, out_9139594623004300603);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_7441847263776914647) {
  inv_err_fun(nom_x, true_x, out_7441847263776914647);
}
void car_H_mod_fun(double *state, double *out_1307752065838593594) {
  H_mod_fun(state, out_1307752065838593594);
}
void car_f_fun(double *state, double dt, double *out_3671525320198024362) {
  f_fun(state,  dt, out_3671525320198024362);
}
void car_F_fun(double *state, double dt, double *out_5753557811453604860) {
  F_fun(state,  dt, out_5753557811453604860);
}
void car_h_25(double *state, double *unused, double *out_8725750883400130937) {
  h_25(state, unused, out_8725750883400130937);
}
void car_H_25(double *state, double *unused, double *out_7910028787137180931) {
  H_25(state, unused, out_7910028787137180931);
}
void car_h_24(double *state, double *unused, double *out_7978321134886029324) {
  h_24(state, unused, out_7978321134886029324);
}
void car_H_24(double *state, double *unused, double *out_1318036398932014294) {
  H_24(state, unused, out_1318036398932014294);
}
void car_h_30(double *state, double *unused, double *out_8450556821115625048) {
  h_30(state, unused, out_8450556821115625048);
}
void car_H_30(double *state, double *unused, double *out_6009018956444762487) {
  H_30(state, unused, out_6009018956444762487);
}
void car_h_26(double *state, double *unused, double *out_6262269178414634790) {
  h_26(state, unused, out_6262269178414634790);
}
void car_H_26(double *state, double *unused, double *out_6795211967698314461) {
  H_26(state, unused, out_6795211967698314461);
}
void car_h_27(double *state, double *unused, double *out_1076405710088796595) {
  h_27(state, unused, out_1076405710088796595);
}
void car_H_27(double *state, double *unused, double *out_8232613027628705704) {
  H_27(state, unused, out_8232613027628705704);
}
void car_h_29(double *state, double *unused, double *out_2561043818165225755) {
  h_29(state, unused, out_2561043818165225755);
}
void car_H_29(double *state, double *unused, double *out_6519250300759154671) {
  H_29(state, unused, out_6519250300759154671);
}
void car_h_28(double *state, double *unused, double *out_8964681543976528368) {
  h_28(state, unused, out_8964681543976528368);
}
void car_H_28(double *state, double *unused, double *out_1436851283689624097) {
  H_28(state, unused, out_1436851283689624097);
}
void car_h_31(double *state, double *unused, double *out_6083543662055398329) {
  h_31(state, unused, out_6083543662055398329);
}
void car_H_31(double *state, double *unused, double *out_7879382825260220503) {
  H_31(state, unused, out_7879382825260220503);
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
