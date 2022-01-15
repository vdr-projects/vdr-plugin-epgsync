/*
 * remoteosd.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <vdr/menuitems.h>
#include <vdr/i18n.h>
#include "setup.h"

cEpgSyncSetup EpgSyncSetup;

cEpgSyncSetup::cEpgSyncSetup() {
	hideMainMenuEntry = 0;
	serverIp[0] = 0;
	serverPort = 0;
	connectAttempts = 1;
	nowNext = 0;
	channelByChannel = 0;
	syncOnStart = 0;
	everyHours = 0;
	redirectChannels = rcmId;
	channelTypes = ctAll;
}

cEpgSyncSetup& cEpgSyncSetup::operator=(const cEpgSyncSetup &Setup) {
	hideMainMenuEntry = Setup.hideMainMenuEntry;
	strn0cpy(serverIp, Setup.serverIp, sizeof(serverIp));
	serverPort = Setup.serverPort;
	connectAttempts = Setup.connectAttempts;
	nowNext = Setup.nowNext;
	channelByChannel = Setup.channelByChannel;
	syncOnStart = Setup.syncOnStart;
	everyHours = Setup.everyHours;
	redirectChannels = Setup.redirectChannels;
	channelTypes = Setup.channelTypes;
	return *this;
}

bool cEpgSyncSetup::Parse(const char *Name, const char *Value) {
	if (!strcasecmp(Name, "HideMainMenuEntry"))
		hideMainMenuEntry = atoi(Value);
	else if (!strcasecmp(Name, "ServerIp"))
		strn0cpy(serverIp, Value, sizeof(serverIp));
	else if (!strcasecmp(Name, "ServerPort"))
		serverPort = atoi(Value);
	else if (!strcasecmp(Name, "ConnectAttempts"))
		connectAttempts = atoi(Value);
	else if (!strcasecmp(Name, "NowNext"))
		nowNext = atoi(Value);
	else if (!strcasecmp(Name, "ChannelByChannel"))
		channelByChannel = atoi(Value);
	else if (!strcasecmp(Name, "SyncOnStart"))
		syncOnStart = atoi(Value);
	else if (!strcasecmp(Name, "EveryHours"))
		everyHours = atoi(Value);
	else if (!strcasecmp(Name, "RedirectChannels"))
		redirectChannels = atoi(Value);
	else if (!strcasecmp(Name, "ChannelTypes"))
		channelTypes = atoi(Value);
	else
		return false;
	return true;
}

void cEpgSyncMenuSetup::Store() {
	SetupStore("HideMainMenuEntry", setupTmp.hideMainMenuEntry);
	SetupStore("ServerIp", setupTmp.serverIp);
	SetupStore("ServerPort", setupTmp.serverPort);
	SetupStore("ConnectAttempts", setupTmp.connectAttempts);
	SetupStore("NowNext", setupTmp.nowNext);
	SetupStore("ChannelByChannel", setupTmp.channelByChannel);
	SetupStore("SyncOnStart", setupTmp.syncOnStart);
	SetupStore("EveryHours", setupTmp.everyHours);
	SetupStore("RedirectChannels", setupTmp.redirectChannels);
	SetupStore("ChannelTypes", setupTmp.channelTypes);
	EpgSyncSetup = setupTmp;
}

cEpgSyncMenuSetup::cEpgSyncMenuSetup() {
	setupTmp = EpgSyncSetup;
	redirectChannelsTexts[rcmId] = "ID";
	redirectChannelsTexts[rcmIdName] = strdup(cString::sprintf("ID, %s", trVDR("Name")));
	redirectChannelsTexts[rcmNameId] = strdup(cString::sprintf("%s, ID", trVDR("Name")));
	
	channelTypeTexts[ctAll] = tr("all");
	channelTypeTexts[ctDVB_C] = "DVB-C";
	channelTypeTexts[ctDVB_S] = "DVB-S";
	channelTypeTexts[ctDVB_T] = "DVB-T";
	channelTypeTexts[ctAnalog] = tr("analog");
	channelTypeTexts[ctIptv] = "IP";

	Add(new cMenuEditBoolItem(tr("Hide mainmenu entry"), &setupTmp.hideMainMenuEntry));
	Add(new cMenuEditStrItem(tr("Server IP"), setupTmp.serverIp, 15, ".1234567890"));
	Add(new cMenuEditIntItem(tr("Server port"), &setupTmp.serverPort, 0, 65535, tr("from svdrpservice")));
	Add(new cMenuEditIntItem(tr("Connection attempts"), &setupTmp.connectAttempts, 0, 10));
	Add(new cMenuEditBoolItem(tr("Update \"now\" and \"next\" first"), &setupTmp.nowNext));
	Add(new cMenuEditBoolItem(tr("Sync channel by channel"), &setupTmp.channelByChannel));
	Add(new cMenuEditBoolItem(tr("EPG sync on startup"), &setupTmp.syncOnStart));
	Add(new cMenuEditIntItem(tr("EPG sync timeout (h)"), &setupTmp.everyHours, 0, INT_MAX, trVDR("off")));
	Add(new cMenuEditStraItem(tr("Map channels by"), &setupTmp.redirectChannels, rcm_Count, redirectChannelsTexts));
	Add(new cMenuEditStraItem(tr("Target channels"), &setupTmp.channelTypes, ct_Count, channelTypeTexts));
}

cEpgSyncMenuSetup::~cEpgSyncMenuSetup() {
	free((void *) redirectChannelsTexts[rcmIdName]);
	free((void *) redirectChannelsTexts[rcmNameId]);
}
