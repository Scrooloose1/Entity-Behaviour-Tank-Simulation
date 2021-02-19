/*******************************************
	TankAssignment.cpp

	Shell scene and game functions
********************************************/

#include <sstream>
#include <string>
using namespace std;

#include <d3d10.h>
#include <d3dx10.h>

#include "Defines.h"
#include "CVector3.h"
#include "Camera.h"
#include "Light.h"
#include "EntityManager.h"
#include "Messenger.h"
#include "TankAssignment.h"

// 3rd party libary for parsing the XML files
#include "tinyxml2.h"


namespace gen
{
	
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Camera control speed
const float CameraRotSpeed  = 2.0f;
float       CameraMoveSpeed = 80.0f;

// Amount of time to pass before calculating new average update time
const float UpdateTimePeriod = 1.0f;

// Total number of tanks in the game
const TUInt32 TotalNumOfTanks = 6;

//-----------------------------------------------------------------------------
// Global system variables
//-----------------------------------------------------------------------------

// Get reference to global DirectX variables from another source file
extern ID3D10Device*           g_pd3dDevice;
extern IDXGISwapChain*         SwapChain;
extern ID3D10DepthStencilView* DepthStencilView;
extern ID3D10RenderTargetView* BackBufferRenderTarget;
extern ID3DX10Font*            OSDFont;

// Actual viewport dimensions (fullscreen or windowed)
extern TUInt32 ViewportWidth;
extern TUInt32 ViewportHeight;

// Current mouse position
extern TUInt32 MouseX;
extern TUInt32 MouseY;

// Messenger class for sending messages to and between entities
extern CMessenger Messenger;

// Location of XML file in the project media folder
const string SCENE_XML_FILE_PATH = "Media\\Scene.xml";


//-----------------------------------------------------------------------------
// Global game/scene variables
//-----------------------------------------------------------------------------

// Entity manager
CEntityManager EntityManager;

// Tank UIDs
TEntityUID TankA;
TEntityUID TankB;
TEntityUID TankC;
TEntityUID TankD;
TEntityUID TankE;
TEntityUID TankF;

// Team UIDs
vector<TEntityUID> TeamOne;
vector<TEntityUID> TeamTwo;

// Other scene elements
const int   NumLights = 2;
CLight*     Lights[NumLights];
SColourRGBA AmbientLight;
CCamera*    MainCamera;

// Sum of recent update times and number of times in the sum - used to calculate
// average over a given time period
float SumUpdateTimes = 0.0f;
int NumUpdateTimes = 0;
float AverageUpdateTime = -1.0f; // Invalid value at first

// Extra information displayed under tank
bool mExtraInfoActive = false;

// Current nearest entity to the mouse cursor
CEntity* NearestEntity = 0;

// Camera track tank flag.
bool chaseCamOn[TotalNumOfTanks] = { false, false, false, false, false, false };

// Matrix of camera used to rotate it in chase cam mode
CMatrix4x4 chaseCamTankMatrix;

// Health pack variables
TFloat32 gHealthPackTimer = 10.0f; // 10 seconds
bool     gHealthPackDeployed = false;

// NEW: Ammo pack vars
TFloat32 gAmmoPackTimer = 15.0f; // 15 seconds
bool     gAmmoPackDeployed = false;

//-----------------------------------------------------------------------------
// Scene management
//-----------------------------------------------------------------------------

// Method to read from the XML file and create the templates
bool LoadSceneTemplates( string filePath )
{
	tinyxml2::XMLDocument xmlFile;

	// Can the file be opened?
	if (xmlFile.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
	{
		return false;  // Return false if failed
	}
	else  // Read in data from the file
	{
		// Find the scene tag
		tinyxml2::XMLElement* pScene = xmlFile.FirstChildElement("Scene");

		// Find the first template tag
		tinyxml2::XMLElement* pTemplate = pScene->FirstChildElement("Template");

		// Loop through the template tags
		while (pTemplate != nullptr)
		{
			// Build the template with the tag attribute values
			EntityManager.CreateTemplate
			(
				pTemplate->Attribute("Type"),
				pTemplate->Attribute("Name"),
				pTemplate->Attribute("Mesh")
			);

			// Next template tag
			pTemplate = pTemplate->NextSiblingElement("Template");

		} // End of while loop

		// Find the first tank template tag
		tinyxml2::XMLElement* pTankTemplate = pScene->FirstChildElement("TankTemplate");

		// Loop through the tank template tags
		while ( pTankTemplate != nullptr )
		{
			// Build the tank template with tag attribute values
			EntityManager.CreateTankTemplate
			(
				pTankTemplate->Attribute("Type"),
				pTankTemplate->Attribute("Name"),
				pTankTemplate->Attribute("Mesh"),
				pTankTemplate->FloatAttribute("TopSpeed"),
				pTankTemplate->FloatAttribute("Acceleration"),
				pTankTemplate->FloatAttribute("TankTurnSpeed"),
				pTankTemplate->FloatAttribute("TurretTurnSpeed"),
				pTankTemplate->IntAttribute("MaxHP"),
				pTankTemplate->IntAttribute("ShellDamage")
			);

			// Next tank template tag
			pTankTemplate = pTankTemplate->NextSiblingElement("TankTemplate");

		} // End of while loop
	}

	return true; // Return success

} // End of LoadSceneTemplates function


// Creates the scene geometry
bool SceneSetup()
{
	//////////////////////////////////////////////
	// Prepare render methods

	InitialiseMethods();

	//////////////////////////////////////////
	// Create scenery templates and entities

	// Load all of the templates for this scene
	if ( LoadSceneTemplates(SCENE_XML_FILE_PATH) == false )
	{
		SystemMessageBox("Failed to load XML file", "XML Error");
		return false;  // Return error

	} // End of if statment

	// Creates the scenery entities
	// Type (template name), entity name, position, rotation, scale
	EntityManager.CreateEntity("Skybox", "Skybox", CVector3(0.0f, -10000.0f, 0.0f), CVector3::kZero, CVector3(10, 10, 10));
	EntityManager.CreateEntity("Floor", "Floor");
	EntityManager.CreateEntity("Building", "Building", CVector3(0.0f, 0.0f, 40.0f));
	for (int tree = 0; tree < 100; ++tree)
	{
		// Load in random trees
		EntityManager.CreateEntity("Tree", "Tree",
			CVector3(Random(-200.0f, 30.0f), 0.0f, Random(40.0f, 150.0f)),
			CVector3(0.0f, Random(0.0f, 2.0f * kfPi), 0.0f));

	} // End of for loop


	/////////////////////////////////
	// Create tank patrol points

	vector<CVector3> tankAPatrolList = // Tank A
	{
		{   0.0f, 0.5f, 0.0f },
		{ -50.0f, 0.5f, 0.0f },
		{ -50.0f, 0.5f, 40.0f },
		{   0.0f, 0.5f, 40.0f },
	};
	vector<CVector3> tankBPatrolList = // Tank B
	{
		{ 10.0f, 0.5f, -50.0f },
		{ 10.0f, 0.5f, -10.0f },
		{ 50.0f, 0.5f, -30.0f },
		{ 50.0f, 0.5f, -50.0f },
	};
	vector<CVector3> tankCPatrolList = // Tank C
	{
		{   0.0f, 0.5f, -50.0f },
		{   0.0f, 0.5f, -10.0f },
		{ -40.0f, 0.5f, -10.0f },
		{ -50.0f, 0.5f, -50.0f },
		
	};
	vector<CVector3> tankDPatrolList = // Tank D
	{
		{  50.0f, 0.5f, 0.0f },
		{  10.0f, 0.5f, 0.0f },
		{  10.0f, 0.5f, 40.0f },
		{  50.0f, 0.5f, 40.0f },

	};
	vector<CVector3> tankEPatrolList = // Tank E
	{
		{ -30.0f, 0.5f, 30.0f },
		{  30.0f, 0.5f, 30.0f },
		{  30.0f, 0.5f, 10.0f },
		{ -30.0f, 0.5f, 10.0f },

	};
	vector<CVector3> tankFPatrolList = // Tank F
	{
		{ -40.0f, 0.5f, -20.0f },
		{  20.0f, 0.5f, -20.0f },
		{  20.0f, 0.5f, -40.0f },
		{ -40.0f, 0.5f, -40.0f },

	};

	////////////////////////////////
	// Create tank entities

	// Type (template name), team number, tank name, position, rotation

	// Team 1
	TankA = EntityManager.CreateTank("Rogue Scout", 0, tankAPatrolList, "Red-1", CVector3(-20.0f, 0.5f, 40.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));
	TankC = EntityManager.CreateTank("Rogue Scout", 0, tankCPatrolList, "Red-2", CVector3(-50.0f, 0.5f, -50.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));
	TankE = EntityManager.CreateTank("Rogue Scout", 0, tankEPatrolList, "Red-3", CVector3(-30.0f, 0.5f,  10.0f),
		CVector3(0.0f, ToRadians(0.0f), 0.0f));

	// Team 2
	TankB = EntityManager.CreateTank("Oberon MkII", 1, tankBPatrolList, "Blue-1", CVector3(20.0f, 0.5f, -40.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));
	TankD = EntityManager.CreateTank("Oberon MkII", 1, tankDPatrolList, "Blue-2", CVector3(50.0f, 0.5f, 10.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));
	TankF = EntityManager.CreateTank("Oberon MkII", 1, tankFPatrolList, "Blue-3", CVector3(0.0f, 0.5f, 0.0f),
		CVector3(0.0f, ToRadians(180.0f), 0.0f));


	////////////////////////////////
	// Populate Team Lists

	TeamOne.push_back(TankA);
	TeamTwo.push_back(TankB);
	TeamOne.push_back(TankC);
	TeamTwo.push_back(TankD);
	TeamOne.push_back(TankE);
	TeamTwo.push_back(TankF);

	/////////////////////////////
	// Camera / light setup

	// Set camera position and clip planes
	MainCamera = new CCamera(CVector3(0.0f, 30.0f, -100.0f), CVector3(ToRadians(15.0f), 0, 0));
	MainCamera->SetNearFarClip(1.0f, 20000.0f);

	// Sunlight and light in building
	Lights[0] = new CLight(CVector3(-5000.0f, 4000.0f, -10000.0f), SColourRGBA(1.0f, 0.9f, 0.6f), 15000.0f);
	Lights[1] = new CLight(CVector3(6.0f, 7.5f, 40.0f), SColourRGBA(1.0f, 0.0f, 0.0f), 1.0f);

	// Ambient light level
	AmbientLight = SColourRGBA(0.6f, 0.6f, 0.6f, 1.0f);

	return true;

} // End of SceneSetup function


// Release everything in the scene
void SceneShutdown()
{
	// Release render methods
	ReleaseMethods();

	// Release lights
	for (int light = NumLights - 1; light >= 0; --light)
	{
		delete Lights[light];
	}

	// Release camera
	delete MainCamera;

	// Destroy all entities
	EntityManager.DestroyAllEntities();
	EntityManager.DestroyAllTemplates();

} // End of SceneShutdown function


//-----------------------------------------------------------------------------
// Game Helper functions
//-----------------------------------------------------------------------------

// Returns a list of the tank UIDs of the team
const vector<TEntityUID>& GetTeamTankUID( int teamID )
{
	return (teamID == 0) ? TeamOne : TeamTwo;
}

// Returns a list of the tank UIDs of the opposite team
const vector<TEntityUID>& GetEnemyTankUID( int teamID )
{
	return (teamID == 0) ? TeamTwo : TeamOne;
}

//-----------------------------------------------------------------------------
// Game loop functions
//-----------------------------------------------------------------------------

// Draw one frame of the scene
void RenderScene( float updateTime )
{
	// Setup the viewport - defines which part of the back-buffer we will render to (usually all of it)
	D3D10_VIEWPORT vp;
	vp.Width  = ViewportWidth;
	vp.Height = ViewportHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );

	// Select the back buffer and depth buffer to use for rendering
	g_pd3dDevice->OMSetRenderTargets( 1, &BackBufferRenderTarget, DepthStencilView );
	
	// Clear previous frame from back buffer and depth buffer
	g_pd3dDevice->ClearRenderTargetView( BackBufferRenderTarget, &AmbientLight.r );
	g_pd3dDevice->ClearDepthStencilView( DepthStencilView, D3D10_CLEAR_DEPTH, 1.0f, 0 );

	// Update camera aspect ratio based on viewport size - for better results when changing window size
	MainCamera->SetAspect( static_cast<TFloat32>(ViewportWidth) / ViewportHeight );

	// Set camera and light data in shaders
	MainCamera->CalculateMatrices();
	SetCamera(MainCamera);
	SetAmbientLight(AmbientLight);
	SetLights(&Lights[0]);

	// Render entities and draw on-screen text
	EntityManager.RenderAllEntities();
	RenderSceneText( updateTime );

    // Present the backbuffer contents to the display
	SwapChain->Present( 0, 0 );

} // End of RenderScene function


// Render a single text string at the given position in the given colour, may optionally centre it
void RenderText(const string& text, int X, int Y, float r, float g, float b, bool centre = false)
{
	RECT rect;
	if (!centre)
	{
		SetRect(&rect, X, Y, 0, 0);
		OSDFont->DrawText(NULL, text.c_str(), -1, &rect, DT_NOCLIP, D3DXCOLOR(r, g, b, 1.0f));
	}
	else
	{
		SetRect(&rect, X - 100, Y, X + 100, 0);
		OSDFont->DrawText(NULL, text.c_str(), -1, &rect, DT_CENTER | DT_NOCLIP, D3DXCOLOR(r, g, b, 1.0f));
	}

} // End of RenderText function

// Render on-screen text each frame
void RenderSceneText(float updateTime)
{
	stringstream outText;

	// Accumulate update times to calculate the average over a given period
	SumUpdateTimes += updateTime;
	++NumUpdateTimes;
	if (SumUpdateTimes >= UpdateTimePeriod)
	{
		AverageUpdateTime = SumUpdateTimes / NumUpdateTimes;
		SumUpdateTimes = 0.0f;
		NumUpdateTimes = 0;
	}

	/////////////////////////////////////
	// Extra tank on screen information

	// Check for key press to activate extra data
	if (KeyHit(Key_0))
	{
		if (mExtraInfoActive == true)
		{
			mExtraInfoActive = false;
		}
		else
		{
			(mExtraInfoActive = true);

		} // End of if statment

	} // End of if statment

	// Output various data displayed underneath each tank

	EntityManager.BeginEnumEntities("", "", "Tank");
	CEntity* pEntity = EntityManager.EnumEntity();

	while (pEntity != 0)
	{
		CVector2 PixelPoint;

		if (MainCamera->PixelFromWorldPt(&PixelPoint, pEntity->Position(), ViewportWidth, ViewportHeight))
		{
			CTankEntity* pTankEntity = dynamic_cast<CTankEntity*>(pEntity);

			if (pTankEntity != nullptr && pTankEntity->IsAlive())
			{
				// Output tank name
				outText << "Name: " << pEntity->GetName() << endl;

				// Key "0" press is used to show and hide extra info data
				if (mExtraInfoActive)
				{
					// Output number of hit points
					outText << "HPs: " << pTankEntity->GetHPs() << endl;
					// Output state of tank
					outText << "State: " << pTankEntity->GetTankStateText() << endl;
					// Output Shells fired
					outText << "Shells Fired: " << pTankEntity->GetNumShellsFired() << endl;
					// Output number of shells left
					outText << "Shells Left: " <<  pTankEntity->GetNumShellsLeft() << endl;

				} // End of if statment

				RenderText(outText.str(), (int)PixelPoint.x, (int)PixelPoint.y, 1.0f, 1.0f, 0.0f, true);
				outText.str("");

			} // End of if statment

		} // End of if statment

		pEntity = EntityManager.EnumEntity();

	} // End of while loop

	EntityManager.EndEnumEntities();

	/////////////////////////////
	// On screen output text

	// Write FPS text string
	if (AverageUpdateTime >= 0.0f)
	{
		outText << "Frame Time: " << AverageUpdateTime * 1000.0f << "ms" << endl << "FPS:" << 1.0f / AverageUpdateTime;
		RenderText(outText.str(), 2, 2, 0.0f, 0.0f, 0.0f);
		RenderText(outText.str(), 0, 0, 1.0f, 1.0f, 0.0f);
		outText.str("");

	} // End of if statment

	/////////////////////////////
	// Mouse button actions

	if (!KeyHeld(Mouse_LButton))
	{
		// Calculate the entity that is nearest to the cursor
		CVector2 cursorPos = CVector2((TFloat32)MouseX, (TFloat32)MouseY);
		TFloat32 closestDistance = 100.0f;
		
		// Find entity closest to cursor.
		for (TUInt32 i = 0; i < EntityManager.NumEntities(); ++i)
		{
			CEntity* pEntity = EntityManager.GetEntityAtIndex(i);

			if (pEntity != nullptr)
			{
				// Get the next entity position in viewport space.
				CVector2 entityPos2D;
				MainCamera->PixelFromWorldPt(&entityPos2D, pEntity->Position(), ViewportWidth, ViewportHeight);

				// Check if this entity position is closest to the cursor.
				if (entityPos2D.DistanceTo(cursorPos) < closestDistance)
				{
					SMessage msg;
					msg.type = Msg_Evade;
					msg.from = SystemUID;
					Messenger.SendMessage(pEntity->GetUID(), msg);

				} // End of if statment

			} // End of if statment

		} // End of foor loop

	} // End of if statment

} // End of RenderSceneText function


// Update the scene between rendering
void UpdateScene(float updateTime)
{
	/////////////////////////////
	// Health and ammo pack deployment

	SMessage msg;
	if (Messenger.FetchMessage(SystemUID, &msg))
	{
		// Check if a new healthpack is needed to be dropped
		if (msg.type == Msg_NewHealthPack)
		{
			gHealthPackTimer = 10.0f;
			gHealthPackDeployed = false;

		} // End of if statment
		
		// Check if a new ammo pack is needed to be dropped
		if (msg.type == Msg_NewAmmoPack)
		{
			gAmmoPackTimer = 15.0f;
			gAmmoPackDeployed = false;

		} // End of if statment

	} // End of if statment

	// Check to update the health pack timer
	if (!gHealthPackDeployed)
	{
		if (gHealthPackTimer > 0.0f)
		{
			gHealthPackTimer -= updateTime;
		}
		else  // Release the health pack
		{
			gHealthPackDeployed = true;

			EntityManager.CreateHealthPack
			(
				"HealthPack", "Health Pack 1", { Random(-50.0f, 0.0f), 25.0f, Random(-40.0f, 10.0f) }
			);

		} // End of if statment

	} // End of if statment

	// Check to update the ammo pack timer
	if (!gAmmoPackDeployed)
	{
		if (gAmmoPackTimer > 0.0f)
		{
			gAmmoPackTimer -= updateTime;
		}
		else  // Release the ammo pack
		{
			gAmmoPackDeployed = true;

			EntityManager.CreateAmmoPack
			(
				"AmmoPack", "Ammo Pack 1", { Random(0.0f, 50.0f), 25.0f, Random(-40.0f, 10.0f) }
			);
		} // End of if statment

	} // End of if statment

	// Call all entity update functions
	EntityManager.UpdateAllEntities(updateTime);

	/////////////////////////////
	// Camera controls

	// Set camera speeds
	// Key F1 used for full screen toggle
	if (KeyHit(Key_F2)) CameraMoveSpeed = 5.0f;
	if (KeyHit(Key_F3)) CameraMoveSpeed = 40.0f;

	if (KeyHit(Key_1))
	{
		// Create and send a start message to all tanks
		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* entity = EntityManager.EnumEntity();
		while (entity != 0)
		{
			SMessage msg;
			msg.type = Msg_Start;
			msg.from = SystemUID;
			Messenger.SendMessage(entity->GetUID(), msg);

			// Next tank entity
			entity = EntityManager.EnumEntity();
		
		} // End of while loop

		EntityManager.EndEnumEntities();

	} // End of if statment

	if (KeyHit(Key_2))
	{
		// Create and send a stop message to all tanks
		EntityManager.BeginEnumEntities("", "", "Tank");
		CEntity* entity = EntityManager.EnumEntity();
		while (entity != 0)
		{
			SMessage msg;
			msg.type = Msg_Stop;
			msg.from = SystemUID;
			Messenger.SendMessage(entity->GetUID(), msg);

			// Next tank entity
			entity = EntityManager.EnumEntity();

		} // End of while loop

		EntityManager.EndEnumEntities();

	} // End of if statment

	// Chase cam Keys

	if (KeyHit(Key_Numpad1)) // Tank A
	{
		chaseCamOn[0] = !chaseCamOn[0];
		chaseCamOn[1] = false;
		chaseCamOn[2] = false;
		chaseCamOn[3] = false;
		chaseCamOn[4] = false;
		chaseCamOn[5] = false;
	}
	else if (KeyHit(Key_Numpad2)) // Tank C
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = false;
		chaseCamOn[2] = !chaseCamOn[2];
		chaseCamOn[3] = false;
		chaseCamOn[4] = false;
		chaseCamOn[5] = false;
	}
	else if (KeyHit(Key_Numpad3)) // Tank E
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = false;
		chaseCamOn[2] = false;
		chaseCamOn[3] = false;
		chaseCamOn[4] = !chaseCamOn[4];
		chaseCamOn[5] = false;
	}
	else if (KeyHit(Key_Numpad4)) // Tank B
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = !chaseCamOn[1];
		chaseCamOn[2] = false;
		chaseCamOn[3] = false;
		chaseCamOn[4] = false;
		chaseCamOn[5] = false;
	}
	else if (KeyHit(Key_Numpad5)) // Tank D
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = false;
		chaseCamOn[2] = false;
		chaseCamOn[3] = !chaseCamOn[3];
		chaseCamOn[4] = false;
		chaseCamOn[5] = false;
	}
	else if (KeyHit(Key_Numpad6)) // Tank F
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = false;
		chaseCamOn[2] = false;
		chaseCamOn[3] = false;
		chaseCamOn[4] = false;
		chaseCamOn[5] = !chaseCamOn[5];

	} 
	else if (KeyHit(Key_Numpad0))
	{
		chaseCamOn[0] = false;
		chaseCamOn[1] = false;
		chaseCamOn[2] = false;
		chaseCamOn[3] = false;
		chaseCamOn[4] = false;
		chaseCamOn[5] = false;

	} // End of if statment

	if (chaseCamOn[0]) // Tank A
	{
		CEntity* pTank = EntityManager.GetEntity(TankA);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else if (chaseCamOn[1]) // Tank B
	{
		CEntity* pTank = EntityManager.GetEntity(TankB);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else if (chaseCamOn[2]) // Tank C
	{
		CEntity* pTank = EntityManager.GetEntity(TankC);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else if (chaseCamOn[3]) // Tank D
	{
		CEntity* pTank = EntityManager.GetEntity(TankD);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else if (chaseCamOn[4]) // Tank E
	{
		CEntity* pTank = EntityManager.GetEntity(TankE);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else if (chaseCamOn[5]) // Tank F
	{
		CEntity* pTank = EntityManager.GetEntity(TankF);

		if (pTank != 0)
		{
			// Position the camera in chase mode.
			MainCamera->Matrix() = pTank->Matrix();
			MainCamera->Matrix().MoveLocalZ(-10.0f);
			MainCamera->Matrix().MoveLocalY(5.0f);
			MainCamera->Matrix().RotateLocalX(ToRadians(15.0f));

		} // End of if statment

	}
	else
	{
		// Move the camera
		MainCamera->Control(Key_Up, Key_Down, Key_Left, Key_Right, Key_W, Key_S, Key_A, Key_D,
			CameraMoveSpeed * updateTime, CameraRotSpeed * updateTime);

	} // End of if statment

} // End of UpdateScene function

} // namespace gen
