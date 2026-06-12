#pragma once

#define QR_QUIET_MODULES 4  /* white quiet zone on each side, in modules */

/* Whole pixels per module so (modules + 2*quiet) fits in area_px. 0 = can't fit. */
int qr_scale_for(int modules, int area_px);

/* Pixel offset (one axis) of the first DATA module, centering quiet zone + data. */
int qr_data_origin(int modules, int area_px, int scale);
