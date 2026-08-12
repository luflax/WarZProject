#include "r3dPCH.h"
#include "r3d.h"

#include "GameCommon.h"
#include "../../EclipseStudio/Sources/ObjectsCode/Nature/obj_LocalColorCorrection.h"

#include "PostFXChief.h"

#include "PFX_BlackWhiteColorCorrection.h"

PFX_BlackWhiteColorCorrection::PFX_BlackWhiteColorCorrection()
: Parent( this )
, m_fPower ( 1.0f )
, mPSID ( -1 )
{

}

PFX_BlackWhiteColorCorrection::~PFX_BlackWhiteColorCorrection()
{

}


void PFX_BlackWhiteColorCorrection::SetPower ( float fPwr )
{
	m_fPower = fPwr;
}
void PFX_BlackWhiteColorCorrection::InitImpl()
{
	mPSID = r3dRenderer->GetPixelShaderIdx( "PS_BLACK_WHITE" );
	mData.PixelShaderID = mPSID;
}

void PFX_BlackWhiteColorCorrection::CloseImpl()
{
}

void PFX_BlackWhiteColorCorrection::PrepareImpl( r3dScreenBuffer* dest, r3dScreenBuffer* src )
{
	// [PORT] Taking the address of a temporary is an MSVC extension; ISO C++ needs a named local.
	const D3DXVECTOR4 constants( m_fPower, 0.0f, 0.0f, 0.0f );
	r3dRenderer->pd3ddev->SetPixelShaderConstantF ( 0, (const float*)&constants, 1 );
}

void PFX_BlackWhiteColorCorrection::FinishImpl()
{

}