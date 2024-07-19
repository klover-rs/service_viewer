#include <iostream>
#include <windows.h>

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
        std::cerr << "failed to open service\n";
         return false;
    }

    SERVICE_STATUS_PROCESS status;
    DWORD bytes_needed;

    if (!ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status)) {
        CloseServiceHandle(service);
        CloseServiceHandle(sc_manager);
        std::cerr << "failed to run ControlService\n";
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