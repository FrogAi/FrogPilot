#if SENSOR_ID == 3

#define BGGR

#define BIT_DEPTH 10
#define PV_MAX10 1023
#define PV_MAX16 65536 // gamma curve is calibrated to 16bit
#define BLACK_LVL 64
#define VIGNETTE_RSZ 2.2545f

float4 combine_dual_pvs(float4 lv, float4 sv, int expo_time) {
  float4 svc = fmax(sv * expo_time, (float)(64 * (PV_MAX10 - BLACK_LVL)));
  float4 svd = sv * fmin(expo_time, 8.0) / 8;

  if (expo_time > 64) {
    return select((svc / 64) / (PV_MAX16 - BLACK_LVL), lv / (PV_MAX16 - BLACK_LVL), isless(lv, (float4)(PV_MAX10 - BLACK_LVL)));
  } else {
    return select(svd / (PV_MAX16 - BLACK_LVL), (lv * 64 / fmax(expo_time, 8.0)) / (PV_MAX16 - BLACK_LVL), isgreater(lv, (float4)(32)));
  }
}

float4 normalize_pv_hdr(int4 parsed, int4 short_parsed, float vignette_factor, int expo_time) {
  float4 pl = convert_float4(parsed - BLACK_LVL);
  float4 ps = convert_float4(short_parsed - BLACK_LVL);
  float4 pv = combine_dual_pvs(pl, ps, expo_time);
  return clamp(pv*vignette_factor, 0.0, 1.0);
}

float3 color_correct(float3 rgb) {
  float3 corrected = rgb.x * (float3)(1.55361989, -0.268894615, -0.000593219);
  corrected += rgb.y * (float3)(-0.421217301, 1.51883144, -0.69760146);
  corrected += rgb.z * (float3)(-0.132402589, -0.249936825, 1.69819468);
  return corrected;
}

float3 apply_gamma(float3 rgb, int expo_time) {
  float s = log2((float)expo_time);
  if (s < 6) {s = fmin(12.0 - s, 9.0);}
  // log function adaptive to number of bits
  return clamp(log(1 + rgb*(PV_MAX16 - BLACK_LVL)) * (0.48*s*s - 12.92*s + 115.0) - (1.08*s*s - 29.2*s + 260.0), 0.0, 255.0) / 255.0;
}

#endif
