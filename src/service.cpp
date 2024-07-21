#include <iostream>
#include <windows.h>
#include <string>
#include <memory>
#include <vector>
#include <cstring>

extern "C" _declspec() bool isServiceRunning(const char* service_name) {

    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return false;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name, SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(sc_manager);
        return false;
    }

    SERVICE_STATUS_PROCESS status;
    DWORD bytes_needed;
    bool is_running = false;

    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(status), &bytes_needed)) {
        is_running = (status.dwCurrentState == SERVICE_RUNNING);
    }

    CloseServiceHandle(service);
    CloseServiceHandle(sc_manager);
    return is_running;

}

extern "C" _declspec() bool doesServiceExist(const char* service_name) {
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return false;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name, SERVICE_QUERY_STATUS);

    if (service) {
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        return true;
    }

    CloseServiceHandle(sc_manager);
    return false;
}


extern "C" {
    struct ServiceDetails {
        char service_name[256];
        char service_display_name[256];
        char executable_path[1024];
        char service_type[1024];
        char service_account[256];
    };

    __declspec(dllexport) void getServiceDetails(const char* service_name, ServiceDetails* details);

    __declspec(dllexport) void freeServiceDetails(ServiceDetails* details);
}

void getServiceDetails(const char* service_name, ServiceDetails* details) {
    memset(details, 0, sizeof(ServiceDetails));

    strncpy_s(
        details->service_name,
        sizeof(details->service_name),
        service_name,
        sizeof(details->service_name)
    );

    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name,  SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);
    if (!service) {
        CloseServiceHandle(sc_manager);
        std::cerr << "failed to open service" << GetLastError() << std::endl;
        return;
    }

    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded = 0;

    if (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&ssp), sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
        std::string serviceType = (ssp.dwServiceType & SERVICE_WIN32_OWN_PROCESS) ? "Own Process" : "Shared Process";
        strncpy_s(
            details->service_type,
            sizeof(details->service_type),
            serviceType.c_str(),
            sizeof(details->service_type) - 1
        );
        
    } else {
        std::cerr << "Failed to query service status. Error: " << GetLastError() << std::endl;
    }

    QueryServiceConfig(service, NULL, 0, &bytesNeeded);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        std::wcerr << L"QueryServiceConfig failed: " << GetLastError() << std::endl;
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        return;
    }

    std::vector<BYTE> buffer(bytesNeeded);
    LPQUERY_SERVICE_CONFIG pServiceConfig = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(buffer.data());
    if (!QueryServiceConfig(service, pServiceConfig, bytesNeeded, &bytesNeeded)) {
        std::wcerr << L"QueryServiceConfig failed: " << GetLastError() << std::endl;
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        return;
    }

    strncpy_s(
        details->executable_path,
        sizeof(details->executable_path),
        pServiceConfig->lpBinaryPathName,
        sizeof(details->executable_path) - 1
    );

    strncpy_s(
        details->service_account,
        sizeof(details->service_account),
        pServiceConfig->lpServiceStartName,
        sizeof(details->service_account)
    );
    

    char displayName[256];
    DWORD displayNameSize = sizeof(displayName) / sizeof(displayName[0]);
    if (GetServiceDisplayNameA(sc_manager, service_name, displayName, &displayNameSize)) {
        strncpy_s(
            details->service_display_name,
            sizeof(details->service_display_name),
            displayName,
            sizeof(details->service_display_name) - 1 
        );
        
    }
    CloseServiceHandle(sc_manager);
    CloseServiceHandle(service);
} 


std::wstring ConvertLPSTRToWString(const LPSTR ansiStr) {
    if (ansiStr == nullptr) {
        return std::wstring();
    }

    int sizeNeeded = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }

    std::wstring wideStr(sizeNeeded, L'\0');

    MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, &wideStr[0], sizeNeeded);

    return wideStr;
}

extern "C" _declspec() bool EnumerateServiceNames(wchar_t*** serviceNames, int* count) {
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!sc_manager) {
        std::cerr << "OpenSCManager failed with error: " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;

    EnumServicesStatus(
        sc_manager,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        NULL,
        0, 
        &bytesNeeded, 
        &servicesReturned, 
        &resumeHandle
    );

    DWORD last_error = GetLastError();
    if (last_error != ERROR_MORE_DATA) {
        std::cerr << "EnumServiceStatus failed with error: " << last_error << std::endl;
        CloseServiceHandle(sc_manager);
        return false;
    }

    std::vector<BYTE> buffer(bytesNeeded);
    ENUM_SERVICE_STATUS* serviceStatus = reinterpret_cast<ENUM_SERVICE_STATUS*>(buffer.data());

    if (!EnumServicesStatus(
        sc_manager, 
        SERVICE_WIN32, 
        SERVICE_STATE_ALL, 
        serviceStatus, 
        bytesNeeded, 
        &bytesNeeded, 
        &servicesReturned, 
        &resumeHandle
    )) {
        std::cerr << "EnumServicesStatus failed with error: " << GetLastError() << std::endl;
        CloseServiceHandle(sc_manager);
        return false;
    }

    wchar_t** namesArray = new wchar_t*[servicesReturned];
    for (DWORD i = 0; i < servicesReturned; ++i) {

        std::wstring service_name = ConvertLPSTRToWString(serviceStatus[i].lpServiceName);

        size_t len = wcslen(service_name.c_str());
        namesArray[i] = new wchar_t[len + 1];
        wmemcpy(namesArray[i], service_name.c_str(), len + 1);
    }

    *serviceNames = namesArray;
    *count = servicesReturned;

    CloseServiceHandle(sc_manager);
    return true;

}

extern "C" _declspec() void FreeServiceNamesArray(wchar_t** serviceNames, int count) {
    for (int i = 0; i < count; ++i) {
        delete[] serviceNames[i];
    }
    delete[] serviceNames;
}
