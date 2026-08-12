//=========================================================================
//  Recast & Detour navigation -- implementation
//
//  See RecastNavMesh.h for why this replaces Autodesk Navigation.
//
//  SCOPE: this brings up the navigation layer on Detour and preserves the API the
//  game calls. Runtime navmesh GENERATION (Recast rcBuildContours/rcBuildPolyMesh
//  over the level geometry) is deliberately not implemented here -- the original
//  pipeline baked nav data offline via Kynapse's generator, and the Recast
//  equivalent belongs in the asset-cook stage, not in the engine. BuildForCurrentLevel
//  and LoadPathData are the seams where that plugs in.
//
//  Consequence: until the cook stage exists, the navmesh is empty, queries fail
//  cleanly, and zombies do not path. That matches the Phase 1 contract -- see
//  ../../../../PHASE1-BUILD-PLAN.md, "What Phase 1 explicitly leaves broken".
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "RecastNavMesh.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCrowd.h"
#include "DetourCommon.h"

//////////////////////////////////////////////////////////////////////////

RecastNavMesh gRecastNavMesh;

namespace
{
    // Detour works in its own float[3]; the engine uses r3dPoint3D. Both are xyz
    // floats in the same handedness, so conversion is a straight copy.
    inline void ToDt(const r3dPoint3D& in, float* out) { out[0] = in.x; out[1] = in.y; out[2] = in.z; }
    inline r3dPoint3D FromDt(const float* in)          { return r3dPoint3D(in[0], in[1], in[2]); }

    // Half-extents used when snapping a world position onto a polygon.
    const float kDefaultExtents[3] = { 2.0f, 4.0f, 2.0f };

    const int   kMaxCrowdAgents = 512;
    const float kMaxAgentRadius = 0.6f;
}

//////////////////////////////////////////////////////////////////////////
// RecastNavMesh::RecastBuildConfig
//////////////////////////////////////////////////////////////////////////

RecastNavMesh::RecastBuildConfig::RecastBuildConfig()
    : cellSize(0.3f)
    , cellHeight(0.2f)
    , agentHeight(2.0f)
    , agentRadius(0.6f)
    , agentMaxClimb(0.9f)
    , agentMaxSlope(45.0f)
    , regionMinSize(8.0f)
    , regionMergeSize(20.0f)
    , edgeMaxLen(12.0f)
    , edgeMaxError(1.3f)
    , vertsPerPoly(6.0f)
    , detailSampleDist(6.0f)
    , detailSampleMaxError(1.0f)
{
}

void RecastNavMesh::PerfBridge::Dump()
{
    // Kynapse's profiler bridge. Detour has no equivalent; the timings the caller
    // wants are already reported by the server's own performance block.
}

//////////////////////////////////////////////////////////////////////////
// RecastNavMesh
//////////////////////////////////////////////////////////////////////////

RecastNavMesh::RecastNavMesh()
    : doDebugDraw(false)
    , initialized(false)
    , m_NavProfileIdSuperZombie(0)
    , m_navMesh(NULL)
    , m_navQuery(NULL)
    , m_crowd(NULL)
    , m_tileCache(NULL)
{
}

RecastNavMesh::~RecastNavMesh()
{
    Close();
}

bool RecastNavMesh::Init(unsigned short /*visualDebugPort*/)
{
    // The port argument drove Kynapse's NavigationLab visual-debug server. Detour
    // ships no such server; debug visualisation is DebugDraw() instead.
    if (initialized)
        return true;

    m_navMesh = dtAllocNavMesh();
    if (!m_navMesh)
    {
        r3dOutToLog("RecastNav: dtAllocNavMesh failed\n");
        return false;
    }

    m_navQuery = dtAllocNavMeshQuery();
    if (!m_navQuery)
    {
        r3dOutToLog("RecastNav: dtAllocNavMeshQuery failed\n");
        Close();
        return false;
    }

    m_crowd = dtAllocCrowd();
    if (!m_crowd)
    {
        r3dOutToLog("RecastNav: dtAllocCrowd failed\n");
        Close();
        return false;
    }

    initialized = true;
    r3dOutToLog("RecastNav: initialized (navmesh empty until nav data is cooked)\n");
    return true;
}

bool RecastNavMesh::Load()
{
    return LoadPathData();
}

bool RecastNavMesh::LoadPathData()
{
    // SEAM: load a Detour navmesh baked by the cook stage (dtNavMesh::init with the
    // serialized tile data) and then m_navQuery->init(m_navMesh, ...) plus
    // m_crowd->init(kMaxCrowdAgents, kMaxAgentRadius, m_navMesh).
    //
    // Until that exists there is no nav data to load; report honestly rather than
    // claiming success.
    return false;
}

void RecastNavMesh::RemovePathData()
{
    if (m_navQuery) { dtFreeNavMeshQuery(m_navQuery); m_navQuery = NULL; }
    if (m_navMesh)  { dtFreeNavMesh(m_navMesh);       m_navMesh  = NULL; }
}

void RecastNavMesh::Close()
{
    for (int i = 0, e = (int)agents.Count(); i < e; ++i)
        delete agents[i];
    agents.Clear();
    obstacles.Clear();

    if (m_crowd)    { dtFreeCrowd(m_crowd);           m_crowd    = NULL; }
    if (m_navQuery) { dtFreeNavMeshQuery(m_navQuery); m_navQuery = NULL; }
    if (m_navMesh)  { dtFreeNavMesh(m_navMesh);       m_navMesh  = NULL; }

    initialized = false;
}

void RecastNavMesh::Update()
{
    if (!initialized || !m_crowd)
        return;

    // DetourCrowd advances path following AND local avoidance in one step -- this
    // is what replaces AutodeskNavAvoidanceFilter.
    m_crowd->update(r3dGetFrameTime(), NULL);

    for (int i = 0, e = (int)agents.Count(); i < e; ++i)
        if (agents[i])
            agents[i]->Update(r3dGetFrameTime());
}

//////////////////////////////////////////////////////////////////////////
// Authoring -- offline cook seams
//////////////////////////////////////////////////////////////////////////

void RecastNavMesh::BuildForCurrentLevel()
{
    // SEAM: run Recast over the level's collision geometry (rcCreateHeightfield ->
    // rcRasterizeTriangles -> rcBuildCompactHeightfield -> rcBuildRegions ->
    // rcBuildContours -> rcBuildPolyMesh -> dtCreateNavMeshData) and write the
    // result next to the level. Belongs in the asset cook, not the engine.
    r3dOutToLog("RecastNav: BuildForCurrentLevel is not implemented; nav data is cooked offline\n");
}

void RecastNavMesh::LoadBuildConfig()  {}
void RecastNavMesh::SaveBuildConfig()  {}
void RecastNavMesh::SaveNavGenProj()   {}

void RecastNavMesh::ExportToObj()
{
    r3dOutToLog("RecastNav: ExportToObj is not implemented\n");
}

//////////////////////////////////////////////////////////////////////////
// Obstacles
//////////////////////////////////////////////////////////////////////////

int RecastNavMesh::GetFreeObstacleIdx()
{
    for (int i = 0, e = (int)obstacles.Count(); i < e; ++i)
        if (obstacles[i] == NULL)
            return i;

    obstacles.PushBack(NULL);
    return (int)obstacles.Count() - 1;
}

int RecastNavMesh::AddObstacle(GameObject* /*obstacle*/, const r3dBoundBox& /*bb*/, float /*rotX*/)
{
    // SEAM: dtTileCache::addBoxObstacle once a tile-cached navmesh is cooked.
    // Dynamic obstacles require the tile-cache build, not the plain navmesh.
    return -1;
}

int RecastNavMesh::AddObstacle(GameObject* /*obstacle*/, const r3dCylinder& /*c*/)
{
    // SEAM: dtTileCache::addObstacle (cylinder form).
    return -1;
}

bool RecastNavMesh::RemoveObstacle(int /*idx*/)
{
    return false;
}

//////////////////////////////////////////////////////////////////////////
// Agents
//////////////////////////////////////////////////////////////////////////

RecastNavAgent* RecastNavMesh::CreateNavAgent(const r3dVector& pos)
{
    RecastNavAgent* a = new RecastNavAgent();
    if (!a->Init(this, pos))
    {
        delete a;
        return NULL;
    }
    AddNavAgent(a);
    return a;
}

void RecastNavMesh::AddNavAgent(RecastNavAgent* a)
{
    if (!a) return;
    for (int i = 0, e = (int)agents.Count(); i < e; ++i)
        if (agents[i] == a)
            return;
    agents.PushBack(a);
}

void RecastNavMesh::DeleteNavAgent(RecastNavAgent* a)
{
    if (!a) return;

    if (m_crowd && a->m_crowdAgentIdx >= 0)
    {
        m_crowd->removeAgent(a->m_crowdAgentIdx);
        a->m_crowdAgentIdx = -1;
    }

    for (int i = 0, e = (int)agents.Count(); i < e; ++i)
    {
        if (agents[i] == a)
        {
            agents.Erase(i);
            break;
        }
    }
    delete a;
}

RecastNavAgent** RecastNavMesh::GetNavAgentsInAABB(const r3dPoint3D& centerPos,
                                                   const r3dPoint3D& halfExtents,
                                                   std::unordered_set<uint32_t>& PoiTypesSet,
                                                   uint32_t& foundNumNavAgents,
                                                   bool skipListCreation)
{
    // Kynapse ran this through its spatial database. Detour has no equivalent
    // query, so this walks the agent list directly -- the counts here are small
    // (zombies near one player) and the caller already bounds it by an AABB.
    foundNumNavAgents = 0;

    const r3dPoint3D lo = centerPos - halfExtents;
    const r3dPoint3D hi = centerPos + halfExtents;

    r3dTL::TArray<RecastNavAgent*> found;

    for (int i = 0, e = (int)agents.Count(); i < e; ++i)
    {
        RecastNavAgent* a = agents[i];
        if (!a) continue;

        if (!PoiTypesSet.empty() &&
            PoiTypesSet.find((uint32_t)a->m_poiType) == PoiTypesSet.end())
            continue;

        const r3dPoint3D& p = a->m_position;
        if (p.x < lo.x || p.x > hi.x) continue;
        if (p.y < lo.y || p.y > hi.y) continue;
        if (p.z < lo.z || p.z > hi.z) continue;

        ++foundNumNavAgents;
        if (!skipListCreation)
            found.PushBack(a);
    }

    if (skipListCreation || foundNumNavAgents == 0)
        return NULL;

    // Caller owns the result and frees it with delete[] -- same contract as before.
    RecastNavAgent** result = new RecastNavAgent*[foundNumNavAgents];
    for (uint32_t i = 0; i < foundNumNavAgents; ++i)
        result[i] = found[i];
    return result;
}

//////////////////////////////////////////////////////////////////////////
// Queries
//////////////////////////////////////////////////////////////////////////

bool RecastNavMesh::GetClosestNavMeshPoint(r3dPoint3D& inOut, float searchHeightRange, float searchWidthRadius)
{
    if (!m_navQuery)
        return false;

    float center[3];
    ToDt(inOut, center);

    const float extents[3] = { searchWidthRadius, searchHeightRange, searchWidthRadius };

    dtQueryFilter filter;
    dtPolyRef     ref = 0;
    float         nearest[3] = { 0, 0, 0 };

    if (dtStatusFailed(m_navQuery->findNearestPoly(center, extents, &filter, &ref, nearest)) || !ref)
        return false;

    inOut = FromDt(nearest);
    return true;
}

bool RecastNavMesh::IsNavPointValid(const r3dPoint3D& in)
{
    if (!m_navQuery)
        return false;

    float center[3];
    ToDt(in, center);

    dtQueryFilter filter;
    dtPolyRef     ref = 0;
    float         nearest[3] = { 0, 0, 0 };

    if (dtStatusFailed(m_navQuery->findNearestPoly(center, kDefaultExtents, &filter, &ref, nearest)))
        return false;

    return ref != 0;
}

bool RecastNavMesh::AdjustNavPointHeight(r3dPoint3D& inOut, float searchHeightRange)
{
    if (!m_navQuery)
        return false;

    float center[3];
    ToDt(inOut, center);

    const float extents[3] = { 0.5f, searchHeightRange, 0.5f };

    dtQueryFilter filter;
    dtPolyRef     ref = 0;
    float         nearest[3] = { 0, 0, 0 };

    if (dtStatusFailed(m_navQuery->findNearestPoly(center, extents, &filter, &ref, nearest)) || !ref)
        return false;

    float h = 0.0f;
    if (dtStatusFailed(m_navQuery->getPolyHeight(ref, nearest, &h)))
        return false;

    inOut.y = h;
    return true;
}

//////////////////////////////////////////////////////////////////////////
// Debug
//////////////////////////////////////////////////////////////////////////

void RecastNavMesh::DebugDraw()
{
#ifndef FINAL_BUILD
    if (!doDebugDraw)
        return;
    // SEAM: DebugUtils/DetourDebugDraw.h renders the navmesh through a duDebugDraw
    // implementation, which needs an r3d-backed line/tri renderer.
#endif
}

void RecastNavMesh::DebugDrawObstacles()
{
}

void* RecastNavMesh::GetWorld() { return m_navMesh;  }
void* RecastNavMesh::GetDB()    { return m_navQuery; }
