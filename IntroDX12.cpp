/*CreateDescriptorHeap
* GPU上にディスクリプタヒープを作成する命令
*/


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


//Vertex vertices[] = {
//	{{ 0.4f, -0.7f, 0.0f},{0.0f,1.0f}},
//	{{-0.4f,  0.7f, 0.0f},{0.0f,0.0f}},
//	{{ 0.4f, -0.7f, 0.0f},{1.0f,1.0f}},
//	{{ 0.4f,  0.7f, 0.0f},{1.0f,0.0f}}
//};
//
//Vertex vertices[] = {
//	{{   0.0f, 100.0f, 0.0f},{0.0f,1.0f}},
//	{{   0.0f,   0.0f, 0.0f},{0.0f,0.0f}},
//	{{ 100.0f, 100.0f, 0.0f},{1.0f,1.0f}},
//	{{ 100.0f,   0.0f, 0.0f},{1.0f,0.0f}}
//};

//Vertex vertices[] = {
//	{{  -1.0f, -1.0f, 0.0f},{0.0f,1.0f}},
//	{{  -1.0f,  1.0f, 0.0f},{0.0f,0.0f}},
//	{{   1.0f, -1.0f, 0.0f},{1.0f,1.0f}},
//	{{   1.0f,  1.0f, 0.0f},{1.0f,0.0f}},
//};


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

std::string GetTexturePathFromModelAndTexPath(
	const std::string& modelPath,
	const char* texPath
)
{
	auto folderPath = modelPath.substr(0, modelPath.rfind('/') + 1);
	return folderPath + texPath;
}

std::wstring GetWideStringFromString(
	const std::string& str
)
{
	auto num1 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0
	);

	std::wstring wstr;
	wstr.resize(num1);

	auto num2 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		&wstr[0],
		num1
	);

	assert(num1 == num2);
	return wstr;
}

ID3D12Resource* LoadTextureFromFile(std::string& texPath) {
	//WIC テクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto result = LoadFromWICFile(
		GetWideStringFromString(texPath).c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	// テクスチャを扱う
	/*
	* ポリゴンにテクスチャを貼る手順
	* １．頂点情報にuv座標（正規化された座標）を追加する
	* ２．頂点シェーダーを書き換える
	* ３．ピクセルシェーダを書き換える
	* ４．ＣＰＵ側のメモリ上にテクスチャ用データを作成する
	* ５．ＧＰＵから利用できるように転送先バッファを作成する
	* ６．ＣＰＵからＧＰＵへテクスチャデータを転送るう
	* ７．ディスクリプタヒープを作成する
	* ８．ディスクリプタヒープ上にシェーダーリソースビューを作成する
	* ９．ルートシグネチャのディスクリプタテーブルに、テクスチャ用の設定を記載する
	* １０．描画の際にディスクリプタヒープをセットする。
	*/
	//テクスチャのヒープ作成
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	// ライトバックで行う
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	// 転送はL0側（CPU側）から行う
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	// アダプタが単一のため0でOK
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	// 画像をテクスチャとして使用する場合は画像のメタデータに合わせる
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;	// 画像フォーマット
	resDesc.Width = static_cast<UINT>(metadata.width);		// テクスチャの幅
	resDesc.Height = static_cast<UINT>(metadata.height);	// テクスチャの高さ
	resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);	// 2D配列でのサイズ
	resDesc.SampleDesc.Count = 1;		// アンチエイリアシング
	resDesc.SampleDesc.Quality = 0;		// クオリティ
	resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);	// ミップマップのレベル
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);	// 2Dテクスチャですよ
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	// レイアウトは決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;		// フラグ無

	// バッファ作成
	ID3D12Resource* texBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	// 画像データをGPUに転送する
	result = texBuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		static_cast<UINT>(img->rowPitch),
		static_cast<UINT>(img->slicePitch)
	);

	if (FAILED(result)) {
		return nullptr;
	}

	return texBuff;

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
		std::wcout << L"video card name: " << adesc.Description << std::endl;

		std::wstring strDesc = adesc.Description;

		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			_tmpAdapter = adpt;
			break;
		}

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

	// バッファーはそのままでは役割を果たせない。
	// 作られたバッファー（この場合画面スワップで使うバッファ）は2枚の切り替えは行うことができても、	
	// 中身を書き換えることができない。
	// 書き換えるためにはRTV（レンダーターゲットビュー）が必要

	// バッファーとビュー
	// バッファー：CPUやGPUとの様々なやり取りに使われるメモリ領域
	// 頂点バッファ・インデックスバッファ・定数バッファ・テクスチャバッファ・深度バッファ
	// 作成したバッファの使い方を定義するのが「ビュー」。幾つかのバッファではビューが必須

	// バッファ：データそのもの、ビュー：その解釈
	// ディスクリプタ：GPUリソースの用途や使用法について説明しているデータ
	// →各種ビューに加えて、サンプラーというものを包括する
	// SRV:シェーダリソースビュー
	// CBV:定数バッファービュー
	// UAV:コンピュートシェーダーで使用するバッファーのビュー（UnorderedAccessVies)
	// Sampler:産婦らについての説明
	// RTV:レンダーターゲットビュー
	// DSV:深度ステンシルビュー

	// ディスクリプタヒープ
	// 各種ビューとサンプラを包括する概念だが、単体で使用されることはない。
	// 複数のディスクリプタを格納するためのメモリ領域。予め確保しておく必要がある。

	//ディスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

	// Discriptorタイプ
	// D3D12_DESCRIPTOR_HEAP_TYPE_RTV // レンダービュー
	// D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV // 定数・テクスチャ・コンピュートシェーダ用バッファ
	// D3D12_DESCRIPTOR_HEAP_TYPE_DSV // 深度ステンシルビュー
	// NodeMask
	// 複数のGPUがある場合に識別を行うためのビットフラグ。一つだけの場合は0をセット
	// NumDiscriptor
	// ディスクリプタの数を表す。ダブルバッファリングの場合は2
	// Flags
	// ビューにあたる情報をシェーダーから参照する必要があるかどうか？
	// SRVやCBV（テクスチャバッファや定数バッファ）であれば見える必要があるが、
	// RTVは見える必要がないのでここではNONE

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
	// CreateRenderTargetViewはスワップチェインの個数分実施しなきゃいけないのでループを使う
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapChain->GetDesc(&swcDesc);

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	for (size_t i = 0; i < swcDesc.BufferCount; ++i) {
		// スワップチェインの中のバッファーと、ビューの関連付けを行う。
		// スワップチェイン内のメモリはGetBuffer()で行う。
		// 第1引数がバッファのインデックス（何枚目か）
		// 第2引数の_backBuffers[i]にスワップチェイン内のバックバッファを取得する
		result = _swapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i]));

		// 第3引数はディスクリプタヒープ上のビューのアドレスに様なもの。
		// ptrメンバを持っていてアドレスが入っている。
		// 実際にRTVを作成する場合には0番目と1番目のディスクリプタを取得する必要があるため
		// ポインタをずらす必要がある。外側で先頭アドレスを取得し、ループ事にptrを進めている。
		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, handle);
		// 引数に指定しているD3D12_DESCRIPTOR_HEAP_TYPE_RTVを指定することで、種類に応じたいサイズを得られる
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}


	/*深度バッファ
	* 1.深度バッファの作成
	* 2.深度バッファビューの作成
	* 3.パイプラインステートオブジェクトへ深度バッファ設定の追加
	* 4.深度バッファビューとレンダーターゲットとの関連付け（毎フレーム行う）
	* 5.深度バッファービューのクリア（毎フレーム行う）
	*/
	// 深度バッファの作成
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// 2次元テクスチャデータですよ
	depthResDesc.Width = 1280;				// レンダーターゲットと同じサイズ
	depthResDesc.Height = 720;				// レンダーターゲットと同じサイズ
	depthResDesc.DepthOrArraySize = 1;		//テクスチャ配列でも3Dテクスチャでもないので1個
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;	// 深度値は深度に 32 ビットをサポートする単一コンポーネントの 32 ビット浮動小数点形式
	depthResDesc.SampleDesc.Count = 1;			// サンプルは1ピクセルあたり1
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// デプスステンシルとして使用

	// 深度用ヒーププロパティ
	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;	// DEFAULTに設定するならあとはUNKNOWNで良い
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// クリアバリュー
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;			// 深度1.0（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;		// 32bitのfloat値でクリア

	ID3D12Resource* depthBuffer = nullptr;
	// 深度バッファーの作成
	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer)
	);

	// 深度用のビューを作成する
	// 深度用ディスクリプタヒープ
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;				// 深度ビューは一つだけ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	// 深度ビューに使うという指定をする
	// ディスクリプタヒープの作成
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	result = _dev->CreateDescriptorHeap(
		&dsvHeapDesc,
		IID_PPV_ARGS(&dsvHeap)
	);

	// 深度ステンシルビューの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;		// 深度値32bitfloat
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	// 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;	// フラグは特になし
	// 深度ステンシルビューの作成
	_dev->CreateDepthStencilView(
		depthBuffer,
		&dsvDesc,
		dsvHeap->GetCPUDescriptorHandleForHeapStart()
	);

	// フェンスの作成
	// フェンスは「GPUの処理が完了したかどうか」を知れるだけで「待つ」仕組みではない。
	// 完了するまで「待つ」という処理が必要。
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;

	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	ShowWindow(hwnd, SW_SHOW);

	// PMDヘッダ構造
	struct PMDHeader {
		float version;
		char model_name[20];
		char comment[256];
	};

#pragma pack(1)

	// PMDマテリアル構造体
	struct PMDMaterial {
		XMFLOAT3 diffuse;	// ディフューズカラー
		float alpha;		// ディフューズのα
		float specularity;	// 鏡面反射の強さ
		XMFLOAT3 specular;	// 鏡面反射の色
		XMFLOAT3 ambient;	// 環境光色
		unsigned char toonIdx;	// トゥーン番号
		unsigned char edgeFlg;	// マテリアルごとの輪郭線フラグ

		// ここに2バイトのパディング（空き）がある。

		unsigned int indicesNum;	// このマテリアルが割り当てられるインデックス数
		char texFilePath[20];		// テクスチャファイルパス+α
	}; // 計72バイト

#pragma pack()

	struct MaterialForHlsl
	{
		XMFLOAT3 diffuse;
		float alpha;
		XMFLOAT3 specular;
		float specularity;
		XMFLOAT3 ambient;
	};

	struct AdditionalMaterial
	{
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};

	struct Material
	{
		unsigned int indecesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};



	char signature[3] = {};
	PMDHeader pmdHeader = {};
	FILE* fp;

	// auto err = fopen_s(&fp, "Model/初音ミク.pmd", "rb");
	std::string strModelPath = "Model/初音ミク.pmd";
	auto fopen_err = fopen_s(&fp, strModelPath.c_str(), "rb");

	if (fopen_err != 0) {
		return -1;
	}

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdHeader, sizeof(pmdHeader), 1, fp);

	// 頂点1つあたりのバッファサイズ
	constexpr size_t pmdvertex_size = 38;
	unsigned int vertNum;

	// 頂点情報の読み込み
	fread(&vertNum, sizeof(vertNum), 1, fp);

	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);
	fread(vertices.data(), vertices.size(), 1, fp);

	// 頂点インデクスデータの読み込み
	// 頂点インデックスとはポリゴンの頂点を結ぶ順の配列
	// 重なる頂点は常に同じ値（座標）にしなければいけない。重なる点を配列で格納している。
	// 頂点を決めた後に、順にポリゴンを貼る。その貼り方が「頂点インデックス」
	std::vector<unsigned short> indices;

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);
	indices.resize(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);



	// マテリアルデータの読み込み
	unsigned int materialNum;	//マテリアル数
	fread(&materialNum, sizeof(materialNum), 1, fp);
	std::vector<Material> materials(materialNum);

	// マテリアルデータを配列へコピー
	std::vector<ID3D12Resource*> textureResources(materialNum);
	{
		std::vector<PMDMaterial> pmdMaterials(materialNum);
		fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

		for (int i = 0; i < pmdMaterials.size(); ++i) {
			materials[i].indecesNum = pmdMaterials[i].indicesNum;
			materials[i].material.diffuse = pmdMaterials[i].diffuse;
			materials[i].material.alpha = pmdMaterials[i].alpha;
			materials[i].material.specular = pmdMaterials[i].specular;
			materials[i].material.specularity = pmdMaterials[i].specularity;
			materials[i].material.ambient = pmdMaterials[i].ambient;
		}

		for (int i = 0; i < pmdMaterials.size(); ++i) {
			if (strlen(pmdMaterials[i].texFilePath) == 0)
			{
				textureResources[i] = nullptr;
				continue;
			}
			// モデルのテクスチャパスからアプリケーションからのテクスチャパスを得る
			auto texFilePath = GetTexturePathFromModelAndTexPath(
				strModelPath,
				pmdMaterials[i].texFilePath
			);
			textureResources[i] = LoadTextureFromFile(texFilePath);
		}


	}
	fclose(fp);

#pragma region 画像をテクスチャにする場合はコメント解除
	// 画像をテクスチャにする場合はコメント解除
	////WICテクスチャのロード
	//TexMetadata metadata = {};
	//ScratchImage scratchImg = {};

	//result = LoadFromWICFile(
	//	L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg
	//);

	//auto img = scratchImg.GetImage(0, 0, 0);

	//ID3D12Resource* texbuff = nullptr;
	//result = _dev->CreateCommittedResource(
	//	&texHeapProp,
	//	D3D12_HEAP_FLAG_NONE,
	//	&resDesc,
	//	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		// 作成するバッファはテクスチャ（シェーダリソース）
	//	nullptr,
	//	IID_PPV_ARGS(&texbuff)
	//);

	//// この方法は比較的わかりやすいが特定の条件で効率が下がるらしい
	//result = texbuff->WriteToSubresource(
	//	0,					// サブリソースインデックス
	//	nullptr,			// 書き込み領域の指定（nullptrなら先頭から全域）
	//	img->pixels,		// 書き込みたいデータのアドレス
	//	img->rowPitch,		// 1行あたりのデータサイズ
	//	img->slicePitch		// スライス当たりのデータサイズ
	//);

#pragma endregion

	// 前提知識
	// この記事の読者は前提としてDirectX12の基礎知識を持っているものとします。記事が非常に長くなってしまうので、基本要素の詳細な説明は実施しません。
	// 具体的には、以下の知識は習得済みとして話を進めます。

	// DirectX12における「リソース」
	// リソースに、利用方法に関する情報を紐づけた「ディスクリプタ」
	// ディスクリプタをCPU、GPUの両方からアクセス可能なメモリ領域に格納する「ディスクリプタヒープ」
	// ディスクリプタヒープ上の連続したディスクリプタの範囲を示す「ディスクリプタレンジ」
	// ディスクリプタヒープ上のディスクリプタとシェーダのレジスタ番号とをマッピングする「ルートシグネチャ」
	// ディスクリプタレンジ情報をルートシグネチャに連携するための「ディスクリプタテーブル」

	// 画像をテクスチャにする場合はコメント解除
		//// テクスチャバッファの作成
	//D3D12_HEAP_PROPERTIES texHeapProp = {};
	//texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;	// 特殊な設定なのでCUSTOMにする
	//texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;	// ライトバック
	//texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;	// 転送はL0。CPU側から行う
	//texHeapProp.CreationNodeMask = 0;		// 単一のアダプタなので0
	//texHeapProp.VisibleNodeMask = 0;		// 単一のアダプタなので0

	//D3D12_RESOURCE_DESC resDesc = {};
	//// テクスチャのメタデータに合わせる
	//resDesc.Format = metadata.format;
	//resDesc.Width = metadata.width;
	//resDesc.Height = metadata.height;
	//resDesc.DepthOrArraySize = metadata.arraySize;
	//resDesc.SampleDesc.Count = 1;
	//resDesc.SampleDesc.Quality = 0;
	//resDesc.MipLevels = metadata.mipLevels;
	//resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	//resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	//resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	//auto img = scratchImg.GetImage(0, 0, 0);

	//ID3D12Resource* texbuff = nullptr;
	//result = _dev->CreateCommittedResource(
	//	&texHeapProp,
	//	D3D12_HEAP_FLAG_NONE,
	//	&resDesc,
	//	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		// 作成するバッファはテクスチャ（シェーダリソース）
	//	nullptr,
	//	IID_PPV_ARGS(&texbuff)
	//);
	//// この方法は比較的わかりやすいが特定の条件で効率が下がるらしい
	//result = texbuff->WriteToSubresource(
	//	0,					// サブリソースインデックス
	//	nullptr,			// 書き込み領域の指定（nullptrなら先頭から全域）
	//	img->pixels,		// 書き込みたいデータのアドレス
	//	img->rowPitch,		// 1行あたりのデータサイズ
	//	img->slicePitch		// スライス当たりのデータサイズ
	//);

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダーから見えるように
	descHeapDesc.NodeMask = 0;											// マスクは0
	descHeapDesc.NumDescriptors = 2;									// SRVとCBV1ずつで２個に設定（chapter6）
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダリソースビュー用

	// ディスクリプタヒープを生成
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));
	// ディスクリプタヒープ内（VRAM上）の先頭ハンドルを取得している
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();


	XMMATRIX matrix = XMMatrixRotationY(XM_PIDIV4);
	auto worldMat = XMMatrixRotationY(XM_PIDIV4);

	XMFLOAT3 eye(0, 10, -15);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);

	// シェーダーに渡す行列データ
	struct MatricesData {
		XMMATRIX world;		// ワールド座標
		XMMATRIX viewproj;	// ビューとプロジェクションの合成行列
	};

	matrix *= XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up)
	);

	matrix *= XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(1280) / static_cast<float>(720),
		1.0f,
		100.0f
	);

	auto viewMat = XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up)
	);

	auto projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(1280) / static_cast<float>(720),
		1.0f,
		100.0f
	);

	/* CPUで設定した行列をGPU側に転送するために「定数バッファー」を使用する。
	* １．定数バッファを作成する
	* ２．定数バッファの中身をMapで書き換える
	* ３．定数バッファビューをディスクリプタヒープに追加する
	* ４．ルートシグネチャに定数バッファー参照用のレンジ設定を追加する
	* ５．シェーダーから利用する
	*/
	// 定数バッファの作成(d3dx12.h使用)
	ID3D12Resource* constBuff = nullptr;

	auto conHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);	// Mapで書き換える必要があるのでUPLOAD
	// 256バイトにする必要がある
	// MatricesDataはワールド変換行列とビュープロジェクション行列を格納した構造体
	auto conResDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xff);
	// バッファの作成
	_dev->CreateCommittedResource(
		&conHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&conResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	MatricesData* mapMatrix;		// マップ先を示すポインタ

	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);	// マップ
	mapMatrix->world = worldMat;								// データのコピー～
	mapMatrix->viewproj = viewMat * projMat;					// データのコピー～
	constBuff->Unmap(0, nullptr);								// アンマップ


	// 画像をテクスチャにする場合はコメント解除
	//// シェーダーリソースビューの生成
	//_dev->CreateShaderResourceView(
	//	texbuff,	// ビューと関連付けるバッファ
	//	&srvDesc,	// 設定したテクスチャ設定情報
	//	basicHeapHandle	// ヒープのどこに割り当てるか
	//);

	// この時点でのbascHeapHandleのptrにはビューを配置すべき先頭アドレスが入っている
	// SRVもCBVも(UAVも)同じ長さなので、GetDescriptorHandleIncrimentSizeを足しこむことで
	// 次のビューの先頭アドレスを取得することができる
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	// 定数バッファビューの作成
	_dev->CreateConstantBufferView(
		&cbvDesc,			// 定数バッファビューの設定 
		basicHeapHandle		// ビューを配置する場所のハンドル
	);

	// 頂点バッファの作成
	// ID3D12Resource自体はC言語のmalloc()（メモリ確保）に近いイメージ
	ID3D12Resource* vertBuff = nullptr;
	// 頂点データはCPU側から設定するので、D3D12_HEAP_TYPE_UPLOADを使用する。
	// D3D12_HEAP_TYPE_UPLOADはパフォーマンスが悪いので、頻繁にアクセスする場合は別設定を検討する必要がある。
	auto heapPropVer = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resoDescBuffVer = CD3DX12_RESOURCE_DESC::Buffer(vertices.size());

	result = _dev->CreateCommittedResource(
		&heapPropVer,
		D3D12_HEAP_FLAG_NONE,
		&resoDescBuffVer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	//XMFLOAT3* vertMap = nullptr;
	// Vertex* vertMap = nullptr;
	// 頂点情報をGPUにマップする
	unsigned char* vertMap = nullptr;
	// バッファーのアドレスを取得するための関数Map
	// CPU側でこのアドレス上のメモリに対して変更を行えば、GPU側にデータが反映される。
	result = vertBuff->Map(
		0,					// とりあえず0
		nullptr,			// 範囲の指定。全ての場合はnullptr
		(void**)&vertMap	// 受け取るためのポインタ変数のアドレス
	);

	// vertMapにCPU側で作った頂点情報をコピーしている。
	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	// マップの解除
	vertBuff->Unmap(0, nullptr);

	// 頂点バッファビューの作成
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	// 頂点バッファービューの初期化
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// 頂点バッファビューのアドレス
	vbView.SizeInBytes = vertices.size();						// 総バイト数
	vbView.StrideInBytes = pmdvertex_size;						// 1頂点あたりのバイト数

	// インデックスバッファ
	// ミクさんモデルはインデックスが多いためバッファを増量する
	ID3D12Resource* idxBuff = nullptr;
	auto idxHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto idxResoDesc = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));
	// バッファの作成
	result = _dev->CreateCommittedResource(
		&idxHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&idxResoDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	// 作ったバッファにインデックスデータをコピーする
	unsigned short* mappedIdx = nullptr;
	// バッファーのアドレスを取得するための関数Map
	// CPU側でこのアドレス上のメモリに対して変更を行えば、GPU側にデータが反映される。
	idxBuff->Map(
		0,
		nullptr,
		(void**)&mappedIdx
	);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	// インデックスバッファビューを作成
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;	//indexをunsigned shortで作ったのでR16UINT
	ibView.SizeInBytes = indices.size() * sizeof(indices[0]);
	// ibView.SizeInBytes = sizeof(indeces);

	// ID3DBlobオブジェクトは「何かのデータの塊」を示すためのポインタとサイズを持っている
	// それがどのようなデータなのかは使用する側が決定する。
	// Blob = Binaly Large Objectの略
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;


	// 頂点シェーダーをコンパイル
	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",				// ファイル名
		nullptr,								// シェーダーマクロオブジェクト
		D3D_COMPILE_STANDARD_FILE_INCLUDE,		// インクルードオブジェクト
		"BasicVS", "vs_5_0",					// エントリポイント・シェーダーの割り当て
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	// コンパイルオプション・デバッグ用/最適化スキップ
		0,										// エフェクトコンパイルオプション(0推奨)
		&_vsBlob,								// データ受け取り用のポインタ
		&errorBlob);							// エラー用のポインタ

	// ピクセルシェーダーをコンパイル
	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",				// ファイル名
		nullptr,								// シェーダーマクロオブジェクト
		D3D_COMPILE_STANDARD_FILE_INCLUDE,		// インクルードオブジェクト
		"BasicPS", "ps_5_0",					// エントリポイント・シェーダーの割り当て
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,	// コンパイルオプション・デバッグ用/最適化スキップ
		0,										// エフェクトコンパイルオプション(0推奨)
		&_psBlob,								// 受け取るポインタ
		&errorBlob								// エラー用ポインタ
	);

	//頂点レイアウト
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",									// セマンティクス名（シェーダーに出てくるやつ）
			0,											// 同じセマンティクス名がある場合に使うインデクス
			DXGI_FORMAT_R32G32B32_FLOAT,				// フォーマット（要素数とビット数）
			0,											// 入力スロットインデックス（GPUが頂点データを見るノゾキアナのようなもの。
			D3D12_APPEND_ALIGNED_ELEMENT,				// データのオフセット位置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,	// 入力データクラスを識別する値
			0											// 一度に描画するインスタンスの数
		},
		{
			"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{ // uv
			"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{
			"BONE_NO",0,DXGI_FORMAT_R16G16_UINT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{
			"WEIGHT",0,DXGI_FORMAT_R8_UINT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
		{
			"EDGE_FLG",0,DXGI_FORMAT_R8_UINT,0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0
		},
	};

	/*マテリアルバッファの作成とシェーダーへの転送・表示
	* 1.マテリアルデータ格納バッファーを作成する
	* 2.Mapを使い、必要なデータをコピーする。
	* 3.マテリアル用のディスクリプタヒープの作成
	* 4.ディスクリプタヒープ上にビューの作成をする
	* 5.マテリアル用ルートパラメータ設定を追加する
	* 6.シェーダーを書き換える
	*/
	// マテリアルバッファの作成
	auto materialBufferSize = sizeof(MaterialForHlsl);				//サイズの取得
	materialBufferSize = (materialBufferSize + 0xff) & ~0xff;		//サイズの調整

	// マテリアルバッファの確保
	ID3D12Resource* materialBuff = nullptr;
	auto matHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto matResDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBufferSize * materialNum);
	result = _dev->CreateCommittedResource(
		&matHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&matResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialBuff)
	);

	// マップマテリアルにバッファの内容をコピー
	char* mapMaterial = nullptr;			// char型なので8bit
	result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);	// マッピングする
	for (auto& m : materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;	// データコピー
		mapMaterial += materialBufferSize;	// 次のアライメントまで進める(+256bit)
	}
	materialBuff->Unmap(0, nullptr);

	// マテリアル用ディスクリプタヒープの作成
	ID3D12DescriptorHeap* materialDescHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = materialNum * 2;
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	// 実際の作成
	result = _dev->CreateDescriptorHeap(
		&materialDescHeapDesc,
		IID_PPV_ARGS(&materialDescHeap)
	);

	// マテリアルのビューを作成
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();	// バッファーのアドレス
	matCBVDesc.SizeInBytes = materialBufferSize;						// 256bit


	// シェーダーリソースビューの作成→テクスチャビュー
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;	//データのRGBAをどのようにマッピングするか。設定値はマクロで「指定されたフォーマットにデータ通りの順序で割り当てられている。
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;		//2Dのテクスチャである
	srvDesc.Texture2D.MipLevels = 1;							//ミップマップは使用しないので1

	// 開始地点を記録
	auto matDescHeapH = materialDescHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < materialNum; ++i) {
		// マテリアル用定数バッファビューの作成
		_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		// ポインタを進める
		matDescHeapH.ptr += incSize;
		// バッファのアドレスを進める
		matCBVDesc.BufferLocation += materialBufferSize;

		// シェーダリソースビューに対して値のセットを行う
		if (textureResources[i] != nullptr) {
			srvDesc.Format = textureResources[i]->GetDesc().Format;
		}
		else {
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		}

		_dev->CreateShaderResourceView(
			textureResources[i],
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += incSize;

	}

	// グラフィックスパイプライン
	// 頂点 > IA > VS(頂点シェーダ) > HS(ハルシェーダー） > TS > DS（ドメインシェーダ） > GS（ジオメトリシェーダ） > RS > PS > OM

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = nullptr;
	//gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	//gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	//gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	//gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
	// ↓短縮すると以下
	gpipeline.VS = { reinterpret_cast<UINT8*>(_vsBlob->GetBufferPointer()),_vsBlob->GetBufferSize() };
	gpipeline.PS = { reinterpret_cast<UINT8*>(_psBlob->GetBufferPointer()),_psBlob->GetBufferSize() };

	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// アンチエイリアス系の処理（今は使用しない）
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;		// カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;		// 中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;				// 深度クリッピング有効

	// ブレンドステート
	gpipeline.BlendState.AlphaToCoverageEnable = false;				// 今はfalseにする。αテストの有無
	gpipeline.BlendState.IndependentBlendEnable = false;			// レンダーターゲット事の設定

	D3D12_RENDER_TARGET_BLEND_DESC	renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;				// ブレンドをするか？（加算乗算α値など
	renderTargetBlendDesc.LogicOpEnable = false;			// 論理演算するかどうか
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;	// 書き込むときのマスク値（RとかGとかBとかαとか）

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// 入力レイアウトの設定
	gpipeline.InputLayout.pInputElementDescs = inputLayout;			// レイアウトの先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);		// レイアウトの要素数
	// _countofマクロ>全体サイズ÷要素サイズ

	// D3D12_INDEX_BUFFER_STRIP_CUT_VALUE列挙型を指定
	// トライアングルリスト or トライアングルストリップ
	// リストはそのまま、ストリップは接続された一連の三角形で頂点を繰り返し指定する必要がない
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;	// カット無し
	// メンバの構成要素が、点 or 線 or 三角形どれかを指定する。
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形

	// レンダーターゲットの設定
	gpipeline.NumRenderTargets = 1;								// 一つしか無いので1
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;		// 正規化されたRGBA

	// アンチエイリアスの指定
	gpipeline.SampleDesc.Count = 1;			// サンプリングは1ピクセルにつき1
	gpipeline.SampleDesc.Quality = 0;		// クオリティ最低

	// 深度バッファーのパイプラインステート設定
	gpipeline.DepthStencilState.DepthEnable = true;				// 深度バッファビューを使う
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;	// 書き込みを行う
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;	// 深度値が小さいほうを描画に使用する
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;	//深度値は32bitfloatを使いますよ

	// ルートシグネチャの実装
	// ルートシグネチャ：ディスクリプタテーブルをまとめたもの
	// 頂点情報以外のデータ（テクスチャや定数）をグラフィックスパイプラインの外からシェーダーに送り込む
	// 空のルートシグネチャを作成
	ID3D12RootSignature* rootsignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	// 頂点情報ありという意味の列挙子を設定
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	/*
	* ディスクリプタテーブルとは
	* ルートシグネチャを作成する際に設定する項目。
	* ティスクリプタヒープと名前が似ているが別物。シェーダーレジスタと関連した概念。
	* 定数バッファやテクスチャなどをCPU-GPU間でやりとりする際に、レジスタ種別とレジスタ番号をリソースに割り当てて
	* それをCPUとGPUの共通認識としてやり取りする。その指定のセットをまとめているのがディスクリプタテーブル
	* DX12ではディスクリプタテーブルの実態はルートパラメータ（D3D12_ROOT_PARAMETER構造体）というデータ。
	* ルートシグネチャ作成時にD3D12_ROOT_SIGNATURE_DESCオブジェクトにルートパラメータを設定して利用する。
	*/
	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {};
	// 定数用レジスタ1
	descTblRange[0].NumDescriptors = 1; // 定数1
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;//種別は定数
	descTblRange[0].BaseShaderRegister = 0;							// スロット番号
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	// 定数用レジスタ2
	descTblRange[1].NumDescriptors = 1;	// 定数2
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 1;							// スロット番号
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	//テクスチャ用レジスタ
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;	//種別はテクスチャ
	descTblRange[2].BaseShaderRegister = 0;							// スロット番号
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam[2] = {};

	/*ディスクリプタレンジとは
	* ヒープ上に種類が同じディスクリプタが連続して存在している場合にそれらをまとめて使用できるように
	* ディスクリプタレンジを使って指定する。
	* 例えば、3個のテクスチャバッファビューを作り、それをヒープ上で3つ連続させているとすると、
	* ディスクリプタレンジを使って「スロット0番から3つ分がテクスチャバッファー」と指定することができる。
	*/
	// シェーダーと一個目の定数をまとめてバインドする。
	rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;	// このルートパラメータはディスクリプタテーブルですよ
	rootparam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];			// ディスクリプタレンジのアドレス
	rootparam[0].DescriptorTable.NumDescriptorRanges = 1;						// ディスクリプタレンジ数
	rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;	//全てのシェーダから見える

	// 定数二つ目？？？？？？
	rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootparam[1].DescriptorTable.NumDescriptorRanges = 2;
	rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootSignatureDesc.pParameters = rootparam;
	rootSignatureDesc.NumParameters = 2;

	// サンプラーの設定
	/*サンプラーとは
	* uv値によってテクスチャデータからどのように色を取り出すか決めるための設定。
	* Filter：どう補間するか
	* AddressU：u方向アドレッシングモード
	* AddressV:v方向アドレッシングモード
	* AddressW:w方向アドレッシングモード
	* MipLODBias:計算結果ミップマップレベルからのオフセット
	* MaxAnisotropy:異方性フィルターの最大値
	* ComparisonFunc:比較関数
	* BorderColor:端の色
	* MinLOD:ミップマップ下限
	* MaxLOD:ミップマップ上限
	* ShaderRegister:スロット番号
	* RegisterSpace:レジスタスペース？（0でいいらしい）
	* ShaderVisibility:どのシェーダーから参照可能か
	*/
	D3D12_STATIC_SAMPLER_DESC samplerDesc[1] = {};
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc[0].MinLOD = 0.0f;
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	rootSignatureDesc.pStaticSamplers = samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ルートシグネチャのバイナリコードを作成
	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);
	// ルートシグネチャのオブジェクトを作成
	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature)
	);
	rootSigBlob->Release();

	// グラフィックスパイプラインステートにルートシグネチャを設定
	gpipeline.pRootSignature = rootsignature;
	ID3D12PipelineState* _pipelinestate = nullptr;
	// グラフィックスパイプラインステート
	result = _dev->CreateGraphicsPipelineState(
		&gpipeline,						// 各パラメータ設定
		IID_PPV_ARGS(&_pipelinestate)
	);

	// ビューポート
	// 画面（ウィンドウ）に対して、レンダリング結果をどのように表示するか。
	// 画面サイズ、深度
	D3D12_VIEWPORT viewport = {};
	viewport.Width = 1280;
	viewport.Height = 720;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;

	// シザー矩形の設定
	// ビューポートに出力された画像のどこからどこまでを実際に画面に出すか
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
	float angle = 0.0f;
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

			angle += 0.02f;

			worldMat = XMMatrixRotationY(angle);
			//*mapMatrix = worldMat * viewMat * projMat;
			mapMatrix->world = worldMat;

			// スワップチェインの現在「裏側」であるインデックスを取得することができる。
			auto bbIdx = _swapChain->GetCurrentBackBufferIndex();

			//// リソースバリア
			//// リソースに対する排他制御という意味で名前がつけられている
			//// →リソースの状態遷移をGPUに教えてあげるもの
			//D3D12_RESOURCE_BARRIER BarrierDesc = {};
			//BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;	// バリアの種別
			//BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;		// 特別な事をしなければNONE
			//// Transitionを使う(unionされたTRANSITION_BARRIER構造体
			//BarrierDesc.Transition.pResource = _backBuffers[bbIdx];		// リソースのアドレス（バックバッファーのアドレス）
			//BarrierDesc.Transition.Subresource = 0;						// サブリソース番号(ゼロ)。複数のサブリソースがある場合は変える
			//// 次のループも考慮してPRESENT状態にする
			//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;	// 元の状態
			//// GPUに「今からこのリソースをレンダーターゲットとして使う」と通知する
			//// PRESENT状態が終わるまでの間、レンダーターゲットが待機する。
			//// バリア実行後にレンダーターゲットとして使用中となる
			//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;	// 後の状態
			//// 使わない
			//// BarrierDesc.Aliasing;
			//// BarrierDesc.UAV;
			//_cmdList->ResourceBarrier(1, &BarrierDesc);

			// リソースバリアをd3dx12で作り直す。
			auto BarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx],
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			_cmdList->ResourceBarrier(1, &BarrierDesc);


			// パイプラインステートのセット
			_cmdList->SetPipelineState(_pipelinestate);
			// ルートシグネチャのセット
			_cmdList->SetGraphicsRootSignature(rootsignature);

			// ビューポートとシザーポートの内容をGPUへ
			_cmdList->RSSetViewports(1, &viewport);
			_cmdList->RSSetScissorRects(1, &scissorrect);

			// プリミティブトポロジ
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			// _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);　// 点のデータとして取り扱う

			// 頂点バッファをセット
			_cmdList->IASetVertexBuffers(
				0,		// スロット番号
				1,		// 頂点バッファビューの数
				&vbView	// 頂点バッファビューの配列
			);
			_cmdList->IASetIndexBuffer(&ibView);


			// レンダーターゲットのアドレス取得
			auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
			// 進めるポインタ数を計算して加算
			rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			// レンダーターゲットと深度バッファービューの関連付け
			// 深度ステンシルバッファビューのアドレス取得
			auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
			// 第4引数に深度バッファビューを指定することでレンダーターゲット書き込み時に同時に深度バッファに書き込む
			_cmdList->OMSetRenderTargets(
				1,				// レンダーターゲット数 
				&rtvH,			// レンダーターゲットのハンドルの先頭アドレス
				true,			// 複数時に連続しているか？ →14章のマルチレンダターゲット
				&dsvH);			// 深度ステンシルバッファービューのハンドル
			// 毎フレーム深度バッファビューはクリアしないとダメ
			_cmdList->ClearDepthStencilView(
				dsvH,						// 深度ステンシルビューのハンドル
				D3D12_CLEAR_FLAG_DEPTH,		// クリア先の指定
				1.0f,						// 深度クリア時の値
				0,							// ステンシルクリア時の値
				0,							// クリア範囲のサイズ
				nullptr						// クリア範囲の配列
			);

			// 画面のクリア
			float r, g, b;
			r = (float)(0xff & frame >> 16) / 255.0f;
			g = (float)(0xff & frame >> 8) / 255.0f;
			b = (float)(0xff & frame >> 0) / 255.0f;
			float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };//黄色
			// レンダーターゲットを指定された色でクリア（塗りつぶす）
			_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
			++frame;

			// 二つのヒープを読み込む
			ID3D12DescriptorHeap* heaps[] = { basicDescHeap,materialDescHeap };
			_cmdList->SetDescriptorHeaps(2, heaps);
			// _cmdList->SetDescriptorHeaps(1, &basicDescHeap);
			// シェーダーリソースビュー→ルートパラメータ番号0に対して、ディスクリプタヒープの場所をバインド
			// 第一引数の数字はルートパラメータのインデックス番号
			// このプログラムだと、定数レジスタ一つ目
			_cmdList->SetGraphicsRootDescriptorTable(0, basicDescHeap->GetGPUDescriptorHandleForHeapStart());

			// マテリアルヒープの先頭を取得する
			auto materialH = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
			// 最初はオフセット無し
			unsigned int idxOffset = 0;

			// これを実行するとミクさんが黒くなる
			// _cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);

			auto cbvsrvIncSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2;
			for (auto& m : materials) {
				// 第一引数の数字はルートパラメータのインデックス番号
				// このプログラムだと、定数レジスタ二つ目
				_cmdList->SetGraphicsRootDescriptorTable(1, materialH);
				_cmdList->DrawIndexedInstanced(m.indecesNum, 1, idxOffset, 0, 0);
				// ヒープポインタとインデックスを先に進める
				materialH.ptr += cbvsrvIncSize;
				idxOffset += m.indecesNum;
			}

			//auto heapHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();
			//heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			//_cmdList->SetGraphicsRootDescriptorTable(1, heapHandle);

			// DrawInstancedでも描画できる（引数が違う）
			// _cmdList->DrawInstanced(
			// 4,	// 頂点数
			// 1,	// インスタンス数
			// 0,	// 頂点データオフセット
			// 0	// インスタンスオフセット
			// );
			// _cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
			// _cmdList->DrawInstanced(vertNum, 1, 0, 0);
			// 全頂点を描画する
			// _cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);

			// レンダーターゲットの状態を入れ替える
			//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			// 第1引数：設定バリアの数
			// 第2引数：設定バリア構造体のアドレス
			//_cmdList->ResourceBarrier(1, &BarrierDesc);	// バリア指定の実行
			// ↑のコマンドをd3dx12で書き直した場合
			BarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx],
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
			_cmdList->ResourceBarrier(1, &BarrierDesc);


			// コマンドキューのクローズ
			_cmdList->Close();


			// コマンドの実行
			ID3D12CommandList* cmdlists[] = { _cmdList };
			_cmdQueue->ExecuteCommandLists(1, cmdlists);

			// cmdQueueの実行完了を待つ処理
			// 第1引数：フェンスオブジェクト
			// 第2引数：GPUの処理か完了した後になっているべき値（フェンス値）
			_cmdQueue->Signal(_fence, ++_fenceVal);

			// フェンスからGPUの処理の完了が来るまで待つ（ビジーループ）
			// GetCompletedValue()はGPUの処理が終わったときに更新される、
			// フェンス値（Signal()の第2引数で渡した値）を返す
			// CPU側でインクリメントしたフェンス値とGetComp（略）の」戻り値が一致すれば
			// 適切な同期処理が完了したと言える
			if (_fence->GetCompletedValue() != _fenceVal)
			{
				// Windowsイベントの作成
				auto event = CreateEvent(nullptr, false, false, nullptr);
				// フェンス値が第1引数で与えられた値になった際に、第2引数のイベントを通知する
				_fence->SetEventOnCompletion(_fenceVal, event);
				// イベントが発生するまで待ち続ける処理期間は無限(INFINITE)
				WaitForSingleObject(event, INFINITE);
				CloseHandle(event);
			}

			// 画面のスワップ
			// 第1引数は垂直同期を待つとか待たないとかなんとか
			// 第2引数はDXGI_PRESENT定数を使用して様々な指定を行うらしい。
			_swapChain->Present(1, 0);

			// キューのクリアと次のフレームでまたコマンドを溜める準備
			_cmdAllocator->Reset();
			_cmdList->Reset(_cmdAllocator, _pipelinestate);

		}
	}
	UnregisterClass(w.lpszClassName, w.hInstance);
	return 0;

}