#include "qr_layout.h"

int qr_scale_for(int modules, int area_px) {
  return area_px / (modules + 2 * QR_QUIET_MODULES);
}

int qr_data_origin(int modules, int area_px, int scale) {
  int total = modules + 2 * QR_QUIET_MODULES;
  int margin = (area_px - total * scale) / 2;
  return margin + QR_QUIET_MODULES * scale;
}
