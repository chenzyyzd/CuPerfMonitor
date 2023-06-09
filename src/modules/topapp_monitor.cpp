#include "topapp_monitor.h"

TopAppMonitor::TopAppMonitor() { }
TopAppMonitor::~TopAppMonitor() { }

void TopAppMonitor::Start()
{
	unblocked_ = false;
	thread_ = std::thread(std::bind(&TopAppMonitor::Main, this));
	thread_.detach();

	using namespace std::placeholders;
	Broadcast_SetBroadcastReceiver("CgroupWatcher.TopAppCgroupModified", std::bind(&TopAppMonitor::CgroupModified, this, _1));
	Broadcast_SetBroadcastReceiver("CgroupWatcher.ForegroundCgroupModified", std::bind(&TopAppMonitor::CgroupModified, this, _1));
}

void TopAppMonitor::Main()
{
	SetThreadName("TopAppMonitor");

	int topAppPid = -1;
	std::string topAppPkgName = "";
	{
		std::string topAppInfo = DumpTopActivityInfo();
		if (StrContains(topAppInfo, "fore")) {
			// Proc # 0: fore   T/A/TOP  trm: 0 4272:xyz.chenzyadb.cu_toolbox/u0a353 (top-activity)
			int pid = StringToInteger(GetPrevString(StrDivide(topAppInfo, 7), ':'));
			if (pid > 0 && pid < 32768) {
				topAppPid = pid;
				topAppPkgName = GetPrevString(GetPostString(StrDivide(topAppInfo, 7), ':'), '/');
			}
		} else if (StrContains(topAppInfo, "fg")) {
			// Proc # 0: fg     T/A/TOP  LCM  t: 0 4272:xyz.chenzyadb.cu_toolbox/u0a353 (top-activity)
			int pid = StringToInteger(GetPrevString(StrDivide(topAppInfo, 8), ':'));
			if (pid > 0 && pid < 32768) {
				topAppPid = pid;
				topAppPkgName = GetPrevString(GetPostString(StrDivide(topAppInfo, 8), ':'), '/');
			}
		}
	}
	if (!topAppPkgName.empty()) {
		Broadcast_SendBroadcast("TopAppMonitor.TopAppChanged", (void*)topAppPkgName.c_str());
	}

	for (;;) {
		{
			std::unique_lock<std::mutex> lck(mtx_);
			while (!unblocked_) {
				cv_.wait(lck);
			}
			unblocked_ = false;
		}

		int prevTopAppPid = topAppPid;
		{
			std::string topAppInfo = DumpTopActivityInfo();
			if (StrContains(topAppInfo, "fore")) {
				// Proc # 0: fore   T/A/TOP  trm: 0 4272:xyz.chenzyadb.cu_toolbox/u0a353 (top-activity)
				int pid = StringToInteger(GetPrevString(StrDivide(topAppInfo, 7), ':'));
				if (pid > 0 && pid < 32768) {
					topAppPid = pid;
					topAppPkgName = GetPrevString(GetPostString(StrDivide(topAppInfo, 7), ':'), '/');
				}
			} else if (StrContains(topAppInfo, "fg")) {
				// Proc # 0: fg     T/A/TOP  LCM  t: 0 4272:xyz.chenzyadb.cu_toolbox/u0a353 (top-activity)
				int pid = StringToInteger(GetPrevString(StrDivide(topAppInfo, 8), ':'));
				if (pid > 0 && pid < 32768) {
					topAppPid = pid;
					topAppPkgName = GetPrevString(GetPostString(StrDivide(topAppInfo, 8), ':'), '/');
				}
			}
		}
		if (topAppPid != prevTopAppPid) {
			Broadcast_SendBroadcast("TopAppMonitor.TopAppChanged", (void*)topAppPkgName.c_str());
		}

		usleep(500000);
	}
}

void TopAppMonitor::CgroupModified(const void* data)
{
	std::unique_lock<std::mutex> lck(mtx_);
	unblocked_ = true;
	cv_.notify_all();
}
