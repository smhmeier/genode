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
#include <base/stdint.h>

/* base-internal includes */
#include <base/internal/pistachio.h>

namespace Genode { struct Native_thread; }

namespace Core { class Platform_thread; }


struct Genode::Native_thread : Noncopyable
{
	Pistachio::L4_ThreadId_t l4id { };

	/**
	 * Only used in core
	 *
	 * For 'Thread' objects created within core, 'pt' points to
	 * the physical thread object, which is going to be destroyed
	 * on destruction of the 'Thread'.
	 */
	struct { Core::Platform_thread *pt = nullptr; };

	Native_thread() { }
};

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_THREAD_H_ */
