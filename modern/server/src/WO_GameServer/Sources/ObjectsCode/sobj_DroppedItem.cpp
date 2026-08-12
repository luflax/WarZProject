#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"

#include "multiplayer/P2PMessages.h"
#include "ServerGameLogic.h"
#include "../EclipseStudio/Sources/ObjectsCode/weapons/WeaponArmory.h"

#include "sobj_DroppedItem.h"

#ifdef ENABLE_GAMEBLOCKS
#include "GBClient/Inc/GBClient.h"
#include "GBClient/Inc/GBReservedEvents.h"

extern GameBlocks::GBClient* g_GameBlocks_Client;
extern GameBlocks::GBPublicSourceId g_GameBlocks_ServerID;
#endif //ENABLE_GAMEBLOCKS

const float DROPPED_ITEM_EXPIRE_TIME = 10.0f * 60.0f; // 10 min

IMPLEMENT_CLASS(obj_DroppedItem, "obj_DroppedItem", "Object");
AUTOREGISTER_CLASS(obj_DroppedItem);

obj_DroppedItem::obj_DroppedItem()
{
	srvObjParams_.ExpireTime = r3dGetTime() + DROPPED_ITEM_EXPIRE_TIME;	// setup here, as it can be overwritten
}

obj_DroppedItem::~obj_DroppedItem()
{
}

BOOL obj_DroppedItem::OnCreate()
{
	r3dOutToLog("obj_DroppedItem %p created. %d, %f sec left\n", this, m_Item.itemID, srvObjParams_.ExpireTime - r3dGetTime());

	r3d_assert(NetworkLocal);
	r3d_assert(GetNetworkID());
	r3d_assert(m_Item.itemID);

	m_Item.ResetClipIfFull();
	
	// overwrite object network visibility
	distToCreateSq = 130 * 130;
	distToDeleteSq = 150 * 150;

	// raycast down to earth in case world was changed or trying to spawn item in the air (player killed during jump)
	r3dPoint3D pos = gServerLogic.AdjustPositionToFloor(GetPosition());
	SetPosition(pos);

	gServerLogic.NetRegisterObjectToPeers(this);

#ifdef ENABLE_GAMEBLOCKS
	if(g_GameBlocks_Client && g_GameBlocks_Client->Connected() && srvObjParams_.CustomerID != 0)
	{
		g_GameBlocks_Client->PrepareEventForSending("DropItem", g_GameBlocks_ServerID, GameBlocks::GBPublicPlayerId(uint32_t(srvObjParams_.CustomerID)));
		g_GameBlocks_Client->AddKeyValueInt("ItemID", m_Item.itemID);
		g_GameBlocks_Client->SendEvent();
	}
#endif

	return parent::OnCreate();
}

BOOL obj_DroppedItem::OnDestroy()
{
	//r3dOutToLog("obj_DroppedItem %p destroyed\n", this);

	PKT_S2C_DestroyNetObject_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	gServerLogic.p2pBroadcastToActive(this, &n, sizeof(n));
	
	return parent::OnDestroy();
}

BOOL obj_DroppedItem::Update()
{
	if(r3dGetTime() > srvObjParams_.ExpireTime)
	{
		setActiveFlag(0);
	}

	return parent::Update();
}

DefaultPacket* obj_DroppedItem::NetGetCreatePacket(int* out_size)
{
	static PKT_S2C_CreateDroppedItem_s n;
	n.spawnID = toP2pNetId(GetNetworkID());
	n.pos     = GetPosition();
	n.Item    = m_Item;
	
	*out_size = sizeof(n);
	return &n;
}

void obj_DroppedItem::LoadServerObjectData()
{
	// deserialize from xml
	IServerObject::CSrvObjXmlReader xml(srvObjParams_.Var1);
	m_Item.itemID      = srvObjParams_.ItemID;
	m_Item.InventoryID = xml.xmlObj.attribute("iid").as_int64();
	m_Item.quantity    = xml.xmlObj.attribute("q").as_int();
	m_Item.Var1        = xml.xmlObj.attribute("v1").as_int();
	m_Item.Var2        = xml.xmlObj.attribute("v2").as_int();
	m_Item.Var3        = xml.xmlObj.attribute("v3").as_int();
}

void obj_DroppedItem::SaveServerObjectData()
{
	srvObjParams_.ItemID     = m_Item.itemID;

	char strInventoryID[64];
	sprintf(strInventoryID, "%I64d", m_Item.InventoryID);

	IServerObject::CSrvObjXmlWriter xml;
	xml.xmlObj.append_attribute("iid") = strInventoryID;
	xml.xmlObj.append_attribute("q")   = m_Item.quantity;
	xml.xmlObj.append_attribute("v1")  = m_Item.Var1;
	xml.xmlObj.append_attribute("v2")  = m_Item.Var2;
	xml.xmlObj.append_attribute("v3")  = m_Item.Var3;
	xml.save(srvObjParams_.Var1);
}
