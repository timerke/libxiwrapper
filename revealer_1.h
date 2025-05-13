#ifndef REVEALER_1_H
#define REVEALER_1_H

#if defined(_WIN32) || defined(WIN64)
#pragma comment(lib, "Ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <stdio.h>
#include <stdlib.h>


#if defined(_WIN32) || defined(WIN64)
#define TIMEOUT_MS 500
#endif
#define ADDRESS_SIZE 16
#define BUFFER_SIZE 100
#define DEFAULT_PORT 8008


#if defined (WIN32) || defined(WIN64)
  #if defined(xiwrapper_EXPORTS)
    #define BINDYC_EXPORT __declspec(dllexport)
  #else
    #define BINDYC_EXPORT __declspec(dllimport)
  #endif
#else
 #define BINDYC_EXPORT
#endif

#if defined(__cplusplus)
extern "C" {
#endif


typedef struct BINDYC_EXPORT {
    int allocated_count;
    int count;
    char** addresses;
} FoundAddresses;


void allocateMemoryForAddresses(FoundAddresses** found_addresses);
int bindSocket(SOCKET sock, const char* adapter_addr);
void closeSocket(SOCKET sock);
SOCKET createUPDSocketForBroadcast();
void BINDYC_EXPORT freeMemoryForAddresses(FoundAddresses* found_addresses);
void increaseMemoryForAddresses(FoundAddresses* found_addresses);
void printFoundAddresses(FoundAddresses* found_addresses);
void receiveResponses(SOCKET sock, FoundAddresses* found_addresses);
void saveAddress(const char* address, FoundAddresses* found_addresses);
void BINDYC_EXPORT searchByRevealer1Protocol(const char* adapter_addr, FoundAddresses** found_addresses);
int sendMessageToBroadcast(SOCKET sock);


#if defined(__cplusplus)
};
#endif

void allocateMemoryForAddresses(FoundAddresses** found_addresses)
{
    FoundAddresses* p_found_addresses = (FoundAddresses*)malloc(sizeof(FoundAddresses));
    p_found_addresses->allocated_count = 3;
    p_found_addresses->count = 0;

    char** p_addresses = (char**)malloc(p_found_addresses->allocated_count * sizeof(char*));
    for (int i = 0; i < p_found_addresses->allocated_count; i++) {
        p_addresses[i] = (char*)malloc(ADDRESS_SIZE * sizeof(char));
    }
    p_found_addresses->addresses = p_addresses;

    *found_addresses = p_found_addresses;
}


int bindSocket(SOCKET sock, const char* adapter_addr)
{
    sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(adapter_addr);
    sock_addr.sin_port = htons(0);
    int result = bind(sock, (SOCKADDR*)&sock_addr, sizeof(sock_addr));
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to bind socket to address = '%hs' (error code = %d)\n", adapter_addr, WSAGetLastError());
    }
    return result;
}


void closeSocket(SOCKET sock)
{
    int result = closesocket(sock);
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to close socket (error code = %d)\n", WSAGetLastError());
    }

    WSACleanup();
}


SOCKET createUPDSocketForBroadcast()
{
    // Declare and initialize variables
    WSADATA wsaData = { 0 };
    // Initialize Winsock
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        wprintf(L"WSAStartup failed: %d\n", result);
        return INVALID_SOCKET;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        wprintf(L"Failed to create socket (error code = %d)\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    ULONG value = 1;
    result = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&value, sizeof(value));
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to set BROADCAST option to socket (error code = %d)\n", WSAGetLastError());
        closeSocket(sock);
        return INVALID_SOCKET;
    }

    DWORD timeout_ms = TIMEOUT_MS;
    result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to set timeout to socket (error code = %d)\n", WSAGetLastError());
        closeSocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}


void freeMemoryForAddresses(FoundAddresses* found_addresses)
{
    if (found_addresses == NULL)
        return;

    for (int i = 0; i < found_addresses->allocated_count; i++) {
        free(found_addresses->addresses[i]);
    }
    free(found_addresses);
}


void increaseMemoryForAddresses(FoundAddresses* found_addresses)
{
    found_addresses->allocated_count = (int)(1.5 * found_addresses->allocated_count);
    found_addresses->addresses = (char**)realloc(found_addresses->addresses, found_addresses->allocated_count * sizeof(char*));
    for (int i = found_addresses->count; i < found_addresses->allocated_count; i++) {
        found_addresses->addresses[i] = (char*)malloc(ADDRESS_SIZE * sizeof(char));
    }
}


void printFoundAddresses(FoundAddresses* found_addresses)
{
    wprintf(L"Number of addresses found: %d\n", found_addresses->count);
    for (int i = 0; i < found_addresses->count; i++) {
        wprintf(L"#%d. %hs\n", i + 1, found_addresses->addresses[i]);
    }
}


void receiveResponses(SOCKET sock, FoundAddresses* found_addresses)
{
    while (true) {
        char recv_buffer[BUFFER_SIZE];
        sockaddr_in sender_addr;
        int sender_addr_size = sizeof(sender_addr);
        int result = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&sender_addr, &sender_addr_size);
        if (result == SOCKET_ERROR) {
            wprintf(L"Failed to receive response (error code = %d)\n", WSAGetLastError());
            break;
        }

        char sender_ip[ADDRESS_SIZE];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        saveAddress(sender_ip, found_addresses);
    }
}


void saveAddress(const char* address, FoundAddresses* found_addresses)
{
    if (found_addresses->count >= found_addresses->allocated_count) {
        increaseMemoryForAddresses(found_addresses);
    }

    strcpy(found_addresses->addresses[found_addresses->count], address);
    found_addresses->count++;
}


void searchByRevealer1Protocol(const char* adapter_addr, FoundAddresses** found_addresses)
{
    SOCKET sock = createUPDSocketForBroadcast();

    if (bindSocket(sock, adapter_addr) == SOCKET_ERROR) {
        closeSocket(sock);
        return;
    }

    if (sendMessageToBroadcast(sock) == SOCKET_ERROR) {
        closeSocket(sock);
        return;
    }

    allocateMemoryForAddresses(found_addresses);
    receiveResponses(sock, *found_addresses);
    closeSocket(sock);
}


int sendMessageToBroadcast(SOCKET sock)
{
    sockaddr_in sock_addr;
    socklen_t len = sizeof(sock_addr);
    int result = getsockname(sock, (struct sockaddr*)&sock_addr, &len);
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to get socket name (error code = %d)\n", WSAGetLastError());
        return result;
    }

    char buffer[BUFFER_SIZE];
    sprintf(buffer, "DISCOVER_CUBIELORD_REQUEST %u\0", ntohs(sock_addr.sin_port));

    sockaddr_in to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    to_addr.sin_port = htons(DEFAULT_PORT);
    result = sendto(sock, buffer, strlen(buffer), 0, (const sockaddr*)&to_addr, sizeof(to_addr));
    if (result == SOCKET_ERROR) {
        wprintf(L"Failed to send message (error code = %d)\n", WSAGetLastError());
    }

    return result;
}

#endif // !REVEALER_1_H
