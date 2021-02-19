/*******************************************
	ShellEntity.cpp

	Shell entity class
********************************************/

#include "ShellEntity.h"
#include "TankEntity.h"
#include "EntityManager.h"
#include "Messenger.h"

namespace gen
{

// Reference to entity manager from TankAssignment.cpp, allows look up of entities by name, UID etc.
// Can then access other entity's data. See the CEntityManager.h file for functions. Example:
//    CVector3 targetPos = EntityManager.GetEntity( targetUID )->GetMatrix().Position();
extern CEntityManager EntityManager;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;

// Helper function made available from TankAssignment.cpp - gets UID of tank A (team 0) or B (team 1).
// Will be needed to implement the required shell behaviour in the Update function below
extern TEntityUID GetTankUID( int team );



/*-----------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
	Shell Entity Class
-------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

// Shell constructor intialises shell-specific data and passes its parameters to the base
// class constructor
CShellEntity::CShellEntity
(
	CEntityTemplate* entityTemplate,
	TEntityUID       UID,
	const string&    name /*=""*/,
	TEntityUID       tankUID, // Additional tank UID
	const CVector3&  position /*= CVector3::kOrigin*/, 
	const CVector3&  rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
	const CVector3&  scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
) : CEntity( entityTemplate, UID, name, position, rotation, scale )
{
	// Initialise shell data
	m_ShellLifeTime = 1.5f;
	m_ShellSpeed = 150.0f;
	m_TankUID = tankUID; // Tank that fired the bullet uid
}


// Update the shell - controls its behaviour. The shell code is empty, it needs to be written as
// one of the assignment requirements
// Return false if the entity is to be destroyed
bool CShellEntity::Update( TFloat32 updateTime )
{
	// Check to see if the shell still exists
	if (m_ShellLifeTime > 0.0f)
	{
		// Reduce life time
		m_ShellLifeTime = m_ShellLifeTime - updateTime;
		// Move the shell (fire)
		Matrix().MoveLocalZ(m_ShellSpeed * updateTime);

		// Check to see if it has collided with the other tank

		// Work out the radius of the shell
		const TFloat32 shellRadius = Template()->Mesh()->BoundingRadius();

		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* pTankEntity = EntityManager.EnumEntity();
		while (pTankEntity != 0)
		{
			// Work out the radius of the tank
			const TFloat32 tankRadius = pTankEntity->Template()->Mesh()->BoundingRadius();

			// Work out the distance between the shell and the tank
			const TFloat32 distanceFromTank = Position().DistanceTo(pTankEntity->Position());

			// Check to see if the shell has collided with the tank and if so, send a message 
			if (distanceFromTank < (shellRadius + tankRadius))
			{
				// Hit has occured
				// Send a hit message to the tank
				SMessage msg;
				msg.type = Msg_Hit;
				msg.from = GetUID();

				Messenger.SendMessage(pTankEntity->GetUID(), msg);

				// Remove the shell (destroy it)
				return false;

			} // End of if statment

			// get the next tank info
			pTankEntity = EntityManager.EnumEntity();

		} // End of while loop

		EntityManager.EndEnumEntities();

	}
	else // Destroy shell
	{
		return false;

	} // End of if-else statment


	return true; // Entity is still alive

} // End of update class


} // namespace gen
