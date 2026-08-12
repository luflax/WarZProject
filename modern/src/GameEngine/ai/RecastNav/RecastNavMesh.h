//=========================================================================
//  Recast & Detour navigation
//
//  REPLACES: Autodesk Navigation (Kynapse / Gameware Navigation), which Autodesk
//  discontinued and which cannot be licensed. See ../../../../DEPENDENCIES.md.
//
//  Recast (mesh generation) + Detour (queries, path following) + DetourCrowd
//  (local avoidance) cover everything the Kynapse wrapper provided. DetourCrowd in
//  particular subsumes AutodeskNavAvoidanceFilter entirely.
//
//  NAMING: the classes keep their AutodeskNav* names, and AutodeskNav/*.h are
//  forwarding headers, so every consumer (AI_Brain, AI_Tactics, obj_Zombie,
//  sobj_Zombie, ZombieNavAgent) compiles unchanged. Renaming them is a follow-up
//  that touches call sites, not a porting concern.
//=========================================================================

#pragma once

#include <unordered_set>

class GameObject;
class AI_Brain;
class RecastNavAgent;

class dtNavMesh;
class dtNavMeshQuery;
class dtCrowd;
class dtTileCache;

//////////////////////////////////////////////////////////////////////////

namespace AutodeskNavAgentEnums
{
    // Preserved from the Kynapse wrapper: consumers switch on these values.
    enum EAvoidanceResult
    {
        AvoidanceResult_None = 0,
        AvoidanceResult_Stop,
        AvoidanceResult_Slowdown,
        AvoidanceResult_Trajectory,
    };
}

//////////////////////////////////////////////////////////////////////////

class RecastNavAgent
{
public:
    // Point-of-interest tagging, used by GetNavAgentsInAABB to filter zombies.
    enum EPoiType
    {
        PoiUndefined = 0,
        PoiZombie,
        PoiSuperZombie,
    };

    enum EStatus
    {
        Idle = 0,
        ComputingPath,
        Moving,
        Arrived,
        PathNotFound,
        Failed,
    };

    EStatus     m_status;
    AutodeskNavAgentEnums::EAvoidanceResult m_prevAvoidanceResult;
    AutodeskNavAgentEnums::EAvoidanceResult m_avoidanceResult;

    r3dPoint3D  m_velocity;
    r3dPoint3D  m_position;
    r3dPoint3D  m_targetPos;
    float       m_arrivalPrecisionRadius;
    float       m_pathStartTime;
    float       m_targetSpeed;

    EPoiType    m_poiType;

    // Index into the dtCrowd agent pool; -1 when not registered.
    int         m_crowdAgentIdx;

    AI_Brain*   m_brain;

public:
    RecastNavAgent();
    virtual ~RecastNavAgent();

    bool        Init(void* world, const r3dVector& pos, EPoiType poiType = PoiUndefined);
    void        Update(float timeStep);
    bool        StartMove(const r3dPoint3D& target, float maxAstarDist = 999999);
    void        StopMove();
    void        SetTargetSpeed(float speed);
    r3dVector   GetPosition() const;

    AutodeskNavAgentEnums::EAvoidanceResult GetAvoidanceResult();
    bool        IsAvoidanceResultChanged();

#ifndef FINAL_BUILD
    void        DebugDraw();
#endif
};

//////////////////////////////////////////////////////////////////////////

class RecastNavMesh
{
public:
    RecastNavMesh();
    ~RecastNavMesh();

    // --- lifecycle --------------------------------------------------------
    // The port argument is the old Kynapse visual-debug port; Detour has no
    // equivalent server, so it is accepted and ignored.
    bool    Init(unsigned short visualDebugPort = 48888);
    bool    Load();
    bool    LoadPathData();
    void    RemovePathData();
    void    Close();
    void    Update();

    // --- authoring --------------------------------------------------------
    void    BuildForCurrentLevel();
    void    LoadBuildConfig();
    void    SaveBuildConfig();
    void    SaveNavGenProj();
    void    ExportToObj();

    // --- obstacles --------------------------------------------------------
    int     AddObstacle(GameObject* obstacle, const r3dBoundBox& bb, float rotX);
    int     AddObstacle(GameObject* obstacle, const r3dCylinder& c);
    bool    RemoveObstacle(int idx);

    // --- agents -----------------------------------------------------------
    RecastNavAgent* CreateNavAgent(const r3dVector& pos);
    void    AddNavAgent(RecastNavAgent* a);
    void    DeleteNavAgent(RecastNavAgent* a);

    RecastNavAgent** GetNavAgentsInAABB(const r3dPoint3D& collectorBoxCenter,
                                        const r3dPoint3D& collectorBoxHalfExtents,
                                        std::unordered_set<uint32_t>& PoiTypesSet,
                                        uint32_t& foundNumNavAgents,
                                        bool skipListCreation = false);

    // --- queries ----------------------------------------------------------
    bool    GetClosestNavMeshPoint(r3dPoint3D& inOut, float searchHeightRange, float searchWidthRadius);
    bool    IsNavPointValid(const r3dPoint3D& inOut);
    bool    AdjustNavPointHeight(r3dPoint3D& inOut, float searchHeightRange);

    // --- debug ------------------------------------------------------------
    void    DebugDraw();
    void    DebugDrawObstacles();

    // Kynapse exposed a Kaim::World / Kaim::Database here. Detour's equivalents are
    // the navmesh and the query object; returned as void* so the old call sites
    // (which only ever passed the value straight back in) keep compiling.
    void*   GetWorld();
    void*   GetDB();

    // DetourCrowd owns agent state; agents read their pose back through this.
    dtCrowd* GetCrowd() { return m_crowd; }

public:
    bool    doDebugDraw;
    bool    initialized;

    // Kept so existing call sites compile. buildGlobalConfig held Kynapse generator
    // parameters; the Recast equivalents live in RecastBuildConfig.
    struct RecastBuildConfig
    {
        float cellSize;
        float cellHeight;
        float agentHeight;
        float agentRadius;
        float agentMaxClimb;
        float agentMaxSlope;
        float regionMinSize;
        float regionMergeSize;
        float edgeMaxLen;
        float edgeMaxError;
        float vertsPerPoly;
        float detailSampleDist;
        float detailSampleMaxError;

        RecastBuildConfig();
    } buildGlobalConfig;

    // Zombie code reads this to pick a nav profile for super zombies.
    int     m_NavProfileIdSuperZombie;

    r3dTL::TArray<RecastNavAgent*> agents;
    r3dTL::TArray<void*>           obstacles;

    // Kynapse's perf bridge exposed Dump(); kept as a no-op so ServerGame.cpp's
    // diagnostic path compiles.
    struct PerfBridge { void Dump(); } perfBridge;

private:
    dtNavMesh*      m_navMesh;
    dtNavMeshQuery* m_navQuery;
    dtCrowd*        m_crowd;
    dtTileCache*    m_tileCache;

    int             GetFreeObstacleIdx();
};

extern RecastNavMesh gRecastNavMesh;
