// Service.c
// Joseph Ryan Ries, 3/18/2017
// A test service to demonstrate registering for user logon session change notifications.

#include "Service.h"

SERVICE_STATUS gServiceStatus;
SERVICE_STATUS_HANDLE gServiceStatusHandle;
HANDLE gServiceStopEvent;
HANDLE gWorkerThread;
wchar_t gLogFilePath[512];

int wmain(_In_ int argc, _In_ wchar_t* argv[])
{
	if (IsWindowsVistaOrGreater() == FALSE)
	{
		wprintf(L"\nWindows versions prior to Vista are not supported.\n");
		return(0);
	}

	if (argc > 1)
	{
		if (_wcsicmp(argv[1], L"-install") == 0)
		{
			InstallService();
			return(0);
		}
		else if (_wcsicmp(argv[1], L"-uninstall") == 0)
		{
			UninstallService();
			return(0);
		}
		else
		{
			PrintUsage();
			wprintf(L"\nUnrecognized parameter.\n");
			return(0);
		}
	}

	DWORD CurrentPID = GetCurrentProcessId();
	DWORD CurrentSessionID = 0;

	if (ProcessIdToSessionId(CurrentPID, &CurrentSessionID) == 0)
	{
		wprintf(L"ERROR: ProcessIdToSessionId failed! Code 0x%08lx\n", GetLastError());
		return(0);
	}

	if (CurrentSessionID != 0)
	{
		PrintUsage();
		wprintf(L"\nThis program only runs when installed as a Windows service.\n");
		return(0);
	}	

	// If this service is running as LOCAL SERVICE, this environment variable should be something
	// like C:\Windows\ServiceProfiles\LocalService\AppData\Local
	if (GetEnvironmentVariable(L"LOCALAPPDATA", gLogFilePath, 512) == 0)
	{
		return(0);
	}

	wcscat_s(gLogFilePath, 512, L"\\LogonNotifier.log");

	LogMessageA("Service is starting.");

	SERVICE_TABLE_ENTRY ServiceTable[] = { { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain }, { NULL, NULL } };

	if (StartServiceCtrlDispatcher(ServiceTable) == 0)
	{
		DWORD LastError = GetLastError();
		LogMessageA("Service failed to start! Error code: 0x%08lx", LastError);
		return(0);
	}

	return(0);
}

void PrintUsage(void)
{
	wprintf(L"\n%s %s by Joseph Ryan Ries\n", SERVICE_NAME, SERVICE_VER);
	wprintf(L"%s\n", SERVICE_DESC);
	wprintf(L"\nUsage:\n");
	wprintf(L"  %s -install\n", SERVICE_NAME);
	wprintf(L"  %s -uninstall\n", SERVICE_NAME);
}

void InstallService(void)
{
	SC_HANDLE ServiceController = NULL;	

	if ((ServiceController = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		wprintf(L"ERROR: Failed to open the service controller! Code 0x%08lx\n", GetLastError());
		return;
	}

	wchar_t ImageFilePath[MAX_PATH] = { 0 };
	DWORD CurrentModulePathLength = 0;

	CurrentModulePathLength = GetModuleFileName(NULL, ImageFilePath, MAX_PATH / sizeof(wchar_t));
	
	if (CurrentModulePathLength == 0 || GetLastError() == ERROR_INSUFFICIENT_BUFFER)
	{
		wprintf(L"ERROR: Failed to get the path to the current image file. Path too long?\n");
		return;
	}

	SC_HANDLE ServiceHandle = NULL;
	
	ServiceHandle = CreateService(
		ServiceController, 
		SERVICE_NAME, 
		SERVICE_NAME, 
		SERVICE_ALL_ACCESS, 
		SERVICE_WIN32_OWN_PROCESS, 
		SERVICE_AUTO_START, 
		SERVICE_ERROR_NORMAL, 
		ImageFilePath, 
		NULL,
		NULL, 
		NULL, 
		L"NT AUTHORITY\\LocalService", 
		NULL);

	if (ServiceHandle == NULL)
	{
		wprintf(L"ERROR: Failed to install service! Code 0x%08lx\n", GetLastError());
		return;
	}

	SERVICE_DESCRIPTION ServiceDescription = { SERVICE_DESC };

	if (ChangeServiceConfig2(ServiceHandle, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription) == 0)
	{
		wprintf(L"WARNING: Service was installed, but failed to set service description! Code: 0x%08lx\n", GetLastError());
	}

	// Need to make sure 'Local Service' has read permissions to the executable image.
	DWORD AddACEResult = AddAceToObjectSecurityDescriptor(ImageFilePath, SE_FILE_OBJECT, L"NT AUTHORITY\\LocalService", TRUSTEE_IS_NAME, GENERIC_READ | GENERIC_EXECUTE, GRANT_ACCESS, NO_INHERITANCE);
	if (AddACEResult != ERROR_SUCCESS)
	{
		wprintf(L"WARNING: Failed to grant Local Service read access on the executable! Code: 0x%08lx\n", AddACEResult);		
	}

	wprintf(L"Service installed.\n");

	if (StartService(ServiceHandle, 0, NULL) == 0)
	{
		wprintf(L"WARNING: Failed to start service! Code 0x%08lx\n", GetLastError());
	}
	else
	{
		wprintf(L"Service started.\n");
	}
}

void UninstallService(void)
{
	SC_HANDLE ServiceController = NULL;

	if ((ServiceController = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == NULL)
	{
		wprintf(L"ERROR: Failed to open the service controller! Code 0x%08lx\n", GetLastError());
		return;
	}

	SC_HANDLE ServiceHandle = NULL;

	if ((ServiceHandle = OpenService(ServiceController, SERVICE_NAME, SERVICE_ALL_ACCESS)) == NULL)
	{
		wprintf(L"ERROR: Failed to open the service! Code 0x%08lx\n", GetLastError());
		return;
	}	

	SERVICE_STATUS_PROCESS ServiceStatus = { 0 };
	DWORD BytesNeeded = 0;

	if (QueryServiceStatusEx(ServiceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&ServiceStatus, sizeof(SERVICE_STATUS_PROCESS), &BytesNeeded) == 0)
	{
		wprintf(L"ERROR: Failed to query service status! Code 0x%08lx\n", GetLastError());
		return;
	}

	if (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
	{
		if (ControlService(ServiceHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ServiceStatus) == 0)
		{
			wprintf(L"ERROR: Failed to send stop control to the service! Code 0x%08lx\n", GetLastError());
			return;
		}

		wprintf(L"Waiting for service to stop...\n");

		DWORD StopTimeout = 0;


		while (ServiceStatus.dwCurrentState != SERVICE_STOPPED && StopTimeout < 6)
		{
			if (QueryServiceStatusEx(ServiceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&ServiceStatus, sizeof(SERVICE_STATUS_PROCESS), &BytesNeeded) == 0)
			{
				wprintf(L"ERROR: Failed to query service status! Code 0x%08lx\n", GetLastError());
				return;
			}
			Sleep(3000);
			StopTimeout++;
		}

		if (StopTimeout >= 6)
		{
			wprintf(L"WARNING: Service did not stop in a timely manner.\n");
		}
	}

	if (DeleteService(ServiceHandle) == 0)
	{
		wprintf(L"ERROR: Failed to delete service! Code 0x%08lx\n", GetLastError());
		return;
	}

	wprintf(L"Service uninstalled.\n");
}

// NOT USED, see ServiceControlHandlerEx
VOID WINAPI ServiceControlHandler(_In_ DWORD ControlCode)
{
	switch (ControlCode)
	{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
		{
			LogMessageA("Service is stopping.");
			if (gServiceStatus.dwCurrentState != SERVICE_RUNNING)
			{
				break;
			}

			SetEvent(gServiceStopEvent);

			gServiceStatus.dwControlsAccepted = 0;
			gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			gServiceStatus.dwWin32ExitCode = 0;
			gServiceStatus.dwCheckPoint = 4;

			if (SetServiceStatus(gServiceStatusHandle, &gServiceStatus) == 0)
			{
				LogMessageA("ERROR setting service status! Error Code: 0x%08lx", GetLastError());
			}		

			break;
		}
		default:
		{
			break;
		}
	}
}

DWORD WINAPI ServiceControlHandlerEx(_In_ DWORD ControlCode, _In_ DWORD EventType, _In_ LPVOID EventData, _In_ LPVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	switch (ControlCode)
	{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
		{
			LogMessageA("Service is stopping.");
			if (gServiceStatus.dwCurrentState != SERVICE_RUNNING)
			{
				return(NO_ERROR);
			}

			SetEvent(gServiceStopEvent);

			gServiceStatus.dwControlsAccepted = 0;
			gServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			gServiceStatus.dwWin32ExitCode = 0;
			gServiceStatus.dwCheckPoint = 4;

			if (SetServiceStatus(gServiceStatusHandle, &gServiceStatus) == 0)
			{
				LogMessageA("ERROR setting service status! Error Code: 0x%08lx", GetLastError());
			}

			return(NO_ERROR);
		}
		case SERVICE_CONTROL_SESSIONCHANGE:
		{
			switch (EventType)
			{
				case WTS_SESSION_LOGON:
				{
					// WTSQuerySessionInformation if you want more info about the session, such as username, etc.
					LogMessageA("Logon session %d", ((WTSSESSION_NOTIFICATION*)EventData)->dwSessionId);
					break;
				}
				case WTS_SESSION_LOGOFF:
				{
					LogMessageA("Logoff session %d", ((WTSSESSION_NOTIFICATION*)EventData)->dwSessionId);
					break;
				}
				default:
				{
					break;
				}
			}

			return(NO_ERROR);
		}
		default:
		{
			return(ERROR_CALL_NOT_IMPLEMENTED);
		}
	}
}

VOID WINAPI ServiceMain(_In_ DWORD dwArgc, _In_ LPTSTR* lpszArgv)
{
	UNREFERENCED_PARAMETER(dwArgc);
	UNREFERENCED_PARAMETER(lpszArgv);

	//gServiceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceControlHandler);
	gServiceStatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceControlHandlerEx, NULL);

	if (gServiceStatusHandle == NULL)
	{
		LogMessageA("ERROR: RegisterServiceCtrlHandlerEx failed! Error code 0x%08lx", GetLastError());
		return;
	}

	gServiceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
	gServiceStatus.dwCurrentState            = SERVICE_START_PENDING;
	gServiceStatus.dwControlsAccepted        = 0;
	gServiceStatus.dwWin32ExitCode           = NO_ERROR;
	gServiceStatus.dwServiceSpecificExitCode = NO_ERROR;
	gServiceStatus.dwWaitHint                = 3000;

	if (SetServiceStatus(gServiceStatusHandle, &gServiceStatus) == 0)
	{
		LogMessageA("ERROR: SetServiceStatus failed! Error code 0x%08lx", GetLastError());
		return;
	}

	gServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (gServiceStopEvent == NULL)
	{
		LogMessageA("ERROR: CreateEvent failed! Error code 0x%08lx", GetLastError());
		gServiceStatus.dwControlsAccepted = 0;
		gServiceStatus.dwCurrentState     = SERVICE_STOPPED;
		gServiceStatus.dwWin32ExitCode    = GetLastError();
		gServiceStatus.dwCheckPoint       = 1;
		SetServiceStatus(gServiceStatusHandle, &gServiceStatus);		
		return;
	}

	gServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
	gServiceStatus.dwCurrentState     = SERVICE_RUNNING;
	gServiceStatus.dwWin32ExitCode    = NO_ERROR;
	gServiceStatus.dwCheckPoint       = 0;

	if (SetServiceStatus(gServiceStatusHandle, &gServiceStatus) == 0)
	{
		LogMessageA("ERROR: SetServiceStatus failed! Error code 0x%08lx", GetLastError());
	}

	gWorkerThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	if (gWorkerThread == NULL)
	{
		LogMessageA("ERROR: CreateThread failed! Error code 0x%08lx", GetLastError());
		return;
	}

	WaitForSingleObject(gWorkerThread, INFINITE);

	gServiceStatus.dwControlsAccepted = 0;
	gServiceStatus.dwCurrentState = SERVICE_STOPPED;
	gServiceStatus.dwWin32ExitCode = NO_ERROR;
	gServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(gServiceStatusHandle, &gServiceStatus) == 0)
	{
		LogMessageA("ERROR: SetServiceStatus failed! Error code 0x%08lx", GetLastError());
	}
}

DWORD WINAPI ServiceWorkerThread(_In_ LPVOID Args)
{
	UNREFERENCED_PARAMETER(Args);

	while (WaitForSingleObject(gServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		Sleep(1000);
	}

	return(0);
}

DWORD AddAceToObjectSecurityDescriptor(LPTSTR ObjectName, SE_OBJECT_TYPE ObjectType, LPTSTR Trustee, TRUSTEE_FORM TrusteeForm, DWORD AccessRights, ACCESS_MODE AccessMode, DWORD Inheritance)
{
	DWORD Result = ERROR_SUCCESS;
	PACL OldDACL = NULL;
	PACL NewDACL = NULL;
	PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
	EXPLICIT_ACCESS ExplicitAccess;

	if (NULL == ObjectName)
	{
		return ERROR_INVALID_PARAMETER;
	}

	// Get a pointer to the existing DACL.
	Result = GetNamedSecurityInfo(ObjectName, ObjectType, DACL_SECURITY_INFORMATION, NULL, NULL, &OldDACL, NULL, &SecurityDescriptor);
	if (Result != ERROR_SUCCESS)
	{
		goto Cleanup;
	}

	// Initialize an EXPLICIT_ACCESS structure for the new ACE. 

	ZeroMemory(&ExplicitAccess, sizeof(EXPLICIT_ACCESS));

	ExplicitAccess.grfAccessPermissions = AccessRights;
	ExplicitAccess.grfAccessMode        = AccessMode;
	ExplicitAccess.grfInheritance       = Inheritance;
	ExplicitAccess.Trustee.TrusteeForm  = TrusteeForm;
	ExplicitAccess.Trustee.ptstrName    = Trustee;

	// Create a new ACL that merges the new ACE into the existing DACL.

	Result = SetEntriesInAcl(1, &ExplicitAccess, OldDACL, &NewDACL);
	if (Result != ERROR_SUCCESS)
	{
		goto Cleanup;
	}

	// Attach the new ACL as the object's DACL.

	Result = SetNamedSecurityInfo(ObjectName, ObjectType, DACL_SECURITY_INFORMATION, NULL, NULL, NewDACL, NULL);
	if (Result != ERROR_SUCCESS)
	{
		goto Cleanup;
	}

Cleanup:

	if (SecurityDescriptor != NULL)
	{
		LocalFree((HLOCAL)SecurityDescriptor);
	}
	if (NewDACL != NULL)
	{
		LocalFree((HLOCAL)NewDACL);
	}
	//if (OldDACL != NULL)
	//{
	//	LocalFree((HLOCAL)OldDACL);
	//}

	return Result;
}

void LogMessageA(_In_ char* Message, _In_ ...)
{
	size_t MessageLength = strlen(Message);

	if (MessageLength < 1 || MessageLength > 4096)
	{
		return;
	}

	SYSTEMTIME Time = { 0 };
	GetLocalTime(&Time);

	char FormattedMessage[4096] = { 0 };

	va_list ArgPointer = NULL;
	va_start(ArgPointer, Message);
	_vsnprintf_s(FormattedMessage, sizeof(FormattedMessage), _TRUNCATE, Message, ArgPointer);
	va_end(ArgPointer);

	HANDLE LogFileHandle = NULL;
	if ((LogFileHandle = CreateFile(gLogFilePath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		return;
	}

	DWORD EndOfFile = SetFilePointer(LogFileHandle, 0, NULL, FILE_END);
	
	if (EndOfFile == INVALID_SET_FILE_POINTER)
	{
		CloseHandle(LogFileHandle);
		return;
	}

	DWORD NumberOfBytesWritten = 0;

	char DateTimeString[96] = { 0 };

	int Error = snprintf(DateTimeString, sizeof(DateTimeString), "\r\n[%02d/%02d/%d %02d:%02d:%02d.%d] ", Time.wMonth, Time.wDay, Time.wYear, Time.wHour, Time.wMinute, Time.wSecond, Time.wMilliseconds);

	if (Error < 1)
	{
		CloseHandle(LogFileHandle);
		return;
	}

	WriteFile(LogFileHandle, DateTimeString, (DWORD)strlen(DateTimeString), &NumberOfBytesWritten, NULL);

	WriteFile(LogFileHandle, FormattedMessage, (DWORD)strlen(FormattedMessage), &NumberOfBytesWritten, NULL);

	CloseHandle(LogFileHandle);

}
