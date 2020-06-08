#include "Hook_Skill.h"
#include "Settings.h"
//#include "GameSettings.h"
//#include "skse64_common/Relocation.h"
//#include "skse64_common/BranchTrampoline.h"
//#include "skse64_common/SafeWrite.h"
#include "RE/Skyrim.h"
#include "SKSE/API.h"
#include "SKSE/CodeGenerator.h"
#include "SKSE/Trampoline.h"
#include "xbyak/xbyak.h"
#include <fstream>

auto g_thePlayer = RE::PlayerCharacter::GetSingleton();
auto g_gameSettingCollection = RE::GameSettingCollection::GetSingleton();


REL::Offset<std::uintptr_t*> kHook_ModifyPerkPool_Ent(0x008F386F);
REL::Offset<std::uintptr_t*> kHook_ModifyPerkPool_Ret(kHook_ModifyPerkPool_Ent.GetOffset() + 0x1C); //1C

REL::Offset<std::uintptr_t*> kHook_SkillCapPatch_Ent(0x0070D108);
REL::Offset<std::uintptr_t*> kHook_SkillCapPatch_Ret(kHook_SkillCapPatch_Ent.GetOffset() + 0x9);

REL::Offset<std::uintptr_t*> kHook_ExecuteLegendarySkill_Ent(0x008F847D);//get it when legendary skill by trace setbaseav.
REL::Offset<std::uintptr_t*> kHook_ExecuteLegendarySkill_Ret(kHook_ExecuteLegendarySkill_Ent.GetOffset() + 0x6);

REL::Offset<std::uintptr_t*> kHook_CheckConditionForLegendarySkill_Ent(0x008EE403);
REL::Offset<std::uintptr_t*> kHook_CheckConditionForLegendarySkill_Ret(kHook_CheckConditionForLegendarySkill_Ent.GetOffset() + 0x13);

REL::Offset<std::uintptr_t*> kHook_HideLegendaryButton_Ent(0x008EFC19);
REL::Offset<std::uintptr_t*> kHook_HideLegendaryButton_Ret(kHook_HideLegendaryButton_Ent.GetOffset() + 0x1D);

typedef void(*_ImproveSkillByTraining)(void* pPlayer, UInt32 skillID, UInt32 count);
REL::Offset<_ImproveSkillByTraining> ImproveSkillByTraining_Original(0x006C30F0);

typedef void(*_ImprovePlayerSkillPoints)(void*, UInt32, float, UInt64, UInt32, UInt8, bool);
REL::Offset<_ImprovePlayerSkillPoints> ImprovePlayerSkillPoints(0x0070D0C0);
_ImprovePlayerSkillPoints ImprovePlayerSkillPoints_Original = nullptr;

typedef UInt64(*_ImproveAttributeWhenLevelUp)(void*, UInt8);
REL::Offset<_ImproveAttributeWhenLevelUp> ImproveAttributeWhenLevelUp(0x008C19F0);
_ImproveAttributeWhenLevelUp ImproveAttributeWhenLevelUp_Original = nullptr;

typedef bool(*_GetSkillCoefficients)(UInt32, float*, float*, float*, float*);
REL::Offset<_GetSkillCoefficients> GetSkillCoefficients(0x003F2FF0);

typedef UInt16(*_GetLevel)(void* pPlayer);
REL::Offset<_GetLevel> GetLevel(0x005DE910);

typedef float(*_ImproveLevelExpBySkillLevel)(float skillLevel);
REL::Offset<_ImproveLevelExpBySkillLevel> ImproveLevelExpBySkillLevel_Original(0x0070E320);

typedef float(*_GetCurrentActorValue)(void*, UInt32);
REL::Offset<_GetCurrentActorValue> GetCurrentActorValue(0x00629B10);
_GetCurrentActorValue GetCurrentActorValue_Original = nullptr;

typedef float(*_GetBaseActorValue)(void*, UInt32);
REL::Offset<_GetBaseActorValue> GetBaseActorValue(0x00629CE0);	//GetBaseActorValue

typedef float(*_CalculateChargePointsPerUse)(float basePoints, float enchantingLevel);
REL::Offset<_CalculateChargePointsPerUse> CalculateChargePointsPerUse_Original(0x003D06C0);

class ActorValueOwner;
typedef float(*_GetEffectiveSkillLevel)(ActorValueOwner *, UInt32 skillID);
REL::Offset<_GetEffectiveSkillLevel> GetEffectiveSkillLevel(0x003F4DA0);	//V1.5.3

//typedef float(*_Pow)(float, float);
//RelocAddr <_Pow> Pow = 0x01312C52;


//#include <cmath>
float CalculateSkillExpForLevel(UInt32 skillID, float skillLevel)
{
#ifdef _DEBUG
	_MESSAGE("function:%s, skillId:%d, skillLevel:%.2f", __FUNCTION__, skillID, skillLevel);
#endif
	float result = 0.0f;
	float fSkillUseCurve = 1.95f;//0x01D88258;
	if ((g_gameSettingCollection) != nullptr)
	{
		auto pSetting = g_gameSettingCollection->GetSetting("fSkillUseCurve");
		fSkillUseCurve = pSetting != nullptr ? pSetting->GetFloat() : 1.95f;
	}
	if(skillLevel < settings.settingsSkillCaps[skillID - 6])
	{
		result = pow(skillLevel, fSkillUseCurve);
		float a = 1.0f, b = 0.0f, c = 1.0f, d = 0.0f;
		if (GetSkillCoefficients(skillID, &a, &b, &c, &d))
			result = result * c + d;
	}
	return result;
}

float CalculateChargePointsPerUse_Hook(float basePoints, float enchantingLevel)
{


	float fEnchantingCostExponent = 1.10f;// 0x01D8A058;			 //1.10
	float fEnchantingSkillCostBase = 0.005f; // 0x01D8A010;		 //1/200 = 0.005
	float fEnchantingSkillCostScale = 0.5f;// 0x01D8A040;		 //0.5 sqrt
	//RelocPtr<float> unk0 = 0x014E8F78;			 //1.00f
	float fEnchantingSkillCostMult = 3.00f;// 0x01D8A028;			 //3.00

	if ((g_gameSettingCollection) != nullptr)
	{
		auto pSetting = g_gameSettingCollection->GetSetting("fEnchantingCostExponent");
		fEnchantingCostExponent = (pSetting != nullptr) ? pSetting->GetFloat() : 1.10f;
		pSetting = g_gameSettingCollection->GetSetting("fEnchantingSkillCostBase");
		fEnchantingSkillCostBase = (pSetting != nullptr) ? pSetting->GetFloat() : 0.005f;
		pSetting = g_gameSettingCollection->GetSetting("fEnchantingSkillCostScale");
		fEnchantingSkillCostScale = (pSetting != nullptr) ? pSetting->GetFloat() : 0.5f;
		pSetting = g_gameSettingCollection->GetSetting("fEnchantingSkillCostMult");
		fEnchantingSkillCostMult = (pSetting != nullptr) ? pSetting->GetFloat() : 3.00f;
	}

	enchantingLevel = (enchantingLevel > 199.0f) ? 199.0f : enchantingLevel;
	float result = fEnchantingSkillCostMult * pow(basePoints, fEnchantingCostExponent) * (1.00f - pow((enchantingLevel * fEnchantingSkillCostBase), fEnchantingSkillCostScale));
#ifdef _DEBUG
	_MESSAGE("function:%s, basePoints:%.2f, enchantingLevel:%.2f, result:%.2f", __FUNCTION__, basePoints, enchantingLevel, result);
#endif
	return result;
	//Charges Per Use = 3 * (base enchantment cost * magnitude / maximum magnitude)^1.1 * (1 - sqrt(skill/200))
}

void ImproveSkillByTraining_Hook(void* pPlayer, UInt32 skillID, UInt32 count)
{
	//PlayerSkills* skillData = *reinterpret_cast<PlayerSkills**>(reinterpret_cast<uintptr_t>(pPlayer)+0x10B0);

	if (count < 1)
		count = 1;
	if ((skillID >= 6) && (skillID <= 23))
	{
		//LevelData* levelData = &(skillData->data->levelData[skillID - 6]);

		RE::PlayerCharacter::PlayerSkills::Data::SkillData* levelData = &(g_thePlayer->skills->data->skills[skillID - 6]);

		_MESSAGE("player:%p, skill:%d, points:%.2f, maxPoints:%.2f, level:%.2f", pPlayer, skillID, levelData->xp, levelData->levelThreshold, levelData->level);
		for (int j = 0; j < 10; ++j) {
			_MESSAGE("player:%p, skill:%d, points:%.2f, level:%.2f", pPlayer, skillID, g_thePlayer->skills->data->skills[j].xp, g_thePlayer->skills->data->skills[j].level);
		}
		
		float skillProgression = 0.0f;
		if (levelData->levelThreshold > 0.0f)
		{
			skillProgression = levelData->xp / levelData->levelThreshold;

			if (skillProgression >= 1.0f)
				skillProgression = 0.99f; 
		}
		else
			skillProgression = 0.0f;
		for (UInt32 i = 0; i < count; ++i)
		{
			//float skillLevel = GetBaseActorValue((char*)(*g_thePlayer) + 0xB0, skillID);
			float expRequired = CalculateSkillExpForLevel(skillID, g_thePlayer->GetBaseActorValue((RE::ActorValue)skillID));

			_MESSAGE("maxPoints:%.2f, expRequired:%.2f", levelData->levelThreshold, expRequired);

			if (levelData->levelThreshold != expRequired)
				levelData->levelThreshold = expRequired;
			if (levelData->xp <= 0.0f)
				levelData->xp = (levelData->levelThreshold > 0.0f) ? 0.1f : 0.0f;
			if (levelData->xp >= levelData->levelThreshold)
				levelData->xp = (levelData->levelThreshold > 0.0f) ? (levelData->levelThreshold - 0.1f) : 0.0f;

			float expNeeded = levelData->levelThreshold - levelData->xp;
			
			ImprovePlayerSkillPoints_Original(g_thePlayer->skills, skillID, expNeeded, 0, 0, 0, (i < count - 1));
			
		}
		levelData->xp += levelData->levelThreshold * skillProgression;
	}		
}

void ImprovePlayerSkillPoints_Hook(RE::PlayerCharacter::PlayerSkills* skillData, UInt32 skillID, float exp, UInt64 unk1, UInt32 unk2, UInt8 unk3, bool unk4)
{
	if ((skillID >= 6) && (skillID <= 23))
	{
#ifdef _DEBUG
		_MESSAGE("function: %s, skillID: %d, SkillExpGainMults: %.2f", __FUNCTION__, skillID, settings.settingsSkillExpGainMults[skillID - 6]);
#endif
		UInt32 baseSkillLevel = static_cast<UInt32>(g_thePlayer->GetBaseActorValue((RE::ActorValue)skillID));
		float skillMult = settings.settingsSkillExpGainMultsWithSkills[skillID - 6].GetValue(baseSkillLevel);
		UInt16 level = g_thePlayer->GetLevel();
		float levelMult = settings.settingsSkillExpGainMultsWithPCLevel[skillID - 6].GetValue(level);
		exp *= settings.settingsSkillExpGainMults[skillID - 6] * skillMult * levelMult;
	}

#ifdef _DEBUG
	_MESSAGE("function: %s", __FUNCTION__);
	void* actorValue = static_cast<char*>(*g_thePlayer) + 0xB0;
	for (size_t i = 0; i <= 8; ++i)
		_MESSAGE("Index: %d, Function: %016I64X", i, *(*static_cast<uintptr_t**>(actorValue)+i));
#endif

	ImprovePlayerSkillPoints_Original(skillData, skillID, exp, unk1, unk2, unk3, unk4);
}

float ImproveLevelExpBySkillLevel_Hook(float skillLevel, UInt32 skillID)
{
	float baseMult = 1.0f, skillMult = 1.0f, levelMult = 1.0f;
	if ((skillID >= 6) && (skillID <= 23))
	{
		baseMult = settings.settingsLevelSkillExpMults[skillID - 6];
		UInt32 baseSkillLevel = static_cast<UInt32>(g_thePlayer->GetBaseActorValue((RE::ActorValue)skillID));
		skillMult = settings.settingsLevelSkillExpMultsWithSkills[skillID - 6].GetValue(baseSkillLevel);
		UInt16 level = g_thePlayer->GetLevel();
		levelMult = settings.settingsLevelSkillExpMultsWithPCLevel[skillID - 6].GetValue(level);
		//PCLevel has some minor glitch.I need to calculate player's actual level.
	}
	float result = ImproveLevelExpBySkillLevel_Original(skillLevel) * baseMult * skillMult * levelMult;
#ifdef _DEBUG
	_MESSAGE("function:%s, skillId:%d, skillLevel:%.2f, skillMult:%.2f, result:%.2f", __FUNCTION__, skillID, skillLevel, skillMult, result);
#endif
	return result;
}

UInt64 ImproveAttributeWhenLevelUp_Hook(void* unk0, UInt8 unk1)
{
	enum
	{
		kHealth = 0x18,
		kMagicka,
		kStamina
	};
	//static RelocPtr<UInt32> iAVDhmsLevelUp = 0x01D7C200;
	//static RelocPtr<float> fLevelUpCarryWeightMod = 0x01D882B8;

	if (g_gameSettingCollection != nullptr)
	{
		auto iAVDhmsLevelUp = g_gameSettingCollection->GetSetting("iAVDhmsLevelUp");
		auto fLevelUpCarryWeightMod = g_gameSettingCollection->GetSetting("fLevelUpCarryWeightMod");
		if (iAVDhmsLevelUp && fLevelUpCarryWeightMod)
		{
			UInt32 choice = *reinterpret_cast<UInt32*>(static_cast<char*>(unk0) + 0x18);
			UInt16 level = g_thePlayer->GetLevel();

			switch (choice)
			{
			case kHealth:
			{
				iAVDhmsLevelUp->data.u = settings.settingsHealthAtLevelUp.GetValue(level);
				fLevelUpCarryWeightMod->data.f = settings.settingsCarryWeightAtHealthLevelUp.GetValue(level);
				break;
			}
			case kMagicka:
			{
				iAVDhmsLevelUp->data.u = settings.settingsMagickaAtLevelUp.GetValue(level);
				fLevelUpCarryWeightMod->data.f = settings.settingsCarryWeightAtMagickaLevelUp.GetValue(level);
				break;
			}
			case kStamina:
			{
				iAVDhmsLevelUp->data.u = settings.settingsStaminaAtLevelUp.GetValue(level);
				fLevelUpCarryWeightMod->data.f = settings.settingsCarryWeightAtStaminaLevelUp.GetValue(level);
				break;
			}
			default:
			{
				iAVDhmsLevelUp->data.u = 5;
				fLevelUpCarryWeightMod->data.f = 10.0f;
			}
		}
		}
	}

#ifdef _DEBUG
//	_MESSAGE("function: %s, attributePerLevel: %d, carryweightPerLevel: %.2f, level: %d, choice: %d", __FUNCTION__, *attributePerLevel, *carryweightPerLevel, level, choice);
#endif

	return ImproveAttributeWhenLevelUp_Original(unk0, unk1);
}

void ModifyPerkPool_Hook(SInt8 count)
{
	//UInt8* points = *reinterpret_cast<UInt8**>(g_thePlayer.GetPtr()) + 0x11FD; //looks like address changed for VR

	if (count > 0) //AddPerkPoints
	{
		//static float mantissa = 0.0f;  //This vlaue needs to be stored to cross save.
		UInt16 level = g_thePlayer->GetLevel();
		float increment = settings.settingsPerksAtLevelUp.GetValue(level) + settings.settingsPerksAtLevelUp.GetDecimal(level);
		count = static_cast<SInt8>(increment);
		//mantissa = increment - count;
#ifdef _DEBUG
		_MESSAGE("function: %s, count: %d, perkPoints: %d, level: %d", __FUNCTION__, count, *points, level);
#endif
		UInt32 sum = count + g_thePlayer->perkCount;
		g_thePlayer->perkCount = (sum > 0xFF) ? 0xFF : static_cast<SInt8>(sum);
	}
	else //RemovePerkPoints
	{
		SInt32 sum = g_thePlayer->perkCount + count;
		g_thePlayer->perkCount = (sum < 0) ? 0 : static_cast<SInt8>(sum);
	}
}

float GetSkillCap_Hook(UInt32 skillID)
{
	if ((skillID >= 6) && (skillID <= 23))
	{
		_MESSAGE("function: %s, skillID: %d, skillCap: %d", __FUNCTION__, skillID, settings.settingsSkillCaps[skillID - 6]);

		return settings.settingsSkillCaps[skillID - 6];
	}
	else
	{
		//static RelocPtr<float> skillCap = 0x014E6B68;
#ifdef _DEBUG
//		_MESSAGE("function: %s, skillCap: %.2f", __FUNCTION__, *skillCap);
#endif
		_MESSAGE("skillID=%d", skillID);
		return 100.0f;
	}
}

float GetCurrentActorValue_Hook(void* avo, UInt32 skillID)   //PC&NPC  //61F6C0
{
	float skillLevel = GetCurrentActorValue_Original(avo, skillID);
	if ((skillID >= 6) && (skillID <= 23))
	{
		UInt32 skillFormulaCap = settings.settingsSkillFormulaCaps[skillID - 6];
		skillLevel = (skillLevel > skillFormulaCap) ? skillFormulaCap : skillLevel;
#ifdef _DEBUG
		//_MESSAGE("function: %s, skillID: %d, skillLevel:%.2f, skillFormulaCap: %d, this:%p", __FUNCTION__, skillID, skillLevel, settings.settingsSkillFormulaCaps[skillID - 6], avo);
#endif
	}
	return skillLevel;
}

void LegendaryResetSkillLevel_Hook(float baseLevel, UInt32 skillID) 
{
	if (g_gameSettingCollection != nullptr)
	{
		auto fLegendarySkillResetValue = g_gameSettingCollection->GetSetting("fLegendarySkillResetValue");
		if (fLegendarySkillResetValue != nullptr)
		{
			static float originalSetting = fLegendarySkillResetValue->GetFloat();
			if ((skillID >= 6) && (skillID <= 23))
			{
				if (settings.settingsLegendarySkill.bLegendaryKeepSkillLevel)
					fLegendarySkillResetValue->data.f = baseLevel;
				else
				{
					UInt32 legendaryLevel = settings.settingsLegendarySkill.iSkillLevelAfterLegendary;
					if ((legendaryLevel && legendaryLevel > baseLevel) || (!legendaryLevel && originalSetting > baseLevel))
						fLegendarySkillResetValue->data.f = baseLevel;
					else
						fLegendarySkillResetValue->data.f = (!legendaryLevel) ? originalSetting : legendaryLevel;
				}
			}
			else
				fLegendarySkillResetValue->data.f = originalSetting;
		}
	}
}

bool CheckConditionForLegendarySkill_Hook(void* pActorValueOwner, UInt32 skillID)
{
	//float skillLevel = GetBaseActorValue(*(char**)(g_thePlayer.GetPtr()) + 0xB0, skillID);
	float skillLevel = g_thePlayer->GetBaseActorValue((RE::ActorValue)skillID);
	return (skillLevel >= settings.settingsLegendarySkill.iSkillLevelEnableLegendary) ? true : false;
}

bool HideLegendaryButton_Hook(UInt32 skillID)
{
	//float skillLevel = GetBaseActorValue(*(char**)(g_thePlayer.GetPtr()) + 0xB0, skillID);
	float skillLevel = g_thePlayer->GetBaseActorValue((RE::ActorValue)skillID);
	if (skillLevel >= settings.settingsLegendarySkill.iSkillLevelEnableLegendary && !settings.settingsLegendarySkill.bHideLegendaryButton)
		return true;
	return false;
}

void Hook_Skill_Commit()
{
	
	SKSE::SafeWrite16(GetEffectiveSkillLevel.GetAddress() + 0x34, 0x9090);


	auto trampoline = SKSE::GetTrampoline();

	trampoline->Write6Branch(ImproveSkillByTraining_Original.GetAddress(), (uintptr_t)ImproveSkillByTraining_Hook);
	_MESSAGE("ImproveLevelExpBySkillLevel_Original: %016I64X", ImproveLevelExpBySkillLevel_Original.GetAddress());

	trampoline->Write6Branch(CalculateChargePointsPerUse_Original.GetAddress(), (uintptr_t)CalculateChargePointsPerUse_Hook);
	_MESSAGE("CalculateChargePointsPerUse_Original: %016I64X", CalculateChargePointsPerUse_Original.GetAddress());


	struct ImprovePlayerSkillPoints_Code : SKSE::CodeGenerator
	{
		ImprovePlayerSkillPoints_Code(int a_int) : SKSE::CodeGenerator() {
			Xbyak::Label retnLabel;
			push(rcx);
			push(rdx);
			mov(rdx, rsi);
			sub(rsp, 0x20);
			mov(rax,(std::uintptr_t)ImproveLevelExpBySkillLevel_Hook);
			call(rax);
			//call((void*)&ImproveLevelExpBySkillLevel_Hook);
			add(rsp, 0x20);
			pop(rdx);
			pop(rcx);
			jmp(ptr[rip + retnLabel]);

			L(retnLabel);
			dq(ImprovePlayerSkillPoints.GetAddress() + 0x230);
		}
	};

	ImprovePlayerSkillPoints_Code ImprovePlayerSkillPoints_Code(0);
	ImprovePlayerSkillPoints_Code.finalize();

	trampoline = SKSE::GetTrampoline();
	trampoline->Write5Branch(ImprovePlayerSkillPoints.GetAddress() + 0x22B, uintptr_t(ImprovePlayerSkillPoints_Code.getCode()));
	_MESSAGE("ImprovePlayerSkillPoints: %016I64X", ImprovePlayerSkillPoints.GetAddress());


	{

		struct ModifyPerkPool_Code : SKSE::CodeGenerator
		{
			ModifyPerkPool_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				mov(rcx, rbx);
				sub(rsp, 0x20);
				mov(rax, (std::uintptr_t)ModifyPerkPool_Hook);
				call(rax);
				add(rsp, 0x20);
				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(kHook_ModifyPerkPool_Ret.GetAddress());
			}
		};

		ModifyPerkPool_Code ModifyPerkPool_Code(0);
		ModifyPerkPool_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		trampoline->Write5Branch(kHook_ModifyPerkPool_Ent.GetAddress() + 0x22B, uintptr_t(ModifyPerkPool_Code.getCode()));
		_MESSAGE("kHook_ModifyPerkPool_Ent: %016I64X", kHook_ModifyPerkPool_Ent.GetAddress());
	}

	{

		struct SkillCapPatch_Code : SKSE::CodeGenerator
		{
			SkillCapPatch_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				mov(rcx, rsi);
				sub(rsp, 0x30);
				movss(ptr[rsp + 0x28], xmm0);
				mov(rax, (std::uintptr_t)GetSkillCap_Hook);
				call(rax);
				movss(xmm8, xmm0);
				movss(xmm0, ptr[rsp + 0x28]);
				add(rsp, 0x30);
				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(kHook_SkillCapPatch_Ret.GetAddress());
			}
		};

		SkillCapPatch_Code SkillCapPatch_Code(0);
		SkillCapPatch_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		trampoline->Write5Branch(kHook_SkillCapPatch_Ent.GetAddress() + 0x22B, uintptr_t(SkillCapPatch_Code.getCode()));
		_MESSAGE("kHook_SkillCapPatch_Ent: %016I64X", kHook_SkillCapPatch_Ent.GetAddress());
	}


	{
		struct ImprovePlayerSkillPoints_Code : SKSE::CodeGenerator
		{
			ImprovePlayerSkillPoints_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				mov(r11, rsp);
				push(rbp);
				push(rsi);
				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(ImprovePlayerSkillPoints.GetAddress() + 0x5);
			}
		};

		ImprovePlayerSkillPoints_Code ImprovePlayerSkillPoints_Code(0);
		ImprovePlayerSkillPoints_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		void * codeBuf = trampoline->StartAlloc();

		ImprovePlayerSkillPoints_Original = (_ImprovePlayerSkillPoints)codeBuf;

		trampoline->Write5Branch(ImprovePlayerSkillPoints.GetAddress(), (uintptr_t)ImprovePlayerSkillPoints_Hook);
	}

	{
		struct ImproveAttributeWhenLevelUp_Code : SKSE::CodeGenerator
		{
			ImproveAttributeWhenLevelUp_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				push(rdi);
				sub(rsp, 0x30);

				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(ImproveAttributeWhenLevelUp.GetAddress() + 0x6);
			}
		};

		ImproveAttributeWhenLevelUp_Code ImproveAttributeWhenLevelUp_Code(0);
		ImproveAttributeWhenLevelUp_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		void* codeBuf = trampoline->StartAlloc();

		ImproveAttributeWhenLevelUp_Original = (_ImproveAttributeWhenLevelUp)codeBuf;

		trampoline->Write6Branch(ImproveAttributeWhenLevelUp.GetAddress(), (uintptr_t)ImproveAttributeWhenLevelUp_Hook);

		SKSE::SafeWrite8(ImproveAttributeWhenLevelUp.GetAddress() + 0x9B, 0);
	}

	{
		struct GetCurrentActorValue_Code : SKSE::CodeGenerator
		{
			GetCurrentActorValue_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				push(rbp);
				push(rsi);
				push(rdi);
				push(r14);

				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(GetCurrentActorValue.GetAddress() + 0x6);
			}
		};

		GetCurrentActorValue_Code GetCurrentActorValue_Code(0);
		GetCurrentActorValue_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		void* codeBuf = trampoline->StartAlloc();

		GetCurrentActorValue_Original = (_GetCurrentActorValue)codeBuf;

		trampoline->Write6Branch(GetCurrentActorValue.GetAddress(), (uintptr_t)GetCurrentActorValue_Hook);
	}

	{
		struct ExecuteLegendarySkill_Code : SKSE::CodeGenerator
		{
			ExecuteLegendarySkill_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				mov(edx, ptr[rsi + 0x1C]);
				mov(rax, (std::uintptr_t)LegendaryResetSkillLevel_Hook);
				call(rax);

				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(kHook_ExecuteLegendarySkill_Ret.GetAddress());
			}
		};

		ExecuteLegendarySkill_Code ExecuteLegendarySkill_Code(0);
		ExecuteLegendarySkill_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		trampoline->Write6Branch(kHook_ExecuteLegendarySkill_Ent.GetAddress(), uintptr_t(ExecuteLegendarySkill_Code.getCode()));
	}

	{
		struct CheckConditionForLegendarySkill_Code : SKSE::CodeGenerator
		{
			CheckConditionForLegendarySkill_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;

				mov(edx, eax);
				lea(rcx, ptr[rdi + 0xB0]);
				mov(rax, (std::uintptr_t)CheckConditionForLegendarySkill_Hook);
				call(rax);
				cmp(al, 1);

				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(kHook_CheckConditionForLegendarySkill_Ret.GetAddress());
			}
		};

		CheckConditionForLegendarySkill_Code CheckConditionForLegendarySkill_Code(0);
		CheckConditionForLegendarySkill_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		trampoline->Write6Branch(kHook_CheckConditionForLegendarySkill_Ent.GetAddress(), uintptr_t(CheckConditionForLegendarySkill_Code.getCode()));
	}

	{
		struct HideLegendaryButton_Code : SKSE::CodeGenerator
		{
			HideLegendaryButton_Code(int a_int) : SKSE::CodeGenerator()
			{
				Xbyak::Label retnLabel;
				mov(ecx, esi);
				mov(rax, (std::uintptr_t)HideLegendaryButton_Hook);
				call(rax);
				cmp(al, 1);

				jmp(ptr[rip + retnLabel]);

			L(retnLabel);
				dq(kHook_HideLegendaryButton_Ret.GetAddress());
			}
		};

		HideLegendaryButton_Code HideLegendaryButton_Code(0);
		HideLegendaryButton_Code.finalize();

		trampoline = SKSE::GetTrampoline();
		trampoline->Write6Branch(kHook_HideLegendaryButton_Ent.GetAddress(), uintptr_t(HideLegendaryButton_Code.getCode()));

	}

	
}


