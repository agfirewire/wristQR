#pragma once

void qr_window_push(int index);
void qr_window_pop_if_open(void);  /* used when a sync replaces the list */
