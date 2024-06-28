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
void err_fun(double *nom_x, double *delta_x, double *out_6192224066353304485) {
   out_6192224066353304485[0] = delta_x[0] + nom_x[0];
   out_6192224066353304485[1] = delta_x[1] + nom_x[1];
   out_6192224066353304485[2] = delta_x[2] + nom_x[2];
   out_6192224066353304485[3] = delta_x[3] + nom_x[3];
   out_6192224066353304485[4] = delta_x[4] + nom_x[4];
   out_6192224066353304485[5] = delta_x[5] + nom_x[5];
   out_6192224066353304485[6] = delta_x[6] + nom_x[6];
   out_6192224066353304485[7] = delta_x[7] + nom_x[7];
   out_6192224066353304485[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_468074439338668765) {
   out_468074439338668765[0] = -nom_x[0] + true_x[0];
   out_468074439338668765[1] = -nom_x[1] + true_x[1];
   out_468074439338668765[2] = -nom_x[2] + true_x[2];
   out_468074439338668765[3] = -nom_x[3] + true_x[3];
   out_468074439338668765[4] = -nom_x[4] + true_x[4];
   out_468074439338668765[5] = -nom_x[5] + true_x[5];
   out_468074439338668765[6] = -nom_x[6] + true_x[6];
   out_468074439338668765[7] = -nom_x[7] + true_x[7];
   out_468074439338668765[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_5633107224219578266) {
   out_5633107224219578266[0] = 1.0;
   out_5633107224219578266[1] = 0;
   out_5633107224219578266[2] = 0;
   out_5633107224219578266[3] = 0;
   out_5633107224219578266[4] = 0;
   out_5633107224219578266[5] = 0;
   out_5633107224219578266[6] = 0;
   out_5633107224219578266[7] = 0;
   out_5633107224219578266[8] = 0;
   out_5633107224219578266[9] = 0;
   out_5633107224219578266[10] = 1.0;
   out_5633107224219578266[11] = 0;
   out_5633107224219578266[12] = 0;
   out_5633107224219578266[13] = 0;
   out_5633107224219578266[14] = 0;
   out_5633107224219578266[15] = 0;
   out_5633107224219578266[16] = 0;
   out_5633107224219578266[17] = 0;
   out_5633107224219578266[18] = 0;
   out_5633107224219578266[19] = 0;
   out_5633107224219578266[20] = 1.0;
   out_5633107224219578266[21] = 0;
   out_5633107224219578266[22] = 0;
   out_5633107224219578266[23] = 0;
   out_5633107224219578266[24] = 0;
   out_5633107224219578266[25] = 0;
   out_5633107224219578266[26] = 0;
   out_5633107224219578266[27] = 0;
   out_5633107224219578266[28] = 0;
   out_5633107224219578266[29] = 0;
   out_5633107224219578266[30] = 1.0;
   out_5633107224219578266[31] = 0;
   out_5633107224219578266[32] = 0;
   out_5633107224219578266[33] = 0;
   out_5633107224219578266[34] = 0;
   out_5633107224219578266[35] = 0;
   out_5633107224219578266[36] = 0;
   out_5633107224219578266[37] = 0;
   out_5633107224219578266[38] = 0;
   out_5633107224219578266[39] = 0;
   out_5633107224219578266[40] = 1.0;
   out_5633107224219578266[41] = 0;
   out_5633107224219578266[42] = 0;
   out_5633107224219578266[43] = 0;
   out_5633107224219578266[44] = 0;
   out_5633107224219578266[45] = 0;
   out_5633107224219578266[46] = 0;
   out_5633107224219578266[47] = 0;
   out_5633107224219578266[48] = 0;
   out_5633107224219578266[49] = 0;
   out_5633107224219578266[50] = 1.0;
   out_5633107224219578266[51] = 0;
   out_5633107224219578266[52] = 0;
   out_5633107224219578266[53] = 0;
   out_5633107224219578266[54] = 0;
   out_5633107224219578266[55] = 0;
   out_5633107224219578266[56] = 0;
   out_5633107224219578266[57] = 0;
   out_5633107224219578266[58] = 0;
   out_5633107224219578266[59] = 0;
   out_5633107224219578266[60] = 1.0;
   out_5633107224219578266[61] = 0;
   out_5633107224219578266[62] = 0;
   out_5633107224219578266[63] = 0;
   out_5633107224219578266[64] = 0;
   out_5633107224219578266[65] = 0;
   out_5633107224219578266[66] = 0;
   out_5633107224219578266[67] = 0;
   out_5633107224219578266[68] = 0;
   out_5633107224219578266[69] = 0;
   out_5633107224219578266[70] = 1.0;
   out_5633107224219578266[71] = 0;
   out_5633107224219578266[72] = 0;
   out_5633107224219578266[73] = 0;
   out_5633107224219578266[74] = 0;
   out_5633107224219578266[75] = 0;
   out_5633107224219578266[76] = 0;
   out_5633107224219578266[77] = 0;
   out_5633107224219578266[78] = 0;
   out_5633107224219578266[79] = 0;
   out_5633107224219578266[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_7525855953315890196) {
   out_7525855953315890196[0] = state[0];
   out_7525855953315890196[1] = state[1];
   out_7525855953315890196[2] = state[2];
   out_7525855953315890196[3] = state[3];
   out_7525855953315890196[4] = state[4];
   out_7525855953315890196[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_7525855953315890196[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_7525855953315890196[7] = state[7];
   out_7525855953315890196[8] = state[8];
}
void F_fun(double *state, double dt, double *out_5076072365826676961) {
   out_5076072365826676961[0] = 1;
   out_5076072365826676961[1] = 0;
   out_5076072365826676961[2] = 0;
   out_5076072365826676961[3] = 0;
   out_5076072365826676961[4] = 0;
   out_5076072365826676961[5] = 0;
   out_5076072365826676961[6] = 0;
   out_5076072365826676961[7] = 0;
   out_5076072365826676961[8] = 0;
   out_5076072365826676961[9] = 0;
   out_5076072365826676961[10] = 1;
   out_5076072365826676961[11] = 0;
   out_5076072365826676961[12] = 0;
   out_5076072365826676961[13] = 0;
   out_5076072365826676961[14] = 0;
   out_5076072365826676961[15] = 0;
   out_5076072365826676961[16] = 0;
   out_5076072365826676961[17] = 0;
   out_5076072365826676961[18] = 0;
   out_5076072365826676961[19] = 0;
   out_5076072365826676961[20] = 1;
   out_5076072365826676961[21] = 0;
   out_5076072365826676961[22] = 0;
   out_5076072365826676961[23] = 0;
   out_5076072365826676961[24] = 0;
   out_5076072365826676961[25] = 0;
   out_5076072365826676961[26] = 0;
   out_5076072365826676961[27] = 0;
   out_5076072365826676961[28] = 0;
   out_5076072365826676961[29] = 0;
   out_5076072365826676961[30] = 1;
   out_5076072365826676961[31] = 0;
   out_5076072365826676961[32] = 0;
   out_5076072365826676961[33] = 0;
   out_5076072365826676961[34] = 0;
   out_5076072365826676961[35] = 0;
   out_5076072365826676961[36] = 0;
   out_5076072365826676961[37] = 0;
   out_5076072365826676961[38] = 0;
   out_5076072365826676961[39] = 0;
   out_5076072365826676961[40] = 1;
   out_5076072365826676961[41] = 0;
   out_5076072365826676961[42] = 0;
   out_5076072365826676961[43] = 0;
   out_5076072365826676961[44] = 0;
   out_5076072365826676961[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_5076072365826676961[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_5076072365826676961[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5076072365826676961[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_5076072365826676961[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_5076072365826676961[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_5076072365826676961[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_5076072365826676961[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_5076072365826676961[53] = -9.8000000000000007*dt;
   out_5076072365826676961[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_5076072365826676961[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_5076072365826676961[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5076072365826676961[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5076072365826676961[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_5076072365826676961[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_5076072365826676961[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_5076072365826676961[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_5076072365826676961[62] = 0;
   out_5076072365826676961[63] = 0;
   out_5076072365826676961[64] = 0;
   out_5076072365826676961[65] = 0;
   out_5076072365826676961[66] = 0;
   out_5076072365826676961[67] = 0;
   out_5076072365826676961[68] = 0;
   out_5076072365826676961[69] = 0;
   out_5076072365826676961[70] = 1;
   out_5076072365826676961[71] = 0;
   out_5076072365826676961[72] = 0;
   out_5076072365826676961[73] = 0;
   out_5076072365826676961[74] = 0;
   out_5076072365826676961[75] = 0;
   out_5076072365826676961[76] = 0;
   out_5076072365826676961[77] = 0;
   out_5076072365826676961[78] = 0;
   out_5076072365826676961[79] = 0;
   out_5076072365826676961[80] = 1;
}
void h_25(double *state, double *unused, double *out_4769798427931225927) {
   out_4769798427931225927[0] = state[6];
}
void H_25(double *state, double *unused, double *out_1132317480129805700) {
   out_1132317480129805700[0] = 0;
   out_1132317480129805700[1] = 0;
   out_1132317480129805700[2] = 0;
   out_1132317480129805700[3] = 0;
   out_1132317480129805700[4] = 0;
   out_1132317480129805700[5] = 0;
   out_1132317480129805700[6] = 1;
   out_1132317480129805700[7] = 0;
   out_1132317480129805700[8] = 0;
}
void h_24(double *state, double *unused, double *out_4880540155120499890) {
   out_4880540155120499890[0] = state[4];
   out_4880540155120499890[1] = state[5];
}
void H_24(double *state, double *unused, double *out_4690243078932128836) {
   out_4690243078932128836[0] = 0;
   out_4690243078932128836[1] = 0;
   out_4690243078932128836[2] = 0;
   out_4690243078932128836[3] = 0;
   out_4690243078932128836[4] = 1;
   out_4690243078932128836[5] = 0;
   out_4690243078932128836[6] = 0;
   out_4690243078932128836[7] = 0;
   out_4690243078932128836[8] = 0;
   out_4690243078932128836[9] = 0;
   out_4690243078932128836[10] = 0;
   out_4690243078932128836[11] = 0;
   out_4690243078932128836[12] = 0;
   out_4690243078932128836[13] = 0;
   out_4690243078932128836[14] = 1;
   out_4690243078932128836[15] = 0;
   out_4690243078932128836[16] = 0;
   out_4690243078932128836[17] = 0;
}
void h_30(double *state, double *unused, double *out_5795672995024276504) {
   out_5795672995024276504[0] = state[4];
}
void H_30(double *state, double *unused, double *out_3650650438637054327) {
   out_3650650438637054327[0] = 0;
   out_3650650438637054327[1] = 0;
   out_3650650438637054327[2] = 0;
   out_3650650438637054327[3] = 0;
   out_3650650438637054327[4] = 1;
   out_3650650438637054327[5] = 0;
   out_3650650438637054327[6] = 0;
   out_3650650438637054327[7] = 0;
   out_3650650438637054327[8] = 0;
}
void h_26(double *state, double *unused, double *out_4852163142122614069) {
   out_4852163142122614069[0] = state[7];
}
void H_26(double *state, double *unused, double *out_4436843449890606301) {
   out_4436843449890606301[0] = 0;
   out_4436843449890606301[1] = 0;
   out_4436843449890606301[2] = 0;
   out_4436843449890606301[3] = 0;
   out_4436843449890606301[4] = 0;
   out_4436843449890606301[5] = 0;
   out_4436843449890606301[6] = 0;
   out_4436843449890606301[7] = 1;
   out_4436843449890606301[8] = 0;
}
void h_27(double *state, double *unused, double *out_8675272378117480044) {
   out_8675272378117480044[0] = state[3];
}
void H_27(double *state, double *unused, double *out_1475887126836629416) {
   out_1475887126836629416[0] = 0;
   out_1475887126836629416[1] = 0;
   out_1475887126836629416[2] = 0;
   out_1475887126836629416[3] = 1;
   out_1475887126836629416[4] = 0;
   out_1475887126836629416[5] = 0;
   out_1475887126836629416[6] = 0;
   out_1475887126836629416[7] = 0;
   out_1475887126836629416[8] = 0;
}
void h_29(double *state, double *unused, double *out_2008206438771399131) {
   out_2008206438771399131[0] = state[1];
}
void H_29(double *state, double *unused, double *out_4160881782951446511) {
   out_4160881782951446511[0] = 0;
   out_4160881782951446511[1] = 1;
   out_4160881782951446511[2] = 0;
   out_4160881782951446511[3] = 0;
   out_4160881782951446511[4] = 0;
   out_4160881782951446511[5] = 0;
   out_4160881782951446511[6] = 0;
   out_4160881782951446511[7] = 0;
   out_4160881782951446511[8] = 0;
}
void h_28(double *state, double *unused, double *out_2299147082593455658) {
   out_2299147082593455658[0] = state[0];
}
void H_28(double *state, double *unused, double *out_921517234118084063) {
   out_921517234118084063[0] = 1;
   out_921517234118084063[1] = 0;
   out_921517234118084063[2] = 0;
   out_921517234118084063[3] = 0;
   out_921517234118084063[4] = 0;
   out_921517234118084063[5] = 0;
   out_921517234118084063[6] = 0;
   out_921517234118084063[7] = 0;
   out_921517234118084063[8] = 0;
}
void h_31(double *state, double *unused, double *out_8868101726210597791) {
   out_8868101726210597791[0] = state[8];
}
void H_31(double *state, double *unused, double *out_3810635347657254825) {
   out_3810635347657254825[0] = 0;
   out_3810635347657254825[1] = 0;
   out_3810635347657254825[2] = 0;
   out_3810635347657254825[3] = 0;
   out_3810635347657254825[4] = 0;
   out_3810635347657254825[5] = 0;
   out_3810635347657254825[6] = 0;
   out_3810635347657254825[7] = 0;
   out_3810635347657254825[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_6192224066353304485) {
  err_fun(nom_x, delta_x, out_6192224066353304485);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_468074439338668765) {
  inv_err_fun(nom_x, true_x, out_468074439338668765);
}
void car_H_mod_fun(double *state, double *out_5633107224219578266) {
  H_mod_fun(state, out_5633107224219578266);
}
void car_f_fun(double *state, double dt, double *out_7525855953315890196) {
  f_fun(state,  dt, out_7525855953315890196);
}
void car_F_fun(double *state, double dt, double *out_5076072365826676961) {
  F_fun(state,  dt, out_5076072365826676961);
}
void car_h_25(double *state, double *unused, double *out_4769798427931225927) {
  h_25(state, unused, out_4769798427931225927);
}
void car_H_25(double *state, double *unused, double *out_1132317480129805700) {
  H_25(state, unused, out_1132317480129805700);
}
void car_h_24(double *state, double *unused, double *out_4880540155120499890) {
  h_24(state, unused, out_4880540155120499890);
}
void car_H_24(double *state, double *unused, double *out_4690243078932128836) {
  H_24(state, unused, out_4690243078932128836);
}
void car_h_30(double *state, double *unused, double *out_5795672995024276504) {
  h_30(state, unused, out_5795672995024276504);
}
void car_H_30(double *state, double *unused, double *out_3650650438637054327) {
  H_30(state, unused, out_3650650438637054327);
}
void car_h_26(double *state, double *unused, double *out_4852163142122614069) {
  h_26(state, unused, out_4852163142122614069);
}
void car_H_26(double *state, double *unused, double *out_4436843449890606301) {
  H_26(state, unused, out_4436843449890606301);
}
void car_h_27(double *state, double *unused, double *out_8675272378117480044) {
  h_27(state, unused, out_8675272378117480044);
}
void car_H_27(double *state, double *unused, double *out_1475887126836629416) {
  H_27(state, unused, out_1475887126836629416);
}
void car_h_29(double *state, double *unused, double *out_2008206438771399131) {
  h_29(state, unused, out_2008206438771399131);
}
void car_H_29(double *state, double *unused, double *out_4160881782951446511) {
  H_29(state, unused, out_4160881782951446511);
}
void car_h_28(double *state, double *unused, double *out_2299147082593455658) {
  h_28(state, unused, out_2299147082593455658);
}
void car_H_28(double *state, double *unused, double *out_921517234118084063) {
  H_28(state, unused, out_921517234118084063);
}
void car_h_31(double *state, double *unused, double *out_8868101726210597791) {
  h_31(state, unused, out_8868101726210597791);
}
void car_H_31(double *state, double *unused, double *out_3810635347657254825) {
  H_31(state, unused, out_3810635347657254825);
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
