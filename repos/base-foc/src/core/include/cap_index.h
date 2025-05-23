/*
 * \brief  Core-specific capability index
 * \author Stefan Kalkowski
 * \date   2012-02-22
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CAP_INDEX_H_
#define _CORE__INCLUDE__CAP_INDEX_H_

/* base-internal includes */
#include <base/internal/native_thread.h>
#include <base/internal/cap_map.h>

/* core includes */
#include <types.h>

namespace Core {

	class Core_cap_index;
	class Platform_thread;
	class Pd_session_component;
}


class Core::Core_cap_index : public Native_capability::Data
{
	private:

		Pd_session_component  *_session;
		Platform_thread const *_pt;

		/*
		 * Noncopyable
		 */
		Core_cap_index(Core_cap_index const &);
		Core_cap_index &operator = (Core_cap_index const &);

	public:

		Core_cap_index(Pd_session_component *session = 0,
		               Platform_thread      *pt      = 0)
		:
			_session(session), _pt(pt)
		{ }

		Pd_session_component const *session() const { return _session; }
		Platform_thread      const *pt()      const { return _pt; }

		void session(Pd_session_component *c) { _session = c; }
		void pt(Platform_thread const *t)     { _pt = t;      }
};

#endif /* _CORE__INCLUDE__CAP_INDEX_H_ */
