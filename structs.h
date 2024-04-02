#ifndef STRUCTS_H
#define STRUCTS_H

#define conn_id_invalid 0
#define CURRENT_PROTOCOL_VERSION 0x00000001

typedef uint32_t conn_id_t;

namespace data_pkt {
	enum {
		RawData                          = 0x00000000,
		OpenDeviceRequest                = 0x00000001,
		OpenDeviceAnswer                 = 0x000000FF,
		CloseDeviceRequest               = 0x00000002,
		CloseDeviceAnswer                = 0x000000FE,
		EnumerateRequest                 = 0x00000003,
		EnumerateAnswer                  = 0x000000FD,
		DeviceReadWriteErrorNotification = 0x00000004,
	};
}

#ifdef _MSC_VER
  #define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#else
  #define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

PACK(
struct my_device
{
	char Manufacturer[4];
	char ManufacturerId[2];
	char ProductDescription[8];
	uint8_t Major;
	uint8_t Minor;
	uint16_t Release;
	uint8_t reserved[12];
});
typedef struct my_device my_device_information_t;

PACK(
struct my_cname
{
	char ControllerName[16];
	uint8_t CtrlFlags;
	uint8_t reserved[7];
});
typedef struct my_cname my_controller_name_t;

PACK(
struct my_sname
{
	char PositionerName[16];
	uint8_t reserved[8];
});
typedef struct my_sname my_stage_name_t;

PACK(
struct device_desc {
	uint32_t serial;
	my_device_information_t my_device_information;
	my_controller_name_t my_cname;
	my_stage_name_t my_sname;
	uint32_t ipv4; // passed in network byte order
	uint32_t reserved1;
	char nodename[16];
	uint32_t axis_state;
	char locker_username[16];
	char locker_nodename[16];
	uint64_t locked_time;
	uint8_t reserved2[30];
});
typedef struct device_desc device_description;

PACK(
struct common_h {
	uint32_t protocol_version;
	uint32_t packet_type;
	uint32_t reserved3;
	uint32_t serial;
	uint32_t reserved4 [2];
});
typedef struct common_h common_header;

// return codes
#define ret_ok 0
#define ret_bindy_error -1
#define ret_nodevice -2




#endif
