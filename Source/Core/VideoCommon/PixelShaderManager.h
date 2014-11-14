// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#include "Common/ConstantBuffer.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/XFMemory.h"
#include "VideoCommon/PixelShaderGen.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager
{
public:
	static const size_t ConstantBufferSize = C_PENVCONST_END * 4;
	static void Init();
	static void Dirty();
	static void Shutdown();
	static void DoState(PointerWrap &p);
	static float* GetBufferToUpdate(u32 const_number, u32 size);
	static const float* GetBuffer();
	static bool IsDirty();
	static void Clear();
	static void EnableDirtyRegions();
	static void DisableDirtyRegions();
	static const regionvector &GetDirtyRegions();
	static void SetConstants(); // sets pixel shader constants

	// constant management, should be called after memory is committed
	static void SetColorChanged(int type, int index, bool high);
	static void SetAlpha(const AlphaTest& alpha);
	static void SetDestAlpha(const ConstantAlpha& alpha);
	static void SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt);
	static void SetZTextureBias(u32 bias);
	static void SetViewportChanged();
	static void SetIndMatrixChanged(int matrixidx);
	static void SetTevKSelChanged(int id);
	static void SetZTextureTypeChanged();
	static void SetIndTexScaleChanged(u8 stagemask);
	static void SetTexCoordChanged(u8 texmapid);
	static void SetFogColorChanged();
	static void SetFogParamChanged();
	static void SetFogRangeAdjustChanged();
	static void SetColorMatrix(const float* pmatrix);
	static void InvalidateXFRange(int start, int end);
	static void SetMaterialColorChanged(int index);
private:
	static void SetPSTextureDims(int texid);
	static float psconstants[ConstantBufferSize];
	static ConstatBuffer m_buffer;
};