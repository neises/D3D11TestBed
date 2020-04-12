//useful header
#include <tchar.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <string>
#include <wrl/client.h>

//d3d11 header
#include <D3D11.h>
#include <D3Dcompiler.h>
#include <d3d11.h>
#include <DirectXMath.h>

//useful data
#include "data.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")

//
static const uint32_t WindowWidth = 1280;
static const uint32_t WindowHeight = 720;
static const char* AppWindowName = "D3D11App";

//useful macro for printing d3d related error messages
#define CHECKHR(hr, message) {if(FAILED(hr)){printf("D3D Error: %s\n", std::string(message).c_str() ); DebugBreak();}}

//yes please use it
using namespace DirectX;
using Microsoft::WRL::ComPtr;

//some global pointers
ComPtr<ID3D11Buffer>			g_VertexBuffer;
ComPtr<ID3D11Buffer>			g_IndexBuffer;
ComPtr<ID3D11Buffer>			g_pConstantBuffer;
ComPtr<ID3D11Buffer>			g_pObjectBuffer;
ComPtr<ID3D11VertexShader>		g_pVertexShader;
ComPtr<ID3D11PixelShader>		g_pPixelShader;
ComPtr<ID3D11InputLayout>		g_pVertexLayout;

XMMATRIX						g_World;
XMMATRIX						g_View;
XMMATRIX						g_Projection;

//our Vertex 
struct Vertex
{
	XMFLOAT3 mPosition;
	XMFLOAT4 mColor;
};

//normally we should move the 
//world matrix to the frame buffer 
struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	float	 mScalar;
};

struct Context
{
	HWND hwnd;
	bool bShutdown;

	ComPtr<ID3D11Device> pDevice;
	ComPtr<ID3D11DeviceContext> pDeviceContext;
	ComPtr<IDXGISwapChain> pSwapChain;

	ComPtr<ID3D11Texture2D> pBackbufferTexture;
	ComPtr<ID3D11RenderTargetView> pBackBufferRTV;

	ComPtr<ID3D11Texture2D> pDepthStencilTexture;
	ComPtr<ID3D11DepthStencilView> pDepthStencilView;
};


LRESULT APIENTRY WndProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	Context* ctx = reinterpret_cast< Context* >( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

	switch( msg )
	{
		case WM_CLOSE:
			ctx->bShutdown = true;

		default:
			return ( LONG )DefWindowProc( hwnd, msg, wparam, lparam );
	}
}

// Helper function for compiling shader file
HRESULT CompileShaderFromFile( const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile( szFileName, NULL, NULL, szEntryPoint, szShaderModel,
							 dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
	if( FAILED( hr ) )
	{
		if( pErrorBlob != NULL )
			OutputDebugStringA( ( char* )pErrorBlob->GetBufferPointer() );
		if( pErrorBlob ) pErrorBlob->Release();
		return hr;
	}
	if( pErrorBlob ) pErrorBlob->Release();

	return S_OK;
}



void Setup( Context* _pContext )
{
	// Create vertex buffer
	D3D11_BUFFER_DESC bd;
	ZeroMemory( &bd, sizeof( bd ) );
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( VertexDataColor );
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;


	//specify data for initializing a subresource.
	D3D11_SUBRESOURCE_DATA init_data = {};
	init_data.pSysMem = VertexDataColor;

	HRESULT result = _pContext->pDevice->CreateBuffer( &bd,
													   &init_data,
													   g_VertexBuffer.GetAddressOf() );
	CHECKHR( result, "Vertex buffer creation failed" );


	//set vertex buffer
	UINT stride = 10 * 4;//sizeof(Vertex);
	UINT offset = 0;
	_pContext->pDeviceContext->IASetVertexBuffers( 0, 1, g_VertexBuffer.GetAddressOf(), &stride, &offset );

	//create index buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( IndexData );
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	init_data.pSysMem = IndexData;
	result = _pContext->pDevice->CreateBuffer( &bd, &init_data, g_IndexBuffer.GetAddressOf() );
	CHECKHR( result, "Index buffer creation failed" );

	//set index buffer
	_pContext->pDeviceContext->IASetIndexBuffer( g_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0 );

	//set primitive topology
	_pContext->pDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	//create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( ConstantBuffer );
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	result = _pContext->pDevice->CreateBuffer( &bd, NULL, g_pConstantBuffer.GetAddressOf() );
	CHECKHR( result, "Constant buffer creation failed" );

	//set the constant buffer
	_pContext->pDeviceContext->VSSetConstantBuffers( 0, 1, g_pConstantBuffer.GetAddressOf() );

	//compile the vertex shader
	ComPtr<ID3DBlob> pVSBlob = NULL;
	std::wstring strShader = L"SimpleShader.fx";
	result = CompileShaderFromFile( strShader.c_str(), "VS", "vs_4_0", pVSBlob.GetAddressOf() );
	CHECKHR( result, "Failed to compile VertexShader" );

	//create the vertex shader
	result = _pContext->pDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader );
	CHECKHR( result, "Failed to create VertexShader" );

	//define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",	 0,	DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,									D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		 0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	D3D11_APPEND_ALIGNED_ELEMENT,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",		 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,	D3D11_APPEND_ALIGNED_ELEMENT,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE( layout );

	//create the input layout
	result = _pContext->pDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
													pVSBlob->GetBufferSize(), g_pVertexLayout.GetAddressOf() );

	CHECKHR( result, "Failed to set InputLayout" );

	//set the input layout
	_pContext->pDeviceContext->IASetInputLayout( g_pVertexLayout.Get() );

	//compile the pixel shader
	ComPtr<ID3DBlob> pPSBlob = NULL;
	result = CompileShaderFromFile( strShader.c_str(), "PS", "ps_4_0", pPSBlob.GetAddressOf() );
	CHECKHR( result, "Failed to compile Pixelshader" );

	//create the pixel shader
	result = _pContext->pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, g_pPixelShader.GetAddressOf() );
	CHECKHR( result, "Failed to create PixelShader" );

	//set the shader
	_pContext->pDeviceContext->VSSetShader( g_pVertexShader.Get(), NULL, 0 );
	_pContext->pDeviceContext->PSSetShader( g_pPixelShader.Get(), NULL, 0 );


	// Initialize the world matrix
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet( 0.0f, 0.0f, -3.0f, 0.0f );
	XMVECTOR At = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
	XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	g_View = XMMatrixLookAtLH( Eye, At, Up );

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH( XM_PIDIV2, WindowWidth / ( FLOAT )WindowHeight, 0.01f, 100.0f );

}

Context* Init()
{
	Context* pContext = new Context;
	ZeroMemory( pContext, sizeof( *pContext ) );

	pContext->bShutdown = false;

	// setup window class
	WNDCLASSEX wc = { 0 };

	HINSTANCE hInst = GetModuleHandle( NULL );

	wc.cbSize = sizeof( WNDCLASSEX );
	wc.style = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = _T( "WindowClass" );
	wc.hIconSm = wc.hIcon;

	RegisterClassEx( &wc );

	uint32_t window_style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	// find screen center and create our window there
	int32_t pos_x = GetSystemMetrics( SM_CXSCREEN ) / 2 - WindowWidth / 2;
	int32_t pos_y = GetSystemMetrics( SM_CYSCREEN ) / 2 - WindowHeight / 2;

	// calculate window size for required client area
	RECT client_rect = { pos_x, pos_y, pos_x + WindowWidth, pos_y + WindowHeight };
	AdjustWindowRect( &client_rect, window_style, FALSE );

	// create window
	pContext->hwnd = CreateWindowA( "WindowClass", AppWindowName, window_style,
									client_rect.left, client_rect.top,
									client_rect.right - client_rect.left, client_rect.bottom - client_rect.top,
									NULL, NULL, hInst, NULL );

	// setup window owner for message handling
	SetWindowLongPtr( pContext->hwnd, GWLP_USERDATA, ( LONG_PTR )pContext );

	ShowWindow( pContext->hwnd, SW_SHOWNORMAL );
	UpdateWindow( pContext->hwnd );

	// Create D3D11
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof( sd ) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = WindowWidth;
	sd.BufferDesc.Height = WindowHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = pContext->hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	uint32_t flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

	D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;

	IDXGIAdapter* adapter = NULL;
	IDXGIAdapter* enumerated_adapter = NULL;

	ComPtr<IDXGIFactory> factory = NULL;
	CHECKHR( CreateDXGIFactory( __uuidof( IDXGIFactory ), ( void** )&factory ) );
	for( uint32_t i = 0; factory->EnumAdapters( i, &enumerated_adapter ) != DXGI_ERROR_NOT_FOUND; ++i )
	{
		DXGI_ADAPTER_DESC adapter_desc;
		if( enumerated_adapter->GetDesc( &adapter_desc ) != S_OK )
		{
			continue;
		}
		if( wcsstr( adapter_desc.Description, L"PerfHUD" ) != 0 )
		{
			type = D3D_DRIVER_TYPE_REFERENCE;
			adapter = enumerated_adapter;
			break;
		}
	}
	factory.ReleaseAndGetAddressOf();

	D3D_FEATURE_LEVEL features[] =
	{
		D3D_FEATURE_LEVEL_11_0
	};
	const uint32_t num_features = sizeof( features ) / sizeof( features[ 0 ] );

	// Create device

	D3D_FEATURE_LEVEL supported_features;
	CHECKHR( D3D11CreateDeviceAndSwapChain( adapter, type, NULL, flags, features, num_features,
			 D3D11_SDK_VERSION, &sd, &pContext->pSwapChain, &pContext->pDevice,
			 &supported_features, &pContext->pDeviceContext ) );

	// Get default back buffer

	CHECKHR( pContext->pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( void** )pContext->pBackbufferTexture.GetAddressOf() ) );

	CHECKHR( pContext->pDevice->CreateRenderTargetView( pContext->pBackbufferTexture.Get(), NULL, &pContext->pBackBufferRTV ) );

	// Create default depth buffer
	// create depth stencil
	D3D11_TEXTURE2D_DESC depth_stencil_desc;
	depth_stencil_desc.Width = WindowWidth;
	depth_stencil_desc.Height = WindowHeight;
	depth_stencil_desc.MipLevels = 1;
	depth_stencil_desc.ArraySize = 1;
	depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_stencil_desc.SampleDesc.Count = 1;
	depth_stencil_desc.SampleDesc.Quality = 0;
	depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
	depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depth_stencil_desc.CPUAccessFlags = 0;
	depth_stencil_desc.MiscFlags = 0;

	CHECKHR( pContext->pDevice->CreateTexture2D( &depth_stencil_desc, NULL, pContext->pDepthStencilTexture.GetAddressOf() ) );

	// create depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc;
	ZeroMemory( &depth_stencil_view_desc, sizeof( depth_stencil_view_desc ) );
	depth_stencil_view_desc.Format = depth_stencil_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	CHECKHR( pContext->pDevice->CreateDepthStencilView( pContext->pDepthStencilTexture.Get(), &depth_stencil_view_desc, pContext->pDepthStencilView.GetAddressOf() ) );

	// Set-up default Color and Depth surfaces
	pContext->pDeviceContext->OMSetRenderTargets( 1, pContext->pBackBufferRTV.GetAddressOf(), pContext->pDepthStencilView.Get() );

	D3D11_VIEWPORT vp;
	vp.Width = ( FLOAT )WindowWidth;
	vp.Height = ( FLOAT )WindowHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	pContext->pDeviceContext->RSSetViewports( 1, &vp );

	//set up our data for the rendering process 
	Setup( pContext );
	return pContext;
}



void Update( Context* _pContext )
{
	float colour_array[ 4 ] = { 0.1f, 0.1f, 0.1f, 1.0f };
	_pContext->pDeviceContext->ClearRenderTargetView( _pContext->pBackBufferRTV.Get(), colour_array );
	_pContext->pDeviceContext->ClearDepthStencilView( _pContext->pDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0.0f );

	static float t = 0.0f;
	static DWORD dwTimeStart = 0;
	DWORD dwTimeCur = GetTickCount();
	if( dwTimeStart == 0 )
		dwTimeStart = dwTimeCur;
	t = ( dwTimeCur - dwTimeStart ) / 1000.0f;
	g_World = XMMatrixMultiply( XMMatrixRotationY( t ), XMMatrixRotationX( -t ) );

	//ConstantBuffer cb;
	//cb.mWorld = XMMatrixTranspose(g_World);
	//cb.mView = XMMatrixTranspose(g_View);
	//cb.mProjection = XMMatrixTranspose(g_Projection);

	//uncomment in shader file the commented constant buffer
	//in this case you don´t need to transpose the matrices 
	//just pass them to the shader 
	ConstantBuffer cb;
	cb.mWorld = g_World;
	cb.mView = g_View;
	cb.mProjection = g_Projection;
	cb.mScalar = t;

	_pContext->pDeviceContext->UpdateSubresource( g_pConstantBuffer.Get(), 0, NULL, &cb, 0, 0 );

	// Renders a triangle
	_pContext->pDeviceContext->DrawIndexed( ARRAYSIZE( IndexData ), 0, 0 );

	// All done, now swap
	CHECKHR( _pContext->pSwapChain->Present( 0, 0 ) );
}

void Shutdown( Context* ctx )
{
	//comptr should take care of the rest 
	delete ctx;
}

int main()
{
	Context* ctx = Init();

	while( !ctx->bShutdown )
	{
		MSG msg;

		while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		Update( ctx );
	}

	Shutdown( ctx );

	return 0;
}
