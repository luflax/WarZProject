#include "r3dPCH.h"
#include "r3d.h"

#include "ChemLight.h"
#include "ObjectsCode/EFFECTS/obj_ParticleSystem.H"
#include "../WORLD/DecalChief.h"
#include "../WORLD/MaterialTypes.h"
#include "ExplosionVisualController.h"
#include "FlashbangVisualController.h"
#include "Gameplay_Params.h"

#include "multiplayer/P2PMessages.h"

#include "../../multiplayer/ClientGameLogic.h"
#include "../AI/AI_Player.H"
#include "WeaponConfig.h"
#include "Weapon.h"

IMPLEMENT_CLASS(obj_ChemLight, "obj_ChemLight", "Object");
AUTOREGISTER_CLASS(obj_ChemLight);

obj_ChemLight::obj_ChemLight()
{
}

void obj_ChemLight::OnExplode()
{
	// Do Nothing!
}
