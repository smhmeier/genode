/*
 * \brief  C-library back end for getrandom/getentropy
 * \author Emery Hemingway
 * \date   2019-05-07
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Libc includes */
extern "C" {
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/random.h>
}

/* libc-internal includes */
#include <internal/errno.h>
#include <internal/types.h>
#include <internal/init.h>

/* Genode includes */
#include <trace/timestamp.h>
#include <base/log.h>
#include <util/string.h>


using namespace Libc;

static Config const *_config_ptr;

void Libc::init_random(Config const &config)
{
	_config_ptr = &config;
}


static ssize_t read_rng(char *buf, size_t buflen)
{
	struct Missing_call_of_init_random : Genode::Exception { };
	if (!_config_ptr)
		throw Missing_call_of_init_random();

	static int rng_fd { -1 };
	static bool fallback { false };

	if (fallback) {
		size_t off = 0;
		while (off < buflen) {
			/* collect 31 bits of random */
			unsigned const nonce = random();
			size_t n = min(4U, buflen-off);
			::memcpy(buf+off, &nonce, n);
			off += n;
		}
		return buflen;
	}

	if (rng_fd == -1) {
		if (_config_ptr->rng.length() <= 1) {
			warning("Libc RNG not configured");

			/* initialize the FreeBSD random facility */
			srandom(Trace::timestamp()|1);
			fallback = true;

			return read_rng(buf, buflen);
		}

		rng_fd = open(_config_ptr->rng.string(), O_RDONLY);
		if (rng_fd == -1) {
			error("RNG device ", _config_ptr->rng.string(), " not readable!");
			exit(~0);
		}
	}

	return read(rng_fd, buf, buflen);
}


extern "C" ssize_t __attribute__((weak))
getrandom(void *buf, size_t buflen, unsigned int flags)
{
	size_t off = 0;
	while (off < buflen && off < 256) {
		ssize_t n = read_rng((char*)buf+off, buflen-off);
		if (n < 1) return Errno(EIO);
		off += n;
	}
	return off;
}


extern "C" int __attribute__((weak))
getentropy(void *buf, size_t buflen)
{
	/* maximum permitted value for the length argument is 256 */
	if (256 < buflen) return Errno(EIO);

	size_t off = 0;
	while (off < buflen) {
		ssize_t n = read_rng((char*)buf+off, buflen-off);
		if (n < 1) return Errno(EIO);
		off += n;
	}
	return 0;
}
