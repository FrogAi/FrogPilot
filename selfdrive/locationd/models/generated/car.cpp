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
void err_fun(double *nom_x, double *delta_x, double *out_9029221129050068959) {
   out_9029221129050068959[0] = delta_x[0] + nom_x[0];
   out_9029221129050068959[1] = delta_x[1] + nom_x[1];
   out_9029221129050068959[2] = delta_x[2] + nom_x[2];
   out_9029221129050068959[3] = delta_x[3] + nom_x[3];
   out_9029221129050068959[4] = delta_x[4] + nom_x[4];
   out_9029221129050068959[5] = delta_x[5] + nom_x[5];
   out_9029221129050068959[6] = delta_x[6] + nom_x[6];
   out_9029221129050068959[7] = delta_x[7] + nom_x[7];
   out_9029221129050068959[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_6139230198769637759) {
   out_6139230198769637759[0] = -nom_x[0] + true_x[0];
   out_6139230198769637759[1] = -nom_x[1] + true_x[1];
   out_6139230198769637759[2] = -nom_x[2] + true_x[2];
   out_6139230198769637759[3] = -nom_x[3] + true_x[3];
   out_6139230198769637759[4] = -nom_x[4] + true_x[4];
   out_6139230198769637759[5] = -nom_x[5] + true_x[5];
   out_6139230198769637759[6] = -nom_x[6] + true_x[6];
   out_6139230198769637759[7] = -nom_x[7] + true_x[7];
   out_6139230198769637759[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_4005708931311870903) {
   out_4005708931311870903[0] = 1.0;
   out_4005708931311870903[1] = 0;
   out_4005708931311870903[2] = 0;
   out_4005708931311870903[3] = 0;
   out_4005708931311870903[4] = 0;
   out_4005708931311870903[5] = 0;
   out_4005708931311870903[6] = 0;
   out_4005708931311870903[7] = 0;
   out_4005708931311870903[8] = 0;
   out_4005708931311870903[9] = 0;
   out_4005708931311870903[10] = 1.0;
   out_4005708931311870903[11] = 0;
   out_4005708931311870903[12] = 0;
   out_4005708931311870903[13] = 0;
   out_4005708931311870903[14] = 0;
   out_4005708931311870903[15] = 0;
   out_4005708931311870903[16] = 0;
   out_4005708931311870903[17] = 0;
   out_4005708931311870903[18] = 0;
   out_4005708931311870903[19] = 0;
   out_4005708931311870903[20] = 1.0;
   out_4005708931311870903[21] = 0;
   out_4005708931311870903[22] = 0;
   out_4005708931311870903[23] = 0;
   out_4005708931311870903[24] = 0;
   out_4005708931311870903[25] = 0;
   out_4005708931311870903[26] = 0;
   out_4005708931311870903[27] = 0;
   out_4005708931311870903[28] = 0;
   out_4005708931311870903[29] = 0;
   out_4005708931311870903[30] = 1.0;
   out_4005708931311870903[31] = 0;
   out_4005708931311870903[32] = 0;
   out_4005708931311870903[33] = 0;
   out_4005708931311870903[34] = 0;
   out_4005708931311870903[35] = 0;
   out_4005708931311870903[36] = 0;
   out_4005708931311870903[37] = 0;
   out_4005708931311870903[38] = 0;
   out_4005708931311870903[39] = 0;
   out_4005708931311870903[40] = 1.0;
   out_4005708931311870903[41] = 0;
   out_4005708931311870903[42] = 0;
   out_4005708931311870903[43] = 0;
   out_4005708931311870903[44] = 0;
   out_4005708931311870903[45] = 0;
   out_4005708931311870903[46] = 0;
   out_4005708931311870903[47] = 0;
   out_4005708931311870903[48] = 0;
   out_4005708931311870903[49] = 0;
   out_4005708931311870903[50] = 1.0;
   out_4005708931311870903[51] = 0;
   out_4005708931311870903[52] = 0;
   out_4005708931311870903[53] = 0;
   out_4005708931311870903[54] = 0;
   out_4005708931311870903[55] = 0;
   out_4005708931311870903[56] = 0;
   out_4005708931311870903[57] = 0;
   out_4005708931311870903[58] = 0;
   out_4005708931311870903[59] = 0;
   out_4005708931311870903[60] = 1.0;
   out_4005708931311870903[61] = 0;
   out_4005708931311870903[62] = 0;
   out_4005708931311870903[63] = 0;
   out_4005708931311870903[64] = 0;
   out_4005708931311870903[65] = 0;
   out_4005708931311870903[66] = 0;
   out_4005708931311870903[67] = 0;
   out_4005708931311870903[68] = 0;
   out_4005708931311870903[69] = 0;
   out_4005708931311870903[70] = 1.0;
   out_4005708931311870903[71] = 0;
   out_4005708931311870903[72] = 0;
   out_4005708931311870903[73] = 0;
   out_4005708931311870903[74] = 0;
   out_4005708931311870903[75] = 0;
   out_4005708931311870903[76] = 0;
   out_4005708931311870903[77] = 0;
   out_4005708931311870903[78] = 0;
   out_4005708931311870903[79] = 0;
   out_4005708931311870903[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_708450726625971407) {
   out_708450726625971407[0] = state[0];
   out_708450726625971407[1] = state[1];
   out_708450726625971407[2] = state[2];
   out_708450726625971407[3] = state[3];
   out_708450726625971407[4] = state[4];
   out_708450726625971407[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_708450726625971407[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_708450726625971407[7] = state[7];
   out_708450726625971407[8] = state[8];
}
void F_fun(double *state, double dt, double *out_4447378851817122637) {
   out_4447378851817122637[0] = 1;
   out_4447378851817122637[1] = 0;
   out_4447378851817122637[2] = 0;
   out_4447378851817122637[3] = 0;
   out_4447378851817122637[4] = 0;
   out_4447378851817122637[5] = 0;
   out_4447378851817122637[6] = 0;
   out_4447378851817122637[7] = 0;
   out_4447378851817122637[8] = 0;
   out_4447378851817122637[9] = 0;
   out_4447378851817122637[10] = 1;
   out_4447378851817122637[11] = 0;
   out_4447378851817122637[12] = 0;
   out_4447378851817122637[13] = 0;
   out_4447378851817122637[14] = 0;
   out_4447378851817122637[15] = 0;
   out_4447378851817122637[16] = 0;
   out_4447378851817122637[17] = 0;
   out_4447378851817122637[18] = 0;
   out_4447378851817122637[19] = 0;
   out_4447378851817122637[20] = 1;
   out_4447378851817122637[21] = 0;
   out_4447378851817122637[22] = 0;
   out_4447378851817122637[23] = 0;
   out_4447378851817122637[24] = 0;
   out_4447378851817122637[25] = 0;
   out_4447378851817122637[26] = 0;
   out_4447378851817122637[27] = 0;
   out_4447378851817122637[28] = 0;
   out_4447378851817122637[29] = 0;
   out_4447378851817122637[30] = 1;
   out_4447378851817122637[31] = 0;
   out_4447378851817122637[32] = 0;
   out_4447378851817122637[33] = 0;
   out_4447378851817122637[34] = 0;
   out_4447378851817122637[35] = 0;
   out_4447378851817122637[36] = 0;
   out_4447378851817122637[37] = 0;
   out_4447378851817122637[38] = 0;
   out_4447378851817122637[39] = 0;
   out_4447378851817122637[40] = 1;
   out_4447378851817122637[41] = 0;
   out_4447378851817122637[42] = 0;
   out_4447378851817122637[43] = 0;
   out_4447378851817122637[44] = 0;
   out_4447378851817122637[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_4447378851817122637[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_4447378851817122637[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4447378851817122637[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_4447378851817122637[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_4447378851817122637[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_4447378851817122637[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_4447378851817122637[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_4447378851817122637[53] = -9.8000000000000007*dt;
   out_4447378851817122637[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_4447378851817122637[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_4447378851817122637[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4447378851817122637[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4447378851817122637[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_4447378851817122637[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_4447378851817122637[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_4447378851817122637[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_4447378851817122637[62] = 0;
   out_4447378851817122637[63] = 0;
   out_4447378851817122637[64] = 0;
   out_4447378851817122637[65] = 0;
   out_4447378851817122637[66] = 0;
   out_4447378851817122637[67] = 0;
   out_4447378851817122637[68] = 0;
   out_4447378851817122637[69] = 0;
   out_4447378851817122637[70] = 1;
   out_4447378851817122637[71] = 0;
   out_4447378851817122637[72] = 0;
   out_4447378851817122637[73] = 0;
   out_4447378851817122637[74] = 0;
   out_4447378851817122637[75] = 0;
   out_4447378851817122637[76] = 0;
   out_4447378851817122637[77] = 0;
   out_4447378851817122637[78] = 0;
   out_4447378851817122637[79] = 0;
   out_4447378851817122637[80] = 1;
}
void h_25(double *state, double *unused, double *out_4251623014853923117) {
   out_4251623014853923117[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7019278493219463555) {
   out_7019278493219463555[0] = 0;
   out_7019278493219463555[1] = 0;
   out_7019278493219463555[2] = 0;
   out_7019278493219463555[3] = 0;
   out_7019278493219463555[4] = 0;
   out_7019278493219463555[5] = 0;
   out_7019278493219463555[6] = 1;
   out_7019278493219463555[7] = 0;
   out_7019278493219463555[8] = 0;
}
void h_24(double *state, double *unused, double *out_632683078013282463) {
   out_632683078013282463[0] = state[4];
   out_632683078013282463[1] = state[5];
}
void H_24(double *state, double *unused, double *out_7869539981687764925) {
   out_7869539981687764925[0] = 0;
   out_7869539981687764925[1] = 0;
   out_7869539981687764925[2] = 0;
   out_7869539981687764925[3] = 0;
   out_7869539981687764925[4] = 1;
   out_7869539981687764925[5] = 0;
   out_7869539981687764925[6] = 0;
   out_7869539981687764925[7] = 0;
   out_7869539981687764925[8] = 0;
   out_7869539981687764925[9] = 0;
   out_7869539981687764925[10] = 0;
   out_7869539981687764925[11] = 0;
   out_7869539981687764925[12] = 0;
   out_7869539981687764925[13] = 0;
   out_7869539981687764925[14] = 1;
   out_7869539981687764925[15] = 0;
   out_7869539981687764925[16] = 0;
   out_7869539981687764925[17] = 0;
}
void h_30(double *state, double *unused, double *out_4200662900743896530) {
   out_4200662900743896530[0] = state[4];
}
void H_30(double *state, double *unused, double *out_4510775238998471306) {
   out_4510775238998471306[0] = 0;
   out_4510775238998471306[1] = 0;
   out_4510775238998471306[2] = 0;
   out_4510775238998471306[3] = 0;
   out_4510775238998471306[4] = 1;
   out_4510775238998471306[5] = 0;
   out_4510775238998471306[6] = 0;
   out_4510775238998471306[7] = 0;
   out_4510775238998471306[8] = 0;
}
void h_26(double *state, double *unused, double *out_8367795685406939998) {
   out_8367795685406939998[0] = state[7];
}
void H_26(double *state, double *unused, double *out_3277775174345407331) {
   out_3277775174345407331[0] = 0;
   out_3277775174345407331[1] = 0;
   out_3277775174345407331[2] = 0;
   out_3277775174345407331[3] = 0;
   out_3277775174345407331[4] = 0;
   out_3277775174345407331[5] = 0;
   out_3277775174345407331[6] = 0;
   out_3277775174345407331[7] = 1;
   out_3277775174345407331[8] = 0;
}
void h_27(double *state, double *unused, double *out_3648307130177445600) {
   out_3648307130177445600[0] = state[3];
}
void H_27(double *state, double *unused, double *out_6685538550798896217) {
   out_6685538550798896217[0] = 0;
   out_6685538550798896217[1] = 0;
   out_6685538550798896217[2] = 0;
   out_6685538550798896217[3] = 1;
   out_6685538550798896217[4] = 0;
   out_6685538550798896217[5] = 0;
   out_6685538550798896217[6] = 0;
   out_6685538550798896217[7] = 0;
   out_6685538550798896217[8] = 0;
}
void h_29(double *state, double *unused, double *out_8027601717181755080) {
   out_8027601717181755080[0] = state[1];
}
void H_29(double *state, double *unused, double *out_8398901277668447250) {
   out_8398901277668447250[0] = 0;
   out_8398901277668447250[1] = 1;
   out_8398901277668447250[2] = 0;
   out_8398901277668447250[3] = 0;
   out_8398901277668447250[4] = 0;
   out_8398901277668447250[5] = 0;
   out_8398901277668447250[6] = 0;
   out_8398901277668447250[7] = 0;
   out_8398901277668447250[8] = 0;
}
void h_28(double *state, double *unused, double *out_6252712450986143251) {
   out_6252712450986143251[0] = state[0];
}
void H_28(double *state, double *unused, double *out_4965443778971573792) {
   out_4965443778971573792[0] = 1;
   out_4965443778971573792[1] = 0;
   out_4965443778971573792[2] = 0;
   out_4965443778971573792[3] = 0;
   out_4965443778971573792[4] = 0;
   out_4965443778971573792[5] = 0;
   out_4965443778971573792[6] = 0;
   out_4965443778971573792[7] = 0;
   out_4965443778971573792[8] = 0;
}
void h_31(double *state, double *unused, double *out_8113306491282424769) {
   out_8113306491282424769[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7049924455096423983) {
   out_7049924455096423983[0] = 0;
   out_7049924455096423983[1] = 0;
   out_7049924455096423983[2] = 0;
   out_7049924455096423983[3] = 0;
   out_7049924455096423983[4] = 0;
   out_7049924455096423983[5] = 0;
   out_7049924455096423983[6] = 0;
   out_7049924455096423983[7] = 0;
   out_7049924455096423983[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_9029221129050068959) {
  err_fun(nom_x, delta_x, out_9029221129050068959);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_6139230198769637759) {
  inv_err_fun(nom_x, true_x, out_6139230198769637759);
}
void car_H_mod_fun(double *state, double *out_4005708931311870903) {
  H_mod_fun(state, out_4005708931311870903);
}
void car_f_fun(double *state, double dt, double *out_708450726625971407) {
  f_fun(state,  dt, out_708450726625971407);
}
void car_F_fun(double *state, double dt, double *out_4447378851817122637) {
  F_fun(state,  dt, out_4447378851817122637);
}
void car_h_25(double *state, double *unused, double *out_4251623014853923117) {
  h_25(state, unused, out_4251623014853923117);
}
void car_H_25(double *state, double *unused, double *out_7019278493219463555) {
  H_25(state, unused, out_7019278493219463555);
}
void car_h_24(double *state, double *unused, double *out_632683078013282463) {
  h_24(state, unused, out_632683078013282463);
}
void car_H_24(double *state, double *unused, double *out_7869539981687764925) {
  H_24(state, unused, out_7869539981687764925);
}
void car_h_30(double *state, double *unused, double *out_4200662900743896530) {
  h_30(state, unused, out_4200662900743896530);
}
void car_H_30(double *state, double *unused, double *out_4510775238998471306) {
  H_30(state, unused, out_4510775238998471306);
}
void car_h_26(double *state, double *unused, double *out_8367795685406939998) {
  h_26(state, unused, out_8367795685406939998);
}
void car_H_26(double *state, double *unused, double *out_3277775174345407331) {
  H_26(state, unused, out_3277775174345407331);
}
void car_h_27(double *state, double *unused, double *out_3648307130177445600) {
  h_27(state, unused, out_3648307130177445600);
}
void car_H_27(double *state, double *unused, double *out_6685538550798896217) {
  H_27(state, unused, out_6685538550798896217);
}
void car_h_29(double *state, double *unused, double *out_8027601717181755080) {
  h_29(state, unused, out_8027601717181755080);
}
void car_H_29(double *state, double *unused, double *out_8398901277668447250) {
  H_29(state, unused, out_8398901277668447250);
}
void car_h_28(double *state, double *unused, double *out_6252712450986143251) {
  h_28(state, unused, out_6252712450986143251);
}
void car_H_28(double *state, double *unused, double *out_4965443778971573792) {
  H_28(state, unused, out_4965443778971573792);
}
void car_h_31(double *state, double *unused, double *out_8113306491282424769) {
  h_31(state, unused, out_8113306491282424769);
}
void car_H_31(double *state, double *unused, double *out_7049924455096423983) {
  H_31(state, unused, out_7049924455096423983);
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
