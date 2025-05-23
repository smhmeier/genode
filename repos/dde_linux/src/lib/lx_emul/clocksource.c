/*
 * \brief  Linux DDE timer
 * \author Stefan Kalkowski
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/timecounter.h>
#include <linux/sched_clock.h>
#include <linux/smp.h>
#include <linux/of_clk.h>
#include <linux/tick.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <lx_emul/debug.h>
#include <lx_emul/time.h>
#include <lx_emul/init.h>

static u32 dde_timer_rate = 1000000; /* we use microseconds as rate */


static int dde_set_next_event(unsigned long evt,
                              struct clock_event_device *clk)
{
	lx_emul_time_event(evt);
	return 0;
}


static int dde_set_state_shutdown(struct clock_event_device *clk)
{
	lx_emul_time_stop();
	return 0;
}


static u64 dde_timer_read_counter(void)
{
	return lx_emul_time_counter();
}


static u64 dde_clocksource_read_counter(struct clocksource * cs)
{
	return lx_emul_time_counter();
}


static u64 dde_cyclecounter_read_counter(const struct cyclecounter * cc)
{
	return lx_emul_time_counter();
}


static struct clock_event_device * dde_clock_event_device;


void lx_emul_time_init()
{
	static struct clocksource clocksource = {
		.name   = "dde_counter",
		.rating = 400,
		.read   = dde_clocksource_read_counter,
		.mask   = CLOCKSOURCE_MASK(56),
		.flags  = CLOCK_SOURCE_IS_CONTINUOUS,
	};

	static struct cyclecounter cyclecounter = {
		.read = dde_cyclecounter_read_counter,
		.mask = CLOCKSOURCE_MASK(56),
	};

	static struct timecounter timecounter;

	static struct clock_event_device clock_event_device = {
		.name                      = "dde_timer",
		.features                  = CLOCK_EVT_FEAT_ONESHOT | CLOCK_SOURCE_VALID_FOR_HRES,
		.rating                    = 400,
		.set_state_shutdown        = dde_set_state_shutdown,
		.set_state_oneshot_stopped = dde_set_state_shutdown,
		.set_next_event            = dde_set_next_event,
	};

	u64 start_count            = dde_timer_read_counter();
	clock_event_device.cpumask = cpumask_of(smp_processor_id()),
	dde_clock_event_device     = &clock_event_device;

	clocksource_register_hz(&clocksource, dde_timer_rate);
	cyclecounter.mult  = clocksource.mult;
	cyclecounter.shift = clocksource.shift;
	timecounter_init(&timecounter, &cyclecounter, start_count);
	clockevents_config_and_register(&clock_event_device, dde_timer_rate, 0xf, 0x7fffffff);
	sched_clock_register(dde_timer_read_counter, 64, dde_timer_rate);

	/* execute setup calls of clock providers in __clk_of_table */
	of_clk_init(NULL);

}


void lx_emul_time_handle(void)
{
	dde_clock_event_device->event_handler(dde_clock_event_device);
}


extern ktime_t tick_next_period;
void lx_emul_time_update_jiffies(void)
{
	/* check if jiffies need an update */
	if (ktime_before(ktime_get(), tick_next_period))
		return;

	/* tick_nohz_idle_stop_tick breaks with error if softirq is pending */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,18,0)
	if (local_softirq_pending() & SOFTIRQ_STOP_IDLE_MASK)
#else
	if (local_softirq_pending() & ~SOFTIRQ_HOTPLUG_SAFE_MASK)
#endif
		return;

	tick_nohz_idle_enter();
	tick_nohz_idle_stop_tick();
	tick_nohz_idle_restart_tick();
	tick_nohz_idle_exit();
}


extern void update_wall_time(void);


void lx_emul_time_update_jiffies_cpu_relax()
{
	/* based upon tick_do_update_jiffies64 in kernel/time/tick-sched.c */

	ktime_t delta, now = ktime_get();

	/* check if jiffies need an update */
	if (ktime_before(now, tick_next_period))
		return;

	delta = ktime_sub(now, tick_next_period);

	if (unlikely(delta >= TICK_NSEC)) {
		/* Slow path for long idle sleep times */
		s64 incr = TICK_NSEC;

		ktime_t ticks = ktime_divns(delta, incr);

		jiffies_64 += ticks;

		tick_next_period = ktime_add_ns(tick_next_period, incr * ticks);

		update_wall_time();
	}
}


enum { LX_EMUL_MAX_OF_CLOCK_PROVIDERS = 256 };


struct of_device_id __clk_of_table[LX_EMUL_MAX_OF_CLOCK_PROVIDERS] = { };


void lx_emul_register_of_clk_initcall(char const *compat, void *fn)
{
	static unsigned count;

	if (count == LX_EMUL_MAX_OF_CLOCK_PROVIDERS) {
		printk("lx_emul_register_of_clk_initcall: __clk_of_table exhausted\n");
		return;
	}

	strncpy(__clk_of_table[count].compatible, compat,
	        sizeof(__clk_of_table[count].compatible));

	__clk_of_table[count].data = fn;

	count++;
}
