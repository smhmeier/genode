/*
 * \brief  Libc-internal file operations
 * \author Norman Feske
 * \date   2019-09-23
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__FILE_OPERATIONS_H_
#define _LIBC__INTERNAL__FILE_OPERATIONS_H_

/* Genode includes */
#include <os/path.h>

/* libc includes */
#include <limits.h>   /* for 'PATH_MAX' */

/* libc-internal includes */
#include <internal/plugin.h>
#include <internal/types.h>

namespace Libc {

	using Absolute_path = Genode::Path<PATH_MAX>;

	struct Symlink_resolve_error { };
	using Symlink_resolve_result = Attempt<Ok, Symlink_resolve_error>;

	Symlink_resolve_result resolve_symlinks(char const *path,
	                                        Absolute_path &resolved_path);
}

#endif /* _LIBC__INTERNAL__FILE_OPERATIONS_H_ */
