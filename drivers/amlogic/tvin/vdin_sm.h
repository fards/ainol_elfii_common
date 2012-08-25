/*
 * TVIN Signal State Machine
 *
 * Author: Lin Xu <lin.xu@amlogic.com>
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TVIN_STATE_MACHINE_H
#define __TVIN_STATE_MACHINE_H

#include "vdin.h"

typedef enum tvin_sm_status_e {
    TVIN_SM_STATUS_NULL = 0, // processing status from init to the finding of the 1st confirmed status
    TVIN_SM_STATUS_NOSIG,    // no signal - physically no signal
    TVIN_SM_STATUS_UNSTABLE, // unstable - physically bad signal
    TVIN_SM_STATUS_NOTSUP,   // not supported - physically good signal & not supported
    TVIN_SM_STATUS_PRESTABLE,
    TVIN_SM_STATUS_STABLE,   // stable - physically good signal & supported
} tvin_sm_status_t;

void tvin_smr(struct vdin_dev_s *pdev);
void tvin_smr_init(void);

enum tvin_sm_status_e tvin_get_sm_status(void);

#endif

