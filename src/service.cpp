#include <iostream>
#include <windows.h>
#include <string>
#include <memory>
#include <vector>

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

extern "C" _declspec() bool start_service(const char* service_name) {
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return false;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name, SERVICE_START);
    if (!service) {
        CloseServiceHandle(sc_manager);
        return false;
    }

    if (!StartService(service, 0, NULL)) {
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        return false;
    }

    CloseServiceHandle(sc_manager);
    CloseServiceHandle(service);

    return true;
}

extern "C" _declspec() bool stop_service(const char* service_name) {
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return false;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(sc_manager);
        std::cerr << "failed to open service" << GetLastError() << "\n";
         return false;
    }

    SERVICE_STATUS_PROCESS status;
    DWORD bytes_needed;

    if (!ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status)) {
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        std::cerr << "failed to run ControlService" << GetLastError() << "\n";
        return false;
    }

    while (QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytes_needed)) {
        if (status.dwCurrentState == SERVICE_STOPPED) {
            break;
        }
        Sleep(10);
    }

    CloseServiceHandle(sc_manager);
    CloseServiceHandle(service);

    return true;

}

enum ServiceStartType {
    AutoStart = 2,
    DemandStart = 3,
    Disabled = 4
};

extern "C" _declspec() bool changeServiceStartType(const char* service_name, ServiceStartType startType) {
    SC_HANDLE sc_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!sc_manager) {
        std::cerr << "failed to open sc manager\n";
        return false;
    }

    SC_HANDLE service = OpenServiceA(sc_manager, service_name, SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(sc_manager);
        std::cerr << "failed to open service\n";
         return false;
    }

    BOOL bSuccess = ChangeServiceConfigA(
        service,
        SERVICE_NO_CHANGE,
        (DWORD)startType,
        SERVICE_NO_CHANGE,
        NULL,             
        NULL,            
        NULL,             
        NULL,            
        NULL,             
        NULL,
        NULL          
    );

    CloseServiceHandle(sc_manager);
    CloseServiceHandle(service);

    if (!bSuccess) {
        std::cerr << "ChangeServiceConfig failed: " << GetLastError() << std::endl;
        return false;
    } else {
        std::cout << "Changed service type successfully.\n";
        return true;
    }

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
        std::cerr << "failed to open service\n";
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
