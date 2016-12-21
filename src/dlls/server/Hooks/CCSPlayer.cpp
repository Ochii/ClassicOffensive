#include "stdafx.h"
#include "CCSPlayer.h"

PLH::Detour* bulletTypeHook = new PLH::Detour;
PLH::Detour* giveAmmoHook = new PLH::Detour;
PLH::Detour* clientCommandHook = new PLH::Detour;
PLH::Detour* setModelClassHook = new PLH::Detour;
PLH::Detour* precacheHook = new PLH::Detour;
PLH::Detour* state_LookupInfoHook = new PLH::Detour;
PLH::Detour* changeTeamHook = new PLH::Detour;
PLH::Detour* handleCommand_JoinClassHook = new PLH::Detour;

void __fastcall hkGetBulletTypeParameters( CCSPlayer* thisptr, void* edx, float &fPenetrationPower, float &flPenetrationDistance )
{
	using fn_t = void( __thiscall* )(CCSPlayer*, float&, float&);

	fPenetrationPower = 50000.f;
	flPenetrationDistance = 50000.f;

	return bulletTypeHook->GetOriginal<fn_t>()(thisptr, fPenetrationPower, flPenetrationDistance);
}

void __fastcall hkGiveAmmo( CWeaponCSBase* thisptr, void* edx, int a1, int iCount, bool bSuppressSound, int a2 )
{
	using fn_t = void( __thiscall* )(CWeaponCSBase*, int, int, bool, int);

	ConsoleDebugW( L"tried to give %i ammo for %S retn address: %p\n", iCount, thisptr->GetNetworkable()->GetClassName(), _ReturnAddress() );

	if ( iCount == 250 )
	{
		CCSWeaponInfo* wepInfo = (CCSWeaponInfo*) thisptr->GetCSWpnData();

		iCount = wepInfo->GetPrimaryClipSize( thisptr->GetAttributeContainer()->GetItem(), false ) * 2;

		ConsoleDebugW( L"Changed iCount to %i\n", iCount );
	}

	return giveAmmoHook->GetOriginal<fn_t>()(thisptr, a1, iCount, bSuppressSound, a2);
}

void BuyWeaponAmmo( CCSPlayer* player, CWeaponCSBase* weapon )
{
	CCSWeaponInfo* wepInfo = (CCSWeaponInfo*) weapon->GetCSWpnData();
	CCSAmmoDef* ammoDef = GetCSAmmoDef();

	ConsoleDebugW( L"buying ammo for %S %p\n", weapon->GetNetworkable()->GetClassName(), weapon );

	ConsoleDebugW( L"primary->m_iPrimaryReserveAmmoCount: %i ammoType: %i reserve ammo: %i maxclip: %i ammoCost: %i buySize: %i\n", weapon->m_iPrimaryReserveAmmoCount.Get(), weapon->GetPrimaryAmmoType(), wepInfo->GetPrimaryReserveAmmo( weapon->GetAttributeContainer()->GetItem(), false ), wepInfo->iMaxClip1, ammoDef->GetCost( weapon->m_iPrimaryAmmoType ), ammoDef->GetBuySize( weapon->m_iPrimaryAmmoType ) );
	ConsoleDebugW( L"%S::m_iAccount: %i\n", player->GetPlayerName(), player->GetAccount() );

	int ammoCost = ammoDef->GetCost( weapon->m_iPrimaryAmmoType );

	if ( !player->CanPlayerBuy( true ) )
		return;

	if ( player->GetAccount() < ammoCost )
	{
		player->HintMessage( "#Cstrike_TitlesTXT_Not_Enough_Money", false, true );
		return;
	}

	if ( weapon->m_iPrimaryReserveAmmoCount < wepInfo->GetPrimaryReserveAmmo( weapon->GetAttributeContainer()->GetItem(), false ) )
	{
		weapon->GiveAmmo( 1, weapon->m_iPrimaryReserveAmmoCount + ammoDef->GetBuySize( weapon->m_iPrimaryAmmoType ), true, 0 );
		player->AddAccount( -ammoCost );
		player->EmitSound( "BuyMenu.BuyAmmo", player );
	}
}

bool __fastcall hkClientCommand( CCSPlayer* thisptr, void* edx, const CCommand &args )
{
	using fn_t = bool( __thiscall* )(CCSPlayer*, const CCommand&);

	const char* pcmd = args[ 0 ];

	ConsoleDebugW( L"ServerCmd: %S called\n", pcmd );

	if ( FStrEq( pcmd, "buyammo1" ) )
	{
		CWeaponCSBase* primary = static_cast<CWeaponCSBase*>(thisptr->Weapon_GetSlot( WEAPON_SLOT_RIFLE ));

		if ( primary )
			BuyWeaponAmmo( thisptr, primary );

		return true;
	}

	else if ( FStrEq( pcmd, "buyammo2" ) )
	{
		CWeaponCSBaseGun* secondary = static_cast<CWeaponCSBaseGun*>(thisptr->Weapon_GetSlot( WEAPON_SLOT_PISTOL ));

		if ( secondary )
			BuyWeaponAmmo( thisptr, secondary );

		return true;
	}

	else if ( FStrEq( pcmd, "joinclass" ) )
	{
		if ( args.ArgC() < 2 )
		{
			Warning( "Player sent bad joinclass syntax. Syntax: joinclass [class number]\n" );
			return true;
		}

		if ( thisptr->ShouldRunRateLimitedCommand( args ) )
		{
			int iClass = atoi( args[ 1 ] );
			thisptr->HandleCommand_JoinClass( iClass );
		}

		return true;
	}

	else if ( FStrEq( pcmd, "menuselect" ) )
	{
		ConsoleDebugW( L"menuselect called" );

		if ( thisptr->State_Get() == STATE_PICKINGCLASS )
		{
			int iClass = atoi( args[ 1 ] );

			if ( thisptr->GetTeamNumber() == TEAM_CT )
				iClass += LAST_T_CLASS;

			ConsoleDebugW( L"ClientCommand: picking class %i\n", iClass );

			thisptr->HandleCommand_JoinClass( iClass );

			return true;
		}
	}

	return clientCommandHook->GetOriginal<fn_t>()(thisptr, args);
}

void __fastcall hkSetModelFromClass( CCSPlayer* thisptr )
{
	using fn_t = void( __thiscall* )(CCSPlayer*);

	ConsoleDebugW( L"Setting Class model...\n" );

	thisptr->SetModelFromClass();
}

void __fastcall hkPrecache( CCSPlayer* thisptr )
{
	using fn_t = void( __thiscall* )(CCSPlayer*);

	Vector mins( -13, -13, -10 );
	Vector maxs( 13, 13, 75 );

	for ( int i = 0; i < CTPlayerModels.Count(); i++ )
	{
		thisptr->PrecacheModel( CTPlayerModels[ i ] );
		g_pEngineServer->ForceModelBounds( CTPlayerModels[ i ], mins, maxs );
	}

	for ( int i = 0; i < TerroristPlayerModels.Count(); i++ )
	{
		thisptr->PrecacheModel( TerroristPlayerModels[ i ] );
		g_pEngineServer->ForceModelBounds( TerroristPlayerModels[ i ], mins, maxs );
	}

	thisptr->PrecacheModel( "models/weapons/t_arms_phoenix.mdl" );
	strcpy_s( thisptr->m_szArmsModel, "models/weapons/t_arms_phoenix.mdl" );

	precacheHook->GetOriginal<fn_t>()(thisptr);
}

CCSPlayerStateInfo* __fastcall hkState_LookupInfo( CSPlayerState state )
{
	using fn_t = CCSPlayerStateInfo*(__fastcall*)(CSPlayerState);

	CCSPlayerStateInfo* res = state_LookupInfoHook->GetOriginal<fn_t>()(state);

	ConsoleDebugW( L"Looking up state %i %S\n", state, res->m_pStateName );

	static bool bSetNewStateEnter = false;

	if ( !bSetNewStateEnter )
	{
		g_pPlayerStateInfos[ 3 ].pfnEnterState = &CCSPlayer::State_Enter_PICKINGCLASS;
		ConsoleDebugW( L"Set new EnterState for %S!\n", g_pPlayerStateInfos[ 3 ].m_pStateName );
		bSetNewStateEnter = true;
	}

	return res;
}

void __fastcall hkChangeTeam( CCSPlayer* thisptr, void*, int iTeamNum )
{
	using fn_t = void( __thiscall* )(CCSPlayer*, int);

	ConsoleDebugW( L"ChangeTeam called!\n" );

	//thisptr->State_Transition( STATE_PICKINGCLASS );

	//DWORD jumpman = Addresses::ChangeTeam + 5; 
	//__asm jmp jumpman;

	changeTeamHook->GetOriginal<fn_t>()(thisptr, iTeamNum);
}

bool __fastcall hkHandleCommand_JoinClass( CCSPlayer* thisptr )
{
	using fn_t = bool( __thiscall* )(CCSPlayer*);

	ConsoleDebugW( L"hkHandleCommand_JoinClass called! %p\n", _ReturnAddress() );

	if ( _ReturnAddress() == (void*) (Addresses::ChangeTeam + 5) )
	{
		ConsoleDebugW( L"Server: calling classmenu...\n" );
		//g_pEngineServer->ClientCommand( thisptr->edict(), "classmenu" );
		//thisptr->State_Enter_PICKINGCLASS();
		thisptr->State_Transition( STATE_PICKINGCLASS );
		return true;
	}

	//return thisptr->HandleCommand_JoinClass( thisptr->m_iClass );
	return handleCommand_JoinClassHook->GetOriginal<fn_t>()(thisptr);
}

void HookCSPlayer()
{
	giveAmmoHook->SetupHook( (BYTE*) Addresses::GiveAmmo, (BYTE*) hkGiveAmmo );
	giveAmmoHook->Hook();

	clientCommandHook->SetupHook( (BYTE*) Addresses::ClientCommand, (BYTE*) hkClientCommand );
	clientCommandHook->Hook();

	setModelClassHook->SetupHook( (BYTE*) Addresses::SetModelFromClass, (BYTE*) &hkSetModelFromClass );
	setModelClassHook->Hook();

	precacheHook->SetupHook( (BYTE*) Addresses::Precache, (BYTE*) &hkPrecache );
	precacheHook->Hook();

	/*DWORD state_LookupInfoAddress = (DWORD) GetModuleHandleW( L"server.dll" ) + 0x416880;
	ConsoleDebugW( L"State_LookupInfo: %X\n", state_LookupInfoAddress );

	state_LookupInfoHook->SetupHook( (BYTE*) state_LookupInfoAddress, (BYTE*) &hkState_LookupInfo );
	state_LookupInfoHook->Hook();*/

	/*changeTeamHook->SetupHook( (BYTE*) Addresses::ChangeTeam, (BYTE*) &hkChangeTeam );
	changeTeamHook->Hook();*/

	handleCommand_JoinClassHook->SetupHook( (BYTE*) Addresses::HandleCommand_JoinClass, (BYTE*) &hkHandleCommand_JoinClass );
	handleCommand_JoinClassHook->Hook(); 	

	/*BYTE* bulletTypeAdd = (BYTE*) (address + 0x4297C0);

	bulletTypeHook->SetupHook( bulletTypeAdd, (BYTE*) hkGetBulletTypeParameters );
	bulletTypeHook->Hook();*/
}