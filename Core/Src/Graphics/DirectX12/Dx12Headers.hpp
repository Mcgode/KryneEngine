/**
 * @file
 * @author Max Godefroy
 * @date 11/03/2023.
 */

#pragma once

#include "KryneEngine/Core/Platform/Windows.h"

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using Microsoft::WRL::ComPtr;

// DirectX 12 specific headers.
#include "directx/d3d12.h"
#include <d3dcompiler.h>
#include <dxgi1_6.h>

// D3D12 extension library.
#include "directx/d3dx12.h"

// Cross-compilation shims for the mingw-w64 SDK (missing __uuidof support and a
// few debug GUID constants). No-op for MSVC.
#include "Graphics/DirectX12/Dx12MingwCompat.hpp"

// Kryne engine includes
#include "KryneEngine/Core/Common/Types.hpp"
#include "KryneEngine/Core/Graphics/GraphicsCommon.hpp"
#include "KryneEngine/Core/Memory/GenerationalPool.hpp"
#include "KryneEngine/Core/Profiling/TracyHeader.hpp"