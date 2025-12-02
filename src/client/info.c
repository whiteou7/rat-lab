#include <windows.h>
#include <stdio.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <lmcons.h>
#include "client/info.h"

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "iphlpapi.lib")

char* print_client_info() {
    char computerName[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    char userName[UNLEN + 1] = {0};
    char publicIP[64] = "N/A";
    char localIP[64] = "N/A";
    char osInfo[256] = {0};
    char domain[256] = "N/A";
    char integrityLevel[32] = "Unknown";
    DWORD size;
    HINTERNET hInternet, hConnect;

    // 1. Computer Name
    size = sizeof(computerName);
    if (!GetComputerNameA(computerName, &size)) {
        strcpy(computerName, "Unknown");
    }

    // 2. Username
    size = UNLEN + 1;
    if (!GetUserNameA(userName, &size)) {
        strcpy(userName, "Unknown");
    }

    // 3. Public IP
    hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        hConnect = InternetOpenUrlA(hInternet, "https://api.ipify.org", NULL, 0, 
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
        if (hConnect) {
            DWORD bytesRead = 0;
            if (InternetReadFile(hConnect, publicIP, sizeof(publicIP)-1, &bytesRead)) {
                publicIP[bytesRead] = '\0';
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }

    // 4. Local IP
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    ULONG bufLen = sizeof(IP_ADAPTER_INFO);
    
    if (pAdapterInfo) {
        if (GetAdaptersInfo(pAdapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
            free(pAdapterInfo);
            pAdapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
        }
        
        if (pAdapterInfo && GetAdaptersInfo(pAdapterInfo, &bufLen) == NO_ERROR) {
            PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
            while (pAdapter) {
                PIP_ADDR_STRING pAddr = &pAdapter->IpAddressList;
                while (pAddr) {
                    if (strcmp(pAddr->IpAddress.String, "0.0.0.0") != 0 &&
                        strcmp(pAddr->IpAddress.String, "") != 0) {
                        strcpy(localIP, pAddr->IpAddress.String);
                        break;
                    }
                    pAddr = pAddr->Next;
                }
                if (strcmp(localIP, "N/A") != 0) break;
                pAdapter = pAdapter->Next;
            }
        }
        if (pAdapterInfo) free(pAdapterInfo);
    }

    // 5. OS Version
    typedef NTSTATUS (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        if (RtlGetVersion) {
            RTL_OSVERSIONINFOW osvi = {0};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (RtlGetVersion(&osvi) == 0) {
                snprintf(osInfo, sizeof(osInfo), "Windows %lu.%lu Build %lu",
                         osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
            }
        }
    }
    if (osInfo[0] == '\0') {
        strcpy(osInfo, "Unknown");
    }

    // 6. Domain / Workgroup
    size = sizeof(domain);
    if (!GetComputerNameExA(ComputerNameDnsDomain, domain, &size) || domain[0] == '\0') {
        strcpy(domain, "WORKGROUP");
    }

    // 7. Process Integrity Level
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD dwSize = 0;
        GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwSize);
        if (dwSize) {
            PTOKEN_MANDATORY_LABEL pTIL = (PTOKEN_MANDATORY_LABEL)malloc(dwSize);
            if (pTIL && GetTokenInformation(hToken, TokenIntegrityLevel, pTIL, dwSize, &dwSize)) {
                DWORD integrityRID = *GetSidSubAuthority(pTIL->Label.Sid, 
                    (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid)-1));
                
                if (integrityRID >= SECURITY_MANDATORY_HIGH_RID)
                    strcpy(integrityLevel, "High (Administrator)");
                else if (integrityRID >= SECURITY_MANDATORY_MEDIUM_RID)
                    strcpy(integrityLevel, "Medium (Standard User)");
                else
                    strcpy(integrityLevel, "Low");
            }
            if (pTIL) free(pTIL);
        }
        CloseHandle(hToken);
    }

    // Build return string
    size_t len = strlen(computerName) + strlen(userName) + strlen(domain) +
                 strlen(publicIP) + strlen(localIP) + strlen(osInfo) +
                 strlen(integrityLevel) + 200;

    char* buffer = (char*)malloc(len);
    if (!buffer) return NULL;

    snprintf(
        buffer,
        len,
        "Hostname      : %s\n"
        "Username      : %s\n"
        "Domain        : %s\n"
        "Public IP     : %s\n"
        "Local IP      : %s\n"
        "OS            : %s\n"
        "Integrity     : %s\n"
        "PID           : %lu\n",
        computerName,
        userName,
        domain,
        publicIP,
        localIP,
        osInfo,
        integrityLevel,
        GetCurrentProcessId()
    );

    return buffer;
}