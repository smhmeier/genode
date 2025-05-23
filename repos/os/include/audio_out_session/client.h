/*
 * \brief  Client-side Audio_out-session
 * \author Sebastian Sumpf
 * \date   2012-12-20
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_
#define _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_

#include <base/env.h>
#include <base/rpc_client.h>
#include <base/attached_dataspace.h>
#include <audio_out_session/audio_out_session.h>

namespace Audio_out {
	struct Signal;
	struct Session_client;
}


struct Audio_out::Signal
{
	Genode::Signal_receiver           recv    { };
	Genode::Signal_context            context { };
	Genode::Signal_context_capability cap;

	Signal() : cap(recv.manage(context)) { }
	~Signal() { recv.dissolve(context); }

	void wait() { recv.wait_for_signal(); }
};


class Audio_out::Session_client : public Genode::Rpc_client<Session>
{
	private:

		Genode::Attached_dataspace _shared_ds;

		Signal _progress { };
		Signal _alloc    { };

		Genode::Signal_transmitter _data_avail;

	public:

		/**
		 * Constructor
		 *
		 * \param rm               region map for attaching shared buffer
		 * \param session          session capability
		 * \param alloc_signal     true, install 'alloc_signal' receiver
		 * \param progress_signal  true, install 'progress_signal' receiver
		 */
		Session_client(Genode::Env::Local_rm &rm,
		               Genode::Capability<Session> session,
		               bool alloc_signal, bool progress_signal)
		:
		  Genode::Rpc_client<Session>(session),
		  _shared_ds(rm, call<Rpc_dataspace>()),
		  _data_avail(call<Rpc_data_avail_sigh>())
		{
			_stream = _shared_ds.local_addr<Stream>();

			if (progress_signal)
				progress_sigh(_progress.cap);

			if (alloc_signal)
				alloc_sigh(_alloc.cap);
		}


		/*************
		 ** Signals **
		 *************/

		void progress_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_progress_sigh>(sigh); }

		void alloc_sigh(Genode::Signal_context_capability sigh) override {
			call<Rpc_alloc_sigh>(sigh); }

		Genode::Signal_context_capability data_avail_sigh() override {
			return Genode::Signal_context_capability(); }


		/***********************
		 ** Session interface **
		 ***********************/

		void start() override
		{
			call<Rpc_start>();

			/* reset tail pointer */
			stream()->reset();
		}

		void stop() override { call<Rpc_stop>();  }


		/**********************************
		 ** Session interface extensions **
		 **********************************/

		/**
		 * Wait for progress signal
		 */
		void wait_for_progress()
		{
			if (!_progress.cap.valid()) {
				Genode::warning("Progress signal is not installed, will not block "
				                "(enable in 'Audio_out::Connection')");
				return;
			}

			_progress.wait();
		}

		/**
		 * Wait for allocation signal
		 *
		 * This can be used when the 'Stream' is full and the application wants
		 * to block until the stream has free elements again.
		 */
		void wait_for_alloc()
		{ 
			if (!_alloc.cap.valid()) {
				Genode::warning("Alloc signal is not installed, will not block "
				                "(enable in 'Audio_out::Connection')");
				return;
			}

			_alloc.wait();
		}

		/**
		 * Submit a packet
		 */
		void submit(Packet *packet)
		{
			bool empty = stream()->empty();

			packet->_submit();
			if (empty)
				_data_avail.submit();
		}
};

#endif /* _INCLUDE__AUDIO_OUT_SESSION__CLIENT_H_ */
