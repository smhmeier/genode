/*
 * \brief   Linux platform interface implementation
 * \author  Norman Feske
 * \date    2006-06-13
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <linux_dataspace/client.h>

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/parent_socket_handle.h>
#include <base/internal/capability_space_tpl.h>

/* local includes */
#include <platform.h>
#include <core_local_rm.h>
#include <resource_path.h>

/* Linux includes */
#include <core_linux_syscalls.h>

using namespace Core;


/**
 * Memory pool used for for core-local meta data
 */
static char _core_mem[80*1024*1024];


/*
 * Basic semaphore implementation based on the 'pipe' syscall.
 *
 * This alternative implementation is needed to be able to wake up the
 * blocked main thread from a signal handler executed by the same thread.
 */
class Pipe_semaphore
{
	private:

		int  _pipefd[2];

	public:

		Pipe_semaphore()
		{
			lx_pipe(_pipefd);
		}

		void down()
		{
			char dummy;
			while(lx_read(_pipefd[0], &dummy, 1) != 1);
		}

		void up()
		{
			char dummy;
			while (lx_write(_pipefd[1], &dummy, 1) != 1);
		}
};


static Pipe_semaphore &_wait_for_exit_sem()
{
	static Pipe_semaphore inst { };
	return inst;
}


static bool _do_exit = false;    /* exit condition */


static void sigint_handler(int)
{
	_do_exit = true;
	_wait_for_exit_sem().up();
}


static void sigchld_handler(int)
{
	_wait_for_exit_sem().up();
}


Core::Platform::Platform()
:
	_core_mem_alloc(nullptr)
{
	/* make 'mmap' behave deterministically */
	lx_disable_aslr();

	/* increase maximum number of open file descriptors to the hard limit */
	lx_boost_rlimit();

	/* catch control-c */
	lx_sigaction(LX_SIGINT, sigint_handler, false);

	/* catch SIGCHLD */
	lx_sigaction(LX_SIGCHLD, sigchld_handler, false);

	/* create resource directory under /tmp */
	lx_mkdir(resource_path(), S_IRWXU);

	if (_core_mem_alloc.add_range((addr_t)_core_mem, sizeof(_core_mem)).failed())
		warning("failed to initialize core memory allocator");

	/*
	 * Occupy the socket handle that will be used to propagate the parent
	 * capability new processes. Otherwise, there may be the chance that the
	 * parent capability as supplied by the process creator will be assigned to
	 * this handle, which would result in a 'dup2' syscall taking
	 * PARENT_SOCKET_HANDLE as both source and target descriptor.
	 */
	lx_dup2(0, PARENT_SOCKET_HANDLE);
}


void Core::Platform::wait_for_exit()
{
	for (;;) {

		/*
		 * Block until a signal occurs.
		 */
		_wait_for_exit_sem().down();

		/*
		 * Each time, the '_wait_for_exit_sem' gets unlocked, we could have
		 * received either a SIGINT or SIGCHLD. If a SIGINT was received, the
		 * '_exit' condition will be set.
		 */
		if (_do_exit)
			break;

		/*
		 * Reflect SIGCHLD as exception signal to the signal context of the CPU
		 * session of the process. Because multiple children could have been
		 * terminated, we iterate until 'pollpid' (wrapper around 'wait4')
		 * returns -1.
		 */
		for (;;) {
			int const pid = lx_pollpid();

			if (pid <= 0)
				break;

			Platform_thread::submit_exception(pid);
		}
	}
	lx_exit_group(0);
}


void Core::init_page_fault_handling(Rpc_entrypoint &) { }


/*********************************
 ** Support for Region_map_mmap **
 *********************************/

static Rpc_entrypoint *_core_ep_ptr;


void Core_local_rm::init(Rpc_entrypoint &ep) { _core_ep_ptr = &ep; }


static auto with_linux_dataspace(Capability<Dataspace> ds,
                                 auto const &fn, auto const &missing_fn) -> decltype(missing_fn())
{
	if (!_core_ep_ptr)
		error("missing call of Core_region_map::init");

	Capability<Linux_dataspace> lx_ds = static_cap_cast<Linux_dataspace>(ds);

	if (_core_ep_ptr)
		return _core_ep_ptr->apply(lx_ds, [&] (Linux_dataspace *ds_ptr) {
			return ds_ptr ? fn(*ds_ptr) : missing_fn(); });

	return missing_fn();
}


size_t Region_map_mmap::_dataspace_size(Capability<Dataspace> ds_cap)
{
	if (!ds_cap.valid())
		return Local_capability<Dataspace>::deref(ds_cap)->size();

	return with_linux_dataspace(ds_cap,
		[&] (Linux_dataspace &ds) -> size_t { return ds.size(); },
		[&] () /* missing */      -> size_t { return 0; });
}


int Region_map_mmap::_dataspace_fd(Capability<Dataspace> ds_cap)
{
	/*
	 * Return a duplicate of the dataspace file descriptor, which will be freed
	 * immediately after mmap'ing the file (see 'Region_map_mmap').
	 *
	 * Handing out the original file descriptor would result in the premature
	 * release of the descriptor. So the descriptor could be reused (i.e., as a
	 * socket descriptor during the RPC handling). When later destroying the
	 * dataspace, the descriptor would unexpectedly be closed again.
	 */

	return with_linux_dataspace(ds_cap,
		[&] (Linux_dataspace &ds)
		{
			return lx_dup(Capability_space::ipc_cap_data(ds.fd()).dst.socket.value);
		},
		[&] /* missing */ { return -1; });
}


bool Region_map_mmap::_dataspace_writeable(Dataspace_capability ds_cap)
{
	return with_linux_dataspace(ds_cap,
		[&] (Linux_dataspace &ds) { return ds.writeable(); },
		[&] /* missing */         { return false; });
}
