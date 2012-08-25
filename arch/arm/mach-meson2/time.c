/*
 *  arch/arm/mach-meson2/time.c
 *
 *  Copyright (C) 2010 AMLOGIC, INC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <asm/memory.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

#include <plat/regops.h>
#include <mach/map.h>
#include <mach/hardware.h>
#include <mach/reg_addr.h>

/***********************************************************************
 * System timer
 **********************************************************************/

/********** Clock Source Device, Timer-A *********/

static cycle_t cycle_read_timerE(struct clocksource *cs)
{
    return (cycles_t) READ_CBUS_REG(ISA_TIMERE);
}

static struct clocksource clocksource_timer_e = {
    .name   = "Timer-E",
    .rating = 300,
    .read   = cycle_read_timerE,
    .mask   = CLOCKSOURCE_MASK(24),
    .flags  = CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init meson_clocksource_init(void)
{
    CLEAR_CBUS_REG_MASK(ISA_TIMER_MUX, TIMER_E_INPUT_MASK);
    SET_CBUS_REG_MASK(ISA_TIMER_MUX, TIMERE_UNIT_1ms << TIMER_E_INPUT_BIT);
    WRITE_CBUS_REG(ISA_TIMERE, 0);

    clocksource_timer_e.shift = clocksource_hz2shift(24, 1000);
    clocksource_timer_e.mult =
    clocksource_khz2mult(1, clocksource_timer_e.shift);
    clocksource_register(&clocksource_timer_e);
}

/*
 * sched_clock()
 */
unsigned long long sched_clock(void)
{
    cycle_t cyc = cycle_read_timerE(NULL);
    struct clocksource *cs = &clocksource_timer_e;

    return clocksource_cyc2ns(cyc, cs->mult, cs->shift);
}

/********** Clock Event Device, Timer-AC *********/

static void meson_clkevt_set_mode(enum clock_event_mode mode,
                                  struct clock_event_device *dev)
{
    switch (mode) {
    case CLOCK_EVT_MODE_RESUME:
    /* FIXME:
     * CLOCK_EVT_MODE_RESUME is always followed by
     * CLOCK_EVT_MODE_PERIODIC or CLOCK_EVT_MODE_ONESHOT.
     * do nothing here.
     */
    break;

    case CLOCK_EVT_MODE_PERIODIC:
    //meson_mask_irq(INT_TIMER_C);
    //meson_unmask_irq(INT_TIMER_A);
    break;

    case CLOCK_EVT_MODE_ONESHOT:
    //meson_mask_irq(INT_TIMER_A);
    break;

    case CLOCK_EVT_MODE_SHUTDOWN:
    case CLOCK_EVT_MODE_UNUSED:
    /* there is no way to actually pause or stop TIMERA/C,
     * so just disable TIMER interrupt.
     */
    //meson_mask_irq(INT_TIMER_A);
    //meson_mask_irq(INT_TIMER_C);
    break;
    }
}

static int meson_set_next_event(unsigned long evt,
    struct clock_event_device *unused)
{
#if 0
    meson_mask_irq(INT_TIMER_C);
    /* use a big number to clear previous trigger cleanly */
    SET_CBUS_REG_MASK(ISA_TIMERC, evt & 0xffff);
    //meson_ack_irq(INT_TIMER_C);

    /* then set next event */
    WRITE_CBUS_REG_BITS(ISA_TIMERC, evt, 0, 16);
    meson_unmask_irq(INT_TIMER_C);
#endif

    return 0;
}

static struct clock_event_device clockevent_meson_1mhz = {
    .name           = "TIMER-AC",
    .rating         = 300, /* Reasonably fast and accurate clock event */

    .features       = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
    .shift          = 20,
    .set_next_event = meson_set_next_event,
    .set_mode       = meson_clkevt_set_mode,
};

/* Clock event timerA interrupt handler */
static irqreturn_t meson_timer_interrupt(int irq, void *dev_id)
{
    struct clock_event_device *evt = &clockevent_meson_1mhz;

    //meson_ack_irq(irq);

    evt->event_handler(evt);

    return IRQ_HANDLED;
}

static struct irqaction meson_timer_irq = {
    .name           = "Meson Timer Tick",
    .flags          = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
    .handler        = meson_timer_interrupt,
};

static void __init meson_clockevent_init(void)
{
    CLEAR_CBUS_REG_MASK(ISA_TIMER_MUX, TIMER_A_INPUT_MASK | TIMER_C_INPUT_MASK);
    SET_CBUS_REG_MASK(ISA_TIMER_MUX,
        (TIMER_UNIT_1us << TIMER_A_INPUT_BIT) |
        (TIMER_UNIT_1us << TIMER_C_INPUT_BIT));
    WRITE_CBUS_REG(ISA_TIMERA, 9999);

    clockevent_meson_1mhz.mult =
    div_sc(1000000, NSEC_PER_SEC, clockevent_meson_1mhz.shift);
    clockevent_meson_1mhz.max_delta_ns =
    clockevent_delta2ns(0xfffe, &clockevent_meson_1mhz);
    clockevent_meson_1mhz.min_delta_ns =
    clockevent_delta2ns(1, &clockevent_meson_1mhz);
    clockevent_meson_1mhz.cpumask = cpumask_of(0);
    clockevents_register_device(&clockevent_meson_1mhz);

    /* Set up the IRQ handler */
    setup_irq(INT_TIMER_A, &meson_timer_irq);
    setup_irq(INT_TIMER_C, &meson_timer_irq);
}

/*
 * This sets up the system timers, clock source and clock event.
 */
static void __init meson_timer_init(void)
{
    meson_clocksource_init();
    meson_clockevent_init();
}

struct sys_timer meson_sys_timer =
{
    .init = meson_timer_init,
};

