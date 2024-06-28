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
void err_fun(double *nom_x, double *delta_x, double *out_3952102652370007548) {
   out_3952102652370007548[0] = delta_x[0] + nom_x[0];
   out_3952102652370007548[1] = delta_x[1] + nom_x[1];
   out_3952102652370007548[2] = delta_x[2] + nom_x[2];
   out_3952102652370007548[3] = delta_x[3] + nom_x[3];
   out_3952102652370007548[4] = delta_x[4] + nom_x[4];
   out_3952102652370007548[5] = delta_x[5] + nom_x[5];
   out_3952102652370007548[6] = delta_x[6] + nom_x[6];
   out_3952102652370007548[7] = delta_x[7] + nom_x[7];
   out_3952102652370007548[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_7464642641330887774) {
   out_7464642641330887774[0] = -nom_x[0] + true_x[0];
   out_7464642641330887774[1] = -nom_x[1] + true_x[1];
   out_7464642641330887774[2] = -nom_x[2] + true_x[2];
   out_7464642641330887774[3] = -nom_x[3] + true_x[3];
   out_7464642641330887774[4] = -nom_x[4] + true_x[4];
   out_7464642641330887774[5] = -nom_x[5] + true_x[5];
   out_7464642641330887774[6] = -nom_x[6] + true_x[6];
   out_7464642641330887774[7] = -nom_x[7] + true_x[7];
   out_7464642641330887774[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_3878321196376381259) {
   out_3878321196376381259[0] = 1.0;
   out_3878321196376381259[1] = 0;
   out_3878321196376381259[2] = 0;
   out_3878321196376381259[3] = 0;
   out_3878321196376381259[4] = 0;
   out_3878321196376381259[5] = 0;
   out_3878321196376381259[6] = 0;
   out_3878321196376381259[7] = 0;
   out_3878321196376381259[8] = 0;
   out_3878321196376381259[9] = 0;
   out_3878321196376381259[10] = 1.0;
   out_3878321196376381259[11] = 0;
   out_3878321196376381259[12] = 0;
   out_3878321196376381259[13] = 0;
   out_3878321196376381259[14] = 0;
   out_3878321196376381259[15] = 0;
   out_3878321196376381259[16] = 0;
   out_3878321196376381259[17] = 0;
   out_3878321196376381259[18] = 0;
   out_3878321196376381259[19] = 0;
   out_3878321196376381259[20] = 1.0;
   out_3878321196376381259[21] = 0;
   out_3878321196376381259[22] = 0;
   out_3878321196376381259[23] = 0;
   out_3878321196376381259[24] = 0;
   out_3878321196376381259[25] = 0;
   out_3878321196376381259[26] = 0;
   out_3878321196376381259[27] = 0;
   out_3878321196376381259[28] = 0;
   out_3878321196376381259[29] = 0;
   out_3878321196376381259[30] = 1.0;
   out_3878321196376381259[31] = 0;
   out_3878321196376381259[32] = 0;
   out_3878321196376381259[33] = 0;
   out_3878321196376381259[34] = 0;
   out_3878321196376381259[35] = 0;
   out_3878321196376381259[36] = 0;
   out_3878321196376381259[37] = 0;
   out_3878321196376381259[38] = 0;
   out_3878321196376381259[39] = 0;
   out_3878321196376381259[40] = 1.0;
   out_3878321196376381259[41] = 0;
   out_3878321196376381259[42] = 0;
   out_3878321196376381259[43] = 0;
   out_3878321196376381259[44] = 0;
   out_3878321196376381259[45] = 0;
   out_3878321196376381259[46] = 0;
   out_3878321196376381259[47] = 0;
   out_3878321196376381259[48] = 0;
   out_3878321196376381259[49] = 0;
   out_3878321196376381259[50] = 1.0;
   out_3878321196376381259[51] = 0;
   out_3878321196376381259[52] = 0;
   out_3878321196376381259[53] = 0;
   out_3878321196376381259[54] = 0;
   out_3878321196376381259[55] = 0;
   out_3878321196376381259[56] = 0;
   out_3878321196376381259[57] = 0;
   out_3878321196376381259[58] = 0;
   out_3878321196376381259[59] = 0;
   out_3878321196376381259[60] = 1.0;
   out_3878321196376381259[61] = 0;
   out_3878321196376381259[62] = 0;
   out_3878321196376381259[63] = 0;
   out_3878321196376381259[64] = 0;
   out_3878321196376381259[65] = 0;
   out_3878321196376381259[66] = 0;
   out_3878321196376381259[67] = 0;
   out_3878321196376381259[68] = 0;
   out_3878321196376381259[69] = 0;
   out_3878321196376381259[70] = 1.0;
   out_3878321196376381259[71] = 0;
   out_3878321196376381259[72] = 0;
   out_3878321196376381259[73] = 0;
   out_3878321196376381259[74] = 0;
   out_3878321196376381259[75] = 0;
   out_3878321196376381259[76] = 0;
   out_3878321196376381259[77] = 0;
   out_3878321196376381259[78] = 0;
   out_3878321196376381259[79] = 0;
   out_3878321196376381259[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_1482051682621306109) {
   out_1482051682621306109[0] = state[0];
   out_1482051682621306109[1] = state[1];
   out_1482051682621306109[2] = state[2];
   out_1482051682621306109[3] = state[3];
   out_1482051682621306109[4] = state[4];
   out_1482051682621306109[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_1482051682621306109[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_1482051682621306109[7] = state[7];
   out_1482051682621306109[8] = state[8];
}
void F_fun(double *state, double dt, double *out_2250329287943289893) {
   out_2250329287943289893[0] = 1;
   out_2250329287943289893[1] = 0;
   out_2250329287943289893[2] = 0;
   out_2250329287943289893[3] = 0;
   out_2250329287943289893[4] = 0;
   out_2250329287943289893[5] = 0;
   out_2250329287943289893[6] = 0;
   out_2250329287943289893[7] = 0;
   out_2250329287943289893[8] = 0;
   out_2250329287943289893[9] = 0;
   out_2250329287943289893[10] = 1;
   out_2250329287943289893[11] = 0;
   out_2250329287943289893[12] = 0;
   out_2250329287943289893[13] = 0;
   out_2250329287943289893[14] = 0;
   out_2250329287943289893[15] = 0;
   out_2250329287943289893[16] = 0;
   out_2250329287943289893[17] = 0;
   out_2250329287943289893[18] = 0;
   out_2250329287943289893[19] = 0;
   out_2250329287943289893[20] = 1;
   out_2250329287943289893[21] = 0;
   out_2250329287943289893[22] = 0;
   out_2250329287943289893[23] = 0;
   out_2250329287943289893[24] = 0;
   out_2250329287943289893[25] = 0;
   out_2250329287943289893[26] = 0;
   out_2250329287943289893[27] = 0;
   out_2250329287943289893[28] = 0;
   out_2250329287943289893[29] = 0;
   out_2250329287943289893[30] = 1;
   out_2250329287943289893[31] = 0;
   out_2250329287943289893[32] = 0;
   out_2250329287943289893[33] = 0;
   out_2250329287943289893[34] = 0;
   out_2250329287943289893[35] = 0;
   out_2250329287943289893[36] = 0;
   out_2250329287943289893[37] = 0;
   out_2250329287943289893[38] = 0;
   out_2250329287943289893[39] = 0;
   out_2250329287943289893[40] = 1;
   out_2250329287943289893[41] = 0;
   out_2250329287943289893[42] = 0;
   out_2250329287943289893[43] = 0;
   out_2250329287943289893[44] = 0;
   out_2250329287943289893[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_2250329287943289893[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_2250329287943289893[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2250329287943289893[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_2250329287943289893[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_2250329287943289893[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_2250329287943289893[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_2250329287943289893[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_2250329287943289893[53] = -9.8000000000000007*dt;
   out_2250329287943289893[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_2250329287943289893[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_2250329287943289893[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2250329287943289893[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2250329287943289893[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_2250329287943289893[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_2250329287943289893[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_2250329287943289893[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_2250329287943289893[62] = 0;
   out_2250329287943289893[63] = 0;
   out_2250329287943289893[64] = 0;
   out_2250329287943289893[65] = 0;
   out_2250329287943289893[66] = 0;
   out_2250329287943289893[67] = 0;
   out_2250329287943289893[68] = 0;
   out_2250329287943289893[69] = 0;
   out_2250329287943289893[70] = 1;
   out_2250329287943289893[71] = 0;
   out_2250329287943289893[72] = 0;
   out_2250329287943289893[73] = 0;
   out_2250329287943289893[74] = 0;
   out_2250329287943289893[75] = 0;
   out_2250329287943289893[76] = 0;
   out_2250329287943289893[77] = 0;
   out_2250329287943289893[78] = 0;
   out_2250329287943289893[79] = 0;
   out_2250329287943289893[80] = 1;
}
void h_25(double *state, double *unused, double *out_1163277483665445692) {
   out_1163277483665445692[0] = state[6];
}
void H_25(double *state, double *unused, double *out_5016162698672639914) {
   out_5016162698672639914[0] = 0;
   out_5016162698672639914[1] = 0;
   out_5016162698672639914[2] = 0;
   out_5016162698672639914[3] = 0;
   out_5016162698672639914[4] = 0;
   out_5016162698672639914[5] = 0;
   out_5016162698672639914[6] = 1;
   out_5016162698672639914[7] = 0;
   out_5016162698672639914[8] = 0;
}
void h_24(double *state, double *unused, double *out_8277671942822223457) {
   out_8277671942822223457[0] = state[4];
   out_8277671942822223457[1] = state[5];
}
void H_24(double *state, double *unused, double *out_795222932350137051) {
   out_795222932350137051[0] = 0;
   out_795222932350137051[1] = 0;
   out_795222932350137051[2] = 0;
   out_795222932350137051[3] = 0;
   out_795222932350137051[4] = 1;
   out_795222932350137051[5] = 0;
   out_795222932350137051[6] = 0;
   out_795222932350137051[7] = 0;
   out_795222932350137051[8] = 0;
   out_795222932350137051[9] = 0;
   out_795222932350137051[10] = 0;
   out_795222932350137051[11] = 0;
   out_795222932350137051[12] = 0;
   out_795222932350137051[13] = 0;
   out_795222932350137051[14] = 1;
   out_795222932350137051[15] = 0;
   out_795222932350137051[16] = 0;
   out_795222932350137051[17] = 0;
}
void h_30(double *state, double *unused, double *out_7476599993053096192) {
   out_7476599993053096192[0] = state[4];
}
void H_30(double *state, double *unused, double *out_1900527642818976841) {
   out_1900527642818976841[0] = 0;
   out_1900527642818976841[1] = 0;
   out_1900527642818976841[2] = 0;
   out_1900527642818976841[3] = 0;
   out_1900527642818976841[4] = 1;
   out_1900527642818976841[5] = 0;
   out_1900527642818976841[6] = 0;
   out_1900527642818976841[7] = 0;
   out_1900527642818976841[8] = 0;
}
void h_26(double *state, double *unused, double *out_5968470812995609779) {
   out_5968470812995609779[0] = state[7];
}
void H_26(double *state, double *unused, double *out_8757666017546696138) {
   out_8757666017546696138[0] = 0;
   out_8757666017546696138[1] = 0;
   out_8757666017546696138[2] = 0;
   out_8757666017546696138[3] = 0;
   out_8757666017546696138[4] = 0;
   out_8757666017546696138[5] = 0;
   out_8757666017546696138[6] = 0;
   out_8757666017546696138[7] = 1;
   out_8757666017546696138[8] = 0;
}
void h_27(double *state, double *unused, double *out_3438806301278497826) {
   out_3438806301278497826[0] = state[3];
}
void H_27(double *state, double *unused, double *out_274235668981448070) {
   out_274235668981448070[0] = 0;
   out_274235668981448070[1] = 0;
   out_274235668981448070[2] = 0;
   out_274235668981448070[3] = 1;
   out_274235668981448070[4] = 0;
   out_274235668981448070[5] = 0;
   out_274235668981448070[6] = 0;
   out_274235668981448070[7] = 0;
   out_274235668981448070[8] = 0;
}
void h_29(double *state, double *unused, double *out_8644343794129237929) {
   out_8644343794129237929[0] = state[1];
}
void H_29(double *state, double *unused, double *out_1987598395850999103) {
   out_1987598395850999103[0] = 0;
   out_1987598395850999103[1] = 1;
   out_1987598395850999103[2] = 0;
   out_1987598395850999103[3] = 0;
   out_1987598395850999103[4] = 0;
   out_1987598395850999103[5] = 0;
   out_1987598395850999103[6] = 0;
   out_1987598395850999103[7] = 0;
   out_1987598395850999103[8] = 0;
}
void h_28(double *state, double *unused, double *out_4042122185954975343) {
   out_4042122185954975343[0] = state[0];
}
void H_28(double *state, double *unused, double *out_7069997412920529677) {
   out_7069997412920529677[0] = 1;
   out_7069997412920529677[1] = 0;
   out_7069997412920529677[2] = 0;
   out_7069997412920529677[3] = 0;
   out_7069997412920529677[4] = 0;
   out_7069997412920529677[5] = 0;
   out_7069997412920529677[6] = 0;
   out_7069997412920529677[7] = 0;
   out_7069997412920529677[8] = 0;
}
void h_31(double *state, double *unused, double *out_888083421380939803) {
   out_888083421380939803[0] = state[8];
}
void H_31(double *state, double *unused, double *out_4985516736795679486) {
   out_4985516736795679486[0] = 0;
   out_4985516736795679486[1] = 0;
   out_4985516736795679486[2] = 0;
   out_4985516736795679486[3] = 0;
   out_4985516736795679486[4] = 0;
   out_4985516736795679486[5] = 0;
   out_4985516736795679486[6] = 0;
   out_4985516736795679486[7] = 0;
   out_4985516736795679486[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_3952102652370007548) {
  err_fun(nom_x, delta_x, out_3952102652370007548);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_7464642641330887774) {
  inv_err_fun(nom_x, true_x, out_7464642641330887774);
}
void car_H_mod_fun(double *state, double *out_3878321196376381259) {
  H_mod_fun(state, out_3878321196376381259);
}
void car_f_fun(double *state, double dt, double *out_1482051682621306109) {
  f_fun(state,  dt, out_1482051682621306109);
}
void car_F_fun(double *state, double dt, double *out_2250329287943289893) {
  F_fun(state,  dt, out_2250329287943289893);
}
void car_h_25(double *state, double *unused, double *out_1163277483665445692) {
  h_25(state, unused, out_1163277483665445692);
}
void car_H_25(double *state, double *unused, double *out_5016162698672639914) {
  H_25(state, unused, out_5016162698672639914);
}
void car_h_24(double *state, double *unused, double *out_8277671942822223457) {
  h_24(state, unused, out_8277671942822223457);
}
void car_H_24(double *state, double *unused, double *out_795222932350137051) {
  H_24(state, unused, out_795222932350137051);
}
void car_h_30(double *state, double *unused, double *out_7476599993053096192) {
  h_30(state, unused, out_7476599993053096192);
}
void car_H_30(double *state, double *unused, double *out_1900527642818976841) {
  H_30(state, unused, out_1900527642818976841);
}
void car_h_26(double *state, double *unused, double *out_5968470812995609779) {
  h_26(state, unused, out_5968470812995609779);
}
void car_H_26(double *state, double *unused, double *out_8757666017546696138) {
  H_26(state, unused, out_8757666017546696138);
}
void car_h_27(double *state, double *unused, double *out_3438806301278497826) {
  h_27(state, unused, out_3438806301278497826);
}
void car_H_27(double *state, double *unused, double *out_274235668981448070) {
  H_27(state, unused, out_274235668981448070);
}
void car_h_29(double *state, double *unused, double *out_8644343794129237929) {
  h_29(state, unused, out_8644343794129237929);
}
void car_H_29(double *state, double *unused, double *out_1987598395850999103) {
  H_29(state, unused, out_1987598395850999103);
}
void car_h_28(double *state, double *unused, double *out_4042122185954975343) {
  h_28(state, unused, out_4042122185954975343);
}
void car_H_28(double *state, double *unused, double *out_7069997412920529677) {
  H_28(state, unused, out_7069997412920529677);
}
void car_h_31(double *state, double *unused, double *out_888083421380939803) {
  h_31(state, unused, out_888083421380939803);
}
void car_H_31(double *state, double *unused, double *out_4985516736795679486) {
  H_31(state, unused, out_4985516736795679486);
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
