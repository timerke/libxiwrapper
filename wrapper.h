#ifndef INC_BINDYC_WRAPPER_H
#define INC_BINDYC_WRAPPER_H

#include <stdint.h>

#define conn_id_invalid 0
typedef uint32_t conn_id_t;

// MSVC symbols export
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

void BINDYC_EXPORT bindy_close(conn_id_t conn_id, int close_timeout);
int BINDYC_EXPORT bindy_enumerate(const char* addr, int enum_timeout, uint8_t** ptr);
int BINDYC_EXPORT bindy_enumerate_specify_adapter(const char* addr, const char* adapter_addr, int enum_timeout, uint8_t** ptr);
void BINDYC_EXPORT bindy_free(uint8_t** ptr);
int BINDYC_EXPORT bindy_init();
uint32_t BINDYC_EXPORT bindy_open(const char* addr, uint32_t serial, int open_timeout);
size_t BINDYC_EXPORT bindy_read(conn_id_t conn_id, uint8_t* buf, size_t size);
int BINDYC_EXPORT bindy_setkey(const char* name);
int BINDYC_EXPORT bindy_write(conn_id_t conn_id, const uint8_t* buf, size_t size);
bool BINDYC_EXPORT find_key(const char* hints, const char* key, char* buf, unsigned int length);

/**
 * Structure for storing IP addresses found by the Revealer-1 protocol.
 */
typedef struct BINDYC_EXPORT {
    int allocated_count;  // number of IP addresses for which memory is allocated
    int count;            // number of IP addresses recorded
    char** addresses;     // pointer to array of IP addresses
} Revealer1Addresses;

/**
 * The function frees the memory allocated for the Revealer1Addresses structure.
 * @param[in] addresses Pointer to Revealer1Addresses structure.
 */
void BINDYC_EXPORT bindy_freeMemoryForRevealer1Addresses(Revealer1Addresses* addresses);

/**
 * The function outputs all IP addresses from the Revealer1Addresses structure.
 * @param[in] addresses Pointer to Revealer1Addresses structure.
 */
void BINDYC_EXPORT bindy_printRevealer1Addresses(Revealer1Addresses* addresses);

/**
 * The function searches for IP addresses of devices using the Revealer-1 protocol.
 * @param[in] adapter_addr IP address of the network adapter.
 * @param[out] addresses Pointer to Revealer1Addresses structure.
 */
void BINDYC_EXPORT bindy_searchByRevealer1Protocol(const char* adapter_addr, Revealer1Addresses** addresses);

#if defined(__cplusplus)
};
#endif


#endif  // !INC_BINDYC_WRAPPER_H
