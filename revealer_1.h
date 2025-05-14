#ifndef REVEALER_1_H
#define REVEALER_1_H

#if defined(_WIN32) || defined(WIN64)
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "wrapper.h"


#if defined(_WIN32) || defined(WIN64)

#else
typedef int SOCKET;
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR -1
#endif

#define ADDRESS_SIZE 16
#define BUFFER_SIZE 100
#define DEFAULT_PORT 8008
#define TIMEOUT_MS 500


/**
 * The function allocates memory for the structure into which IP addresses will be written.
 * @param[out] addresses Pointer to Revealer1Addresses structure.
 */
void allocateMemoryForRevealer1Addresses(Revealer1Addresses** addresses);

/**
 * The function binds socket to the specified network adapter address.
 * @param[in] sock Socket.
 * @param[in] adapter_addr IP address of the network adapter.
 * \return If SOCKET_ERROR, then the function failed to execute.
 */
int bindSocket(SOCKET sock, const char* adapter_addr);

/**
 * The function closes the socket.
 * @param[in] sock Socket.
 */
void closeSocket(SOCKET sock);

/**
 * The function creates a UDP socket for sending broadcast messages.
 * \return Socket.
 */
SOCKET createUPDSocketForBroadcast();

/**
 * The function increases the memory allocated to the Revealer1Addresses structure.
 * @param[in] addresses Pointer to Revealer1Addresses structure.
 */
void increaseMemoryForRevealer1Addresses(Revealer1Addresses* addresses);

/**
 * The function receives responses to a broadcast message and stores the IP addresses from which the responses came.
 * @param[in] sock Socket.
 * @param[out] addresses Pointer to Revealer1Addresses structure.
 */
void receiveResponses(SOCKET sock, Revealer1Addresses* addresses);

/**
 * The function saves the IP address to the Revealer1Addresses structure.
 * @param[in] address IP address.
 * @param[in] addresses Pointer to Revealer1Addresses structure.
 */
void saveAddress(const char* address, Revealer1Addresses* addresses);

/**
 * The function sends a broadcast message using the Revealer-1 protocol.
 * @param[in] sock Socket.
 */
int sendMessageToBroadcast(SOCKET sock);

#endif // !REVEALER_1_H
