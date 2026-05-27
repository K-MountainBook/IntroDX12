#include <tchar.h>
#include <vector>

#include <Windows.h>
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

using namespace std;
using namespace DirectX;

static int window_width = 1280;
static int window_height = 720;

// デバイスオブジェクト
ID3D12Device* _dev = nullptr;
// DirectXよりハードウェアに近い場所をいじるため/数字はバージョンみたいなもの
IDXGIFactory6* _dxgiFactory = nullptr;
// スワップチェイン
IDXGISwapChain4* _swapChain = nullptr;

// コマンドアロケータ
ID3D12CommandAllocator* _cmdAllocator = nullptr;
// コマンドリスト
ID3D12GraphicsCommandList* _cmdList = nullptr;
// コマンドキュー
ID3D12CommandQueue* _cmdQueue = nullptr;

// 頂点シェーダ用
ID3DBlob* vsBlob = nullptr;
// ピクセルシェーダ用
ID3DBlob* psBlob = nullptr;

//フィーチャーレベル配列
D3D_FEATURE_LEVEL levels[] =
{
	D3D_FEATURE_LEVEL_12_2,
	D3D_FEATURE_LEVEL_12_1,
	D3D_FEATURE_LEVEL_12_0,
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
};

// 三角形の頂点モデル
//XMFLOAT3 vertices[] = {
//	{-0.5f, -0.7f, 0.0f},
//	{ 0.0f,  0.7f, 0.0f},
//	{ 0.5f, -0.7f, 0.0f},
//};

//4角形
//XMFLOAT3 vertices[] = {
//	{-0.4f, -0.7f, 0.0f},
//	{-0.4f,  0.7f, 0.0f},
//	{ 0.4f, -0.7f, 0.0f},
//	{ 0.4f,  0.7f, 0.0f},
//};

// インデックスデータ
// 上の4角形配列をどの順番で使うかの添え字
unsigned short indeces[] = {
	0, 1, 2,
	2, 1, 3
};

// 頂点データの構造体を追加
struct Vertex {
	XMFLOAT3 pos;	// xyz座標
	XMFLOAT2 uv;	// uv座標
};

// xyz座標にuv座標も加えた構造体の配列を作る
Vertex vertices[] =
{
	{{-0.4f, -0.7f, 0.0f},{0.0f,1.0f}},
	{{-0.4f,  0.7f, 0.0f},{0.0f,0.0f}},
	{{ 0.4f, -0.7f, 0.0f},{1.0f,1.0f}},
	{{ 0.4f,  0.7f, 0.0f},{1.0f,0.0f}},
};

// テクスチャデータの作成
struct TexRGBA
{
	unsigned char R, G, B, A;
};

std::vector<TexRGBA> texturedata(256 * 256);

// コンソールにデバッグ情報を表示
void DebugOutputFormatString(const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

// ウィンドウプロシージャ
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wpram, LPARAM lparam) {
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wpram, lparam);
}

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


#ifdef _DEBUG
/// <summary>
/// デバッグの場合はmain関数
/// </summary>
int main() {
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	OutputDebugString(L"Show window test.");
	// ウィンドウクラスの生成、登録
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T("DX12Sample");
	w.hInstance = GetModuleHandle(0);
	RegisterClassEx(&w);

	RECT wrc = { 0, 0, window_width, window_height };
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

	// アダプタ列挙用
	vector<IDXGIAdapter*> adapters;
	// 特定のアダプタ格納用変数
	IDXGIAdapter* tmpAdapter = nullptr;
	//刺さっているグラフィックアダプタの列挙
#ifdef _DEBUG
	HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif // _DEBUG


	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		// 検出されたグラフィックアダプタを全て格納
		adapters.push_back(tmpAdapter);
	}

	for (IDXGIAdapter* adpt : adapters) {
		DXGI_ADAPTER_DESC aDesc = {};
		adpt->GetDesc(&aDesc);		// アダプタのオブジェクトを取得
		// アダプタの説明（名前）を取得
		wcout << L"video card name :" << aDesc.Description << endl;
		wstring strDesc = aDesc.Description;
		// NVIDIAのアダプタを探して格納。ループの終了
		if (strDesc.find(L"NVIDIA") != string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	// 生成可能バージョンを調査
	for (auto lv : levels) {
		if (D3D12CreateDevice(nullptr, lv, IID_PPV_ARGS(&_dev)) == S_OK) {
			featureLevel = lv;
			break;
		}
	}

#pragma region ランダムなテクスチャデータを作成（一時的な処理
	for (auto& rgba : texturedata) {
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255;
	}
#pragma endregion

#pragma region テクスチャ用画像のロード
	result = CoInitializeEx(0, COINIT_MULTITHREADED);

	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	result = LoadFromWICFile(
		L"img/textest.png",
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	auto img = scratchImg.GetImage(0, 0, 0);

#pragma endregion

	// コマンドリストの作成とコマンドアロケータ
	// コマンドリストはGPUに対する命令のインターフェイス
	// コマンドアロケータが本体。命令オブジェクトはアロケータにPushBackされていく。
	// 命令の例：
	// DrawInstanced:点やポリゴンを描画
	// DrawIndexedInstanced:インデックス付きの線やポリゴンを描写
	// ClearRenderTargetView:レンダーターゲットのクリア
	// Close:命令のクローズ等々
	// アロケータの作成
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	// コマンドリストの作成
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));
	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	// タイムアウトの設定（なし）
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// アダプタの設定（一つしか使わないので0）
	cmdQueueDesc.NodeMask = 0;
	// プライオリティ（優先度）の指定は無
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// タイプをコマンドリストと合わせる
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// キューの作成
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

	// スワップチェインの生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};

	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;

	// バックバッファの伸縮可能にする
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	// フリップ後はすぐに破棄する
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// 指定なし
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// ウィンドウ＜＞フルスクリーンの切り替えが可能
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		// 本来はQueryInterfaceなどを用いてIDXGISwapChaing4*への変換チェックを行うが
		// 解りやすさ重視のためキャストを利用
		(IDXGISwapChain1**)&_swapChain
	);



	// レンダーターゲットビュー(RTV)
	// バックバッファに対してデータの書き込みを行う
	// バッファに格納されたデータの使い方を定義するのがビュー

	// DirectX12では、ビューはディスクリプタヒープから作成されるため
	// まずはディスクリプタヒープを作成する。
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// レンダーターゲットビュー
	heapDesc.NodeMask = 0;								// 複数のGPUがある場合に数値を指定
	heapDesc.NumDescriptors = 2;						// ディスクリプタの数を表す、
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;	// シェーダーから見える必要がないのでNONE

	ID3D12DescriptorHeap* rtvHeaps = nullptr;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	//Swapchain内のバッファとビューを紐づける処理
	DXGI_SWAP_CHAIN_DESC swcDesc = {};

	// これはバックバッファの値を取得する別解
	result = _swapChain->GetDesc(&swcDesc);

	// バックバッファの数ID3D12Resourceを作成
	vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);

	// SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// 先頭のアドレスを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// メインループの前だからここでバッファを二つ作る
	for (unsigned int idx = 0; idx < swcDesc.BufferCount; ++idx) {
		// スワップチェインの中身とID3D12Resourceを関連付ける
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		// サイズを取得してその分だけptrメンバーに加算してポインタを進める
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		// レンダービューの作成
		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			//nullptr,
			&rtvDesc,
			handle
		);
	}

	// フェンスの作成
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;

#pragma region 描画関連処理

#pragma region 頂点バッファの設定
	// 頂点ヒープ
	/*
	D3D12_HEAP_PROPERTIES heapProp = {};

	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;							// CPUからアクセスできるヒープを表す。Mapメソッドでアクセス可能
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;		// CPUのページング設定
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;		// メモリプールの場所を示す
	*/
	// d3dx12.hを使う様に変更
	ID3D12Resource* vertBuff = nullptr;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto verHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));


	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;	// バッファに利用するのでバッファを指定
	resDesc.Alignment = 0;									// 0で良い（らしい）
	resDesc.Width = sizeof(vertices);						// 幅で全てまかなうので頂点情報が入るサイズにする
	resDesc.Height = 1;										// 上記理由により1
	resDesc.DepthOrArraySize = 1;							// ここでは１でよい
	resDesc.MipLevels = 1;									// ここでは１でよい
	resDesc.Format = DXGI_FORMAT_UNKNOWN;					// 画像を利用しないのでUNKNOWNでよい
	resDesc.SampleDesc.Count = 1;							// 1とする
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;				// NONEでよい
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;		// メモリが最初から終わりまで連続していることを示す値を指定

	// ID3D12Resource* vertBuff = nullptr;

	//result = _dev->CreateCommittedResource(
	//	&heapProp,								// ヒープ構造体のアドレス
	//	D3D12_HEAP_FLAG_NONE,					// とりあえず指定なしを設定
	//	&resDesc,								// リソース構造体のアドレス
	//	D3D12_RESOURCE_STATE_GENERIC_READ,		// GPU側からは読み取り専用
	//	nullptr,								// 使わないのでnullptrを設定
	//	IID_PPV_ARGS(&vertBuff)					// vertBuffにデータを書き出し
	//);

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&verHeapDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

#pragma endregion

#pragma region 頂点バッファに対して頂点情報のコピー
	//XMFLOAT3* vertMap = nullptr;
	// 頂点データにuv座標を追加したため型の変更(chapter4)
	Vertex* vertMap = nullptr;

	result = vertBuff->Map(
		0,
		nullptr,
		(void**)&vertMap
	);

	copy(begin(vertices), end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);
#pragma endregion

#pragma region インデックスバッファーの作成
	ID3D12Resource* idxBuff = nullptr;

	// 頂点バッファーの設定を使いまわす（サイズ以外）
	resDesc.Width = sizeof(indeces);
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	copy(begin(indeces), end(indeces), mappedIdx);
	idxBuff->Unmap(0, nullptr);
#pragma endregion

#pragma region 頂点バッファビューの作成
	// 何バイトのデータが存在するのか？１頂点あたり何バイトなのか？などを知らせるデータ
	D3D12_VERTEX_BUFFER_VIEW vbView = {};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// バッファの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices);						// 総バイト数
	vbView.StrideInBytes = sizeof(vertices[0]);					// 1頂点あたりのバイト数
#pragma endregion

#pragma region 頂点データのレイアウトを設定
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,	// xyz座標は3つ必要
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		//シェーダーの情報追加
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,		// UV座標は二つしか必要ない
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};
#pragma endregion

#pragma region インデックスバッファビューの作成
	D3D12_INDEX_BUFFER_VIEW ibView = {};

	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indeces);
#pragma endregion

#pragma region テクスチャバッファの作成
	// WriteToSubresourceで転送するためのヒープの設定

	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;	// 転送はL0、CPU側から直接行う
	heapProp.CreationNodeMask = 0;		// 単一アダプタのため0
	heapProp.VisibleNodeMask = 0;		// 単一アダプタのため0

	// ノイズ版
	/*
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 256;
	resDesc.Height = 256;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	*/
	// 画像テクスチャ版
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
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

	ID3D12Resource* texBuff = nullptr;

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		// テクスチャ用を指定
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	// ノイズ版
	/*
	result = texBuff->WriteToSubresource(
		0,
		nullptr,
		texturedata.data(),
		sizeof(TexRGBA) * 256,					// 1ラインのサイズ
		sizeof(TexRGBA) * texturedata.size()	// 全サイズ
	);
	*/
	// 画像テクスチャ版
	result = texBuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,		// 元データのアドレス
		img->rowPitch,		// 1ラインのサイズ
		img->slicePitch		// 1枚のサイズ
	);
	/* ここまででテクスチャVRAMに転送完了 */
#pragma endregion


#pragma region 定数バッファの作成

	// 例
	XMMATRIX matrix = XMMatrixIdentity();

	ID3D12Resource* constBuff = nullptr;

	auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto constResDescBuff = CD3DX12_RESOURCE_DESC::Buffer((sizeof(matrix) + 0xff) & ~0xff);

	_dev->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constResDescBuff,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	XMMATRIX* mapMatrix;

	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);
	*mapMatrix = matrix;

#pragma endregion

#pragma region ディスクリプタヒープ関連処理

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダーから見えるようにする
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;									// ビューは今のところ一つだけ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダーリソースビュー用

	// 生成
	result = _dev->CreateDescriptorHeap(
		&descHeapDesc,
		IID_PPV_ARGS(&texDescHeap)
	);

	// 上記で作成したディスクリプタヒープ上にシェーダーリソースビューを作成する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;	// rgbaを正規化→画像読み込みの際は画像のメタデータに合わせる
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;		// RGBAをどのようにマッピングするか指定する
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;		// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	_dev->CreateShaderResourceView(
		texBuff,							// ビューと関連付けるバッファ
		&srvDesc,							// テクスチャ設定情報
		texDescHeap->GetCPUDescriptorHandleForHeapStart()		// ヒープのどこに割り当てるか
	);

#pragma endregion


#pragma region シェーダーファイルの読み込み等
	// エラー用
	ID3DBlob* errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"VertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);

	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			OutputDebugStringA("ファイルが見当たりません。");
			return -1;
		}
		else {
#if _DEBUG
			// シェーダエラー表示処理
			string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
#endif
		}
	}

	result = D3DCompileFromFile(
		L"PixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		&errorBlob
	);

	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			OutputDebugStringA("ファイルが見当たりません。");
			return -1;
		}
		else {
#if _DEBUG
			// シェーダエラー表示処理
			string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
#endif
		}
	}
#pragma endregion

#pragma region ディスクリプタテーブルレンジの作成
	D3D12_DESCRIPTOR_RANGE descTblRange = {};

	descTblRange.NumDescriptors = 1;		// テクスチャの数（今回は1）
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;		// 種類はテクスチャ
	descTblRange.BaseShaderRegister = 0;				// テクスチャレジスタ番号0(t0)
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;	// とりあえず「連続したディスクリプタレンジが前の直後に来る」を指定

#pragma endregion

#pragma region ルートパラメータの作成
	D3D12_ROOT_PARAMETER rootParam = {};

	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;				// ピクセルシェーダから見ることができる

	rootParam.DescriptorTable.pDescriptorRanges = &descTblRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;

#pragma endregion

#pragma region サンプラーの作成
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 横（U)方向の繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 縦（V)方向の繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 奥（W)方向の繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;		// ボーダーカラーは黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;		// 線形補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;		// ミップマップの最大値
	samplerDesc.MinLOD = 0.0f;					// ミップマップの最小値
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;		// ピクセルシェーダから見えるようにする
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;			// リサンプリングしない
#pragma endregion

#pragma region パイプラインの作成
	ID3D12RootSignature* rootsignature = nullptr;

#pragma region ルートシグネチャ
	// ルートシグネチャとは頂点情報以外のデータをグラフィックスパイプラインの外からシェーダーに送り込む仕組み
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	rootSignatureDesc.pParameters = &rootParam;
	rootSignatureDesc.NumParameters = 1;

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	// バイナリコードの作成
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);

	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature)
	);

#pragma endregion

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = rootsignature;		// 後でセット

	//シェーダーのセット
	gpipeline.VS = { vsBlob->GetBufferPointer(),vsBlob->GetBufferSize() };
	gpipeline.PS = { psBlob->GetBufferPointer(),psBlob->GetBufferSize() };

	// サンプルマスクとラスタライザのセット
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;			// 定数(0xffffffff)

	gpipeline.RasterizerState.MultisampleEnable = false;		// アンチエイリアス使用しないのでOFF
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	// カリング無し
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;	// 中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;			// 深度クリッピング有効

	// ブレンドステートのセット
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;
	{
		renderTargetBlendDesc.BlendEnable = false;
		renderTargetBlendDesc.LogicOpEnable = false;
		renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// 入力レイアウトの設定
	gpipeline.InputLayout.pInputElementDescs = inputLayout;					// レイアウトの先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);				// レイアウト配列の要素数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;	// カット無
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;	// 三角形で校正

	// レンダーターゲットの設定
	gpipeline.NumRenderTargets = 1;				// 数は一つ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;	// 上記指定のレンダーターゲットに対して指定

	// アンチエイリアスの設定 とりあえず最低
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	// グラフィックスパイプラインステートオブジェクトの生成
	ID3D12PipelineState* _piplinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(
		&gpipeline,
		IID_PPV_ARGS(&_piplinestate)
	);
#pragma endregion
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	D3D12_VIEWPORT viewPort = {};

	viewPort.Width = window_width;
	viewPort.Height = window_height;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;

	D3D12_RECT scissorRect = {};

	scissorRect.top = 0;
	scissorRect.left = 0;
	scissorRect.right = scissorRect.left + window_width;
	scissorRect.bottom = scissorRect.top + window_height;

	// チェック用
	if (FAILED(result)) {
		return -1;
	}

	// ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	EnableDebugLayer();
#endif

	MSG msg = {};

#pragma region mainloop
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		// 次に表示するバッファのインデックスが取得出来ればOK
		UINT bbIdx = _swapChain->GetCurrentBackBufferIndex();


		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		// バックバッファが書き込み可能になるまで待つ

		/*
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;			// バリアはリソースの遷移に対して行う
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx];				// 現在のバックバッファのインデックス（描画ターゲット）
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;		// 遷移前はPRESENT
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;	// 遷移後はRENDER_TARGET
		_cmdList->ResourceBarrier(1, &BarrierDesc);
		*/
		// CD3DX12ヘルパーを使う場合
		auto resBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		_cmdList->ResourceBarrier(
			1,
			&resBarrier
		);

		// 取得したインデックスのビューを利用するレンダーターゲットとしてセットする
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		// レンダーたーげとを設定する
		_cmdList->OMSetRenderTargets(
			1,				// レンダーターゲットの数
			&rtvH,			// レンダーターゲットのハンドル
			true,			// レンダーターゲットが複数の際に連続しているか
			nullptr			// 深度ステンシルバッファビューのハンドル
		);

		// レンダーターゲットを特定の色でクリア
		float clearColor[] = { 0.0f, 0.0f ,0.5f ,1.0f };		// 黄色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// パイプラインステートの指定
		_cmdList->SetPipelineState(_piplinestate);
		// ルートシグネチャの指定
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// ディスクリプタヒープのセット
		_cmdList->SetDescriptorHeaps(1, &texDescHeap);

		// ルートパラメータとディスクリプタヒープの関連付け
		_cmdList->SetGraphicsRootDescriptorTable(
			0,			// ルートパラメータインデックス
			texDescHeap->GetGPUDescriptorHandleForHeapStart()	// ヒープアドレス
		);

		// ビューポート、シザー矩形の設定
		_cmdList->RSSetViewports(1, &viewPort);
		_cmdList->RSSetScissorRects(1, &scissorRect);
		// プリミティブトポロジの設定
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 頂点バッファの指定
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		// インデクスバッファの指定
		_cmdList->IASetIndexBuffer(&ibView);
		// 描画
		// _cmdList->DrawInstanced(6, 1, 0, 0);				// インデックス情報を使わない描画
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);			// インデクス情報を使う描画

		// バックバッファの書き込み完了を待つ
		// CD3DX12ヘルパーを使う場合これは不要
		//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//_cmdList->ResourceBarrier(1, &BarrierDesc);

		// コマンドリストは実行する前にクローズする必要がある
		_cmdList->Close();

		// 実行
		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);

		_cmdQueue->Signal(_fence, ++_fenceVal);

		while (_fence->GetCompletedValue() != _fenceVal) {
			;
		}

		// コマンドアロケータ、コマンドリストの後始末
		_cmdAllocator->Reset();
		_cmdList->Reset(_cmdAllocator, nullptr);

		// バッファの入れ替え
		_swapChain->Present(1, 0);
	}
#pragma endregion
	UnregisterClass(w.lpszClassName, w.hInstance);

}