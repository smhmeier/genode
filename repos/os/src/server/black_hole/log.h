/*
 * \brief  LOG session component and root
 * \author Silas Meier
 * \date   2025-05-15
 */

#ifndef _LOG_H_
#define _LOG_H_

/* base includes */
#include <base/attached_ram_dataspace.h>
#include <rom_session/rom_session.h>
#include <root/component.h>
#include <log_session/connection.h>

namespace Black_hole {

	using namespace Genode;

	class  Log_session;
	class  Log_root;
}


class Black_hole::Log_session : public Session_object<Genode::Log_session>
{
public:

		Log_session(Env             &env,
		            Resources const &resources,
		            Label     const &label,
		            Diag      const &diag)
		: Session_object(env.ep(), resources, label, diag)
		{ }

		void write(String const &) override { }

};


class Black_hole::Log_root : public Root_component<Log_session>
{
private:

		Env &_env;

protected:

		Log_session *_create_session(const char *args)
		override
		{
			return new (md_alloc())
			       Log_session {
				       _env, session_resources_from_args(args),
				       session_label_from_args(args),
				       session_diag_from_args(args) };
		}

public:

	Log_root(Env       &env, Allocator &alloc)
            :
	Root_component<Log_session> { &env.ep().rpc_ep(), &alloc },
	_env                        { env }
	{ }
};

#endif /* _LOG_H_ */
