/*******************************************
	AmmoEntity.h

	Ammo entity class
********************************************/

#pragma once

#include <string>
using namespace std;

#include "Defines.h"
#include "CVector3.h"
#include "Entity.h"

namespace gen
{
	/*-----------------------------------------------------------------------------------------
	-------------------------------------------------------------------------------------------
		Ammo Entity Class
	-------------------------------------------------------------------------------------------
	-----------------------------------------------------------------------------------------*/

    // Ammo box Class 
	class CAmmoEntity : public CEntity
	{
		/////////////////////////////////////
		//	Constructors/Destructors
	public:
		// Ammo box constructor 
		CAmmoEntity
		(
			CEntityTemplate* entityTemplate,
			TEntityUID       UID,
			const string&    name     = "",
			const CVector3&  position = CVector3::kOrigin,
			const CVector3&  rotation = CVector3(0.0f, 0.0f, 0.0f),
			const CVector3&  scale    = CVector3(1.0f, 1.0f, 1.0f),
			const TUInt32    amount   = 10
		);

		// No destructor needed


	/////////////////////////////////////
	//	Public interface
	public:

		/////////////////////////////////////
		// Types

		// Ammo pack states
		enum class EAmmoState
		{
			Dropping,
			TimeOut
		};

		/////////////////////////////////////
		// Update

		// Update the Ammo pack - performs simple health behaviour
		// Return false if the entity is to be destroyed
		// Virtual function
		virtual bool Update( TFloat32 updateTime );

		// Returns true if the Ammo pack is not in Dropping state
		const bool OnGround()
		{
			bool isNotDropping = m_State != EAmmoState::Dropping;
			
			return isNotDropping;

		};

		/////////////////////////////////////
		//	Private interface
	private:

		/////////////////////////////////////
		// Data

		// The current state of the Ammo pack
		EAmmoState m_State;

		// Time left before Ammo pack is destroyed
		TFloat32 m_LifeTime;

		// The amount of ammo that this Ammo pack holds
		TUInt32 m_Amount;

	};

} // namespace gen