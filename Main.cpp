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
//unsigned short indeces[] = {
//	0, 1, 2,
//	2, 1, 3
//};

// 頂点データの構造体を追加
struct Vertex {
	XMFLOAT3 pos;	// xyz座標
	XMFLOAT2 uv;	// uv座標
};

// PMDヘッダ構造体
struct PMDHeader
{
	float version;
	char model_name[20];
	char comment[256];
};


// PMD頂点用構造体
struct PDMVertex
{
	XMFLOAT3 pos;		// 頂点座標
	XMFLOAT3 normal;	// 法線ベクトル
	XMFLOAT2 uv;		// uv座標
	unsigned short boneName[2];	// ボーン番号
	unsigned char boneWeight;	// ボーンウェイト
	unsigned char edgeFlg;		// 輪郭線フラグ
};
// 頂点データのサイズ
constexpr size_t pmdvertex_size = 38;

#pragma pack(1)
// PMDマテリアル構造体
struct PMDMaterial
{
	XMFLOAT3 diffuse;
	float alpha;
	XMFLOAT3 specular;
	float specularity;
	XMFLOAT3 ambient;
	unsigned char toonIdx;
	unsigned char edgeFlg;
	// 2バイトパディング
	unsigned int indicesNum;	// マテリアルが割り当てられるインデックス数
	char texFilePath[20];		// テクスチャファイルパス+α
};
#pragma pack()

// シェーダーに投げるマテリアルデータ
struct MaterialForHlsl
{
	XMFLOAT3 diffuse;
	float alpha;
	XMFLOAT3 specular;
	float specularity;
	XMFLOAT3 ambient;
};

// それ以外のマテリアルデータ
struct AdditionalMaterial
{
	string texPath;
	unsigned char toonIdx;
	unsigned char edgeFlg;

};

struct Material
{
	unsigned int indicesNum;	// インデックス数
	MaterialForHlsl material;
	AdditionalMaterial additional;
};

// xyz座標にuv座標も加えた構造体の配列を作る
// 結果ワールド座標
//Vertex vertices[] =
//{
//	{{  -1.0f,  -1.0f, 0.0f},{0.0f,1.0f}},
//	{{  -1.0f,   1.0f, 0.0f},{0.0f,0.0f}},
//	{{   1.0f,  -1.0f, 0.0f},{1.0f,1.0f}},
//	{{   1.0f,   1.0f, 0.0f},{1.0f,0.0f}},
//};

// ビュー座標系
XMFLOAT3 eye(0, 10, -15);
XMFLOAT3 target(0, 10, 0);
XMFLOAT3 up(0, 1, 0);

// テクスチャデータの作成
struct TexRGBA
{
	unsigned char R, G, B, A;
};

std::vector<TexRGBA> texturedata(256 * 256);

// シェーダーにわたすための行列データ
struct MatricesData {
	XMMATRIX world;		// モデルの座標
	XMMATRIX viewproj;	// ビューとプロジェクション合成行列
};

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


ID3D12Resource* CreateWhiteTexture() {
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 4;
	resDesc.Height = 4;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* whiteBuff = nullptr;

	auto result = _dev->CreateCommittedResource
	(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&whiteBuff)
	);

	if (FAILED(result)) {
		return nullptr;
	}

	vector<unsigned char> data(4 * 4 * 4);
	fill(data.begin(), data.end(), 255);	// RGBA全て255の白色データを作成	

	result = whiteBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		data.size()
	);

	return whiteBuff;
}

// パス合成
string GetTexturePathFromModelAndTexPath(
	const string& modelPath,
	const char* texPath
) {
	string folderPath = modelPath.substr(0, modelPath.rfind('/') + 1);
	return folderPath + texPath;
}

// マルチバイト文字列からワイド文字列を得る
wstring GetWideStringFromString(
	const string& str
) {
	// 呼び出し1回目
	auto num1 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0
	);

	wstring wstr;
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

ID3D12Resource* loadTextureFromFile(
	string& texPath
) {
	// WICテクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	auto result = LoadFromWICFile(
		GetWideStringFromString(texPath).c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	if (FAILED(result)) {
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	// WriteToSubresourceで転送するためのヒープの設定
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;	// 転送はL0、CPU側から直接行う
	texHeapProp.CreationNodeMask = 0;		// 単一アダプタのため0
	texHeapProp.VisibleNodeMask = 0;		// 単一アダプタのため0

	D3D12_RESOURCE_DESC resDesc = {};
	// 画像テクスチャ版
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

#pragma region モデルデータの読み込み

	char signature[3] = {};
	unsigned int vertNum;		// 頂点数
	unsigned int indicesNum;	// インデックス数
	vector<unsigned short> indices;	// インデックス情報
	PMDHeader pmdHeader;
	FILE* fp = nullptr;

	fopen_s(&fp, "Model/初音ミク.pmd", "rb");

	if (fp == nullptr) {
		return -1;
	}

	// ちゃんとデータファイルの内容の順序で読み込まないとNG
	// シグネチャ(3byte)
	fread(signature, sizeof(signature), 1, fp);
	// ヘッダ情報の読み込み
	fread(&pmdHeader, sizeof(pmdHeader), 1, fp);
	// 頂点情報の読み込み
	fread(&vertNum, sizeof(vertNum), 1, fp);
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);	// バッファの確保（頂点数 * 頂点サイズ）
	fread(vertices.data(), vertices.size(), 1, fp);				// 頂点データの読み込み

	// インデックス情報の読み込み
	fread(&indicesNum, sizeof(indicesNum), 1, fp);
	indices.resize(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	// マテリアルの読み込み
	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp);

	fclose(fp);

#pragma endregion

#pragma region ランダムなテクスチャデータを作成（一時的な処理
	for (auto& rgba : texturedata) {
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255;
	}
#pragma endregion

#pragma region マテリアルデータを構造体にコピーする
	vector<Material> materials(pmdMaterials.size());

	for (int i = 0; i < pmdMaterials.size(); ++i) {
		materials[i].indicesNum = pmdMaterials[i].indicesNum;
		materials[i].material.diffuse = pmdMaterials[i].diffuse;
		materials[i].material.alpha = pmdMaterials[i].alpha;
		materials[i].material.specular = pmdMaterials[i].specular;
		materials[i].material.specularity = pmdMaterials[i].specularity;
		materials[i].material.ambient = pmdMaterials[i].ambient;
	}
#pragma	endregion

#pragma region テクスチャ用画像のロード
	result = CoInitializeEx(0, COINIT_MULTITHREADED);

	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	result = LoadFromWICFile(
		L"Model/eye2.bmp",
		//L"img/textest.png",
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
	// PMDモデル用にサイズ変更
	//auto verHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));
	auto verHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size());


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
	// PMDデータを読み込むためにサイズ変更
	unsigned char* vertMap = nullptr;
	//Vertex* vertMap = nullptr;

	result = vertBuff->Map(
		0,
		nullptr,
		(void**)&vertMap
	);

	copy(begin(vertices), end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);
#pragma endregion

#pragma region インデックスバッファーの作成>モデル読み込みに修正
	ID3D12Resource* idxBuff = nullptr;

	// 頂点バッファーの設定を使いまわす（サイズ以外）
	// →モデル読み込みに変更したのでモデルの情報を使う
	// resDesc.Width = sizeof(indeces);
	auto idxHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto idxResDescBuf = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));
	result = _dev->CreateCommittedResource(
		// &heapProp,
		&idxHeapProp,
		D3D12_HEAP_FLAG_NONE,
		// &resDesc,
		&idxResDescBuf,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	copy(begin(indices), end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);
#pragma endregion


#pragma region インデックス情報の読み込み
#pragma endregion


#pragma region 頂点バッファビューの作成
	// 何バイトのデータが存在するのか？１頂点あたり何バイトなのか？などを知らせるデータ
	D3D12_VERTEX_BUFFER_VIEW vbView = {};

	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();	// バッファの仮想アドレス
	//PMDモデル用にサイズを変更
	//vbView.SizeInBytes = sizeof(vertices);						// 総バイト数
	//vbView.StrideInBytes = sizeof(vertices[0]);					// 1頂点あたりのバイト数
	vbView.SizeInBytes = vertices.size();						// 総バイト数
	vbView.StrideInBytes = pmdvertex_size;					// 1頂点あたりのバイト数
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
		// 法線の追加
		{
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
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
		// ボーン番号の追加
		{
			"BONE_NO",
			0,
			DXGI_FORMAT_R16G16_UINT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// ボーンウェイトの追加
		{
			"WEIGHT",
			0,
			DXGI_FORMAT_R8_UINT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0,
		},
		// 境界線の追加
		{
			"EDGE_FLG",
			0,
			DXGI_FORMAT_R8_UINT,
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
	//	ibView.SizeInBytes = sizeof(indeces);
	ibView.SizeInBytes = indices.size() * sizeof(indices[0]);
#pragma endregion

#pragma region テクスチャバッファの作成

	// WriteToSubresourceで転送するためのヒープの設定
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;	// 転送はL0、CPU側から直接行う
	heapProp.CreationNodeMask = 0;		// 単一アダプタのため0
	heapProp.VisibleNodeMask = 0;		// 単一アダプタのため0

	D3D12_RESOURCE_DESC resDesc = {};
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

	//matrix.r[0].m128_f32[0] =  2.0f / window_width;
	//matrix.r[1].m128_f32[1] = -2.0f / window_height;

	//matrix.r[3].m128_f32[0] = -1.0f;
	//matrix.r[3].m128_f32[1] = 1.0f;

	// オブジェクトに回転を加える
	auto worldMatrix = XMMatrixRotationY(XM_PIDIV4);
	XMMATRIX viewMatrix;
	XMMATRIX projMatrix;
	// 視錐台
	{
		// カメラ座標・向き
		viewMatrix = XMMatrixLookAtLH(
			XMLoadFloat3(&eye),			// 視点座標
			XMLoadFloat3(&target),		// 注視点座標
			XMLoadFloat3(&up)			// 上方向を示すベクトル
		);

		// プロジェクション（見え方）
		projMatrix = XMMatrixPerspectiveFovLH(
			XM_PIDIV2,		// 画角(FOV)
			static_cast<float>(window_width) / static_cast<float>(window_height),	// アスペクト比
			1.0f,				// 見える範囲の手前(near面)
			100.0f				// 見える範囲の奥(far面)
		);
	}

	ID3D12Resource* constBuff = nullptr;

	auto constHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	// 座標を入れるサイズが増えたのでバッファのサイズも合わせる
	auto constResDescBuff = CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xff);

	_dev->CreateCommittedResource(
		&constHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&constResDescBuff,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	// 陰影が正しくつくように書き換え
	//XMMATRIX* mapMatrix;
	MatricesData* mapMatrix;

	//// 定数バッファへのデータのコピー
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);
	// *mapMatrix = matrix;
	mapMatrix->world = worldMatrix;
	mapMatrix->viewproj = viewMatrix * projMatrix;

#pragma endregion

#pragma region ディスクリプタヒープ関連処理

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// シェーダーから見えるようにする
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;									// シェーダーリソースと定数バッファ用の二つ
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;			// シェーダーリソースビュー用

	// 生成
	result = _dev->CreateDescriptorHeap(
		&descHeapDesc,
		IID_PPV_ARGS(&basicDescHeap)
	);

	// 上記で作成したディスクリプタヒープ上にシェーダーリソースビューを作成する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;	// rgbaを正規化→画像読み込みの際は画像のメタデータに合わせる
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;		// RGBAをどのようにマッピングするか指定する
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;		// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;

	// ディスクリプタヒープの先頭アドレスを取得→SRV>CBVの順序で入っているのでSRVの位置を取得できるはず。
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateShaderResourceView(
		texBuff,							// ビューと関連付けるバッファ
		&srvDesc,							// テクスチャ設定情報
		basicDescHeap->GetCPUDescriptorHandleForHeapStart()		// ヒープのどこに割り当てるか
	);

	// 取得したアドレスをバッファサイズ分進めてCBVの位置にする。大きさはCBV、SRV、UAVとも同一
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 定数バッファビューの設定
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constBuff->GetDesc().Width;
	// 定数バッファビューの作成
	_dev->CreateConstantBufferView(
		&cbvDesc,				// ビューに紐づけるバッファ 
		basicHeapHandle			// ヒープのアドレス
	);

#pragma endregion

#pragma region 深度バッファの作成
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// 2次元テクスチャデータ
	depthResDesc.Width = window_width;
	depthResDesc.Height = window_height;
	depthResDesc.DepthOrArraySize = 1;								// テクスチャ配列でも3Dテクスチャでもない
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;					// 深度値書き込み用
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// 深度ステンシルとして仕様

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;					// ここがデフォルトならあとはUNKNOWNでよい
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// クリアバリューが必要
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0f;		// 深さ1.0f（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;	// 32bitのfloat値でクリア

	ID3D12Resource* depthBuffer = nullptr;
	result = _dev->CreateCommittedResource(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,		// 深度書き込みに利用
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer)
	);

	//ディスクリプタヒープを作成する
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;				// 深度ビューは一つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;	// デプスステンシルビューとして利用する

	ID3D12DescriptorHeap* dsvHeap = nullptr;
	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));	//ヒープの生成

	// 上記で作成したヒープ内にビューを作成する
	D3D12_DEPTH_STENCIL_VIEW_DESC dsView = {};
	dsView.Format = DXGI_FORMAT_D32_FLOAT;
	dsView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsView.Flags = D3D12_DSV_FLAG_NONE;

	_dev->CreateDepthStencilView(
		depthBuffer,									// 元にするバッファ
		&dsView,										// ビューの作成先
		dsvHeap->GetCPUDescriptorHandleForHeapStart()	// ヒープの先頭アドレス
	);

#pragma endregion

#pragma region マテリアルバッファの作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	ID3D12Resource* materialBuff = nullptr;

	auto matHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto matResDescBuff = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * materialNum);

	result = _dev->CreateCommittedResource(
		&matHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&matResDescBuff,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&materialBuff)
	);

	// マップにコピー
	char* mapMaterial = nullptr;
	result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	for (auto& m : materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	materialBuff->Unmap(0, nullptr);

	// ディスクリプタヒープを作成する
	ID3D12DescriptorHeap* materialDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = materialNum;		// マテリアル数を指定
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(
		&materialDescHeapDesc,
		IID_PPV_ARGS(&materialDescHeap)
	);

	// ビューの作成
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress();	// バッファのアドレス
	matCBVDesc.SizeInBytes = materialBuffSize;							// 1マテリアル分のサイズ

	D3D12_CPU_DESCRIPTOR_HANDLE matDescHeapH = materialDescHeap->GetCPUDescriptorHandleForHeapStart(); // ヒープの先頭アドレス

	for (int i = 0; i < materialNum; ++i) {
		// matCBVDescからビューの作成
		_dev->CreateConstantBufferView(
			&matCBVDesc,
			matDescHeapH
		);
		//
		matDescHeapH.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // ヒープのアドレスも進める
		matCBVDesc.BufferLocation += materialBuffSize;	// 次のマテリアル分だけアドレスを進める
	}

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
	D3D12_DESCRIPTOR_RANGE descTblRange[3] = {};

	// テクスチャ用レジスタ１(t0)
	descTblRange[0].NumDescriptors = 1;		// テクスチャの数（今回は1）
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;		// 種類はテクスチャ
	descTblRange[0].BaseShaderRegister = 0;				// テクスチャレジスタ番号0(t0)
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;	// とりあえず「連続したディスクリプタレンジが前の直後に来る」を指定

	// 定数用レジスタ１(b0)
	descTblRange[1].NumDescriptors = 1;
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[1].BaseShaderRegister = 0;
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 定数用レジスタ２(b1)
	descTblRange[2].NumDescriptors = 1;
	descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descTblRange[2].BaseShaderRegister = 1;
	descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
#pragma endregion

#pragma region ルートパラメータの作成
	D3D12_ROOT_PARAMETER rootParam[3] = {};

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
	rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;				// ピクセルシェーダから見ることができる

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;				// 頂点シェーダから見ることができる

	rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[2].DescriptorTable.pDescriptorRanges = &descTblRange[2];
	rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;				// ピクセルシェーダから見ることができる


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

	rootSignatureDesc.pParameters = rootParam;
	rootSignatureDesc.NumParameters = 3;

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

	// 深度バッファの設定
	gpipeline.DepthStencilState.DepthEnable = true;		// 深度バッファを使用
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;		// 書き込みを行う>ピクセル描画時に深度バッファに深度値を書き込む
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;				// 小さい方を使用する

	// グラフィックスパイプラインステートオブジェクトの生成
	ID3D12PipelineState* _pipelinestate = nullptr;
	result = _dev->CreateGraphicsPipelineState(
		&gpipeline,
		IID_PPV_ARGS(&_pipelinestate)
	);
#pragma endregion
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	// 見える範囲
	D3D12_VIEWPORT viewPort = {};

	viewPort.Width = window_width;
	viewPort.Height = window_height;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;

	// 切り取る範囲
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
	float angle = 0.0f;
#pragma region mainloop
	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		angle += 0.03f;
		worldMatrix = XMMatrixRotationY(angle);
		// matMatrixの型が変わったのでワールド座標だけ代入
		//*mapMatrix = worldMatrix * viewMatrix * projMatrix;
		mapMatrix->world = worldMatrix;

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
		auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		// レンダーたーげとを設定する
		_cmdList->OMSetRenderTargets(
			1,				// レンダーターゲットの数
			&rtvH,			// レンダーターゲットのハンドル
			true,			// レンダーターゲットが複数の際に連続しているか
			&dsvH			// 深度ステンシルバッファビューのハンドル
		);

		if (_pipelinestate == nullptr) {
			return -1;
		}

		//以下は描画処理前に用意しておく必要がある
		{
			// ビューポート、シザー矩形の設定
			_cmdList->RSSetViewports(1, &viewPort);
			_cmdList->RSSetScissorRects(1, &scissorRect);
			// プリミティブトポロジの設定
			_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// _cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			// 頂点バッファの指定
			_cmdList->IASetVertexBuffers(0, 1, &vbView);
			// インデクスバッファの指定
			_cmdList->IASetIndexBuffer(&ibView);
		}

		// レンダーターゲットを特定の色でクリア
		float clearColor[] = { 1.0f, 1.0f ,1.0f ,1.0f };		// 黄色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		// パイプラインステートの指定
		_cmdList->SetPipelineState(_pipelinestate);
		// ルートシグネチャの指定
		_cmdList->SetGraphicsRootSignature(rootsignature);
		// ディスクリプタヒープのセット
		// SRVとCBVの両方が入っているヒープをセットすれば、ルートパラメータで指定したレジスタに対応するアドレスを自動的に取得してくれる
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);

		// ルートパラメータとディスクリプタヒープの関連付け
		// マテリアルが複数ある場合は、ルートパラメータとディスクリプタヒープの関連付けを描画するマテリアルごとに行う必要がある	
		_cmdList->SetGraphicsRootDescriptorTable(
			0,			// ルートパラメータインデックス
			basicDescHeap->GetGPUDescriptorHandleForHeapStart()	// ヒープアドレス
		);

		// 二つ目のディスクリプタヒープのアドレスを取得
		auto heapHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();
		// そのままだと上と同じ（テクスチャのヒープ）なので、進めて定数のヒープを示すようにする
		heapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// ルートパラメータとディスクリプタヒープの関連付けその弐
		_cmdList->SetGraphicsRootDescriptorTable(
			1,
			heapHandle
		);

		// マテリアルのディスクリプタヒープのアドレスを取得
		// マテリアルヒープにはCBV一つしか入っていないため、SetDescriptorHeapをしなくても良い？
		// アドレスをそのまま使える。
		auto matHeapHandle = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		unsigned int idxOffset = 0;

		for (auto& m : materials) {
			// マテリアルのセット
			// ルートパラメータとディスクリプタヒープの関連付けその参
			// memo:先にマテリアルのハンドルを進めると色が変わる。
			_cmdList->SetGraphicsRootDescriptorTable(
				2,
				matHeapHandle
			);
			_cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);		// モデルのインデックス情報を使う

			matHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);	// マテリアル分だけアドレスを進める
			idxOffset += m.indicesNum;
		}

		// 描画
		// _cmdList->DrawInstanced(vertNum, 1, 0, 0);				// インデックス情報を使わない描画
		// _cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);		// インデクス情報を使う描画
		// _cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);		// モデルのインデックス情報を使う

		// バックバッファの書き込み完了を待つ
		// CD3DX12ヘルパーを使う場合これは不要(Li1099らへん)
		//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//_cmdList->ResourceBarrier(1, &BarrierDesc);

		// 深度バッファのクリア
		_cmdList->ClearDepthStencilView(
			dsvH,							//デプスステンシルビューのハンドル
			D3D12_CLEAR_FLAG_DEPTH,			// クリア先の指定
			1.0f,							// デプスクリアの際に指定する値(near~farが正規化されているため、最大値）
			0,								// ステンシルクリア時に指定する値
			0,								// クリア範囲の配列サイズ
			nullptr							// クリア範囲の配列
		);

		// コマンドリストは実行する前にクローズする必要がある
		_cmdList->Close();

		// 実行
		ID3D12CommandList* cmdLists[] = { _cmdList };
		_cmdQueue->ExecuteCommandLists(1, cmdLists);

		_cmdQueue->Signal(_fence, ++_fenceVal);

		// 非同期で待つ
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
	constBuff->Unmap(0, nullptr);
}