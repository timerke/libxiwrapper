#ifndef INC_BINDYC_WRAPPER_H
#define INC_BINDYC_WRAPPER_H

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

int BINDYC_EXPORT bindy_init();
int BINDYC_EXPORT bindy_setkey(const char * name);
int BINDYC_EXPORT bindy_enumerate_specify_adapter(const char * addr, const char * adapter_addr, int enum_timeout, uint8_t ** ptr);
int BINDYC_EXPORT bindy_enumerate(const char * addr, int enum_timeout, uint8_t ** ptr);
void BINDYC_EXPORT bindy_free(uint8_t **ptr);
uint32_t BINDYC_EXPORT bindy_open(const char * addr, uint32_t serial, int open_timeout);
int BINDYC_EXPORT bindy_write(conn_id_t conn_id, const uint8_t* buf, size_t size);
size_t BINDYC_EXPORT bindy_read(conn_id_t conn_id, uint8_t* buf, size_t size);
void BINDYC_EXPORT bindy_close(conn_id_t conn_id, int close_timeout);
bool BINDYC_EXPORT find_key(const char* hints, const char* key, char* buf, unsigned int length);

#if defined(__cplusplus)
};
#endif


#endif
