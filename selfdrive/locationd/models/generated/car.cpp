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
void err_fun(double *nom_x, double *delta_x, double *out_5662147043452870344) {
   out_5662147043452870344[0] = delta_x[0] + nom_x[0];
   out_5662147043452870344[1] = delta_x[1] + nom_x[1];
   out_5662147043452870344[2] = delta_x[2] + nom_x[2];
   out_5662147043452870344[3] = delta_x[3] + nom_x[3];
   out_5662147043452870344[4] = delta_x[4] + nom_x[4];
   out_5662147043452870344[5] = delta_x[5] + nom_x[5];
   out_5662147043452870344[6] = delta_x[6] + nom_x[6];
   out_5662147043452870344[7] = delta_x[7] + nom_x[7];
   out_5662147043452870344[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_2016113788005242335) {
   out_2016113788005242335[0] = -nom_x[0] + true_x[0];
   out_2016113788005242335[1] = -nom_x[1] + true_x[1];
   out_2016113788005242335[2] = -nom_x[2] + true_x[2];
   out_2016113788005242335[3] = -nom_x[3] + true_x[3];
   out_2016113788005242335[4] = -nom_x[4] + true_x[4];
   out_2016113788005242335[5] = -nom_x[5] + true_x[5];
   out_2016113788005242335[6] = -nom_x[6] + true_x[6];
   out_2016113788005242335[7] = -nom_x[7] + true_x[7];
   out_2016113788005242335[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_8997301641043644665) {
   out_8997301641043644665[0] = 1.0;
   out_8997301641043644665[1] = 0;
   out_8997301641043644665[2] = 0;
   out_8997301641043644665[3] = 0;
   out_8997301641043644665[4] = 0;
   out_8997301641043644665[5] = 0;
   out_8997301641043644665[6] = 0;
   out_8997301641043644665[7] = 0;
   out_8997301641043644665[8] = 0;
   out_8997301641043644665[9] = 0;
   out_8997301641043644665[10] = 1.0;
   out_8997301641043644665[11] = 0;
   out_8997301641043644665[12] = 0;
   out_8997301641043644665[13] = 0;
   out_8997301641043644665[14] = 0;
   out_8997301641043644665[15] = 0;
   out_8997301641043644665[16] = 0;
   out_8997301641043644665[17] = 0;
   out_8997301641043644665[18] = 0;
   out_8997301641043644665[19] = 0;
   out_8997301641043644665[20] = 1.0;
   out_8997301641043644665[21] = 0;
   out_8997301641043644665[22] = 0;
   out_8997301641043644665[23] = 0;
   out_8997301641043644665[24] = 0;
   out_8997301641043644665[25] = 0;
   out_8997301641043644665[26] = 0;
   out_8997301641043644665[27] = 0;
   out_8997301641043644665[28] = 0;
   out_8997301641043644665[29] = 0;
   out_8997301641043644665[30] = 1.0;
   out_8997301641043644665[31] = 0;
   out_8997301641043644665[32] = 0;
   out_8997301641043644665[33] = 0;
   out_8997301641043644665[34] = 0;
   out_8997301641043644665[35] = 0;
   out_8997301641043644665[36] = 0;
   out_8997301641043644665[37] = 0;
   out_8997301641043644665[38] = 0;
   out_8997301641043644665[39] = 0;
   out_8997301641043644665[40] = 1.0;
   out_8997301641043644665[41] = 0;
   out_8997301641043644665[42] = 0;
   out_8997301641043644665[43] = 0;
   out_8997301641043644665[44] = 0;
   out_8997301641043644665[45] = 0;
   out_8997301641043644665[46] = 0;
   out_8997301641043644665[47] = 0;
   out_8997301641043644665[48] = 0;
   out_8997301641043644665[49] = 0;
   out_8997301641043644665[50] = 1.0;
   out_8997301641043644665[51] = 0;
   out_8997301641043644665[52] = 0;
   out_8997301641043644665[53] = 0;
   out_8997301641043644665[54] = 0;
   out_8997301641043644665[55] = 0;
   out_8997301641043644665[56] = 0;
   out_8997301641043644665[57] = 0;
   out_8997301641043644665[58] = 0;
   out_8997301641043644665[59] = 0;
   out_8997301641043644665[60] = 1.0;
   out_8997301641043644665[61] = 0;
   out_8997301641043644665[62] = 0;
   out_8997301641043644665[63] = 0;
   out_8997301641043644665[64] = 0;
   out_8997301641043644665[65] = 0;
   out_8997301641043644665[66] = 0;
   out_8997301641043644665[67] = 0;
   out_8997301641043644665[68] = 0;
   out_8997301641043644665[69] = 0;
   out_8997301641043644665[70] = 1.0;
   out_8997301641043644665[71] = 0;
   out_8997301641043644665[72] = 0;
   out_8997301641043644665[73] = 0;
   out_8997301641043644665[74] = 0;
   out_8997301641043644665[75] = 0;
   out_8997301641043644665[76] = 0;
   out_8997301641043644665[77] = 0;
   out_8997301641043644665[78] = 0;
   out_8997301641043644665[79] = 0;
   out_8997301641043644665[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2973469614408186678) {
   out_2973469614408186678[0] = state[0];
   out_2973469614408186678[1] = state[1];
   out_2973469614408186678[2] = state[2];
   out_2973469614408186678[3] = state[3];
   out_2973469614408186678[4] = state[4];
   out_2973469614408186678[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2973469614408186678[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2973469614408186678[7] = state[7];
   out_2973469614408186678[8] = state[8];
}
void F_fun(double *state, double dt, double *out_4040592134712196686) {
   out_4040592134712196686[0] = 1;
   out_4040592134712196686[1] = 0;
   out_4040592134712196686[2] = 0;
   out_4040592134712196686[3] = 0;
   out_4040592134712196686[4] = 0;
   out_4040592134712196686[5] = 0;
   out_4040592134712196686[6] = 0;
   out_4040592134712196686[7] = 0;
   out_4040592134712196686[8] = 0;
   out_4040592134712196686[9] = 0;
   out_4040592134712196686[10] = 1;
   out_4040592134712196686[11] = 0;
   out_4040592134712196686[12] = 0;
   out_4040592134712196686[13] = 0;
   out_4040592134712196686[14] = 0;
   out_4040592134712196686[15] = 0;
   out_4040592134712196686[16] = 0;
   out_4040592134712196686[17] = 0;
   out_4040592134712196686[18] = 0;
   out_4040592134712196686[19] = 0;
   out_4040592134712196686[20] = 1;
   out_4040592134712196686[21] = 0;
   out_4040592134712196686[22] = 0;
   out_4040592134712196686[23] = 0;
   out_4040592134712196686[24] = 0;
   out_4040592134712196686[25] = 0;
   out_4040592134712196686[26] = 0;
   out_4040592134712196686[27] = 0;
   out_4040592134712196686[28] = 0;
   out_4040592134712196686[29] = 0;
   out_4040592134712196686[30] = 1;
   out_4040592134712196686[31] = 0;
   out_4040592134712196686[32] = 0;
   out_4040592134712196686[33] = 0;
   out_4040592134712196686[34] = 0;
   out_4040592134712196686[35] = 0;
   out_4040592134712196686[36] = 0;
   out_4040592134712196686[37] = 0;
   out_4040592134712196686[38] = 0;
   out_4040592134712196686[39] = 0;
   out_4040592134712196686[40] = 1;
   out_4040592134712196686[41] = 0;
   out_4040592134712196686[42] = 0;
   out_4040592134712196686[43] = 0;
   out_4040592134712196686[44] = 0;
   out_4040592134712196686[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_4040592134712196686[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_4040592134712196686[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4040592134712196686[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4040592134712196686[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_4040592134712196686[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_4040592134712196686[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_4040592134712196686[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_4040592134712196686[53] = -9.8000000000000007*dt;
   out_4040592134712196686[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_4040592134712196686[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_4040592134712196686[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4040592134712196686[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4040592134712196686[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_4040592134712196686[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_4040592134712196686[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_4040592134712196686[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4040592134712196686[62] = 0;
   out_4040592134712196686[63] = 0;
   out_4040592134712196686[64] = 0;
   out_4040592134712196686[65] = 0;
   out_4040592134712196686[66] = 0;
   out_4040592134712196686[67] = 0;
   out_4040592134712196686[68] = 0;
   out_4040592134712196686[69] = 0;
   out_4040592134712196686[70] = 1;
   out_4040592134712196686[71] = 0;
   out_4040592134712196686[72] = 0;
   out_4040592134712196686[73] = 0;
   out_4040592134712196686[74] = 0;
   out_4040592134712196686[75] = 0;
   out_4040592134712196686[76] = 0;
   out_4040592134712196686[77] = 0;
   out_4040592134712196686[78] = 0;
   out_4040592134712196686[79] = 0;
   out_4040592134712196686[80] = 1;
}
void h_25(double *state, double *unused, double *out_1319061140791389331) {
   out_1319061140791389331[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7793576592585487069) {
   out_7793576592585487069[0] = 0;
   out_7793576592585487069[1] = 0;
   out_7793576592585487069[2] = 0;
   out_7793576592585487069[3] = 0;
   out_7793576592585487069[4] = 0;
   out_7793576592585487069[5] = 0;
   out_7793576592585487069[6] = 1;
   out_7793576592585487069[7] = 0;
   out_7793576592585487069[8] = 0;
}
void h_24(double *state, double *unused, double *out_3718149663781647640) {
   out_3718149663781647640[0] = state[4];
   out_3718149663781647640[1] = state[5];
}
void H_24(double *state, double *unused, double *out_7095241882321741411) {
   out_7095241882321741411[0] = 0;
   out_7095241882321741411[1] = 0;
   out_7095241882321741411[2] = 0;
   out_7095241882321741411[3] = 0;
   out_7095241882321741411[4] = 1;
   out_7095241882321741411[5] = 0;
   out_7095241882321741411[6] = 0;
   out_7095241882321741411[7] = 0;
   out_7095241882321741411[8] = 0;
   out_7095241882321741411[9] = 0;
   out_7095241882321741411[10] = 0;
   out_7095241882321741411[11] = 0;
   out_7095241882321741411[12] = 0;
   out_7095241882321741411[13] = 0;
   out_7095241882321741411[14] = 1;
   out_7095241882321741411[15] = 0;
   out_7095241882321741411[16] = 0;
   out_7095241882321741411[17] = 0;
}
void h_30(double *state, double *unused, double *out_1043867078506883442) {
   out_1043867078506883442[0] = state[4];
}
void H_30(double *state, double *unused, double *out_3736477139632447792) {
   out_3736477139632447792[0] = 0;
   out_3736477139632447792[1] = 0;
   out_3736477139632447792[2] = 0;
   out_3736477139632447792[3] = 0;
   out_3736477139632447792[4] = 1;
   out_3736477139632447792[5] = 0;
   out_3736477139632447792[6] = 0;
   out_3736477139632447792[7] = 0;
   out_3736477139632447792[8] = 0;
}
void h_26(double *state, double *unused, double *out_6922152566065598530) {
   out_6922152566065598530[0] = state[7];
}
void H_26(double *state, double *unused, double *out_4052073273711430845) {
   out_4052073273711430845[0] = 0;
   out_4052073273711430845[1] = 0;
   out_4052073273711430845[2] = 0;
   out_4052073273711430845[3] = 0;
   out_4052073273711430845[4] = 0;
   out_4052073273711430845[5] = 0;
   out_4052073273711430845[6] = 0;
   out_4052073273711430845[7] = 1;
   out_4052073273711430845[8] = 0;
}
void h_27(double *state, double *unused, double *out_7701482271649087111) {
   out_7701482271649087111[0] = state[3];
}
void H_27(double *state, double *unused, double *out_5911240451432872703) {
   out_5911240451432872703[0] = 0;
   out_5911240451432872703[1] = 0;
   out_5911240451432872703[2] = 0;
   out_5911240451432872703[3] = 1;
   out_5911240451432872703[4] = 0;
   out_5911240451432872703[5] = 0;
   out_5911240451432872703[6] = 0;
   out_5911240451432872703[7] = 0;
   out_5911240451432872703[8] = 0;
}
void h_29(double *state, double *unused, double *out_7544295603297957966) {
   out_7544295603297957966[0] = state[1];
}
void H_29(double *state, double *unused, double *out_3226245795318055608) {
   out_3226245795318055608[0] = 0;
   out_3226245795318055608[1] = 1;
   out_3226245795318055608[2] = 0;
   out_3226245795318055608[3] = 0;
   out_3226245795318055608[4] = 0;
   out_3226245795318055608[5] = 0;
   out_3226245795318055608[6] = 0;
   out_3226245795318055608[7] = 0;
   out_3226245795318055608[8] = 0;
}
void h_28(double *state, double *unused, double *out_3095916628749130163) {
   out_3095916628749130163[0] = state[0];
}
void H_28(double *state, double *unused, double *out_8308644812387586182) {
   out_8308644812387586182[0] = 1;
   out_8308644812387586182[1] = 0;
   out_8308644812387586182[2] = 0;
   out_8308644812387586182[3] = 0;
   out_8308644812387586182[4] = 0;
   out_8308644812387586182[5] = 0;
   out_8308644812387586182[6] = 0;
   out_8308644812387586182[7] = 0;
   out_8308644812387586182[8] = 0;
}
void h_31(double *state, double *unused, double *out_4956510669045411681) {
   out_4956510669045411681[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7824222554462447497) {
   out_7824222554462447497[0] = 0;
   out_7824222554462447497[1] = 0;
   out_7824222554462447497[2] = 0;
   out_7824222554462447497[3] = 0;
   out_7824222554462447497[4] = 0;
   out_7824222554462447497[5] = 0;
   out_7824222554462447497[6] = 0;
   out_7824222554462447497[7] = 0;
   out_7824222554462447497[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_5662147043452870344) {
  err_fun(nom_x, delta_x, out_5662147043452870344);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2016113788005242335) {
  inv_err_fun(nom_x, true_x, out_2016113788005242335);
}
void car_H_mod_fun(double *state, double *out_8997301641043644665) {
  H_mod_fun(state, out_8997301641043644665);
}
void car_f_fun(double *state, double dt, double *out_2973469614408186678) {
  f_fun(state,  dt, out_2973469614408186678);
}
void car_F_fun(double *state, double dt, double *out_4040592134712196686) {
  F_fun(state,  dt, out_4040592134712196686);
}
void car_h_25(double *state, double *unused, double *out_1319061140791389331) {
  h_25(state, unused, out_1319061140791389331);
}
void car_H_25(double *state, double *unused, double *out_7793576592585487069) {
  H_25(state, unused, out_7793576592585487069);
}
void car_h_24(double *state, double *unused, double *out_3718149663781647640) {
  h_24(state, unused, out_3718149663781647640);
}
void car_H_24(double *state, double *unused, double *out_7095241882321741411) {
  H_24(state, unused, out_7095241882321741411);
}
void car_h_30(double *state, double *unused, double *out_1043867078506883442) {
  h_30(state, unused, out_1043867078506883442);
}
void car_H_30(double *state, double *unused, double *out_3736477139632447792) {
  H_30(state, unused, out_3736477139632447792);
}
void car_h_26(double *state, double *unused, double *out_6922152566065598530) {
  h_26(state, unused, out_6922152566065598530);
}
void car_H_26(double *state, double *unused, double *out_4052073273711430845) {
  H_26(state, unused, out_4052073273711430845);
}
void car_h_27(double *state, double *unused, double *out_7701482271649087111) {
  h_27(state, unused, out_7701482271649087111);
}
void car_H_27(double *state, double *unused, double *out_5911240451432872703) {
  H_27(state, unused, out_5911240451432872703);
}
void car_h_29(double *state, double *unused, double *out_7544295603297957966) {
  h_29(state, unused, out_7544295603297957966);
}
void car_H_29(double *state, double *unused, double *out_3226245795318055608) {
  H_29(state, unused, out_3226245795318055608);
}
void car_h_28(double *state, double *unused, double *out_3095916628749130163) {
  h_28(state, unused, out_3095916628749130163);
}
void car_H_28(double *state, double *unused, double *out_8308644812387586182) {
  H_28(state, unused, out_8308644812387586182);
}
void car_h_31(double *state, double *unused, double *out_4956510669045411681) {
  h_31(state, unused, out_4956510669045411681);
}
void car_H_31(double *state, double *unused, double *out_7824222554462447497) {
  H_31(state, unused, out_7824222554462447497);
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
