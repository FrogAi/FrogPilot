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
void err_fun(double *nom_x, double *delta_x, double *out_5279200563100949282) {
   out_5279200563100949282[0] = delta_x[0] + nom_x[0];
   out_5279200563100949282[1] = delta_x[1] + nom_x[1];
   out_5279200563100949282[2] = delta_x[2] + nom_x[2];
   out_5279200563100949282[3] = delta_x[3] + nom_x[3];
   out_5279200563100949282[4] = delta_x[4] + nom_x[4];
   out_5279200563100949282[5] = delta_x[5] + nom_x[5];
   out_5279200563100949282[6] = delta_x[6] + nom_x[6];
   out_5279200563100949282[7] = delta_x[7] + nom_x[7];
   out_5279200563100949282[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_2794898786486738948) {
   out_2794898786486738948[0] = -nom_x[0] + true_x[0];
   out_2794898786486738948[1] = -nom_x[1] + true_x[1];
   out_2794898786486738948[2] = -nom_x[2] + true_x[2];
   out_2794898786486738948[3] = -nom_x[3] + true_x[3];
   out_2794898786486738948[4] = -nom_x[4] + true_x[4];
   out_2794898786486738948[5] = -nom_x[5] + true_x[5];
   out_2794898786486738948[6] = -nom_x[6] + true_x[6];
   out_2794898786486738948[7] = -nom_x[7] + true_x[7];
   out_2794898786486738948[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_3002480874372569413) {
   out_3002480874372569413[0] = 1.0;
   out_3002480874372569413[1] = 0;
   out_3002480874372569413[2] = 0;
   out_3002480874372569413[3] = 0;
   out_3002480874372569413[4] = 0;
   out_3002480874372569413[5] = 0;
   out_3002480874372569413[6] = 0;
   out_3002480874372569413[7] = 0;
   out_3002480874372569413[8] = 0;
   out_3002480874372569413[9] = 0;
   out_3002480874372569413[10] = 1.0;
   out_3002480874372569413[11] = 0;
   out_3002480874372569413[12] = 0;
   out_3002480874372569413[13] = 0;
   out_3002480874372569413[14] = 0;
   out_3002480874372569413[15] = 0;
   out_3002480874372569413[16] = 0;
   out_3002480874372569413[17] = 0;
   out_3002480874372569413[18] = 0;
   out_3002480874372569413[19] = 0;
   out_3002480874372569413[20] = 1.0;
   out_3002480874372569413[21] = 0;
   out_3002480874372569413[22] = 0;
   out_3002480874372569413[23] = 0;
   out_3002480874372569413[24] = 0;
   out_3002480874372569413[25] = 0;
   out_3002480874372569413[26] = 0;
   out_3002480874372569413[27] = 0;
   out_3002480874372569413[28] = 0;
   out_3002480874372569413[29] = 0;
   out_3002480874372569413[30] = 1.0;
   out_3002480874372569413[31] = 0;
   out_3002480874372569413[32] = 0;
   out_3002480874372569413[33] = 0;
   out_3002480874372569413[34] = 0;
   out_3002480874372569413[35] = 0;
   out_3002480874372569413[36] = 0;
   out_3002480874372569413[37] = 0;
   out_3002480874372569413[38] = 0;
   out_3002480874372569413[39] = 0;
   out_3002480874372569413[40] = 1.0;
   out_3002480874372569413[41] = 0;
   out_3002480874372569413[42] = 0;
   out_3002480874372569413[43] = 0;
   out_3002480874372569413[44] = 0;
   out_3002480874372569413[45] = 0;
   out_3002480874372569413[46] = 0;
   out_3002480874372569413[47] = 0;
   out_3002480874372569413[48] = 0;
   out_3002480874372569413[49] = 0;
   out_3002480874372569413[50] = 1.0;
   out_3002480874372569413[51] = 0;
   out_3002480874372569413[52] = 0;
   out_3002480874372569413[53] = 0;
   out_3002480874372569413[54] = 0;
   out_3002480874372569413[55] = 0;
   out_3002480874372569413[56] = 0;
   out_3002480874372569413[57] = 0;
   out_3002480874372569413[58] = 0;
   out_3002480874372569413[59] = 0;
   out_3002480874372569413[60] = 1.0;
   out_3002480874372569413[61] = 0;
   out_3002480874372569413[62] = 0;
   out_3002480874372569413[63] = 0;
   out_3002480874372569413[64] = 0;
   out_3002480874372569413[65] = 0;
   out_3002480874372569413[66] = 0;
   out_3002480874372569413[67] = 0;
   out_3002480874372569413[68] = 0;
   out_3002480874372569413[69] = 0;
   out_3002480874372569413[70] = 1.0;
   out_3002480874372569413[71] = 0;
   out_3002480874372569413[72] = 0;
   out_3002480874372569413[73] = 0;
   out_3002480874372569413[74] = 0;
   out_3002480874372569413[75] = 0;
   out_3002480874372569413[76] = 0;
   out_3002480874372569413[77] = 0;
   out_3002480874372569413[78] = 0;
   out_3002480874372569413[79] = 0;
   out_3002480874372569413[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_8611914425259886085) {
   out_8611914425259886085[0] = state[0];
   out_8611914425259886085[1] = state[1];
   out_8611914425259886085[2] = state[2];
   out_8611914425259886085[3] = state[3];
   out_8611914425259886085[4] = state[4];
   out_8611914425259886085[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_8611914425259886085[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_8611914425259886085[7] = state[7];
   out_8611914425259886085[8] = state[8];
}
void F_fun(double *state, double dt, double *out_8493233740844108942) {
   out_8493233740844108942[0] = 1;
   out_8493233740844108942[1] = 0;
   out_8493233740844108942[2] = 0;
   out_8493233740844108942[3] = 0;
   out_8493233740844108942[4] = 0;
   out_8493233740844108942[5] = 0;
   out_8493233740844108942[6] = 0;
   out_8493233740844108942[7] = 0;
   out_8493233740844108942[8] = 0;
   out_8493233740844108942[9] = 0;
   out_8493233740844108942[10] = 1;
   out_8493233740844108942[11] = 0;
   out_8493233740844108942[12] = 0;
   out_8493233740844108942[13] = 0;
   out_8493233740844108942[14] = 0;
   out_8493233740844108942[15] = 0;
   out_8493233740844108942[16] = 0;
   out_8493233740844108942[17] = 0;
   out_8493233740844108942[18] = 0;
   out_8493233740844108942[19] = 0;
   out_8493233740844108942[20] = 1;
   out_8493233740844108942[21] = 0;
   out_8493233740844108942[22] = 0;
   out_8493233740844108942[23] = 0;
   out_8493233740844108942[24] = 0;
   out_8493233740844108942[25] = 0;
   out_8493233740844108942[26] = 0;
   out_8493233740844108942[27] = 0;
   out_8493233740844108942[28] = 0;
   out_8493233740844108942[29] = 0;
   out_8493233740844108942[30] = 1;
   out_8493233740844108942[31] = 0;
   out_8493233740844108942[32] = 0;
   out_8493233740844108942[33] = 0;
   out_8493233740844108942[34] = 0;
   out_8493233740844108942[35] = 0;
   out_8493233740844108942[36] = 0;
   out_8493233740844108942[37] = 0;
   out_8493233740844108942[38] = 0;
   out_8493233740844108942[39] = 0;
   out_8493233740844108942[40] = 1;
   out_8493233740844108942[41] = 0;
   out_8493233740844108942[42] = 0;
   out_8493233740844108942[43] = 0;
   out_8493233740844108942[44] = 0;
   out_8493233740844108942[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_8493233740844108942[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_8493233740844108942[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8493233740844108942[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_8493233740844108942[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_8493233740844108942[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_8493233740844108942[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_8493233740844108942[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_8493233740844108942[53] = -9.8000000000000007*dt;
   out_8493233740844108942[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_8493233740844108942[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_8493233740844108942[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8493233740844108942[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8493233740844108942[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_8493233740844108942[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_8493233740844108942[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_8493233740844108942[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_8493233740844108942[62] = 0;
   out_8493233740844108942[63] = 0;
   out_8493233740844108942[64] = 0;
   out_8493233740844108942[65] = 0;
   out_8493233740844108942[66] = 0;
   out_8493233740844108942[67] = 0;
   out_8493233740844108942[68] = 0;
   out_8493233740844108942[69] = 0;
   out_8493233740844108942[70] = 1;
   out_8493233740844108942[71] = 0;
   out_8493233740844108942[72] = 0;
   out_8493233740844108942[73] = 0;
   out_8493233740844108942[74] = 0;
   out_8493233740844108942[75] = 0;
   out_8493233740844108942[76] = 0;
   out_8493233740844108942[77] = 0;
   out_8493233740844108942[78] = 0;
   out_8493233740844108942[79] = 0;
   out_8493233740844108942[80] = 1;
}
void h_25(double *state, double *unused, double *out_3148022997593239894) {
   out_3148022997593239894[0] = state[6];
}
void H_25(double *state, double *unused, double *out_745662918452737419) {
   out_745662918452737419[0] = 0;
   out_745662918452737419[1] = 0;
   out_745662918452737419[2] = 0;
   out_745662918452737419[3] = 0;
   out_745662918452737419[4] = 0;
   out_745662918452737419[5] = 0;
   out_745662918452737419[6] = 1;
   out_745662918452737419[7] = 0;
   out_745662918452737419[8] = 0;
}
void h_24(double *state, double *unused, double *out_5661741839916811149) {
   out_5661741839916811149[0] = state[4];
   out_5661741839916811149[1] = state[5];
}
void H_24(double *state, double *unused, double *out_5076586435370679900) {
   out_5076586435370679900[0] = 0;
   out_5076586435370679900[1] = 0;
   out_5076586435370679900[2] = 0;
   out_5076586435370679900[3] = 0;
   out_5076586435370679900[4] = 1;
   out_5076586435370679900[5] = 0;
   out_5076586435370679900[6] = 0;
   out_5076586435370679900[7] = 0;
   out_5076586435370679900[8] = 0;
   out_5076586435370679900[9] = 0;
   out_5076586435370679900[10] = 0;
   out_5076586435370679900[11] = 0;
   out_5076586435370679900[12] = 0;
   out_5076586435370679900[13] = 0;
   out_5076586435370679900[14] = 1;
   out_5076586435370679900[15] = 0;
   out_5076586435370679900[16] = 0;
   out_5076586435370679900[17] = 0;
}
void h_30(double *state, double *unused, double *out_1104814561170473263) {
   out_1104814561170473263[0] = state[4];
}
void H_30(double *state, double *unused, double *out_875001865595977489) {
   out_875001865595977489[0] = 0;
   out_875001865595977489[1] = 0;
   out_875001865595977489[2] = 0;
   out_875001865595977489[3] = 0;
   out_875001865595977489[4] = 1;
   out_875001865595977489[5] = 0;
   out_875001865595977489[6] = 0;
   out_875001865595977489[7] = 0;
   out_875001865595977489[8] = 0;
}
void h_26(double *state, double *unused, double *out_161304708268810828) {
   out_161304708268810828[0] = state[7];
}
void H_26(double *state, double *unused, double *out_4487166237326793643) {
   out_4487166237326793643[0] = 0;
   out_4487166237326793643[1] = 0;
   out_4487166237326793643[2] = 0;
   out_4487166237326793643[3] = 0;
   out_4487166237326793643[4] = 0;
   out_4487166237326793643[5] = 0;
   out_4487166237326793643[6] = 0;
   out_4487166237326793643[7] = 1;
   out_4487166237326793643[8] = 0;
}
void h_27(double *state, double *unused, double *out_3751338882269717411) {
   out_3751338882269717411[0] = state[3];
}
void H_27(double *state, double *unused, double *out_3049765177396402400) {
   out_3049765177396402400[0] = 0;
   out_3049765177396402400[1] = 0;
   out_3049765177396402400[2] = 0;
   out_3049765177396402400[3] = 1;
   out_3049765177396402400[4] = 0;
   out_3049765177396402400[5] = 0;
   out_3049765177396402400[6] = 0;
   out_3049765177396402400[7] = 0;
   out_3049765177396402400[8] = 0;
}
void h_29(double *state, double *unused, double *out_3709219881979170914) {
   out_3709219881979170914[0] = state[1];
}
void H_29(double *state, double *unused, double *out_4763127904265953433) {
   out_4763127904265953433[0] = 0;
   out_4763127904265953433[1] = 1;
   out_4763127904265953433[2] = 0;
   out_4763127904265953433[3] = 0;
   out_4763127904265953433[4] = 0;
   out_4763127904265953433[5] = 0;
   out_4763127904265953433[6] = 0;
   out_4763127904265953433[7] = 0;
   out_4763127904265953433[8] = 0;
}
void h_28(double *state, double *unused, double *out_4654317937374509242) {
   out_4654317937374509242[0] = state[0];
}
void H_28(double *state, double *unused, double *out_8601217152374067609) {
   out_8601217152374067609[0] = 1;
   out_8601217152374067609[1] = 0;
   out_8601217152374067609[2] = 0;
   out_8601217152374067609[3] = 0;
   out_8601217152374067609[4] = 0;
   out_8601217152374067609[5] = 0;
   out_8601217152374067609[6] = 0;
   out_8601217152374067609[7] = 0;
   out_8601217152374067609[8] = 0;
}
void h_31(double *state, double *unused, double *out_7535455819295639281) {
   out_7535455819295639281[0] = state[8];
}
void H_31(double *state, double *unused, double *out_715016956575776991) {
   out_715016956575776991[0] = 0;
   out_715016956575776991[1] = 0;
   out_715016956575776991[2] = 0;
   out_715016956575776991[3] = 0;
   out_715016956575776991[4] = 0;
   out_715016956575776991[5] = 0;
   out_715016956575776991[6] = 0;
   out_715016956575776991[7] = 0;
   out_715016956575776991[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_5279200563100949282) {
  err_fun(nom_x, delta_x, out_5279200563100949282);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_2794898786486738948) {
  inv_err_fun(nom_x, true_x, out_2794898786486738948);
}
void car_H_mod_fun(double *state, double *out_3002480874372569413) {
  H_mod_fun(state, out_3002480874372569413);
}
void car_f_fun(double *state, double dt, double *out_8611914425259886085) {
  f_fun(state,  dt, out_8611914425259886085);
}
void car_F_fun(double *state, double dt, double *out_8493233740844108942) {
  F_fun(state,  dt, out_8493233740844108942);
}
void car_h_25(double *state, double *unused, double *out_3148022997593239894) {
  h_25(state, unused, out_3148022997593239894);
}
void car_H_25(double *state, double *unused, double *out_745662918452737419) {
  H_25(state, unused, out_745662918452737419);
}
void car_h_24(double *state, double *unused, double *out_5661741839916811149) {
  h_24(state, unused, out_5661741839916811149);
}
void car_H_24(double *state, double *unused, double *out_5076586435370679900) {
  H_24(state, unused, out_5076586435370679900);
}
void car_h_30(double *state, double *unused, double *out_1104814561170473263) {
  h_30(state, unused, out_1104814561170473263);
}
void car_H_30(double *state, double *unused, double *out_875001865595977489) {
  H_30(state, unused, out_875001865595977489);
}
void car_h_26(double *state, double *unused, double *out_161304708268810828) {
  h_26(state, unused, out_161304708268810828);
}
void car_H_26(double *state, double *unused, double *out_4487166237326793643) {
  H_26(state, unused, out_4487166237326793643);
}
void car_h_27(double *state, double *unused, double *out_3751338882269717411) {
  h_27(state, unused, out_3751338882269717411);
}
void car_H_27(double *state, double *unused, double *out_3049765177396402400) {
  H_27(state, unused, out_3049765177396402400);
}
void car_h_29(double *state, double *unused, double *out_3709219881979170914) {
  h_29(state, unused, out_3709219881979170914);
}
void car_H_29(double *state, double *unused, double *out_4763127904265953433) {
  H_29(state, unused, out_4763127904265953433);
}
void car_h_28(double *state, double *unused, double *out_4654317937374509242) {
  h_28(state, unused, out_4654317937374509242);
}
void car_H_28(double *state, double *unused, double *out_8601217152374067609) {
  H_28(state, unused, out_8601217152374067609);
}
void car_h_31(double *state, double *unused, double *out_7535455819295639281) {
  h_31(state, unused, out_7535455819295639281);
}
void car_H_31(double *state, double *unused, double *out_715016956575776991) {
  H_31(state, unused, out_715016956575776991);
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
