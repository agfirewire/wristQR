#pragma once
#include "model.h"

void storage_load(QrList *list);   /* zeroes list on missing/invalid data */
void storage_save(const QrList *list);
