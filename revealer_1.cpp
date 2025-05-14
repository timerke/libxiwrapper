#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "revealer_1.h"
#include "wrapper.h"


void bindy_freeMemoryForRevealer1Addresses(Revealer1Addresses* addresses)
{
    if (addresses == NULL)
        return;

    for (int i = 0; i < addresses->allocated_count; i++) {
        free(addresses->addresses[i]);
    }
    free(addresses);
}


void bindy_printRevealer1Addresses(Revealer1Addresses* addresses)
{
    if (!addresses) {
        printf("No addresses found by Revealer-1 protocol\n");
        return;
    }

    printf("Number of addresses found by Revealer-1 protocol: %d\n", addresses->count);
    for (int i = 0; i < addresses->count; i++) {
        printf("#%d. %s\n", i + 1, addresses->addresses[i]);
    }
}


void bindy_searchByRevealer1Protocol(const char* adapter_addr, Revealer1Addresses** addresses)
{
    SOCKET sock = createUPDSocketForBroadcast();
    if (sock == INVALID_SOCKET) {
        return;
    }

    if (bindSocket(sock, adapter_addr) == SOCKET_ERROR) {
        closeSocket(sock);
        return;
    }

    if (sendMessageToBroadcast(sock) == SOCKET_ERROR) {
        closeSocket(sock);
        return;
    }

    allocateMemoryForRevealer1Addresses(addresses);
    receiveResponses(sock, *addresses);
    closeSocket(sock);
}


void allocateMemoryForRevealer1Addresses(Revealer1Addresses** addresses)
{
    Revealer1Addresses* p_addresses = (Revealer1Addresses*)malloc(sizeof(Revealer1Addresses));
    p_addresses->allocated_count = 10;
    p_addresses->count = 0;

    char** ip_addresses = (char**)malloc(p_addresses->allocated_count * sizeof(char*));
    for (int i = 0; i < p_addresses->allocated_count; i++) {
        ip_addresses[i] = (char*)malloc(ADDRESS_SIZE * sizeof(char));
    }
    p_addresses->addresses = ip_addresses;

    *addresses = p_addresses;
}


int bindSocket(SOCKET sock, const char* adapter_addr)
{
    sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(0);
    inet_pton(AF_INET, adapter_addr, &(sock_addr.sin_addr));
    int result = bind(sock, (sockaddr*)&sock_addr, sizeof(sock_addr));
    if (result == SOCKET_ERROR) {
        printf("Failed to bind socket to address = '%s'\n", adapter_addr);
    }
    return result;
}


void closeSocket(SOCKET sock)
{
    int result;
#if defined(_WIN32) || defined(WIN64)
    result = closesocket(sock);
#else
    result = close(sock);
#endif
    if (result == SOCKET_ERROR) {
        printf("Failed to close socket\n");
    }

#if defined(_WIN32) || defined(WIN64)
    WSACleanup();
#endif
}


SOCKET createUPDSocketForBroadcast()
{
#if defined(_WIN32) || defined(WIN64)
    // Declare and initialize variables
    WSADATA wsaData = { 0 };
    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return INVALID_SOCKET;
    }
#endif

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        printf("Failed to create socket\n");
        return INVALID_SOCKET;
    }

    unsigned long value = 1;
    int result = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&value, sizeof(value));
    if (result == SOCKET_ERROR) {
        printf("Failed to set BROADCAST option to socket\n");
        closeSocket(sock);
        return INVALID_SOCKET;
    }

#if defined(_WIN32) || defined(WIN64)
    unsigned long rec_timeout = TIMEOUT_MS;
#else
    timeval rec_timeout;
    rec_timeout.tv_sec = 0;
    rec_timeout.tv_usec = TIMEOUT_MS * 1000;
#endif
    result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&rec_timeout, sizeof(rec_timeout));
    if (result == SOCKET_ERROR) {
        printf("Failed to set timeout to socket\n");
        closeSocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}


void increaseMemoryForRevealer1Addresses(Revealer1Addresses* addresses)
{
    addresses->allocated_count = (int)(1.5 * addresses->allocated_count);
    addresses->addresses = (char**)realloc(addresses->addresses, addresses->allocated_count * sizeof(char*));
    for (int i = addresses->count; i < addresses->allocated_count; i++) {
        addresses->addresses[i] = (char*)malloc(ADDRESS_SIZE * sizeof(char));
    }
}


void receiveResponses(SOCKET sock, Revealer1Addresses* addresses)
{
    while (true) {
        char recv_buffer[BUFFER_SIZE];
        sockaddr_in sender_addr;
#if defined(_WIN32) || defined(WIN64)
        int sender_addr_size;
#else
        unsigned int sender_addr_size;
#endif
        sender_addr_size = sizeof(sender_addr);
        int result = recvfrom(sock, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&sender_addr, &sender_addr_size);
        if (result == SOCKET_ERROR) {
            printf("Failed to receive response\n");
            break;
        }

        char sender_ip[ADDRESS_SIZE];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));
        saveAddress(sender_ip, addresses);
    }
}


void saveAddress(const char* address, Revealer1Addresses* addresses)
{
    if (addresses->count >= addresses->allocated_count) {
        increaseMemoryForRevealer1Addresses(addresses);
    }

#if defined(_WIN32) || defined(WIN64)
    strcpy_s(addresses->addresses[addresses->count], ADDRESS_SIZE, address);
#else
    strcpy(addresses->addresses[addresses->count], address);
#endif
    addresses->count++;
}


int sendMessageToBroadcast(SOCKET sock)
{
    sockaddr_in sock_addr;
    socklen_t len = sizeof(sock_addr);
    int result = getsockname(sock, (sockaddr*)&sock_addr, &len);
    if (result == SOCKET_ERROR) {
        printf("Failed to get socket name\n");
        return result;
    }

    char buffer[BUFFER_SIZE];
#if defined(_WIN32) || defined(WIN64)
    sprintf_s(buffer, BUFFER_SIZE, "DISCOVER_CUBIELORD_REQUEST %u\0", ntohs(sock_addr.sin_port));
#else
    sprintf(buffer, "DISCOVER_CUBIELORD_REQUEST %u\0", ntohs(sock_addr.sin_port));
#endif

    sockaddr_in to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    to_addr.sin_port = htons(DEFAULT_PORT);
    result = sendto(sock, buffer, strlen(buffer), 0, (const sockaddr*)&to_addr, sizeof(to_addr));
    if (result == SOCKET_ERROR) {
        printf("Failed to send message\n");
    }

    return result;
}