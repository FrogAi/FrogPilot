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
void err_fun(double *nom_x, double *delta_x, double *out_1677816508566937490) {
   out_1677816508566937490[0] = delta_x[0] + nom_x[0];
   out_1677816508566937490[1] = delta_x[1] + nom_x[1];
   out_1677816508566937490[2] = delta_x[2] + nom_x[2];
   out_1677816508566937490[3] = delta_x[3] + nom_x[3];
   out_1677816508566937490[4] = delta_x[4] + nom_x[4];
   out_1677816508566937490[5] = delta_x[5] + nom_x[5];
   out_1677816508566937490[6] = delta_x[6] + nom_x[6];
   out_1677816508566937490[7] = delta_x[7] + nom_x[7];
   out_1677816508566937490[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_1895284364952760231) {
   out_1895284364952760231[0] = -nom_x[0] + true_x[0];
   out_1895284364952760231[1] = -nom_x[1] + true_x[1];
   out_1895284364952760231[2] = -nom_x[2] + true_x[2];
   out_1895284364952760231[3] = -nom_x[3] + true_x[3];
   out_1895284364952760231[4] = -nom_x[4] + true_x[4];
   out_1895284364952760231[5] = -nom_x[5] + true_x[5];
   out_1895284364952760231[6] = -nom_x[6] + true_x[6];
   out_1895284364952760231[7] = -nom_x[7] + true_x[7];
   out_1895284364952760231[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_2734817720835474974) {
   out_2734817720835474974[0] = 1.0;
   out_2734817720835474974[1] = 0;
   out_2734817720835474974[2] = 0;
   out_2734817720835474974[3] = 0;
   out_2734817720835474974[4] = 0;
   out_2734817720835474974[5] = 0;
   out_2734817720835474974[6] = 0;
   out_2734817720835474974[7] = 0;
   out_2734817720835474974[8] = 0;
   out_2734817720835474974[9] = 0;
   out_2734817720835474974[10] = 1.0;
   out_2734817720835474974[11] = 0;
   out_2734817720835474974[12] = 0;
   out_2734817720835474974[13] = 0;
   out_2734817720835474974[14] = 0;
   out_2734817720835474974[15] = 0;
   out_2734817720835474974[16] = 0;
   out_2734817720835474974[17] = 0;
   out_2734817720835474974[18] = 0;
   out_2734817720835474974[19] = 0;
   out_2734817720835474974[20] = 1.0;
   out_2734817720835474974[21] = 0;
   out_2734817720835474974[22] = 0;
   out_2734817720835474974[23] = 0;
   out_2734817720835474974[24] = 0;
   out_2734817720835474974[25] = 0;
   out_2734817720835474974[26] = 0;
   out_2734817720835474974[27] = 0;
   out_2734817720835474974[28] = 0;
   out_2734817720835474974[29] = 0;
   out_2734817720835474974[30] = 1.0;
   out_2734817720835474974[31] = 0;
   out_2734817720835474974[32] = 0;
   out_2734817720835474974[33] = 0;
   out_2734817720835474974[34] = 0;
   out_2734817720835474974[35] = 0;
   out_2734817720835474974[36] = 0;
   out_2734817720835474974[37] = 0;
   out_2734817720835474974[38] = 0;
   out_2734817720835474974[39] = 0;
   out_2734817720835474974[40] = 1.0;
   out_2734817720835474974[41] = 0;
   out_2734817720835474974[42] = 0;
   out_2734817720835474974[43] = 0;
   out_2734817720835474974[44] = 0;
   out_2734817720835474974[45] = 0;
   out_2734817720835474974[46] = 0;
   out_2734817720835474974[47] = 0;
   out_2734817720835474974[48] = 0;
   out_2734817720835474974[49] = 0;
   out_2734817720835474974[50] = 1.0;
   out_2734817720835474974[51] = 0;
   out_2734817720835474974[52] = 0;
   out_2734817720835474974[53] = 0;
   out_2734817720835474974[54] = 0;
   out_2734817720835474974[55] = 0;
   out_2734817720835474974[56] = 0;
   out_2734817720835474974[57] = 0;
   out_2734817720835474974[58] = 0;
   out_2734817720835474974[59] = 0;
   out_2734817720835474974[60] = 1.0;
   out_2734817720835474974[61] = 0;
   out_2734817720835474974[62] = 0;
   out_2734817720835474974[63] = 0;
   out_2734817720835474974[64] = 0;
   out_2734817720835474974[65] = 0;
   out_2734817720835474974[66] = 0;
   out_2734817720835474974[67] = 0;
   out_2734817720835474974[68] = 0;
   out_2734817720835474974[69] = 0;
   out_2734817720835474974[70] = 1.0;
   out_2734817720835474974[71] = 0;
   out_2734817720835474974[72] = 0;
   out_2734817720835474974[73] = 0;
   out_2734817720835474974[74] = 0;
   out_2734817720835474974[75] = 0;
   out_2734817720835474974[76] = 0;
   out_2734817720835474974[77] = 0;
   out_2734817720835474974[78] = 0;
   out_2734817720835474974[79] = 0;
   out_2734817720835474974[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2384083579918943231) {
   out_2384083579918943231[0] = state[0];
   out_2384083579918943231[1] = state[1];
   out_2384083579918943231[2] = state[2];
   out_2384083579918943231[3] = state[3];
   out_2384083579918943231[4] = state[4];
   out_2384083579918943231[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2384083579918943231[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2384083579918943231[7] = state[7];
   out_2384083579918943231[8] = state[8];
}
void F_fun(double *state, double dt, double *out_6808241585095737233) {
   out_6808241585095737233[0] = 1;
   out_6808241585095737233[1] = 0;
   out_6808241585095737233[2] = 0;
   out_6808241585095737233[3] = 0;
   out_6808241585095737233[4] = 0;
   out_6808241585095737233[5] = 0;
   out_6808241585095737233[6] = 0;
   out_6808241585095737233[7] = 0;
   out_6808241585095737233[8] = 0;
   out_6808241585095737233[9] = 0;
   out_6808241585095737233[10] = 1;
   out_6808241585095737233[11] = 0;
   out_6808241585095737233[12] = 0;
   out_6808241585095737233[13] = 0;
   out_6808241585095737233[14] = 0;
   out_6808241585095737233[15] = 0;
   out_6808241585095737233[16] = 0;
   out_6808241585095737233[17] = 0;
   out_6808241585095737233[18] = 0;
   out_6808241585095737233[19] = 0;
   out_6808241585095737233[20] = 1;
   out_6808241585095737233[21] = 0;
   out_6808241585095737233[22] = 0;
   out_6808241585095737233[23] = 0;
   out_6808241585095737233[24] = 0;
   out_6808241585095737233[25] = 0;
   out_6808241585095737233[26] = 0;
   out_6808241585095737233[27] = 0;
   out_6808241585095737233[28] = 0;
   out_6808241585095737233[29] = 0;
   out_6808241585095737233[30] = 1;
   out_6808241585095737233[31] = 0;
   out_6808241585095737233[32] = 0;
   out_6808241585095737233[33] = 0;
   out_6808241585095737233[34] = 0;
   out_6808241585095737233[35] = 0;
   out_6808241585095737233[36] = 0;
   out_6808241585095737233[37] = 0;
   out_6808241585095737233[38] = 0;
   out_6808241585095737233[39] = 0;
   out_6808241585095737233[40] = 1;
   out_6808241585095737233[41] = 0;
   out_6808241585095737233[42] = 0;
   out_6808241585095737233[43] = 0;
   out_6808241585095737233[44] = 0;
   out_6808241585095737233[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_6808241585095737233[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_6808241585095737233[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_6808241585095737233[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_6808241585095737233[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_6808241585095737233[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_6808241585095737233[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_6808241585095737233[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_6808241585095737233[53] = -9.8000000000000007*dt;
   out_6808241585095737233[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_6808241585095737233[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_6808241585095737233[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_6808241585095737233[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_6808241585095737233[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_6808241585095737233[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_6808241585095737233[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_6808241585095737233[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_6808241585095737233[62] = 0;
   out_6808241585095737233[63] = 0;
   out_6808241585095737233[64] = 0;
   out_6808241585095737233[65] = 0;
   out_6808241585095737233[66] = 0;
   out_6808241585095737233[67] = 0;
   out_6808241585095737233[68] = 0;
   out_6808241585095737233[69] = 0;
   out_6808241585095737233[70] = 1;
   out_6808241585095737233[71] = 0;
   out_6808241585095737233[72] = 0;
   out_6808241585095737233[73] = 0;
   out_6808241585095737233[74] = 0;
   out_6808241585095737233[75] = 0;
   out_6808241585095737233[76] = 0;
   out_6808241585095737233[77] = 0;
   out_6808241585095737233[78] = 0;
   out_6808241585095737233[79] = 0;
   out_6808241585095737233[80] = 1;
}
void h_25(double *state, double *unused, double *out_5529562536143676140) {
   out_5529562536143676140[0] = state[6];
}
void H_25(double *state, double *unused, double *out_4185841419278708954) {
   out_4185841419278708954[0] = 0;
   out_4185841419278708954[1] = 0;
   out_4185841419278708954[2] = 0;
   out_4185841419278708954[3] = 0;
   out_4185841419278708954[4] = 0;
   out_4185841419278708954[5] = 0;
   out_4185841419278708954[6] = 1;
   out_4185841419278708954[7] = 0;
   out_4185841419278708954[8] = 0;
}
void h_24(double *state, double *unused, double *out_3020037646968069856) {
   out_3020037646968069856[0] = state[4];
   out_3020037646968069856[1] = state[5];
}
void H_24(double *state, double *unused, double *out_7685330847839324561) {
   out_7685330847839324561[0] = 0;
   out_7685330847839324561[1] = 0;
   out_7685330847839324561[2] = 0;
   out_7685330847839324561[3] = 0;
   out_7685330847839324561[4] = 1;
   out_7685330847839324561[5] = 0;
   out_7685330847839324561[6] = 0;
   out_7685330847839324561[7] = 0;
   out_7685330847839324561[8] = 0;
   out_7685330847839324561[9] = 0;
   out_7685330847839324561[10] = 0;
   out_7685330847839324561[11] = 0;
   out_7685330847839324561[12] = 0;
   out_7685330847839324561[13] = 0;
   out_7685330847839324561[14] = 1;
   out_7685330847839324561[15] = 0;
   out_7685330847839324561[16] = 0;
   out_7685330847839324561[17] = 0;
}
void h_30(double *state, double *unused, double *out_9167012064397698490) {
   out_9167012064397698490[0] = state[4];
}
void H_30(double *state, double *unused, double *out_7344212312939225907) {
   out_7344212312939225907[0] = 0;
   out_7344212312939225907[1] = 0;
   out_7344212312939225907[2] = 0;
   out_7344212312939225907[3] = 0;
   out_7344212312939225907[4] = 1;
   out_7344212312939225907[5] = 0;
   out_7344212312939225907[6] = 0;
   out_7344212312939225907[7] = 0;
   out_7344212312939225907[8] = 0;
}
void h_26(double *state, double *unused, double *out_4165338796736852845) {
   out_4165338796736852845[0] = state[7];
}
void H_26(double *state, double *unused, double *out_7490367389039509555) {
   out_7490367389039509555[0] = 0;
   out_7490367389039509555[1] = 0;
   out_7490367389039509555[2] = 0;
   out_7490367389039509555[3] = 0;
   out_7490367389039509555[4] = 0;
   out_7490367389039509555[5] = 0;
   out_7490367389039509555[6] = 0;
   out_7490367389039509555[7] = 1;
   out_7490367389039509555[8] = 0;
}
void h_27(double *state, double *unused, double *out_3452406635863164625) {
   out_3452406635863164625[0] = state[3];
}
void H_27(double *state, double *unused, double *out_8927768448969900798) {
   out_8927768448969900798[0] = 0;
   out_8927768448969900798[1] = 0;
   out_8927768448969900798[2] = 0;
   out_8927768448969900798[3] = 1;
   out_8927768448969900798[4] = 0;
   out_8927768448969900798[5] = 0;
   out_8927768448969900798[6] = 0;
   out_8927768448969900798[7] = 0;
   out_8927768448969900798[8] = 0;
}
void h_29(double *state, double *unused, double *out_8223502211496036055) {
   out_8223502211496036055[0] = state[1];
}
void H_29(double *state, double *unused, double *out_6833980968624833723) {
   out_6833980968624833723[0] = 0;
   out_6833980968624833723[1] = 1;
   out_6833980968624833723[2] = 0;
   out_6833980968624833723[3] = 0;
   out_6833980968624833723[4] = 0;
   out_6833980968624833723[5] = 0;
   out_6833980968624833723[6] = 0;
   out_6833980968624833723[7] = 0;
   out_6833980968624833723[8] = 0;
}
void h_28(double *state, double *unused, double *out_1549266631797681020) {
   out_1549266631797681020[0] = state[0];
}
void H_28(double *state, double *unused, double *out_6530364088015187319) {
   out_6530364088015187319[0] = 1;
   out_6530364088015187319[1] = 0;
   out_6530364088015187319[2] = 0;
   out_6530364088015187319[3] = 0;
   out_6530364088015187319[4] = 0;
   out_6530364088015187319[5] = 0;
   out_6530364088015187319[6] = 0;
   out_6530364088015187319[7] = 0;
   out_6530364088015187319[8] = 0;
}
void h_31(double *state, double *unused, double *out_915048594877817131) {
   out_915048594877817131[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7184227403919025409) {
   out_7184227403919025409[0] = 0;
   out_7184227403919025409[1] = 0;
   out_7184227403919025409[2] = 0;
   out_7184227403919025409[3] = 0;
   out_7184227403919025409[4] = 0;
   out_7184227403919025409[5] = 0;
   out_7184227403919025409[6] = 0;
   out_7184227403919025409[7] = 0;
   out_7184227403919025409[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_1677816508566937490) {
  err_fun(nom_x, delta_x, out_1677816508566937490);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_1895284364952760231) {
  inv_err_fun(nom_x, true_x, out_1895284364952760231);
}
void car_H_mod_fun(double *state, double *out_2734817720835474974) {
  H_mod_fun(state, out_2734817720835474974);
}
void car_f_fun(double *state, double dt, double *out_2384083579918943231) {
  f_fun(state,  dt, out_2384083579918943231);
}
void car_F_fun(double *state, double dt, double *out_6808241585095737233) {
  F_fun(state,  dt, out_6808241585095737233);
}
void car_h_25(double *state, double *unused, double *out_5529562536143676140) {
  h_25(state, unused, out_5529562536143676140);
}
void car_H_25(double *state, double *unused, double *out_4185841419278708954) {
  H_25(state, unused, out_4185841419278708954);
}
void car_h_24(double *state, double *unused, double *out_3020037646968069856) {
  h_24(state, unused, out_3020037646968069856);
}
void car_H_24(double *state, double *unused, double *out_7685330847839324561) {
  H_24(state, unused, out_7685330847839324561);
}
void car_h_30(double *state, double *unused, double *out_9167012064397698490) {
  h_30(state, unused, out_9167012064397698490);
}
void car_H_30(double *state, double *unused, double *out_7344212312939225907) {
  H_30(state, unused, out_7344212312939225907);
}
void car_h_26(double *state, double *unused, double *out_4165338796736852845) {
  h_26(state, unused, out_4165338796736852845);
}
void car_H_26(double *state, double *unused, double *out_7490367389039509555) {
  H_26(state, unused, out_7490367389039509555);
}
void car_h_27(double *state, double *unused, double *out_3452406635863164625) {
  h_27(state, unused, out_3452406635863164625);
}
void car_H_27(double *state, double *unused, double *out_8927768448969900798) {
  H_27(state, unused, out_8927768448969900798);
}
void car_h_29(double *state, double *unused, double *out_8223502211496036055) {
  h_29(state, unused, out_8223502211496036055);
}
void car_H_29(double *state, double *unused, double *out_6833980968624833723) {
  H_29(state, unused, out_6833980968624833723);
}
void car_h_28(double *state, double *unused, double *out_1549266631797681020) {
  h_28(state, unused, out_1549266631797681020);
}
void car_H_28(double *state, double *unused, double *out_6530364088015187319) {
  H_28(state, unused, out_6530364088015187319);
}
void car_h_31(double *state, double *unused, double *out_915048594877817131) {
  h_31(state, unused, out_915048594877817131);
}
void car_H_31(double *state, double *unused, double *out_7184227403919025409) {
  H_31(state, unused, out_7184227403919025409);
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
