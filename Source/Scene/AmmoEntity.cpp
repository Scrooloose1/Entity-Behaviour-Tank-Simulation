/*******************************************
	AmmoEntity.cpp

	Ammo entity class
********************************************/

#include "AmmoEntity.h"
#include "TankEntity.h"
#include "EntityManager.h"
#include "Messenger.h"

namespace gen
{
	extern CEntityManager EntityManager;
	extern CMessenger Messenger;

	/*-----------------------------------------------------------------------------------------
		Ammo Entity Class
	-----------------------------------------------------------------------------------------*/

	// Ammo constructor
	CAmmoEntity::CAmmoEntity
	(
		CEntityTemplate* entityTemplate,
		TEntityUID       UID,
		const string&    name,
		const CVector3&  position,
		const CVector3&  rotation,
		const CVector3&  scale,
		const TUInt32    amount
	) : CEntity( entityTemplate, UID, name, position, rotation, scale )
	{
		m_State = EAmmoState::Dropping;
		m_LifeTime = 10.0f;  // 10 seconds
		m_Amount = amount;
	}

	// Return false if the entity is to be destroyed
	bool CAmmoEntity::Update(TFloat32 updateTime)
	{
		if (m_State == EAmmoState::Dropping)
		{
			if (Position().y > 0.5f)
			{
				Matrix().MoveLocalY(-14.0f * updateTime);
			}
			else  // Change to expire state
			{
				Position().y = 0.5f;
				m_State = EAmmoState::TimeOut;

			} // End of if-else statment
		}
		else if (m_State == EAmmoState::TimeOut)
		{
			if (m_LifeTime > 0.0f)
			{
				m_LifeTime -= updateTime;

				EntityManager.BeginEnumEntities("", "", "Tank");
				CEntity* pTank = EntityManager.EnumEntity();
				while (pTank != 0)
				{
					// Calculate the distance between the ammo pack and tank
					const TFloat32 distanceToTank = Position().DistanceTo(pTank->Position());

					// Check if there has been a collision
					if (distanceToTank < 5.0f)
					{
						// Send a ammo pickup messge to the tank
						SMessage m;
						m.type = Msg_AmmoCollected;
						m.from = GetUID();
						m.data = m_Amount;
						Messenger.SendMessage(pTank->GetUID(), m);

						// Send a new ammo pack messge to the system
						m = {};
						m.type = Msg_NewAmmoPack;
						m.from = GetUID();
						Messenger.SendMessage(SystemUID, m);

						return false;

					} // End of if statment

					pTank = EntityManager.EnumEntity();

				} // End of while loop

				EntityManager.EndEnumEntities();
			}
			else  // Destroy this ammo pack
			{
				// Send a new ammo pack messge to the system
				SMessage m;
				m.type = Msg_NewAmmoPack;
				m.from = GetUID();
				Messenger.SendMessage(SystemUID, m);

				return false;

			} // End of if-else statment

		} // End of if-else statment

		return true;  // Keep this entity alive

	} // End of Update function

} // namespace gen