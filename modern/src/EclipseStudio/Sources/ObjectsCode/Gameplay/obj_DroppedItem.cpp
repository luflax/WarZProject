#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "obj_DroppedItem.h"

#include "ObjectsCode/weapons/WeaponArmory.h"
#include "../EFFECTS/obj_ParticleSystem.h"
#include "../../multiplayer/ClientGameLogic.h"
#include "../ai/AI_Player.H"

extern bool g_bEditMode;

IMPLEMENT_CLASS(obj_DroppedItem, "obj_DroppedItem", "Object");
AUTOREGISTER_CLASS(obj_DroppedItem);

obj_DroppedItem::obj_DroppedItem()
{
	m_AllowAsyncLoading = 1;
	NeedInitPhysics = 0;
}

obj_DroppedItem::~obj_DroppedItem()
{
}

void obj_DroppedItem::SetHighlight( bool highlight )
{
	m_FillGBufferTarget = highlight ? rsFillGBufferAfterEffects : rsFillGBuffer;
}

bool obj_DroppedItem::GetHighlight() const
{
	return m_FillGBufferTarget == rsFillGBufferAfterEffects;
}

BOOL obj_DroppedItem::Load(const char *fname)
{
	return TRUE;
}

BOOL obj_DroppedItem::OnCreate()
{
	R3DPROFILE_FUNCTION( "obj_DroppedItem::OnCreate" );

#ifndef FINAL_BUILD
	if( g_bEditMode )
	{
		m_Item.itemID = 'GOLD';
		SetHighlight( true );
	}
#endif

	r3d_assert(m_Item.itemID);
	
	const char* cpMeshName = "";
	if(m_Item.itemID == 'GOLD')
	{
		cpMeshName = "Data\\ObjectsDepot\\Weapons\\Item_Money_Stack_01.sco";
	}
	else if(m_Item.itemID == 'LOOT')
	{
		cpMeshName = "Data\\ObjectsDepot\\Weapons\\Item_LootCrate_01.sco";
	}
	else
	{
		const ModelItemConfig* cfg = (const ModelItemConfig*)g_pWeaponArmory->getConfig(m_Item.itemID);
		switch(cfg->category)
		{
			case storecat_Account:
			case storecat_Boost:
			case storecat_LootBox:
			case storecat_HeroPackage:
				r3dError("spawned item is not model");
				break;
		}
		cpMeshName = cfg->m_ModelPath;
	}
	
	if(!parent::Load(cpMeshName)) 
		return FALSE;

	if(m_Item.itemID == 'GOLD')
	{
		m_ActionUI_Title = gLangMngr.getString("Money");
		m_ActionUI_Msg = gLangMngr.getString("HoldEToPickUpMoney");
	}
	else if(m_Item.itemID == 'LOOT')
	{
		m_ActionUI_Title = gLangMngr.getString("Item");
		m_ActionUI_Msg = gLangMngr.getString("HoldEToPickUpItem");
	}
	else
	{
		const BaseItemConfig* cfg = g_pWeaponArmory->getConfig(m_Item.itemID);
		m_ActionUI_Title = cfg->m_StoreName;
		m_ActionUI_Msg = gLangMngr.getString("HoldEToPickUpItem");
	}

	ReadPhysicsConfig();
	PhysicsConfig.isDynamic = false; // to prevent items from falling through the ground
	PhysicsConfig.isKinematic = true; // to prevent them from being completely static, as moving static object is causing physx performance issues, and we cannot place item at this point, as it is not loaded fully yet
	PhysicsConfig.group = PHYSCOLL_TINY_GEOMETRY; // skip collision with players
	PhysicsConfig.requireNoBounceMaterial = false;
	PhysicsConfig.isFastMoving = false; // fucked up PhysX CCD is crashing all the time

	SetPosition(GetPosition()+r3dPoint3D(0,0.25f,0));
	
	m_spawnPos = GetPosition();

	// will have to create it later - when we are loaded.
	if( m_bEnablePhysics )
	{
		NeedInitPhysics = 1;
		m_bEnablePhysics = 0;
	}

	parent::OnCreate();

	return 1;
}

void obj_DroppedItem::UpdateObjectPositionAfterCreation()
{
	if(!PhysicsObject)
		return;

	PxActor* actor = PhysicsObject->getPhysicsActor();
	if(!actor)
		return;

	PxBounds3 pxBbox = actor->getWorldBounds();
	PxVec3 pxCenter = pxBbox.getCenter();

	// place object on the ground, to prevent excessive bouncing
	{
		PxRaycastHit hit;
		PxSceneQueryFilterData filter(PxFilterData(COLLIDABLE_STATIC_MASK, 0, 0, 0), PxSceneQueryFilterFlag::eSTATIC);
		if(g_pPhysicsWorld->raycastSingle(PxVec3(pxCenter.x, pxCenter.y, pxCenter.z), PxVec3(0, -1, 0), 50.0f, PxSceneQueryFlag::eIMPACT, hit, filter))
		{
			float diff = GetPosition().y - pxBbox.minimum.y;
			SetPosition(r3dPoint3D(hit.impact.x, hit.impact.y+diff, hit.impact.z));
		}
	}
}

BOOL obj_DroppedItem::OnDestroy()
{
	return parent::OnDestroy();
}

BOOL obj_DroppedItem::Update()
{
	if( NeedInitPhysics && MeshLOD[ 0 ] && MeshLOD[ 0 ]->IsDrawable() )
	{
		m_bEnablePhysics = 1;
		CreatePhysicsData();
		NeedInitPhysics = 0;
		UpdateObjectPositionAfterCreation();
	}

	parent::Update();
	
	r3dPoint3D pos = GetBBoxWorld().Center();

#ifndef FINAL_BUILD
	if( !g_bEditMode )
#endif
	{
		const ClientGameLogic& CGL = gClientLogic();
		if(CGL.localPlayer_ && (CGL.localPlayer_->GetPosition() - pos).Length() < 10.0f)
			SetHighlight(true);
		else
			SetHighlight(false);
	}

	return TRUE;
}

void obj_DroppedItem::AppendRenderables( RenderArray ( & render_arrays )[ rsCount ], const r3dCamera& Cam )
{
	MeshGameObject::AppendRenderables( render_arrays, Cam );

	if( GetHighlight() )
	{
		MeshObjDeferredHighlightRenderable rend;

		rend.Init( MeshGameObject::GetObjectLodMesh(), this, r3dColor::blue.GetPacked() );
		rend.SortValue = 0;
		rend.DoExtrude = 0;
		
		render_arrays[ rsDrawHighlightPass0 ].PushBack( rend );

		rend.DoExtrude = 1;
		render_arrays[ rsDrawHighlightPass1 ].PushBack( rend );
	}
}

