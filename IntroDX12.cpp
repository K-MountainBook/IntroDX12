
#include <tchar.h>
#include <vector>

// DirectX12 headers
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <d3dx12.h>

#include <d3dcompiler.h>

#ifdef _DEBUG
#include <iostream>
#endif


// Static library linking
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace DirectX;

// DX12系Intefaceのポインタを宣言
ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapChain = nullptr;
// コマンドアロケータ
ID3D12CommandAllocator* _cmdAllocator = nullptr;
// コマンドリスト
ID3D12GraphicsCommandList* _cmdList = nullptr;
// コマンドキュー
ID3D12CommandQueue* _cmdQueue = nullptr;



/*
XMFLOAT3 verticles[3] = {
	{-0.5f, -0.7f, 0.0f},
	{0.0f, 0.7f, 0.0f},
	{0.5f, -0.7f, 0.0f}
}; //頂点3個
*/


//XMFLOAT3 verticles[] = {
//	{-0.4f, -0.7f, 0.0f},
//	{-0.4f, 0.7f, 0.0f},
//	{0.4f, -0.7f, 0.0f},
//	{ 0.4f, 0.7f, 0.0f }
//}; //頂点4個


struct Vertex
{
	XMFLOAT3 pos;	// xyz座標
	XMFLOAT2 uv;	// uv座標
};


Vertex vertices[] = {
	{{-0.4f, -0.7f, 0.0f},{0.0f,1.0f}},
	{{-0.4f,  0.7f, 0.0f},{0.0f,0.0f}},
	{{ 0.4f, -0.7f, 0.0f},{1.0f,1.0f}},
	{{ 0.4f,  0.7f, 0.0f},{1.0f,0.0f}}
};

float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };

/// <summary>
/// デバッグレイヤーの有効化
/// </summary>
void EnableDebugLayer() {
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (result == S_OK) {
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
}

// ウィンドウプロシージャ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wpram, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wpram, lparam);
}


#ifdef _DEBUG
/// <summary>
/// デバッグの場合はmain関数
/// </summary>
int main() {
#else
#include <Windows.h>
/// <summary>
/// リリースの場合はWinMainを呼び出す
/// </summary>
/// <param name=""></param>
/// <param name=""></param>
/// <param name=""></param>
/// <param name=""></param>
/// <returns></returns>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	OutputDebugString(L"Show window test.");
	// ウィンドウの生成
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DX12Sample");
	w.hInstance = GetModuleHandle(0);
	RegisterClassEx(&w);

	RECT wrc = { 0, 0, 1280, 720 };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	//　ウィンドウの生成	
	HWND hwnd = CreateWindow(
		w.lpszClassName,
		_T("DX12Test"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr);


#ifdef _DEBUG
	EnableDebugLayer();
#endif

	// D3Dの機能レベルの配列	
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};


	// テクスチャの準備
	struct TexRGBA
	{
		unsigned char R, G, B, A;
	};

	std::vector<TexRGBA> texturedata(256 * 256);

	for (auto& rgba : texturedata)
	{
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255;
	}

#ifdef _DEBUG
	auto result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else 
	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif // DEBUG


	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* _tmpAdapter = nullptr;

	for (int i = 0; _dxgiFactory->EnumAdapters(i, &_tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back(_tmpAdapter);
	}

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc;
		adpt->GetDesc(&adesc);
		//std::wcout << L"video card name: " << adesc.Description << std::endl;

		//std::wstring strDesc = adesc.Description;

		//if (strDesc.find(L"NVIDIA") != std::string::npos) {
		//	_tmpAdapter = adpt;
		//	break;
		//}

	}

	//D3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto lv : levels) {
		if (D3D12CreateDevice(_tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = lv;
			break;
		}
	}
	// コマンドキューの生成
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));
	if (result != S_OK) {
		return -1;
	}

	// スワップチェインの生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = 1280;
	swapchainDesc.Height = 720;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapChain);

	if (result != S_OK) {
		return -1;
	}

	//ディスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //レンダーターゲットビュー
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // バックバッファ数
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));
	if (result != S_OK) {
		return -1;
	}
	// スワップチェーンとメモリの紐づけを行う
	DXGI_SWAP_CHAIN_DESC swcDesc = {};

	result = _swapChain->GetDesc(&swcDesc);

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();

	for (size_t i = 0; i < swcDesc.BufferCount; ++i) {
		result = _swapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i]));

		if (result != S_OK) {
			return -1;
		}

		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}


	// フェンスの作成
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;

	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hwnd, SW_SHOW);


	D3D12_HEAP_PROPERTIES heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;


	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;


	D3D12_RESOURCE_DESC resdesc = {};

	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices);
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	//WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	result = LoadFromWICFile(
		L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg
	);

	auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_RESOURCE_DESC resDesc = {};
	//resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//resDesc.Width = 256;
	//resDesc.Height = 256;
	//resDesc.DepthOrArraySize = 1;
	//resDesc.SampleDesc.Count = 1;
	//resDesc.SampleDesc.Quality = 0;
	//resDesc.MipLevels = 1;
	//resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	//resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		//texturedata.data(),
		//sizeof(TexRGBA) * 256,
		//sizeof(TexRGBA) * texturedata.size(),
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	//descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	//descHeapDesc.NodeMask = 0;
	//descHeapDesc.NumDescriptors = 1;
	//descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	//result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap));

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	// 定数バッファの作成(d3dx12.h使用)
	ID3D12Resource* constBuff = nullptr;


	XMMATRIX matrix = XMMatrixIdentity();

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(matrix) + 0xff) & ~0xff);

	_dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	XMMATRIX* mapMatrix;

	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateShaderResourceView(
		texbuff,
		&srvDesc,
		basicHeapHandle
	);

	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;

	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	//XMFLOAT3* vertMap = nullptr;
	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(vertices);
	vbView.StrideInBytes = sizeof(vertices[0]);


	unsigned short indeces[] = {
		0,1,2,
		2,1,3
	};

	ID3D12Resource* idxBuff = nullptr;

	resdesc.Width = sizeof(indeces);

	result = _dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);


	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indeces), std::end(indeces), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indeces);

	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_vsBlob,
		&errorBlob);

	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_psBlob,
		&errorBlob);

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{ // uv
			"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
	};

	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);
	*mapMatrix = matrix;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = nullptr;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// アンチエイリアス系の処理（今は使用しない）
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC	renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof(inputLayout);

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	ID3D12RootSignature* rootsignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	//D3D12_DESCRIPTOR_RANGE descTblRange = {};
	//descTblRange.NumDescriptors = 1;
	//descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	//descTblRange.BaseShaderRegister = 0;
	//descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};
	descTblRange[0].NumDescriptors = 1;
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange[0].BaseShaderRegister = 0;
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 0;
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam[2] = {};
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootparam[0].DescriptorTable.NumDescriptorRanges = 1;
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 1;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 2;

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;

	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);

	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature)
	);
	rootSigBlob->Release();

	gpipeline.pRootSignature = rootsignature;
	ID3D12PipelineState* _pipelinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

	D3D12_VIEWPORT viewport = {};

	viewport.Width = 1280;
	viewport.Height = 720;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorrect = {};

	scissorrect.top = 0;
	scissorrect.left = 0;
	scissorrect.right = scissorrect.left + 1280;
	scissorrect.bottom = scissorrect.top + 720;

	if (result != S_OK) {
		return -1;
	}

	MSG msg = {};
	unsigned int frame = 0;
	// メインループの定義
	{

		while (true) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}

			if (msg.message == WM_QUIT) {
				break;
			}

			auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

			// リソースバリア
			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
			BarrierDesc.Transition.Subresource = 0;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			_cmdList->ResourceBarrier(1, &BarrierDesc);

			_cmdList->SetPipelineState(_pipelinestate);

			auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
			rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

			// 画面のクリア
			float r, g, b;
			r = (float)(0xff & frame >> 16) / 255.0f;
			g = (float)(0xff & frame >> 8) / 255.0f;
			b = (float)(0xff & frame >> 0) / 255.0f;
			float clearColor[] = { r,g,b,1.0f };//黄色
			_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
			++frame;


			_cmdList->RSSetViewports(1, &viewport);
			_cmdList->RSSetScissorRects(1, &scissorrect);
			_cmdList->SetGraphicsRootSignature(rootsignature);
			_cmdList->SetDescriptorHeaps(1, &basicDescHeap);
			_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

			auto heapHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();
			heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);


			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers(0, 1, &vbView);
			_cmdList->IASetIndexBuffer(&ibView);

			// _cmdList->DrawInstanced(4, 1, 0, 0);
			_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			_cmdList->ResourceBarrier(1, &BarrierDesc);

			//命令のクローズ
			_cmdList->Close();


			ID3D12CommandList* cmdlists[] = { _cmdList };

			_cmdQueue->ExecuteCommandLists(1, cmdlists);

			// cmdQueueの実行完了を待つ処理
			_cmdQueue->Signal(_fence, ++_fenceVal);

			if (_fence->GetCompletedValue() != _fenceVal)
			{
				auto event = CreateEvent(nullptr, false, false, nullptr);
				_fence->SetEventOnCompletion(_fenceVal, event);
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}

			_cmdAllocator->Reset();
			_cmdList->Reset(_cmdAllocator, nullptr);

			// フリップ
			_swapChain->Present(1, 0);

		}
	}
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;

}