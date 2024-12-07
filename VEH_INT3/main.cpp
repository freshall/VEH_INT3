#include <windows.h>
#include <iostream>
#include <dbghelp.h>
#include <psapi.h>

#pragma comment (lib, "dbghelp.lib")
#pragma comment(lib, "ntdll.lib")


BYTE originalByte = 0;
void* pGetAsyncKeyState = nullptr;
bool userPressedKey = false; 

void LogCallerAddress( void* callerAddress )
{
	HMODULE hModule = NULL;
	char moduleName[MAX_PATH] = { 0 };

	if ( GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		reinterpret_cast< LPCSTR >( callerAddress ),
		&hModule ) )
	{
		GetModuleFileNameA( hModule, moduleName, MAX_PATH );
		std::cout << "Call from: " << moduleName << " at address: " << callerAddress << std::endl;
	}
	else
	{
		std::cout << "Call from unknown module at address: " << callerAddress << std::endl;
	}
}

LONG WINAPI VectoredHandler( PEXCEPTION_POINTERS ExceptionInfo )
{
	if ( ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT )
	{
		void* rip = reinterpret_cast< void* >( ExceptionInfo->ContextRecord->Rip );

		if ( rip == pGetAsyncKeyState )
		{
			void* callerAddress = reinterpret_cast< void* >( ExceptionInfo->ContextRecord->Rbp + sizeof( void* ) );

			if ( callerAddress && userPressedKey )
			{
				LogCallerAddress( callerAddress );
				LogCallerAddress( pGetAsyncKeyState );
				userPressedKey = false;
			}

			DWORD oldProtect;
			VirtualProtect( pGetAsyncKeyState, 1, PAGE_EXECUTE_READWRITE, &oldProtect );
			memcpy( pGetAsyncKeyState, &originalByte, 1 );
			VirtualProtect( pGetAsyncKeyState, 1, oldProtect, &oldProtect );

			ExceptionInfo->ContextRecord->Rip++;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

void SetInt3Trap( )
{
	HMODULE hUser32 = GetModuleHandle( "user32.dll" );
	pGetAsyncKeyState = GetProcAddress( hUser32, "GetAsyncKeyState" );

	if ( !pGetAsyncKeyState )
	{
		std::cout << "Failed to find GetAsyncKeyState: " << pGetAsyncKeyState << std::endl;
		return;
	}

	DWORD oldProtect;
	VirtualProtect( pGetAsyncKeyState, 1, PAGE_EXECUTE_READWRITE, &oldProtect );
	memcpy( &originalByte, pGetAsyncKeyState, 1 );

	BYTE int3 = 0xCC;
	memcpy( pGetAsyncKeyState, &int3, 1 );
	VirtualProtect( pGetAsyncKeyState, 1, oldProtect, &oldProtect );
}


int main( )
{
	LoadLibrary( "win32u.dll" );
	LoadLibrary( "user32.dll" );

	SetInt3Trap( );

	std::cout << "Press ESC to test GetAsyncKeyState detection..." << std::endl;

	while ( true )
	{
		AddVectoredExceptionHandler( 1, VectoredHandler );
		SetInt3Trap( );

		if ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
		{
		    userPressedKey = true; //
		    std::cout << "ESC detected by user.\n" << std::endl;
		}

		SetInt3Trap( );

		Sleep( 100 );
	}

	return 0;
}
