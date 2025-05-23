/*
 * \brief  Core implementation of the CPU thread interface
 * \author Norman Feske
 * \date   2016-05-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu_session_component.h>

using namespace Core;


void Cpu_thread_component::_update_exception_sigh()
{
	Signal_context_capability sigh = _thread_sigh.valid()
	                               ? _thread_sigh : _session_sigh;

	_platform_thread.pager().exception_handler(sigh);
}


void Cpu_thread_component::destroy() { _cpu.kill_thread(cap()); }


void Cpu_thread_component::quota(size_t quota)
{
	_platform_thread.quota(quota);
}


void Cpu_thread_component::session_exception_sigh(Signal_context_capability sigh)
{
	_session_sigh = sigh;
	_update_exception_sigh();
}


void Cpu_thread_component::start(addr_t ip, addr_t sp)
{
	_platform_thread.start((void *)ip, (void *)sp);
}


void Cpu_thread_component::pause()
{
	_platform_thread.pause();
}


void Cpu_thread_component::single_step(bool enabled)
{
	_platform_thread.single_step(enabled);
}


void Cpu_thread_component::resume()
{
	_platform_thread.resume();
}


Thread_state Cpu_thread_component::state()
{
	return _platform_thread.state();
}


void Cpu_thread_component::state(Thread_state const &state)
{
	_platform_thread.state(state);
}


void Cpu_thread_component::exception_sigh(Signal_context_capability sigh)
{
	_thread_sigh = sigh;
	_update_exception_sigh();
}


void Cpu_thread_component::affinity(Affinity::Location location)
{
	_platform_thread.affinity(location);
}


unsigned Cpu_thread_component::trace_control_index()
{
	return _trace_control_slot.convert<unsigned>(
		[&] (Trace::Control_area::Slot const &slot) { return slot.index; },
		[&] (Trace::Control_area::Error)            { return 0u; });
}


Dataspace_capability Cpu_thread_component::trace_buffer()
{
	if (!_trace_source.constructed())
		return { };
	return _trace_source->buffer();
}


Dataspace_capability Cpu_thread_component::trace_policy()
{
	if (!_trace_source.constructed())
		return { };
	return _trace_source->policy();
}

