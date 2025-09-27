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
void err_fun(double *nom_x, double *delta_x, double *out_4627553483824810991) {
   out_4627553483824810991[0] = delta_x[0] + nom_x[0];
   out_4627553483824810991[1] = delta_x[1] + nom_x[1];
   out_4627553483824810991[2] = delta_x[2] + nom_x[2];
   out_4627553483824810991[3] = delta_x[3] + nom_x[3];
   out_4627553483824810991[4] = delta_x[4] + nom_x[4];
   out_4627553483824810991[5] = delta_x[5] + nom_x[5];
   out_4627553483824810991[6] = delta_x[6] + nom_x[6];
   out_4627553483824810991[7] = delta_x[7] + nom_x[7];
   out_4627553483824810991[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_3034444928256465443) {
   out_3034444928256465443[0] = -nom_x[0] + true_x[0];
   out_3034444928256465443[1] = -nom_x[1] + true_x[1];
   out_3034444928256465443[2] = -nom_x[2] + true_x[2];
   out_3034444928256465443[3] = -nom_x[3] + true_x[3];
   out_3034444928256465443[4] = -nom_x[4] + true_x[4];
   out_3034444928256465443[5] = -nom_x[5] + true_x[5];
   out_3034444928256465443[6] = -nom_x[6] + true_x[6];
   out_3034444928256465443[7] = -nom_x[7] + true_x[7];
   out_3034444928256465443[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_1571749858522309971) {
   out_1571749858522309971[0] = 1.0;
   out_1571749858522309971[1] = 0;
   out_1571749858522309971[2] = 0;
   out_1571749858522309971[3] = 0;
   out_1571749858522309971[4] = 0;
   out_1571749858522309971[5] = 0;
   out_1571749858522309971[6] = 0;
   out_1571749858522309971[7] = 0;
   out_1571749858522309971[8] = 0;
   out_1571749858522309971[9] = 0;
   out_1571749858522309971[10] = 1.0;
   out_1571749858522309971[11] = 0;
   out_1571749858522309971[12] = 0;
   out_1571749858522309971[13] = 0;
   out_1571749858522309971[14] = 0;
   out_1571749858522309971[15] = 0;
   out_1571749858522309971[16] = 0;
   out_1571749858522309971[17] = 0;
   out_1571749858522309971[18] = 0;
   out_1571749858522309971[19] = 0;
   out_1571749858522309971[20] = 1.0;
   out_1571749858522309971[21] = 0;
   out_1571749858522309971[22] = 0;
   out_1571749858522309971[23] = 0;
   out_1571749858522309971[24] = 0;
   out_1571749858522309971[25] = 0;
   out_1571749858522309971[26] = 0;
   out_1571749858522309971[27] = 0;
   out_1571749858522309971[28] = 0;
   out_1571749858522309971[29] = 0;
   out_1571749858522309971[30] = 1.0;
   out_1571749858522309971[31] = 0;
   out_1571749858522309971[32] = 0;
   out_1571749858522309971[33] = 0;
   out_1571749858522309971[34] = 0;
   out_1571749858522309971[35] = 0;
   out_1571749858522309971[36] = 0;
   out_1571749858522309971[37] = 0;
   out_1571749858522309971[38] = 0;
   out_1571749858522309971[39] = 0;
   out_1571749858522309971[40] = 1.0;
   out_1571749858522309971[41] = 0;
   out_1571749858522309971[42] = 0;
   out_1571749858522309971[43] = 0;
   out_1571749858522309971[44] = 0;
   out_1571749858522309971[45] = 0;
   out_1571749858522309971[46] = 0;
   out_1571749858522309971[47] = 0;
   out_1571749858522309971[48] = 0;
   out_1571749858522309971[49] = 0;
   out_1571749858522309971[50] = 1.0;
   out_1571749858522309971[51] = 0;
   out_1571749858522309971[52] = 0;
   out_1571749858522309971[53] = 0;
   out_1571749858522309971[54] = 0;
   out_1571749858522309971[55] = 0;
   out_1571749858522309971[56] = 0;
   out_1571749858522309971[57] = 0;
   out_1571749858522309971[58] = 0;
   out_1571749858522309971[59] = 0;
   out_1571749858522309971[60] = 1.0;
   out_1571749858522309971[61] = 0;
   out_1571749858522309971[62] = 0;
   out_1571749858522309971[63] = 0;
   out_1571749858522309971[64] = 0;
   out_1571749858522309971[65] = 0;
   out_1571749858522309971[66] = 0;
   out_1571749858522309971[67] = 0;
   out_1571749858522309971[68] = 0;
   out_1571749858522309971[69] = 0;
   out_1571749858522309971[70] = 1.0;
   out_1571749858522309971[71] = 0;
   out_1571749858522309971[72] = 0;
   out_1571749858522309971[73] = 0;
   out_1571749858522309971[74] = 0;
   out_1571749858522309971[75] = 0;
   out_1571749858522309971[76] = 0;
   out_1571749858522309971[77] = 0;
   out_1571749858522309971[78] = 0;
   out_1571749858522309971[79] = 0;
   out_1571749858522309971[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_6067441169159859193) {
   out_6067441169159859193[0] = state[0];
   out_6067441169159859193[1] = state[1];
   out_6067441169159859193[2] = state[2];
   out_6067441169159859193[3] = state[3];
   out_6067441169159859193[4] = state[4];
   out_6067441169159859193[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_6067441169159859193[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_6067441169159859193[7] = state[7];
   out_6067441169159859193[8] = state[8];
}
void F_fun(double *state, double dt, double *out_7765987793843153008) {
   out_7765987793843153008[0] = 1;
   out_7765987793843153008[1] = 0;
   out_7765987793843153008[2] = 0;
   out_7765987793843153008[3] = 0;
   out_7765987793843153008[4] = 0;
   out_7765987793843153008[5] = 0;
   out_7765987793843153008[6] = 0;
   out_7765987793843153008[7] = 0;
   out_7765987793843153008[8] = 0;
   out_7765987793843153008[9] = 0;
   out_7765987793843153008[10] = 1;
   out_7765987793843153008[11] = 0;
   out_7765987793843153008[12] = 0;
   out_7765987793843153008[13] = 0;
   out_7765987793843153008[14] = 0;
   out_7765987793843153008[15] = 0;
   out_7765987793843153008[16] = 0;
   out_7765987793843153008[17] = 0;
   out_7765987793843153008[18] = 0;
   out_7765987793843153008[19] = 0;
   out_7765987793843153008[20] = 1;
   out_7765987793843153008[21] = 0;
   out_7765987793843153008[22] = 0;
   out_7765987793843153008[23] = 0;
   out_7765987793843153008[24] = 0;
   out_7765987793843153008[25] = 0;
   out_7765987793843153008[26] = 0;
   out_7765987793843153008[27] = 0;
   out_7765987793843153008[28] = 0;
   out_7765987793843153008[29] = 0;
   out_7765987793843153008[30] = 1;
   out_7765987793843153008[31] = 0;
   out_7765987793843153008[32] = 0;
   out_7765987793843153008[33] = 0;
   out_7765987793843153008[34] = 0;
   out_7765987793843153008[35] = 0;
   out_7765987793843153008[36] = 0;
   out_7765987793843153008[37] = 0;
   out_7765987793843153008[38] = 0;
   out_7765987793843153008[39] = 0;
   out_7765987793843153008[40] = 1;
   out_7765987793843153008[41] = 0;
   out_7765987793843153008[42] = 0;
   out_7765987793843153008[43] = 0;
   out_7765987793843153008[44] = 0;
   out_7765987793843153008[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_7765987793843153008[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_7765987793843153008[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7765987793843153008[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7765987793843153008[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_7765987793843153008[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_7765987793843153008[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_7765987793843153008[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_7765987793843153008[53] = -9.8000000000000007*dt;
   out_7765987793843153008[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_7765987793843153008[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_7765987793843153008[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7765987793843153008[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7765987793843153008[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_7765987793843153008[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_7765987793843153008[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_7765987793843153008[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7765987793843153008[62] = 0;
   out_7765987793843153008[63] = 0;
   out_7765987793843153008[64] = 0;
   out_7765987793843153008[65] = 0;
   out_7765987793843153008[66] = 0;
   out_7765987793843153008[67] = 0;
   out_7765987793843153008[68] = 0;
   out_7765987793843153008[69] = 0;
   out_7765987793843153008[70] = 1;
   out_7765987793843153008[71] = 0;
   out_7765987793843153008[72] = 0;
   out_7765987793843153008[73] = 0;
   out_7765987793843153008[74] = 0;
   out_7765987793843153008[75] = 0;
   out_7765987793843153008[76] = 0;
   out_7765987793843153008[77] = 0;
   out_7765987793843153008[78] = 0;
   out_7765987793843153008[79] = 0;
   out_7765987793843153008[80] = 1;
}
void h_25(double *state, double *unused, double *out_3070103496777372817) {
   out_3070103496777372817[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7817550884622297004) {
   out_7817550884622297004[0] = 0;
   out_7817550884622297004[1] = 0;
   out_7817550884622297004[2] = 0;
   out_7817550884622297004[3] = 0;
   out_7817550884622297004[4] = 0;
   out_7817550884622297004[5] = 0;
   out_7817550884622297004[6] = 1;
   out_7817550884622297004[7] = 0;
   out_7817550884622297004[8] = 0;
}
void h_24(double *state, double *unused, double *out_732461673771864771) {
   out_732461673771864771[0] = state[4];
   out_732461673771864771[1] = state[5];
}
void H_24(double *state, double *unused, double *out_567694474361489429) {
   out_567694474361489429[0] = 0;
   out_567694474361489429[1] = 0;
   out_567694474361489429[2] = 0;
   out_567694474361489429[3] = 0;
   out_567694474361489429[4] = 1;
   out_567694474361489429[5] = 0;
   out_567694474361489429[6] = 0;
   out_567694474361489429[7] = 0;
   out_567694474361489429[8] = 0;
   out_567694474361489429[9] = 0;
   out_567694474361489429[10] = 0;
   out_567694474361489429[11] = 0;
   out_567694474361489429[12] = 0;
   out_567694474361489429[13] = 0;
   out_567694474361489429[14] = 1;
   out_567694474361489429[15] = 0;
   out_567694474361489429[16] = 0;
   out_567694474361489429[17] = 0;
}
void h_30(double *state, double *unused, double *out_3345297559061878706) {
   out_3345297559061878706[0] = state[4];
}
void H_30(double *state, double *unused, double *out_5299217926115048377) {
   out_5299217926115048377[0] = 0;
   out_5299217926115048377[1] = 0;
   out_5299217926115048377[2] = 0;
   out_5299217926115048377[3] = 0;
   out_5299217926115048377[4] = 1;
   out_5299217926115048377[5] = 0;
   out_5299217926115048377[6] = 0;
   out_5299217926115048377[7] = 0;
   out_5299217926115048377[8] = 0;
}
void h_26(double *state, double *unused, double *out_1880609656773446241) {
   out_1880609656773446241[0] = state[7];
}
void H_26(double *state, double *unused, double *out_6887689870213198388) {
   out_6887689870213198388[0] = 0;
   out_6887689870213198388[1] = 0;
   out_6887689870213198388[2] = 0;
   out_6887689870213198388[3] = 0;
   out_6887689870213198388[4] = 0;
   out_6887689870213198388[5] = 0;
   out_6887689870213198388[6] = 0;
   out_6887689870213198388[7] = 1;
   out_6887689870213198388[8] = 0;
}
void h_27(double *state, double *unused, double *out_8948388984336087905) {
   out_8948388984336087905[0] = state[3];
}
void H_27(double *state, double *unused, double *out_7473981237915473288) {
   out_7473981237915473288[0] = 0;
   out_7473981237915473288[1] = 0;
   out_7473981237915473288[2] = 0;
   out_7473981237915473288[3] = 1;
   out_7473981237915473288[4] = 0;
   out_7473981237915473288[5] = 0;
   out_7473981237915473288[6] = 0;
   out_7473981237915473288[7] = 0;
   out_7473981237915473288[8] = 0;
}
void h_29(double *state, double *unused, double *out_8346970826722724351) {
   out_8346970826722724351[0] = state[1];
}
void H_29(double *state, double *unused, double *out_4788986581800656193) {
   out_4788986581800656193[0] = 0;
   out_4788986581800656193[1] = 1;
   out_4788986581800656193[2] = 0;
   out_4788986581800656193[3] = 0;
   out_4788986581800656193[4] = 0;
   out_4788986581800656193[5] = 0;
   out_4788986581800656193[6] = 0;
   out_4788986581800656193[7] = 0;
   out_4788986581800656193[8] = 0;
}
void h_28(double *state, double *unused, double *out_8895039204696986194) {
   out_8895039204696986194[0] = state[0];
}
void H_28(double *state, double *unused, double *out_8575358474839364849) {
   out_8575358474839364849[0] = 1;
   out_8575358474839364849[1] = 0;
   out_8575358474839364849[2] = 0;
   out_8575358474839364849[3] = 0;
   out_8575358474839364849[4] = 0;
   out_8575358474839364849[5] = 0;
   out_8575358474839364849[6] = 0;
   out_8575358474839364849[7] = 0;
   out_8575358474839364849[8] = 0;
}
void h_31(double *state, double *unused, double *out_7483701082047655440) {
   out_7483701082047655440[0] = state[8];
}
void H_31(double *state, double *unused, double *out_6261481767979846912) {
   out_6261481767979846912[0] = 0;
   out_6261481767979846912[1] = 0;
   out_6261481767979846912[2] = 0;
   out_6261481767979846912[3] = 0;
   out_6261481767979846912[4] = 0;
   out_6261481767979846912[5] = 0;
   out_6261481767979846912[6] = 0;
   out_6261481767979846912[7] = 0;
   out_6261481767979846912[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_4627553483824810991) {
  err_fun(nom_x, delta_x, out_4627553483824810991);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_3034444928256465443) {
  inv_err_fun(nom_x, true_x, out_3034444928256465443);
}
void car_H_mod_fun(double *state, double *out_1571749858522309971) {
  H_mod_fun(state, out_1571749858522309971);
}
void car_f_fun(double *state, double dt, double *out_6067441169159859193) {
  f_fun(state,  dt, out_6067441169159859193);
}
void car_F_fun(double *state, double dt, double *out_7765987793843153008) {
  F_fun(state,  dt, out_7765987793843153008);
}
void car_h_25(double *state, double *unused, double *out_3070103496777372817) {
  h_25(state, unused, out_3070103496777372817);
}
void car_H_25(double *state, double *unused, double *out_7817550884622297004) {
  H_25(state, unused, out_7817550884622297004);
}
void car_h_24(double *state, double *unused, double *out_732461673771864771) {
  h_24(state, unused, out_732461673771864771);
}
void car_H_24(double *state, double *unused, double *out_567694474361489429) {
  H_24(state, unused, out_567694474361489429);
}
void car_h_30(double *state, double *unused, double *out_3345297559061878706) {
  h_30(state, unused, out_3345297559061878706);
}
void car_H_30(double *state, double *unused, double *out_5299217926115048377) {
  H_30(state, unused, out_5299217926115048377);
}
void car_h_26(double *state, double *unused, double *out_1880609656773446241) {
  h_26(state, unused, out_1880609656773446241);
}
void car_H_26(double *state, double *unused, double *out_6887689870213198388) {
  H_26(state, unused, out_6887689870213198388);
}
void car_h_27(double *state, double *unused, double *out_8948388984336087905) {
  h_27(state, unused, out_8948388984336087905);
}
void car_H_27(double *state, double *unused, double *out_7473981237915473288) {
  H_27(state, unused, out_7473981237915473288);
}
void car_h_29(double *state, double *unused, double *out_8346970826722724351) {
  h_29(state, unused, out_8346970826722724351);
}
void car_H_29(double *state, double *unused, double *out_4788986581800656193) {
  H_29(state, unused, out_4788986581800656193);
}
void car_h_28(double *state, double *unused, double *out_8895039204696986194) {
  h_28(state, unused, out_8895039204696986194);
}
void car_H_28(double *state, double *unused, double *out_8575358474839364849) {
  H_28(state, unused, out_8575358474839364849);
}
void car_h_31(double *state, double *unused, double *out_7483701082047655440) {
  h_31(state, unused, out_7483701082047655440);
}
void car_H_31(double *state, double *unused, double *out_6261481767979846912) {
  H_31(state, unused, out_6261481767979846912);
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
