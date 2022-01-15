#include <vdr/plugin.h>
#include <vdr/epg.h>
#include <vdr/channels.h>
#include <vdr/skins.h>
#include <vdr/i18n.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include "setup.h"
#include "thread.h"

#define EPGSYNC_SLEEPMS 30

bool IsType(const cChannel* Channel, eChannelTypes Type)
{
	if (!Channel)
		return false;

	if (Type == ctAll)
		return true;

	cString source = cSource::ToString(Channel->Source());
	char c = ((const char*) source)[0];
	switch (c) {
		case 'C':
			// analogtv-, pvrinput-, pvrusb2-plugin
			if (Channel->Ca() >= 0xa0 && Channel->Ca() <= 0xa2)
				return Type == ctAnalog;
			else
				return Type == ctDVB_C;
		case 'I':
			return Type == ctIptv;
		case 'S':
			return Type == ctDVB_S;
		case 'T':
			return Type == ctDVB_T;
		case 'V':
			return Type == ctAnalog;
		default:
			return false;
	}
}

cChannel *GetChannelByName(const char* Name, const cChannel *IgnoreChannel = NULL, eChannelTypes Type = ctAll)
{
	for (cChannel *channel = Channels.First(); channel; channel = Channels.Next(channel)) {
		if (strcasecmp(Name, channel->Name()) == 0 || strcasecmp(Name, channel->ShortName()) == 0) {
			if (IsType(channel, Type) && channel != IgnoreChannel)
				return channel;
		}
	}
	return NULL;
}

void cEpgSyncThread::Action() {
	SetPriority(15);

	plugin = cPluginManager::GetPlugin("svdrpservice");
	if (!plugin) {
		esyslog("EpgSync: Plugin svdrpservice not available");
		return;
	}

	svdrp.handle = -1;

	for (int i = 0; svdrp.handle < 0 && i < EpgSyncSetup.connectAttempts && Running(); i++) {
		if (i > 0)
			cCondWait::SleepMs((1 << i) * 1000);
		svdrp.serverIp = EpgSyncSetup.serverIp;
		svdrp.serverPort = EpgSyncSetup.serverPort;
		svdrp.shared = EpgSyncSetup.channelByChannel;
		plugin->Service("SvdrpConnection-v1.0", &svdrp);
	}

	if (svdrp.handle < 0) {
		Skins.QueueMessage(mtError, tr("EpgSync: Unable to connect to Server"));
		return;
	}

	FILE *f = tmpfile();
	if (!f) {
		esyslog("EpgSync: Unable to open temporary file");
		plugin->Service("SvdrpConnection-v1.0", &svdrp);
		return;
	}

	// Get now and next
	if (EpgSyncSetup.nowNext && CmdLSTE(f, "now") && CmdLSTE(f, "next")) {
		AddSchedule(f);
	}

	if (EpgSyncSetup.channelByChannel) {
		// Get channel by channel
		if (EpgSyncSetup.redirectChannels == rcmId) {
			// Direct import, no mapping:
			// loop through local channels, get channels by ID
			cSchedulesLock *lock = NULL;
			for (cChannel *channel = Channels.First(); channel && Running();
					channel = Channels.Next(channel)) {
				if (!lock)
					lock = new cSchedulesLock();
				if (cSchedules::Schedules(*lock)->GetSchedule(channel)) {
					DELETENULL(lock);
					if (CmdLSTE(f, *channel->GetChannelID().ToString())) {
						AddSchedule(f);
					}
					cCondWait::SleepMs(EPGSYNC_SLEEPMS);
				}
			}
			DELETENULL(lock);
		}
		else {
			// Map channels by name:
			// loop through remote channel list, get channels by number
			SvdrpCommand_v1_0 cmd;
			cmd.command = "LSTC\r\n";
			cmd.handle = svdrp.handle;

			plugin->Service("SvdrpCommand-v1.0", &cmd);

			if (cmd.responseCode == 250) {
				for (cLine *line = cmd.reply.First(); line && Running();
						line = cmd.reply.Next(line)) {
					const char* s = line->Text();
					const char* p = strchr(s, ' ');
					if (p && p > s) {
						if (CmdLSTE(f, cString::sprintf("%.*s", (int)(p - s), s))) {
							AddSchedule(f);
						}
						cCondWait::SleepMs(EPGSYNC_SLEEPMS);
					}
					else {
	        				esyslog("EpgSync: LSTC returned channel without number: %s", line->Text());
					}
				}
			}
			else {
				cLine *line = cmd.reply.First();
	        		esyslog("EpgSync: LSTC error %hu %s", cmd.responseCode,
					line ? line->Text() : "");
			}
		}
	}
	else {
		// Get complete epg
		if (CmdLSTE(f))
			AddSchedule(f);
	}
	fclose(f);

	plugin->Service("SvdrpConnection-v1.0", &svdrp);
	cSchedules::Cleanup(true);
	last = time(NULL);
}

bool cEpgSyncThread::CmdLSTE(FILE *f, const char *Arg) {
	SvdrpCommand_v1_0 cmd;
	cmd.command = cString::sprintf("LSTE %s\r\n", Arg ? Arg : "");
	cmd.handle = svdrp.handle;

	if (!Running())
		return false;

	plugin->Service("SvdrpCommand-v1.0", &cmd);
	cLine *line = cmd.reply.First();
	if (cmd.responseCode == 550) {
		// "Channel not defined" or "No schedule found"
		return false;
	}
	else if (cmd.responseCode != 215 || !line) {
	        esyslog("EpgSync: LSTE error %hu %s", cmd.responseCode,
				line ? line->Text() : "");
		return false;
	}

	const cChannel* targetChannel = NULL;
	while (cmd.reply.Next(line)) {
		const char* s = line->Text();
		if (*s == 'C') {
			// new channel begins
			targetChannel = NULL;

			const char* p = skipspace(s + 1);
			cChannel *c = Channels.GetByChannelID(tChannelID::FromString(p));
			bool cOk = IsType(c, (eChannelTypes) EpgSyncSetup.channelTypes);

			if (cOk && EpgSyncSetup.redirectChannels != rcmNameId) {
				// got acceptable channel by ID
				targetChannel = c;
			}
			else if (EpgSyncSetup.redirectChannels != rcmId) {
				// get channel by name
				p = strchr(p, ' ');
				if (p) {
					// got channel name
					p = skipspace(p);
					targetChannel = GetChannelByName(p, c, (eChannelTypes) EpgSyncSetup.channelTypes);
				}
				// fallback to channel with original ID
				if (!targetChannel && cOk)
					targetChannel = c;
			}

			if (targetChannel) {
				// generate channel header
				if (fputs("C ", f) < 0 ||
						fputs(targetChannel->GetChannelID().ToString(), f) < 0 ||
						fputs(" ", f) < 0 ||
						fputs(targetChannel->Name(), f) < 0 ||
						fputs("\n", f) < 0) {
					LOG_ERROR;
					return false;
				}
			}
		}
		else if (targetChannel) {
			// copy channel data
			if (fputs(s, f) < 0 || fputs("\n", f) < 0) {
				LOG_ERROR;
				return false;
			}
		}
		line = cmd.reply.Next(line);
	}
	return Running();
}

void cEpgSyncThread::AddSchedule(FILE *f) {
	rewind(f);

	if (!cSchedules::Read(f))
		esyslog("EpgSync: Error parsing EPG data");

	rewind(f);
	if (ftruncate(fileno(f), 0) < 0) {
		LOG_ERROR;
	}
}

cEpgSyncThread::cEpgSyncThread(): cThread("epgsync") {
	plugin = NULL;
	// initialized to "now", so no scheduled sync right after VDR start
	// use syncOnStart option instead
	last = time(NULL);
}

cEpgSyncThread::~cEpgSyncThread() {
	Cancel(5);
}
