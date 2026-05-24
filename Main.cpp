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

//フィーチャーレベル配列
D3D_FEATURE_LEVEL levels[] =
{
	D3D_FEATURE_LEVEL_12_2,
	D3D_FEATURE_LEVEL_12_1,
	D3D_FEATURE_LEVEL_12_0,
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
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

	// 先頭のアドレスを取得
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	// メインループの前だからここでバッファを二つ作る
	for (int idx = 0; idx < swcDesc.BufferCount; ++idx) {
		// スワップチェインの中身とID3D12Resourceを関連付ける
		result = _swapChain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		// サイズを取得してその分だけptrメンバーに加算してポインタを進める
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		// レンダービューの作成
		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			nullptr,
			handle
		);
	}

	// フェンスの作成
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;

	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

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

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

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
		float clearColor[] = { 1.0f, 1.0f ,0.0f ,1.0f };		// 黄色
		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		_cmdList->ResourceBarrier(1, &BarrierDesc);

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