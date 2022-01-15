/*
 * thread.h: thread doing the actual work
 *
 * See the README file for copyright information and how to reach the author.
 */

#ifndef _EPGSYNC_THREAD__H
#define _EPGSYNC_THREAD__H

#include <vdr/thread.h>
#include <vdr/tools.h>
#include <vdr/plugin.h>
#include "svdrpservice.h"
#include <time.h>

class cEpgSyncThread: public cThread {
	private:
		cPlugin *plugin;
		SvdrpConnection_v1_0 svdrp;
		time_t last;
	protected:
		virtual void Action();
		bool CmdLSTE(FILE *f, const char *Arg = NULL);
		void AddSchedule(FILE *f);
	public:
		time_t LastRun() const { return last; };
		cEpgSyncThread();
		virtual ~cEpgSyncThread();
};

#endif //_EPGSYNC_THREAD__H
