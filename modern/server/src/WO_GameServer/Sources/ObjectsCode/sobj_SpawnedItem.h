#pragma once

#include "GameCommon.h"
#include "NetworkHelper.h"

class obj_SpawnedItem : public GameObject, INetworkHelper
{
	DECLARE_CLASS(obj_SpawnedItem, GameObject)

public:
	gobjid_t	m_SpawnObj;
	int		m_SpawnIdx;
	wiInventoryItem m_Item;
	float	m_DestroyIn; // time when item should be de-spawned
	bool	m_isHackerDecoy;
	bool	m_SpawnLootCrateOnClient;
	
	void		AdjustDroppedWeaponAmmo();

public:
	obj_SpawnedItem();
	~obj_SpawnedItem();
	
	virtual BOOL	OnCreate();
	virtual BOOL	OnDestroy();
	virtual BOOL	Update();
	
	INetworkHelper*	GetNetworkHelper() { return dynamic_cast<INetworkHelper*>(this); }
	DefaultPacket*	NetGetCreatePacket(int* out_size);

	void		LoadServerObjectData() { r3dError("not implemented"); }
	void		SaveServerObjectData() { r3dError("not implemented"); }
};
