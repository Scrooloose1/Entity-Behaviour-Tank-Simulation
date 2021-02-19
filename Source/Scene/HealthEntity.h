/*******************************************
	HealthEntity.h

	Health entity class
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
		Health Entity Class
	-----------------------------------------------------------------------------------------*/

    // Health box class
	class CHealthEntity : public CEntity
	{
		/////////////////////////////////////
		//	Constructors/Destructors
	public:
		// Health box constructor intialises shell data and passes its parameters to the base
		// class constructor
		CHealthEntity
		(
			CEntityTemplate* entityTemplate,
			TEntityUID       UID,
			const string&    name     = "",
			const CVector3&  position = CVector3::kOrigin,
			const CVector3&  rotation = CVector3(0.0f, 0.0f, 0.0f),
			const CVector3&  scale    = CVector3(1.0f, 1.0f, 1.0f),
			const TUInt32    amount   = 50
		);

		// No destructor needed


	/////////////////////////////////////
	//	Public interface
	public:

		/////////////////////////////////////
		// Types

		// Create states for a health box
		enum class EHealthState
		{
			Dropping,
			TimeOut
		};

		/////////////////////////////////////
		// Update

		// Update the health box - performs simple health behaviour
		// Return false if the entity is to be destroyed
		// Virtual function
		virtual bool Update( TFloat32 updateTime );

		// Returns true if the health box is not in Dropping state
		const bool OnGround()
		{
			bool isNotDropping = m_State != EHealthState::Dropping;

			return isNotDropping;
		};

		/////////////////////////////////////
		//	Private interface
	private:

		/////////////////////////////////////
		// Data

		// The current state of the health box
		EHealthState m_State;

		// Time left before health box is destroyed
		TFloat32 m_LifeTime;

		// The amount of health this health box contains
		TUInt32 m_Amount;

	};

} // namespace gen