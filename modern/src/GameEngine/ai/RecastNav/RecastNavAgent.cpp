//=========================================================================
//  Recast & Detour navigation -- agent implementation
//
//  Replaces AutodeskNavAgent (Kynapse Bot) and AutodeskNavAvoidanceFilter.
//  DetourCrowd handles path following AND local avoidance in one system, so the
//  separate avoidance filter has no counterpart -- its results are read back off
//  the crowd agent instead.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "RecastNavMesh.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"
#include "DetourCrowd.h"
#include "DetourCommon.h"

//////////////////////////////////////////////////////////////////////////

RecastNavAgent::RecastNavAgent()
    : m_status(Idle)
    , m_prevAvoidanceResult(AutodeskNavAgentEnums::AvoidanceResult_None)
    , m_avoidanceResult(AutodeskNavAgentEnums::AvoidanceResult_None)
    , m_velocity(0, 0, 0)
    , m_position(0, 0, 0)
    , m_targetPos(0, 0, 0)
    , m_arrivalPrecisionRadius(0.5f)
    , m_pathStartTime(0.0f)
    , m_targetSpeed(1.0f)
    , m_poiType(PoiUndefined)
    , m_crowdAgentIdx(-1)
    , m_brain(NULL)
{
}

RecastNavAgent::~RecastNavAgent()
{
    // The crowd slot is released by RecastNavMesh::DeleteNavAgent, which owns the
    // dtCrowd. Destroying an agent directly must not touch it.
}

bool RecastNavAgent::Init(void* /*world*/, const r3dVector& pos, EPoiType poiType)
{
    m_position = pos;
    m_poiType  = poiType;
    m_status   = Idle;

    // SEAM: once nav data is cooked, register with dtCrowd here:
    //   dtCrowdAgentParams params = {};
    //   params.radius = 0.4f; params.height = 2.0f;
    //   params.maxAcceleration = 8.0f; params.maxSpeed = m_targetSpeed;
    //   params.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OBSTACLE_AVOIDANCE;
    //   m_crowdAgentIdx = crowd->addAgent(p, &params);
    //
    // With an empty navmesh addAgent would fail, so registration is deferred and
    // the agent simply reports Idle.
    return true;
}

void RecastNavAgent::Update(float /*timeStep*/)
{
    if (m_crowdAgentIdx < 0)
        return;

    dtCrowd* crowd = gRecastNavMesh.GetCrowd();
    if (!crowd)
        return;

    const dtCrowdAgent* ag = crowd->getAgent(m_crowdAgentIdx);
    if (!ag || !ag->active)
        return;

    m_position = r3dPoint3D(ag->npos[0], ag->npos[1], ag->npos[2]);
    m_velocity = r3dPoint3D(ag->vel[0],  ag->vel[1],  ag->vel[2]);

    // DetourCrowd reports avoidance implicitly: a large gap between the desired and
    // actual velocity means the agent is being pushed off its path.
    const float desiredSpeed = dtVlen(ag->dvel);
    const float actualSpeed  = dtVlen(ag->vel);

    m_prevAvoidanceResult = m_avoidanceResult;

    if (desiredSpeed < 0.01f)
        m_avoidanceResult = AutodeskNavAgentEnums::AvoidanceResult_None;
    else if (actualSpeed < desiredSpeed * 0.1f)
        m_avoidanceResult = AutodeskNavAgentEnums::AvoidanceResult_Stop;
    else if (actualSpeed < desiredSpeed * 0.7f)
        m_avoidanceResult = AutodeskNavAgentEnums::AvoidanceResult_Slowdown;
    else
        m_avoidanceResult = AutodeskNavAgentEnums::AvoidanceResult_Trajectory;

    if (m_status == Moving)
    {
        const r3dPoint3D d = m_targetPos - m_position;
        if (d.Length() <= m_arrivalPrecisionRadius)
            m_status = Arrived;
    }
}

bool RecastNavAgent::StartMove(const r3dPoint3D& target, float /*maxAstarDist*/)
{
    m_targetPos    = target;
    m_pathStartTime = r3dGetTime();

    if (m_crowdAgentIdx < 0)
    {
        // No navmesh, so no path can be computed. Report honestly -- callers treat
        // PathNotFound as "pick another target", which is the correct behaviour.
        m_status = PathNotFound;
        return false;
    }

    // SEAM: dtNavMeshQuery::findNearestPoly on the target, then
    // dtCrowd::requestMoveTarget(m_crowdAgentIdx, ref, pos).
    m_status = ComputingPath;
    return true;
}

void RecastNavAgent::StopMove()
{
    m_status   = Idle;
    m_velocity = r3dPoint3D(0, 0, 0);

    // SEAM: dtCrowd::resetMoveTarget(m_crowdAgentIdx)
}

void RecastNavAgent::SetTargetSpeed(float speed)
{
    m_targetSpeed = speed;
    // SEAM: update dtCrowdAgentParams::maxSpeed via dtCrowd::updateAgentParameters.
}

r3dVector RecastNavAgent::GetPosition() const
{
    return m_position;
}

AutodeskNavAgentEnums::EAvoidanceResult RecastNavAgent::GetAvoidanceResult()
{
    return m_avoidanceResult;
}

bool RecastNavAgent::IsAvoidanceResultChanged()
{
    return m_avoidanceResult != m_prevAvoidanceResult;
}

#ifndef FINAL_BUILD
void RecastNavAgent::DebugDraw()
{
}
#endif
