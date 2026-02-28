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
void err_fun(double *nom_x, double *delta_x, double *out_5588406888365626113) {
   out_5588406888365626113[0] = delta_x[0] + nom_x[0];
   out_5588406888365626113[1] = delta_x[1] + nom_x[1];
   out_5588406888365626113[2] = delta_x[2] + nom_x[2];
   out_5588406888365626113[3] = delta_x[3] + nom_x[3];
   out_5588406888365626113[4] = delta_x[4] + nom_x[4];
   out_5588406888365626113[5] = delta_x[5] + nom_x[5];
   out_5588406888365626113[6] = delta_x[6] + nom_x[6];
   out_5588406888365626113[7] = delta_x[7] + nom_x[7];
   out_5588406888365626113[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_8022320061418085482) {
   out_8022320061418085482[0] = -nom_x[0] + true_x[0];
   out_8022320061418085482[1] = -nom_x[1] + true_x[1];
   out_8022320061418085482[2] = -nom_x[2] + true_x[2];
   out_8022320061418085482[3] = -nom_x[3] + true_x[3];
   out_8022320061418085482[4] = -nom_x[4] + true_x[4];
   out_8022320061418085482[5] = -nom_x[5] + true_x[5];
   out_8022320061418085482[6] = -nom_x[6] + true_x[6];
   out_8022320061418085482[7] = -nom_x[7] + true_x[7];
   out_8022320061418085482[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_7995506957028497918) {
   out_7995506957028497918[0] = 1.0;
   out_7995506957028497918[1] = 0;
   out_7995506957028497918[2] = 0;
   out_7995506957028497918[3] = 0;
   out_7995506957028497918[4] = 0;
   out_7995506957028497918[5] = 0;
   out_7995506957028497918[6] = 0;
   out_7995506957028497918[7] = 0;
   out_7995506957028497918[8] = 0;
   out_7995506957028497918[9] = 0;
   out_7995506957028497918[10] = 1.0;
   out_7995506957028497918[11] = 0;
   out_7995506957028497918[12] = 0;
   out_7995506957028497918[13] = 0;
   out_7995506957028497918[14] = 0;
   out_7995506957028497918[15] = 0;
   out_7995506957028497918[16] = 0;
   out_7995506957028497918[17] = 0;
   out_7995506957028497918[18] = 0;
   out_7995506957028497918[19] = 0;
   out_7995506957028497918[20] = 1.0;
   out_7995506957028497918[21] = 0;
   out_7995506957028497918[22] = 0;
   out_7995506957028497918[23] = 0;
   out_7995506957028497918[24] = 0;
   out_7995506957028497918[25] = 0;
   out_7995506957028497918[26] = 0;
   out_7995506957028497918[27] = 0;
   out_7995506957028497918[28] = 0;
   out_7995506957028497918[29] = 0;
   out_7995506957028497918[30] = 1.0;
   out_7995506957028497918[31] = 0;
   out_7995506957028497918[32] = 0;
   out_7995506957028497918[33] = 0;
   out_7995506957028497918[34] = 0;
   out_7995506957028497918[35] = 0;
   out_7995506957028497918[36] = 0;
   out_7995506957028497918[37] = 0;
   out_7995506957028497918[38] = 0;
   out_7995506957028497918[39] = 0;
   out_7995506957028497918[40] = 1.0;
   out_7995506957028497918[41] = 0;
   out_7995506957028497918[42] = 0;
   out_7995506957028497918[43] = 0;
   out_7995506957028497918[44] = 0;
   out_7995506957028497918[45] = 0;
   out_7995506957028497918[46] = 0;
   out_7995506957028497918[47] = 0;
   out_7995506957028497918[48] = 0;
   out_7995506957028497918[49] = 0;
   out_7995506957028497918[50] = 1.0;
   out_7995506957028497918[51] = 0;
   out_7995506957028497918[52] = 0;
   out_7995506957028497918[53] = 0;
   out_7995506957028497918[54] = 0;
   out_7995506957028497918[55] = 0;
   out_7995506957028497918[56] = 0;
   out_7995506957028497918[57] = 0;
   out_7995506957028497918[58] = 0;
   out_7995506957028497918[59] = 0;
   out_7995506957028497918[60] = 1.0;
   out_7995506957028497918[61] = 0;
   out_7995506957028497918[62] = 0;
   out_7995506957028497918[63] = 0;
   out_7995506957028497918[64] = 0;
   out_7995506957028497918[65] = 0;
   out_7995506957028497918[66] = 0;
   out_7995506957028497918[67] = 0;
   out_7995506957028497918[68] = 0;
   out_7995506957028497918[69] = 0;
   out_7995506957028497918[70] = 1.0;
   out_7995506957028497918[71] = 0;
   out_7995506957028497918[72] = 0;
   out_7995506957028497918[73] = 0;
   out_7995506957028497918[74] = 0;
   out_7995506957028497918[75] = 0;
   out_7995506957028497918[76] = 0;
   out_7995506957028497918[77] = 0;
   out_7995506957028497918[78] = 0;
   out_7995506957028497918[79] = 0;
   out_7995506957028497918[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_2360361574345086061) {
   out_2360361574345086061[0] = state[0];
   out_2360361574345086061[1] = state[1];
   out_2360361574345086061[2] = state[2];
   out_2360361574345086061[3] = state[3];
   out_2360361574345086061[4] = state[4];
   out_2360361574345086061[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_2360361574345086061[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_2360361574345086061[7] = state[7];
   out_2360361574345086061[8] = state[8];
}
void F_fun(double *state, double dt, double *out_7631457160567166404) {
   out_7631457160567166404[0] = 1;
   out_7631457160567166404[1] = 0;
   out_7631457160567166404[2] = 0;
   out_7631457160567166404[3] = 0;
   out_7631457160567166404[4] = 0;
   out_7631457160567166404[5] = 0;
   out_7631457160567166404[6] = 0;
   out_7631457160567166404[7] = 0;
   out_7631457160567166404[8] = 0;
   out_7631457160567166404[9] = 0;
   out_7631457160567166404[10] = 1;
   out_7631457160567166404[11] = 0;
   out_7631457160567166404[12] = 0;
   out_7631457160567166404[13] = 0;
   out_7631457160567166404[14] = 0;
   out_7631457160567166404[15] = 0;
   out_7631457160567166404[16] = 0;
   out_7631457160567166404[17] = 0;
   out_7631457160567166404[18] = 0;
   out_7631457160567166404[19] = 0;
   out_7631457160567166404[20] = 1;
   out_7631457160567166404[21] = 0;
   out_7631457160567166404[22] = 0;
   out_7631457160567166404[23] = 0;
   out_7631457160567166404[24] = 0;
   out_7631457160567166404[25] = 0;
   out_7631457160567166404[26] = 0;
   out_7631457160567166404[27] = 0;
   out_7631457160567166404[28] = 0;
   out_7631457160567166404[29] = 0;
   out_7631457160567166404[30] = 1;
   out_7631457160567166404[31] = 0;
   out_7631457160567166404[32] = 0;
   out_7631457160567166404[33] = 0;
   out_7631457160567166404[34] = 0;
   out_7631457160567166404[35] = 0;
   out_7631457160567166404[36] = 0;
   out_7631457160567166404[37] = 0;
   out_7631457160567166404[38] = 0;
   out_7631457160567166404[39] = 0;
   out_7631457160567166404[40] = 1;
   out_7631457160567166404[41] = 0;
   out_7631457160567166404[42] = 0;
   out_7631457160567166404[43] = 0;
   out_7631457160567166404[44] = 0;
   out_7631457160567166404[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_7631457160567166404[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_7631457160567166404[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7631457160567166404[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_7631457160567166404[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_7631457160567166404[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_7631457160567166404[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_7631457160567166404[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_7631457160567166404[53] = -9.8000000000000007*dt;
   out_7631457160567166404[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_7631457160567166404[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_7631457160567166404[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7631457160567166404[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7631457160567166404[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_7631457160567166404[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_7631457160567166404[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_7631457160567166404[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_7631457160567166404[62] = 0;
   out_7631457160567166404[63] = 0;
   out_7631457160567166404[64] = 0;
   out_7631457160567166404[65] = 0;
   out_7631457160567166404[66] = 0;
   out_7631457160567166404[67] = 0;
   out_7631457160567166404[68] = 0;
   out_7631457160567166404[69] = 0;
   out_7631457160567166404[70] = 1;
   out_7631457160567166404[71] = 0;
   out_7631457160567166404[72] = 0;
   out_7631457160567166404[73] = 0;
   out_7631457160567166404[74] = 0;
   out_7631457160567166404[75] = 0;
   out_7631457160567166404[76] = 0;
   out_7631457160567166404[77] = 0;
   out_7631457160567166404[78] = 0;
   out_7631457160567166404[79] = 0;
   out_7631457160567166404[80] = 1;
}
void h_25(double *state, double *unused, double *out_6987712039313090741) {
   out_6987712039313090741[0] = state[6];
}
void H_25(double *state, double *unused, double *out_4445037697698347388) {
   out_4445037697698347388[0] = 0;
   out_4445037697698347388[1] = 0;
   out_4445037697698347388[2] = 0;
   out_4445037697698347388[3] = 0;
   out_4445037697698347388[4] = 0;
   out_4445037697698347388[5] = 0;
   out_4445037697698347388[6] = 1;
   out_4445037697698347388[7] = 0;
   out_4445037697698347388[8] = 0;
}
void h_24(double *state, double *unused, double *out_8502122595903451231) {
   out_8502122595903451231[0] = state[4];
   out_8502122595903451231[1] = state[5];
}
void H_24(double *state, double *unused, double *out_887112098896024252) {
   out_887112098896024252[0] = 0;
   out_887112098896024252[1] = 0;
   out_887112098896024252[2] = 0;
   out_887112098896024252[3] = 0;
   out_887112098896024252[4] = 1;
   out_887112098896024252[5] = 0;
   out_887112098896024252[6] = 0;
   out_887112098896024252[7] = 0;
   out_887112098896024252[8] = 0;
   out_887112098896024252[9] = 0;
   out_887112098896024252[10] = 0;
   out_887112098896024252[11] = 0;
   out_887112098896024252[12] = 0;
   out_887112098896024252[13] = 0;
   out_887112098896024252[14] = 1;
   out_887112098896024252[15] = 0;
   out_887112098896024252[16] = 0;
   out_887112098896024252[17] = 0;
}
void h_30(double *state, double *unused, double *out_4039986048957605780) {
   out_4039986048957605780[0] = state[4];
}
void H_30(double *state, double *unused, double *out_2471652643793269367) {
   out_2471652643793269367[0] = 0;
   out_2471652643793269367[1] = 0;
   out_2471652643793269367[2] = 0;
   out_2471652643793269367[3] = 0;
   out_2471652643793269367[4] = 1;
   out_2471652643793269367[5] = 0;
   out_2471652643793269367[6] = 0;
   out_2471652643793269367[7] = 0;
   out_2471652643793269367[8] = 0;
}
void h_26(double *state, double *unused, double *out_290244726764480927) {
   out_290244726764480927[0] = state[7];
}
void H_26(double *state, double *unused, double *out_8186541016572403612) {
   out_8186541016572403612[0] = 0;
   out_8186541016572403612[1] = 0;
   out_8186541016572403612[2] = 0;
   out_8186541016572403612[3] = 0;
   out_8186541016572403612[4] = 0;
   out_8186541016572403612[5] = 0;
   out_8186541016572403612[6] = 0;
   out_8186541016572403612[7] = 1;
   out_8186541016572403612[8] = 0;
}
void h_27(double *state, double *unused, double *out_885192603579896351) {
   out_885192603579896351[0] = state[3];
}
void H_27(double *state, double *unused, double *out_296889331992844456) {
   out_296889331992844456[0] = 0;
   out_296889331992844456[1] = 0;
   out_296889331992844456[2] = 0;
   out_296889331992844456[3] = 1;
   out_296889331992844456[4] = 0;
   out_296889331992844456[5] = 0;
   out_296889331992844456[6] = 0;
   out_296889331992844456[7] = 0;
   out_296889331992844456[8] = 0;
}
void h_29(double *state, double *unused, double *out_2752256924674125999) {
   out_2752256924674125999[0] = state[1];
}
void H_29(double *state, double *unused, double *out_2981883988107661551) {
   out_2981883988107661551[0] = 0;
   out_2981883988107661551[1] = 1;
   out_2981883988107661551[2] = 0;
   out_2981883988107661551[3] = 0;
   out_2981883988107661551[4] = 0;
   out_2981883988107661551[5] = 0;
   out_2981883988107661551[6] = 0;
   out_2981883988107661551[7] = 0;
   out_2981883988107661551[8] = 0;
}
void h_28(double *state, double *unused, double *out_3921978655024229036) {
   out_3921978655024229036[0] = state[0];
}
void H_28(double *state, double *unused, double *out_6498872411946237151) {
   out_6498872411946237151[0] = 1;
   out_6498872411946237151[1] = 0;
   out_6498872411946237151[2] = 0;
   out_6498872411946237151[3] = 0;
   out_6498872411946237151[4] = 0;
   out_6498872411946237151[5] = 0;
   out_6498872411946237151[6] = 0;
   out_6498872411946237151[7] = 0;
   out_6498872411946237151[8] = 0;
}
void h_31(double *state, double *unused, double *out_6162059933525247885) {
   out_6162059933525247885[0] = state[8];
}
void H_31(double *state, double *unused, double *out_4414391735821386960) {
   out_4414391735821386960[0] = 0;
   out_4414391735821386960[1] = 0;
   out_4414391735821386960[2] = 0;
   out_4414391735821386960[3] = 0;
   out_4414391735821386960[4] = 0;
   out_4414391735821386960[5] = 0;
   out_4414391735821386960[6] = 0;
   out_4414391735821386960[7] = 0;
   out_4414391735821386960[8] = 1;
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
void car_err_fun(double *nom_x, double *delta_x, double *out_5588406888365626113) {
  err_fun(nom_x, delta_x, out_5588406888365626113);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_8022320061418085482) {
  inv_err_fun(nom_x, true_x, out_8022320061418085482);
}
void car_H_mod_fun(double *state, double *out_7995506957028497918) {
  H_mod_fun(state, out_7995506957028497918);
}
void car_f_fun(double *state, double dt, double *out_2360361574345086061) {
  f_fun(state,  dt, out_2360361574345086061);
}
void car_F_fun(double *state, double dt, double *out_7631457160567166404) {
  F_fun(state,  dt, out_7631457160567166404);
}
void car_h_25(double *state, double *unused, double *out_6987712039313090741) {
  h_25(state, unused, out_6987712039313090741);
}
void car_H_25(double *state, double *unused, double *out_4445037697698347388) {
  H_25(state, unused, out_4445037697698347388);
}
void car_h_24(double *state, double *unused, double *out_8502122595903451231) {
  h_24(state, unused, out_8502122595903451231);
}
void car_H_24(double *state, double *unused, double *out_887112098896024252) {
  H_24(state, unused, out_887112098896024252);
}
void car_h_30(double *state, double *unused, double *out_4039986048957605780) {
  h_30(state, unused, out_4039986048957605780);
}
void car_H_30(double *state, double *unused, double *out_2471652643793269367) {
  H_30(state, unused, out_2471652643793269367);
}
void car_h_26(double *state, double *unused, double *out_290244726764480927) {
  h_26(state, unused, out_290244726764480927);
}
void car_H_26(double *state, double *unused, double *out_8186541016572403612) {
  H_26(state, unused, out_8186541016572403612);
}
void car_h_27(double *state, double *unused, double *out_885192603579896351) {
  h_27(state, unused, out_885192603579896351);
}
void car_H_27(double *state, double *unused, double *out_296889331992844456) {
  H_27(state, unused, out_296889331992844456);
}
void car_h_29(double *state, double *unused, double *out_2752256924674125999) {
  h_29(state, unused, out_2752256924674125999);
}
void car_H_29(double *state, double *unused, double *out_2981883988107661551) {
  H_29(state, unused, out_2981883988107661551);
}
void car_h_28(double *state, double *unused, double *out_3921978655024229036) {
  h_28(state, unused, out_3921978655024229036);
}
void car_H_28(double *state, double *unused, double *out_6498872411946237151) {
  H_28(state, unused, out_6498872411946237151);
}
void car_h_31(double *state, double *unused, double *out_6162059933525247885) {
  h_31(state, unused, out_6162059933525247885);
}
void car_H_31(double *state, double *unused, double *out_4414391735821386960) {
  H_31(state, unused, out_4414391735821386960);
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
