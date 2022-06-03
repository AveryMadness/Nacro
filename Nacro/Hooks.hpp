#pragma once

#include "MinHook/MinHook.h"
#pragma comment(lib, "MinHook/libMinHook.x64.lib")


#include "SDK.hpp"
#include "Player.hpp"
#include "World.hpp"
#include "Cheats.hpp"

#define NPOS std::string::npos
using namespace SDK;

typedef PVOID(__fastcall* fOriginalPE)(void*, void*, void*);
static fOriginalPE OriginalPE = *Utils::Offset<fOriginalPE>(Offsets::ProcessEventOffset);

typedef void(__fastcall* CollectGarbage_Internal)(int32_t KeepFlags, bool bPerformFullPurge);
static CollectGarbage_Internal OriginalGC = *Utils::Offset<CollectGarbage_Internal>(Offsets::CGInternalOffset);

namespace Hooks
{
	void CreateHooks();
	void* PEHook(UObject* Object, SDK::UFunction* Function, void* Parameters);
	void* CGHook(int32_t KeepFlags, bool bPerformFullPurge);

	void* PEHook(UObject* Object, SDK::UFunction* Function, void* Parameters)
	{
		std::string FullFuncName = Function->GetFullName();
		std::string FuncName = Function->GetName();

		if (FuncName.find("BP_PlayButton") != NPOS)
		{
			//Get CharacterParts
			Player::GrabCharacterParts();

			Globals::GameplayStatics->STATIC_OpenLevel(Globals::GEngine->GameViewport->World, "Athena_Terrain", true, L"");
			Globals::bIsInLobby = false;
		}
		
		

		if (FuncName.find("ReadyToStartMatch") != NPOS && !Globals::bIsInitialized && !Globals::bIsInLobby)
		{
			Globals::InitGlobalsAthena();

			Player::SpawnPlayer();
			Globals::AthenaController->Possess(Globals::AthenaPawn);

			World::LoadItems();
			Player::Equip(Globals::Pickaxe, FGuid{ 0,0,0,0 });

			World::SetupMiniMap();

			Player::ChooseParts(Globals::charPartHead, Globals::charPartBody);
			Player::ShowParts();

			Globals::DeathMontage = UObject::FindObject<UAnimMontage>("AnimMontage PlayerDeath_Athena.PlayerDeath_Athena");

			Player::SetTeamIndex(EFortTeam::HumanPvP_Team1);

			static_cast<UFortCheatManager*>(Globals::AthenaController->CheatManager)->ToggleInfiniteAmmo();

			World::StartMatch();

			CreateThread(0, 0, Player::UpdatePawn, 0, 0, 0);
			CreateHooks();
		}

		if (FuncName.find("LoadingScreenDropped") != NPOS && Globals::bIsInitialized && !Globals::bIsInGame && !Globals::bIsInLobby)
		{
			Globals::bIsInGame = true;
			Globals::AthenaGameState->FortTimeOfDayManager->TimeOfDay = rand() % 25;

			Globals::AthenaController->bHasClientFinishedLoading = true;
			Globals::AthenaController->ServerSetClientHasFinishedLoading(true);

			Globals::AthenaController->bHasServerFinishedLoading = true;
			Globals::AthenaController->OnRep_bHasServerFinishedLoading();
		}

		if (FuncName.find("AttemptAircraftJump") != NPOS && Globals::bIsInGame || FuncName.find("AircraftExitedDropZone") != NPOS && Globals::bIsInGame)
		{
			if (Globals::AthenaController->IsInAircraft())
			{
				Player::SpawnPlayer();
				Globals::AthenaPawn->K2_SetActorRotation(FRotator{ 0,Globals::AthenaPawn->K2_GetActorRotation().Yaw,0 }, false);
				Globals::AthenaController->Possess(Globals::AthenaPawn);

				Player::Equip(Globals::Pickaxe, FGuid{ 0,0,0,0 });

				if (Globals::JillMode)
					Globals::AthenaPawn->Mesh->SetSkeletalMesh(Globals::JillMesh, true);

				Player::ShowParts();
			}
		}

		if (FuncName.find("ServerHandlePickup") != NPOS && Globals::bIsInGame)
		{
			struct ServerHandlePickupParams
		{
			FFortItemEntry* Pickup;
			float InFlyTime;
			FVector InStartDirection;
			bool bPlayPickupSound;
		};
			
			auto CurrentParams = (ServerHandlePickupParams*)Params;

		auto ItemInstances = Globals::AthenaController->WorldInventory->Inventory->ItemInstances;

		if (CurrentParams->Pickup != nullptr)
		{
			// get world item definition from item entry
			auto WorldItemDefinition = (UFortWorldItemDefinition*)(CurrentParams->Pickup->ItemDefinition);
			auto QuickbarSlots = Globals::AthenaController->QuickBars->PrimaryQuickbar->Slots;

			for (int i = 0; i < QuickbarSlots.Num(); i++)
			{
				if (QuickbarSlots[i].Items.Data == 0)
				{
					if (i >= 6)
					{
						// no space left in inventory, we should replace the current focused quickbar with this new pickup.
						int CurrentFocusedSlot = Globals::AthenaController->QuickBars->PrimaryQuickbar->CurrentFocusedSlot;

						// do not replace pickaxe
						if (*CurrentFocusedSlot == 0)
						{
							continue;
						}

						i = *CurrentFocusedSlot;

						FGuid CurrentFocusedGUID = QuickbarSlots[CurrentFocusedSlot].Items[0];

						// loop through item entries and see which item matches the current focused slot GUID
						for (int j = 0; i < ItemInstances->Num(); j++)
						{
							auto ItemInstance = ItemInstances->operator[](j);
							auto ItemEntryDefinition = ItemInstance->ItemEntry->
							auto ItemEntryDefinition = reinterpret_cast<UObject**>(__int64(ItemInstance) + __int64(Offsets::ItemEntryOffset) + __int64(Offsets::ItemDefinitionOffset));
							auto ItemEntryGuid = reinterpret_cast<FGuid*>(__int64(ItemInstance) + __int64(Offsets::ItemEntryOffset) + __int64(Offsets::ItemGuidOffset));

							if (IsMatchingGuid(CurrentFocusedGUID, *ItemEntryGuid))
							{
								// spawn the item we are replacing as a pickup
								SpawnPickupAtLocation(*ItemEntryDefinition, 1, AActor::GetLocation(Globals::Pawn));
							}
						}

						// empty current slot
						Player::EmptySlot(Globals::Quickbar, *CurrentFocusedSlot);
					}

					// give player item
					Inventory::AddItemToInventoryWithUpdate(*WorldItemDefinition, EFortQuickBars::Primary, i, 1);

					// destroy pickup in world
					AActor::Destroy(CurrentParams->Pickup);

					break;
				}
			}
		}
	}

		if (FuncName.find("ClientOnPawnDied") != NPOS && Globals::bIsInGame)
		{
			Globals::AthenaPawn->PlayAnimMontage(Globals::DeathMontage, 0.7, "");
			Globals::VictoryDrone = World::SpawnActor(ABP_VictoryDrone_C::StaticClass(), Globals::AthenaPawn->K2_GetActorLocation(), FRotator{ 0,0,0 });
		}

		if (FullFuncName.find("Function BP_VictoryDrone.BP_VictoryDrone_C.OnSpawnOutAnimEnded") != NPOS && Globals::bIsInGame)
		{
			Globals::VictoryDrone->K2_DestroyActor();
			Globals::AthenaPawn->K2_DestroyActor();
		}

		if (FullFuncName.find("Function Engine.CheatManager.Walk") != NPOS && Globals::bIsInGame)
		{
			Globals::AthenaPawn->SetActorEnableCollision(true);
			Globals::AthenaPawn->CharacterMovement->MovementMode = EMovementMode::MOVE_Walking;
		}

		if (FullFuncName.find("Function Engine.CheatManager.Fly") != NPOS && Globals::bIsInGame)
		{
			Globals::AthenaPawn->SetActorEnableCollision(true);
			Globals::AthenaPawn->CharacterMovement->MovementMode = EMovementMode::MOVE_Flying;
		}

		if (FullFuncName.find("Function Engine.CheatManager.Ghost") != NPOS && Globals::bIsInGame)
		{
			Globals::AthenaPawn->SetActorEnableCollision(false);
			Globals::AthenaPawn->CharacterMovement->MovementMode = EMovementMode::MOVE_Flying;
		}

		if (FuncName.find("ServerReturnToMainMenu") != NPOS && Globals::bIsInGame)
		{
			MH_DisableHook(CGHook);
			Globals::bIsInLobby = true;
			Globals::bIsInitialized = false;
			Globals::bIsInGame = false;
			Globals::bInstantReload = false;
			Globals::bHasJumped = false;
		}

		if (FuncName.find("CheatScript") != NPOS)
		{
			if (static_cast<UCheatManager_CheatScript_Params*>(Parameters)->ScriptName.IsValid() && Globals::bIsInGame)
			{
				if (!Cheats::HandleCheats(static_cast<UCheatManager_CheatScript_Params*>(Parameters)->ScriptName.ToString()))
					Globals::AthenaGameMode->Say
					(L"CheatScript not recognized, please use \'cheatscript help\' for a list of available CheatScript commands.");
			}
		}

		return OriginalPE(Object, Function, Parameters);
	}

	void* CGHook(int32_t KeepFlags, bool bPerformFullPurge)
	{
		return NULL;
	}

	inline void CreateHooks()
	{
		if (Globals::bIsInLobby && !Globals::bIsInitialized)
		{
			MH_Initialize();

			uintptr_t* PEAddress = Utils::Offset<uintptr_t>(Offsets::ProcessEventOffset);
			MH_CreateHook(reinterpret_cast<LPVOID>(PEAddress), PEHook, reinterpret_cast<LPVOID*>(&OriginalPE));
			MH_EnableHook(reinterpret_cast<LPVOID>(PEAddress));
		}

		if (Globals::bIsInitialized && !Globals::bIsInLobby)
		{
			uintptr_t* CGAddress = Utils::Offset<uintptr_t>(Offsets::CGInternalOffset);
			MH_CreateHook(reinterpret_cast<LPVOID>(CGAddress), CGHook, reinterpret_cast<LPVOID*>(&OriginalGC));
			MH_EnableHook(reinterpret_cast<LPVOID>(CGAddress));
		}
	}
}
