// Service.h
// Joseph Ryan Ries, 3/18/2017
// A test service to demonstrate registering for user logon session change notifications.
#pragma once
#pragma warning(disable: 4710)

#pragma warning(push, 0)
#include <AclAPI.h>			// Also includes windows.h, et al
#include <VersionHelpers.h>
#pragma warning(pop)
#include <stdio.h>



#define SERVICE_NAME L"LogonNotifier"
#define SERVICE_VER  L"1.0"
#define SERVICE_DESC L"A test service to demonstrate registering for user logon session change notifications."

void PrintUsage(void);
void InstallService(void);
void UninstallService(void);
VOID WINAPI ServiceControlHandler(_In_ DWORD ControlCode);
DWORD WINAPI ServiceControlHandlerEx(_In_ DWORD dwControl, _In_ DWORD dwEventType, _In_ LPVOID lpEventData, _In_ LPVOID lpContext);
VOID WINAPI ServiceMain(_In_ DWORD dwArgc, _In_ LPTSTR* lpszArgv);
DWORD WINAPI ServiceWorkerThread(_In_ LPVOID Args);
DWORD AddAceToObjectSecurityDescriptor(LPTSTR ObjectName, SE_OBJECT_TYPE ObjectType, LPTSTR Trustee, TRUSTEE_FORM TrusteeForm, DWORD AccessRights, ACCESS_MODE AccessMode, DWORD Inheritance);
void LogMessageA(_In_ char* Message, _In_ ...);