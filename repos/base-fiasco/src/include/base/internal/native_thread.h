/*
 * \brief  Kernel-specific thread meta data
 * \author Norman Feske
 * \date   2016-03-11
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_

/* Genode includes */
#include <util/noncopyable.h>

/* L4/Fiasco includes */
#include <fiasco/syscall.h>

namespace Core { struct Platform_thread; }

namespace Genode { struct Native_thread; }


struct Genode::Native_thread : Noncopyable
{
	Fiasco::l4_threadid_t l4id { };

	/**
	 * Only used in core
	 *
	 * For 'Thread' objects created within core, 'pt' points to the physical
	 * thread object, which is going to be destroyed on destruction of the
	 * 'Thread'.
	 */
	struct { Core::Platform_thread *pt = nullptr; };

	Native_thread() { }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */
