/*******************************************
	TankEntity.cpp

	Tank entity template and entity classes
********************************************/

// Additional technical notes for the assignment:
// - Each tank has a team number (0 or 1), HP and other instance data - see the end of TankEntity.h
//   You will need to add other instance data suitable for the assignment requirements
// - A function GetTankUID is defined in TankAssignment.cpp and made available here, which returns
//   the UID of the tank on a given team. This can be used to get the enemy tank UID
// - Tanks have three parts: the root, the body and the turret. Each part has its own matrix, which
//   can be accessed with the Matrix function - root: Matrix(), body: Matrix(1), turret: Matrix(2)
//   However, the body and turret matrix are relative to the root's matrix - so to get the actual 
//   world matrix of the body, for example, we must multiply: Matrix(1) * Matrix()
// - Vector facing work similar to the car tag lab will be needed for the turret->enemy facing 
//   requirements for the Patrol and Aim states
// - The CMatrix4x4 function DecomposeAffineEuler allows you to extract the x,y & z rotations
//   of a matrix. This can be used on the *relative* turret matrix to help in rotating it to face
//   forwards in Evade state
// - The CShellEntity class is simply an outline. To support shell firing, you will need to add
//   member data to it and rewrite its constructor & update function. You will also need to update 
//   the CreateShell function in EntityManager.cpp to pass any additional constructor data required
// - Destroy an entity by returning false from its Update function - the entity manager wil perform
//   the destruction. Don't try to call DestroyEntity from within the Update function.
// - As entities can be destroyed, you must check that entity UIDs refer to existant entities, before
//   using their entity pointers. The return value from EntityManager.GetEntity will be NULL if the
//   entity no longer exists. Use this to avoid trying to target a tank that no longer exists etc.

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
// Will be needed to implement the required tank behaviour in the Update function below
extern const vector<TEntityUID>& GetEnemyTankUID( int team );
extern const vector<TEntityUID>& GetTeamTankUID( int team );

/*-----------------------------------------------------------------------------------------
-------------------------------------------------------------------------------------------
	Tank Entity Class
-------------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

// Tank constructor intialises tank-specific data and passes its parameters to the base
// class constructor
CTankEntity::CTankEntity
(
	CTankTemplate*  tankTemplate,
	TEntityUID      UID,
	TUInt32         team,
	const vector<CVector3>& patrolList,
	const string&   name /*=""*/,
	const CVector3& position /*= CVector3::kOrigin*/, 
	const CVector3& rotation /*= CVector3( 0.0f, 0.0f, 0.0f )*/,
	const CVector3& scale /*= CVector3( 1.0f, 1.0f, 1.0f )*/
) : CEntity( tankTemplate, UID, name, position, rotation, scale )
{
	m_TankTemplate = tankTemplate;

	// Tanks are on teams so they know who the enemy is
	m_Team = team;

	// Initialise other tank data and state
	m_Speed = 0.0f;
	m_HP = m_TankTemplate->GetMaxHP();
	m_State = Inactive;
	m_Timer = 0.0f;
	m_numShellsFired = 0;

	m_patrolList = patrolList;

	// Set starting waypoint
	m_CurrentPatrolWP = 0;

	// Reset starting evade points
	m_EvadePoint = { 0.0f, 0.0f, 0.0f };

	// Set the fire time of the bullet
	m_BulletLifeTime = 2.0f;

	// Set tank state starter text
	m_TankStateText = "Inactive";

	// Set the target enemy UID
	m_TargetEnemyUID = 0;
}


// Update the tank - controls its behaviour. The shell code just performs some test behaviour, it
// is to be rewritten as one of the assignment requirements
// Return false if the entity is to be destroyed
bool CTankEntity::Update(TFloat32 updateTime)
{
	if (IsAlive() == false)
	{
		if (m_animationTime > 0.0f)
		{
			m_animationTime -= updateTime;

			// Slowly sink into the floor
			Matrix().Position().y -= m_kSinkingSpeed * updateTime;
		}
		else  // Destroy this tank
		{
			return false;

		} // End of if statment

		return true;

	} // End of if statment


	// Fetch any messages sent to the tanks
	SMessage msg;
	while (Messenger.FetchMessage(GetUID(), &msg))
	{
		// Set state variables based on received messages
		switch (msg.type)
		{
			case Msg_Start:
			{
				// Switch to patrol state
				m_State = Patrol;
				m_TankStateText = "Patrol";
				break;
			}
			case Msg_Stop:
			{
				m_State = Inactive;
				m_TankStateText = "Inactive";
				break;
			}
			case Msg_Hit:
			{
				// Only reduce hitpoints if the tank is not in the evade state
				if (m_State != Evade)
				{
					// Reduce hit points
					m_HP = m_HP - 20;

				} // End of if statment

				if (m_HP <= 0) // Has the tank no health left?
				{
					m_State = Dead; // Tank is dead
					m_TankStateText = "Dead";
				}
				else // Call for help!
				{
					// Get enemy tank uid
					TEntityUID enemyUID = msg.data;
					// Get a list of UIDs for this team.
					const vector<TEntityUID> teamUIDs = GetTeamTankUID(m_Team);

					// Ranged based for loop (had to look this up)
					for (TEntityUID teamUID : teamUIDs)
					{
						// Send a help messge to the team tank
						SMessage msg;
						msg.type = Msg_HelpMe;
						msg.from = GetUID();
						msg.data = enemyUID;
						Messenger.SendMessage(teamUID, msg);
					}

				}  // End of if statment

				break;
			}
			case Msg_HelpMe:
			{
				// Action a call for help
				if (m_State != Evade && m_Ammo > 0) // Only if not in evade state and has ammo
				{
					m_TargetEnemyUID = msg.data;
					m_State = Aim; // Enter Aim state
					m_TankStateText = "Aim";
				}
				break;
			}
			case Msg_Evade:
			{
				CVector3 tankCurrentPosition = Position();

				// Work out a random point in the world for the tank to evade too
				m_EvadePoint.x = Random(tankCurrentPosition.x - 40.0f, tankCurrentPosition.x + 40.0f);
				m_EvadePoint.z = Random(tankCurrentPosition.z - 40.0f, tankCurrentPosition.z + 40.0f);

				m_State = Evade;
				m_TankStateText = "Evade";

				break;
			}
			case Msg_HealthCollected:
			{
				// Add extra health to the tanks total health when picked up
				m_HP += msg.data;
				break;
			}
			case Msg_AmmoCollected:
			{
				// Add extra ammo to the tanks total ammo when picked up
				m_Ammo += msg.data;
				break;
			}

		} // End of switch statment

	} // End of while loop


	// Tank behaviour
	if (m_State == Inactive)
	{
		// Inactive state - The tank does not move
		// Waits for a start message, then enters the patrol state
	}
	else if (m_State == Patrol)
	{
		// Patrol state. Actions performed:
		// The tank patrols back and forward between two points.
		// The tank turns and faces the direction it is moving.
		// The turrent rotates as the tank moves.
		// If the turret is pointing at the enemy tank, it enters the "Aim" state

		// Is current tanks health low?
		if (m_HP <= 50.0f)
		{
			if (LookForHealth(updateTime)) return true;

		} // End of if statment

		// Is current ammo low?
		if (m_Ammo <= 3)
		{
			if (LookForAmmo(updateTime)) return true;

		} // End of if statment

		// Get data for the tank
		TFloat32 tankAcceleration   = m_TankTemplate->GetAcceleration();
		TFloat32 tankMaxSpeed       = m_TankTemplate->GetMaxSpeed();
		TFloat32 tankTurnSpeed      = m_TankTemplate->GetTurnSpeed();
		TFloat32 turrentRotateSpeed = m_TankTemplate->GetTurretTurnSpeed();

		// Work out the distance between the tank and the first waypoint
		TFloat32 m_DistanceToWP = Position().DistanceTo(m_patrolList[m_CurrentPatrolWP]);

		// Is the tank close to the next waypoint?
		if (m_DistanceToWP < 10.0f)
		{
			m_CurrentPatrolWP++;

			if (m_CurrentPatrolWP == m_patrolList.size())
			{
				m_CurrentPatrolWP = 0;

			} // End of if statment

			// Stop the tank
			m_Speed = 0.0f;
		}
		else // Carry on moving
		{
			// Get the facing Vector of the tank
			CVector3 tankFacingVector = Matrix().ZAxis();
			// Normalise this vector
			tankFacingVector.Normalise();
			// Get the right Vector of the tank
			CVector3 tankRightVector = Matrix().XAxis();
			// Normalise this vector
			tankRightVector.Normalise();
			// Get the distance Vector to the next patrol point
			CVector3 distanceVector = m_patrolList[m_CurrentPatrolWP] - Position();
			// Normalise this vector
			distanceVector.Normalise();
			// Work out the forward dot product 
			TFloat32 forwardDotProduct = Dot(distanceVector, tankFacingVector);
			// Work out the Right dot product 
			TFloat32 rightDotProduct = Dot(distanceVector, tankRightVector);

			// Check to see if not facing correct direction and if not, turn tank
			if (forwardDotProduct < Cos(tankTurnSpeed * updateTime))
			{
				// Work out which way to turn
				if (rightDotProduct > 0.0f)
				{
					Matrix().RotateLocalY(tankTurnSpeed * updateTime);
				}
				else
				{
					Matrix().RotateLocalY(-tankTurnSpeed * updateTime);

				} // End of if statment

				// Move the tank forward (along local Z axis)
				Matrix().MoveLocalZ(m_Speed * updateTime);
			}
			else
			{
				// Correct the facing of the tank to make it look directly at the waypoint
				Matrix().FaceTarget(m_patrolList[m_CurrentPatrolWP]);

				(m_Speed < tankMaxSpeed) ? m_Speed += tankAcceleration : m_Speed = tankMaxSpeed;

				// Move tank along Z axis
				Matrix().MoveLocalZ(m_Speed * updateTime);

			} // End of if statment

			// Rotate the turret
			Matrix(2).RotateLocalY(turrentRotateSpeed * updateTime);

			// Is there enough ammo to shoot at the enemy?
			if (m_Ammo > 0)
			{
				// Get list of enemy tank ids
				const vector<TEntityUID> enemyUIDs = GetEnemyTankUID(m_Team);

				for (auto enemyUID : enemyUIDs)
				{
					CEntity* pEnemyTank = EntityManager.GetEntity(enemyUID);

					// Check if the enemy tank exists (is not dead)
					if (pEnemyTank != 0)
					{
						// Get the position of the enemy tank
						CVector3 enemyPosition = pEnemyTank->Position();

						// Calculate distance to the enemyTank
						distanceVector = enemyPosition - Position();
						// Normalise this vector
						distanceVector.Normalise();

						// Get the World matrix of the turrent:
						CMatrix4x4 turretWorldMatrix = Matrix(2) * Matrix();
						// Calculate dot product using the distance vector and the Z Axis 
						// of the turrets world Matrix (normalised)
						TFloat32 forwardDP = Dot((distanceVector), Normalise(turretWorldMatrix.ZAxis()));

						// Check to see if the angle of the turrent against the enemy tank is within range
						if (forwardDP > Cos(ToRadians(15.0f)))
						{
							// Change to Aim State
							m_State = Aim;
							// Stop the tank from moving
							m_Speed = 0.0f;
							m_TargetEnemyUID = enemyUID;
						
						} // End of if statment

					} // End of if statment

				} // End of for loop

			} // End of if statment

		} // End of if statment
	}
	else if (m_State == Aim)
	{
		// Aim state. Actions performed:
		// The Tank stops moving.
		// A timer starts counting down (initialised at one second)
		// During this time, the turren rotates more quickly to try and point exactly at the enemy tank
		// When the counter reaches zero, the tank fires (it creates a shell)
		// Tank then enters the evade state.

		// Check countdown time for the bullet
		if (m_BulletLifeTime > 0.0f)
		{
			// Start the timer
			m_BulletLifeTime = m_BulletLifeTime - updateTime;

			// Get data of enemy tank
			CEntity* pEnemyTankEntity = EntityManager.GetEntity(m_TargetEnemyUID);

			// Check if the enemy tank exists (is not dead)
			if (pEnemyTankEntity != 0)
			{
				// Work out faster turn speed (twice as fast)
				TFloat32 turrentFastTurnSpeed = m_TankTemplate->GetTurretTurnSpeed() * 2.0f;
				// Get position of enemy tank
				CVector3 targetPosition = pEnemyTankEntity->Position();
				// Get the distance to the target point
				CVector3 distanceVector = targetPosition - Position();
				// Normalise this vector
				distanceVector.Normalise();

				// Get the turrent World matrix
				CMatrix4x4 turretWorldMatrix = Matrix(2) * Matrix();

				// Calculate forward dot product using the distance vector and the Z Axis 
				// of the turrets world Matrix (normalised)
				TFloat32 forwardDP = Dot((distanceVector), Normalise(turretWorldMatrix.ZAxis()));
				// Do similar calculation to calculate the right dot product (except using the X Axis
				// of the turrets world matrix)
				TFloat32 rightDP = Dot((distanceVector), Normalise(turretWorldMatrix.XAxis()));

				// Next we check if the angle of the turret to the enemy tank is small enough
				// and rotate it in the correct direction
				if (forwardDP < Cos(ToRadians(1.0f)))
				{
					if (rightDP > 0.0f) // Rotate right
					{
						Matrix(2).RotateLocalY(turrentFastTurnSpeed * updateTime);
					}
					else // Rotate it left
					{
						Matrix(2).RotateLocalY(-turrentFastTurnSpeed * updateTime);

					} // End of if statment

				} // End of if statment

			}
			else // Is the enemy dead?
			{
				m_State = Patrol;
				return true;
			}
		}
		else // Fire the bullet
		{
			// Reduce the amount of ammout the tank has
			m_Ammo -= 1;
			m_numShellsFired++;

			// First we need to get the current Y rotation from the turret matrix
			CMatrix4x4 turretWorldMatrix = Matrix(2) * Matrix();
			// Next we get the rotation angle of the turret as a vector
			// Pass in turret World matrix to the DecompaseAffineFunction
			// We can pass in NULL for the parameters that are not needed
			CVector3 turretRotationAngles = {};
			turretWorldMatrix.DecomposeAffineEuler(NULL, &turretRotationAngles, NULL);

			// Next we need to adjust the position of the bullet so it is just infront of the tank
			// so it does not collide with the tank it is currently firing from
			turretWorldMatrix.MoveLocalZ(Template()->Mesh()->BoundingRadius() + 1.0f);
			CVector3 bulletPosition = turretWorldMatrix.Position();

			// Next we need to create the actual bullet and place it in the world
			EntityManager.CreateShell("Shell Type 1", "Bullet", GetUID(), bulletPosition, turretRotationAngles);

			// Set the fire time of the next bullet
			m_BulletLifeTime = 2.0f;

			CVector3 tankCurrentPosition = Position();

			// Work out a new point in the world for the tank to evade too
			m_EvadePoint.x = Random(tankCurrentPosition.x - 40.0f, tankCurrentPosition.x + 40.0f);
			m_EvadePoint.z = Random(tankCurrentPosition.z - 40.0f, tankCurrentPosition.z + 40.0f);

			// Change to Evade State
			m_State = Evade;

		} // End of if statment

	}
	else if (m_State == Evade)
	{
		// Evade state. Actions performed:
		// On entering this state, tank selects a random position within 40 units of its current location
		// Tank moves towards this position.
		// Turret also rotates to face this direction
		// When it reaches this point, it enters patrol state

		// Get data for the tank
		TFloat32 turrentRotateSpeed = m_TankTemplate->GetTurretTurnSpeed();
		TFloat32 tankAcceleration   = m_TankTemplate->GetAcceleration();
		TFloat32 tankMaxSpeed       = m_TankTemplate->GetMaxSpeed();
		TFloat32 tankTurnSpeed      = m_TankTemplate->GetTurnSpeed();

		// Is the tank close to the evade point?
		// If not, go back into the patrol state
		if (Position().DistanceTo(m_EvadePoint) < 20.0f)
		{
			m_State = Patrol;
		}
		else // otherwise move towards the evade point
		{
			// Calculate distance vector to the evade point
			CVector3 distanceVector = m_EvadePoint - Position();

			// Normalise
			distanceVector.Normalise();

			// Calculate forward dot product
			TFloat32 forwardDotProduct = Dot(distanceVector, Normalise(Matrix().ZAxis()));
			// Calculate right dot product
			TFloat32 rightDotProduct = Dot(distanceVector, Normalise(Matrix().XAxis()));

			// Check which direction is needed to turn
			if (forwardDotProduct < Cos(tankTurnSpeed * updateTime))
			{
				// Check for turning
				if (rightDotProduct > 0.0f)
				{
					Matrix().RotateLocalY(tankTurnSpeed * updateTime);
				}
				else
				{
					Matrix().RotateLocalY(-tankTurnSpeed * updateTime);

				}  // End of if statment

				// Recalcualte speed
				(m_Speed < tankMaxSpeed) ? m_Speed += tankAcceleration : m_Speed = tankMaxSpeed;

				// Move forward
				Matrix().MoveLocalZ(m_Speed * updateTime);
			}
			else
			{
				// Set tank to face exact direction
				Matrix().FaceTarget(m_EvadePoint);

				// Calculate the Acceleration
				(m_Speed < tankMaxSpeed) ? m_Speed += tankAcceleration : m_Speed = tankMaxSpeed;

				// Move forward 
				Matrix().MoveLocalZ(m_Speed * updateTime);

				// Double the turn speed for the turret
				TFloat32 turretFastTurn = tankTurnSpeed * 2.0f;

				// Calculate distance vector to target
				distanceVector = m_EvadePoint = Position();
				// Normalise
				distanceVector.Normalise();

				// get turret world matrix
				CMatrix4x4 turretWorldMatrix = Matrix(2) * Matrix();
				// Calculate forward dot product
				TFloat32 forwardDotProduct = Dot(distanceVector, Normalise(turretWorldMatrix.ZAxis()));
				// Calculate right dot product
				TFloat32 rightDotProduct = Dot(distanceVector, Normalise(turretWorldMatrix.XAxis()));

				// Is the angle of the turret small enough?
				if (forwardDotProduct < Cos(ToRadians(1.0f)))
				{
					if (rightDotProduct > 0.0f)
					{
						Matrix(2).RotateLocalY(turretFastTurn * updateTime);
					}
					else
					{
						Matrix(2).RotateLocalY(-turretFastTurn * updateTime);

					} // End of if statment

				} // End of if statment

			} // End of if statment

		} // End of if statment

	} // End of if-else statment

	return true; // Don't destroy the entity
}

	// Function to search for any health packs
	bool CTankEntity::LookForHealth(float updateTime)
	{
		// Is there a health pack?
		CEntity* pEntity = EntityManager.GetEntity("Health Pack 1", "", "");

		if (pEntity != 0)
		{
			CHealthEntity* pHealthEntity = dynamic_cast<CHealthEntity*>(pEntity);
			if (pHealthEntity != nullptr && pHealthEntity->OnGround())
			{
				// Get data for the tank
				TFloat32 tankAcceleration = m_TankTemplate->GetAcceleration();
				TFloat32 tankMaxSpeed = m_TankTemplate->GetMaxSpeed();
				TFloat32 tankTurnSpeed = m_TankTemplate->GetTurnSpeed();
				TFloat32 turrentRotateSpeed = m_TankTemplate->GetTurretTurnSpeed();

				CVector3 distanceToHealthPack = pEntity->Position() - Position();
				TFloat32 forwardDotProduct = Dot(Normalise(distanceToHealthPack), Normalise(Matrix().ZAxis()));
				TFloat32 rightDotProduct = Dot(Normalise(distanceToHealthPack), Normalise(Matrix().XAxis()));

				// Turn if not facing right direction
				if (forwardDotProduct < Cos(tankTurnSpeed * updateTime))
				{
					// Check which way to rotate
					if (rightDotProduct > 0.0f)
					{
						Matrix().RotateLocalY(tankTurnSpeed * updateTime);
					}
					else
					{
						Matrix().RotateLocalY(-tankTurnSpeed * updateTime);

					}  // End of if statment

				}
				else
				{
					Matrix().FaceTarget(pEntity->Position());
					(m_Speed < tankMaxSpeed * 1.5f) ? m_Speed += tankAcceleration : m_Speed = tankMaxSpeed * 1.5f;

				} // End of if-else statment

				Matrix().MoveLocalZ(m_Speed * updateTime);
				Matrix(2).RotateLocalY(turrentRotateSpeed * updateTime);

				return true;

			} // End of if statment

		} // End of if statment

		return false;

	} // End of LookForHealth function

	// function to search for any ammo packs
	bool CTankEntity::LookForAmmo(float updateTime)
	{
		// Is there an ammo pack?
		CEntity* pEntity = EntityManager.GetEntity("Ammo Pack 1", "", "");
		if (pEntity != 0)
		{
			CAmmoEntity* pAmmoEntity = dynamic_cast<CAmmoEntity*>(pEntity);
			if (pAmmoEntity != nullptr && pAmmoEntity->OnGround())
			{
				// Get data for the tank
				TFloat32 tankAcceleration   = m_TankTemplate->GetAcceleration();
				TFloat32 tankMaxSpeed       = m_TankTemplate->GetMaxSpeed();
				TFloat32 tankTurnSpeed      = m_TankTemplate->GetTurnSpeed();
				TFloat32 turrentRotateSpeed = m_TankTemplate->GetTurretTurnSpeed();

				CVector3 distanceToAmmoPack = pEntity->Position() - Position();
				TFloat32 forwardDotProduct = Dot(Normalise(distanceToAmmoPack), Normalise(Matrix().ZAxis()));
				TFloat32 rightDotProduct = Dot(Normalise(distanceToAmmoPack), Normalise(Matrix().XAxis()));

				// Turn the tank if not facing the right direction
				if (forwardDotProduct < Cos(tankTurnSpeed * updateTime))
				{
					// Check which way to rotate
					if (rightDotProduct > 0.0f)
					{
						Matrix().RotateLocalY(tankTurnSpeed * updateTime);
					}
					else
					{
						Matrix().RotateLocalY(-tankTurnSpeed * updateTime);

					} // End of if statment

				}
				else
				{
					// Face the pack
					Matrix().FaceTarget(pEntity->Position());

					// Check speed
					if (m_Speed < tankMaxSpeed * 1.5f)
					{
						m_Speed += tankAcceleration;
					}
					else
					{
						m_Speed = tankMaxSpeed * 1.5f;
					}

				} // End of if statment

				// move tank towards the pack
				Matrix().MoveLocalZ(m_Speed * updateTime);
				Matrix(2).RotateLocalY(turrentRotateSpeed * updateTime);

				return true;

			} // End of if statment

		} // End of if statment

		return false;

	} // End of LookForAmmo function

} // namespace gen
