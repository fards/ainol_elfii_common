/*
 * TVIN Decoder Device Driver Subsystem
 *
 * Author: Bobby Yang <bo.yang@amlogic.com>
 *
 *
 * Copyright (C) 2010 Amlogic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Standard Linux headers */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>

/* Amlogic headers */
#include <linux/amports/vframe.h>

/* Local headers */
#include "tvin_frontend.h"

static struct list_head head = LIST_HEAD_INIT(head);
static DEFINE_SPINLOCK(list_lock);

int tvin_frontend_init(tvin_frontend_t *fe,
    tvin_decoder_ops_t *dec_ops, tvin_state_machine_ops_t *sm_ops, int index)
{
    if (!fe)
        return -1;
    fe->dec_ops = dec_ops;
    fe->sm_ops  = sm_ops;
    fe->index   = index;

    INIT_LIST_HEAD(&fe->list);
    return 0;
}
EXPORT_SYMBOL(tvin_frontend_init);


int tvin_reg_frontend(struct tvin_frontend_s *fe)
{
    ulong flags;
    struct tvin_frontend_s *f, *t;

    if (!fe->name || !fe->dec_ops || !fe->dec_ops->support || !fe->sm_ops)
        return -1;

    /* check whether the frontend is registered already */
    list_for_each_entry_safe(f, t, &head, list) {
        if (!strcmp(f->name, fe->name))
            return -1;
    }
    spin_lock_irqsave(&list_lock, flags);
    list_add_tail(&fe->list, &head);
    spin_unlock_irqrestore(&list_lock, flags);
    return 0;
}
EXPORT_SYMBOL(tvin_reg_frontend);

/*
 * Notes: This unregister interface doesn't call kfree to free memory
 * for the object, because the register interface doesn't call kmalloc
 * to allocate memory for the object. You should call the kfree yourself
 * to free memory for the object you allocated.
 */
void tvin_unreg_frontend(struct tvin_frontend_s *fe)
{
    ulong flags;
    struct tvin_frontend_s *f, *t;

    list_for_each_entry_safe(f, t, &head, list) {
        if (!strcmp(f->name, fe->name)) {
            spin_lock_irqsave(&list_lock, flags);
            list_del(&f->list);
            spin_unlock_irqrestore(&list_lock, flags);
            return;
        }
    }
}
EXPORT_SYMBOL(tvin_unreg_frontend);

struct tvin_frontend_s * tvin_get_frontend(enum tvin_port_e port, int index)
{
    struct tvin_frontend_s *f = NULL;

    list_for_each_entry(f, &head, list) {
        if (f->dec_ops && f->dec_ops->support)
            if ((!f->dec_ops->support(f, port)) && (f->index == index)) {
                return f;
            }
    }
    return NULL;
}
EXPORT_SYMBOL(tvin_get_frontend);




