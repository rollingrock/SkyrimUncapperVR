//#include <ShlObj.h>
//#include "skse64_common/Relocation.h"
//#include "Relocation/RVA.h"
//#include "skse64_common/BranchTrampoline.h"
//#include "Utilities.h"
//#include "PluginAPI.h"


#include "common/IDebugLog.h"
#include "skse64_common/skse_version.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64/PluginAPI.h"
#include <ShlObj.h>

#include "SKSE/API.h"
#include "SKSE/Interfaces.h"
#include "SKSE/Logger.h"

#include "Settings.h"
#include "Hook_Skill.h"

//void SkyrimUncapper_Initialize(void)
//{
//	static bool isInit = false;
//	if (isInit) return;
//	isInit = true;
//
//	gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim VR\\SkyrimUncapper\\SkyrimUncapper.log");
//
//
//
//	std::string version;
//
//	if (GetFileVersion(GAME_EXE_NAME, version))
//	{
//
//	}
//	else
//	{
//		MessageBox(NULL, "Can't access SkyrimVR.exe's version info, SkyrimUncapper won't be initialized", "SkyrimUncapper", MB_OK);
//		return;
//	}
//
//	if (!g_branchTrampoline.Create(1024 * 64))
//	{
//		_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
//		return;
//	}
//
//	if (!g_localTrampoline.Create(1024 * 64, g_moduleHandle))
//	{
//		_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
//		return;
//	}
//	settings.ReadConfig();
//
//	Hook_Skill_Commit();
//
//	_MESSAGE("Init complete");
//}


extern "C" {
	bool SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
	{
		SKSE::Logger::OpenRelative(FOLDERID_Documents, R"(\\My Games\\Skyrim VR\\SKSE\\SkyrimUncapper.log)");
		SKSE::Logger::SetPrintLevel(SKSE::Logger::Level::kDebugMessage);
		SKSE::Logger::SetFlushLevel(SKSE::Logger::Level::kDebugMessage);
		SKSE::Logger::TrackTrampolineStats(true);
		SKSE::Logger::UseLogStamp(true);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "EngineFixesVR";
		a_info->version = 1;

		_MESSAGE("imagebase = %016I64X ===============", GetModuleHandle(NULL));
		_MESSAGE("reloc mgr imagebase = %016I64X ==============", REL::Module::BaseAddr());

		a_info->infoVersion = PluginInfo::kInfoVersion;
		a_info->name = "SkyrimUncapper";
		a_info->version = 1;

		if (a_skse->IsEditor())
		{
			_ERROR("loaded in editor, marking as incompatible");

			return false;
		}

		auto ver = a_skse->RuntimeVersion();
		if (ver <= SKSE::RUNTIME_VR_1_4_15)
		{
			_FATALERROR("Unsupported runtime version %s!\n", ver.GetString().c_str());
			return false;
		}
		return true;
	}

	bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
	{
		if (!SKSE::Init(a_skse))
		{
			_FATALERROR("Cannot INIT SKSE CommonLib!!!!");
			return false;
		}

		if (!SKSE::AllocTrampoline(1 << 9))
		{
			_FATALERROR("Cannot Allocate Trampoline!!!!");
			return false;
		}

		settings.ReadConfig();
		_MESSAGE("settings ini successfully read in");

		Hook_Skill_Commit();

		_MESSAGE("[MESSAGE] SkyrimUncapperVR loaded");
		return true;
	}
};