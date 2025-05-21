#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <bindy.h>
#include "structs.h"
#include "wrapper.h"
#include <map>
//#include "iobuf.h"
#include <deque>
#include "tinythread.h"
#include <exception>
#include <stdexcept>

#if defined(COMPILER_SUPPORTS_CXX0X) && !defined(COMPILER_SUPPORTS_CXX11)
#define nullptr NULL
#endif

/// Crossplatform sleep
void sleep_ms(size_t ms)
{
#if defined(WIN32) || defined(WIN64)
	Sleep((DWORD)ms);
#else
	usleep(1000 * ms);
#endif
}

std::string trim (std::string s, const std::string& trails)
{
	size_t ind = s.find_first_not_of(trails);
	if (ind != std::string::npos)
		s.erase(0, ind);
	ind = s.find_last_not_of(trails);
	if (ind != std::string::npos)
		s.erase(ind+1);
	else
		s.clear();
	return s;
}

// hints format: "key1=val1\nkey2=val2"
std::map<std::string, std::string> read_hints(std::string source)
{
	std::map<std::string, std::string> parameters;
	for (int pos = 0, last_pos = 0; pos != -1; )
	{
		pos = source.find_first_of("\n\r", pos+1);
		std::string next = trim(source.substr( last_pos, pos == -1 ? -1 : pos-last_pos ),
				" \t");
		if (!next.empty())
		{
			size_t inner_delim_pos = next.find("=");
			if (inner_delim_pos != std::string::npos)
			{
				std::string key = trim(next.substr(0, inner_delim_pos), " \t");
				std::string val = trim(next.substr(inner_delim_pos+1), " \t");
				parameters[key] = val;
			}
		}
		last_pos = pos ? pos + 1 : 0;
	}
	return parameters;
}

#if defined(__cplusplus)
extern "C" {
#endif

// Timeout in milliseconds
#define SLEEP_WAIT_TIME_MS 100

// Total wait timeout until connection is established
#define SEND_WAIT_TIMEOUT_MS 5000

bindy::Bindy* instance = NULL;
char* keyfile = NULL;


class Device {
public:
    Device(uint32_t _serial);
    ~Device();

//private:
	uint32_t serial;
//	IO_BUF buffer;
	std::deque<uint8_t> *buffer; // tmp
	tthread::mutex *mutex;

};

Device::Device(uint32_t _serial) {
	this->serial = _serial;
	buffer = new std::deque<uint8_t>;
	mutex = new tthread::mutex;
}

Device::~Device() {
	delete buffer;
	delete mutex;
}

typedef tthread::lock_guard<tthread::mutex> tlock;

typedef struct enum_struct {
	bool recv;
	uint8_t* ptr;
	size_t size;

	enum_struct()
		: recv(false), ptr(NULL), size(0)
	{
	}
} enum_struct;
std::map<bindy::conn_id_t, enum_struct> s_enum;
std::map<uint32_t, bool> open_ok;

std::map<bindy::conn_id_t, Device*> device_by_conn;
tthread::mutex global_mutex;

inline void uint32_to_buf(uint32_t value, uint8_t * p) {
	*(p+0) = ((value & 0xFF000000) >> 24);
	*(p+1) = ((value & 0x00FF0000) >> 16);
	*(p+2) = ((value & 0x0000FF00) >> 8);
	*(p+3) = ((value & 0x000000FF) >> 0);
}

inline void read_uint32(uint32_t * value, uint8_t * p) {
	*value = ((uint32_t)(p[0]<<24)) | ((uint32_t)(p[1]<<16)) | ((uint32_t)(p[2]<<8)) | ((uint32_t)(p[3]<<0));
}

int adaptive_wait_send(conn_id_t conn_id, std::vector<uint8_t> data, int timeout_ms)
{
	bool send_ok = false;
	int delay = 2;
	int total_delay = 0;
	while (!send_ok && (total_delay + delay < timeout_ms)) {
		try {
			instance->send_data(conn_id, data);
			send_ok = true;
		} catch (std::runtime_error &e) {
			sleep_ms(delay);
			delay = delay * 1.5;
			total_delay += delay;
		}
	}
	return total_delay;
}

void callback_data(conn_id_t conn_id, std::vector<uint8_t> data) {
	tlock lock(global_mutex);
//	assert(data.size() >= 4); // We need at least the command code
	uint32_t protocol_ver;
	uint32_t command_code;
	uint32_t serial;
	read_uint32(&protocol_ver, &data[0]);
	read_uint32(&command_code, &data[4]);
	read_uint32(&serial, &data[12]); // strictly speaking it might read junk in case of enumerate_reply or something else which does not have the serial... if someone sends us such packet
	switch (command_code) {
		case data_pkt::RawData: {
			Device *d;
			if (device_by_conn.count(conn_id) == 0) {
				d = new Device(serial);
			} else {
				d = device_by_conn[conn_id];
			}

			d->mutex->lock();
			d->buffer->insert( d->buffer->end(), data.begin()+sizeof(common_header), data.end() );
			d->mutex->unlock();
			break;
		}
		case data_pkt::OpenDeviceAnswer: {
			Device *d;
			if (device_by_conn.count(conn_id) == 0) {
				d = new Device(serial);
			} else {
				d = device_by_conn[conn_id];
			}

			open_ok[serial] = data.at(27); // according to exchange protocol v1
			break;
		}
		case data_pkt::EnumerateAnswer: { // safe to modify s_enum because we're under global mutex
			s_enum[conn_id].recv = true;
			s_enum[conn_id].size = data.size();
			s_enum[conn_id].ptr = (uint8_t*)malloc(s_enum[conn_id].size);
			memcpy(s_enum[conn_id].ptr, &data.at(0), s_enum[conn_id].size);
			break;
		}
		case data_pkt::CloseDeviceAnswer:
		case data_pkt::DeviceReadWriteErrorNotification: {
			if (device_by_conn.count(conn_id) == 0)
				break;
			Device *d = device_by_conn[conn_id];

			device_by_conn.erase(conn_id);
			delete d;

			break;
		}
		default: {
			//DEBUG( "DBG> pkt code unknown" );
			break;
		}
	}
}

int bindy_init()
{
	if (instance != NULL)
		return true; // assumes old bindy is alive
	if (keyfile == NULL)
	{   // can't work without key set
        if (!bindy_setkey(":memory:")) 
		{
			return false;
		}
	}
	try {
		bindy::Bindy::initialize_network();

		instance = new bindy::Bindy(keyfile, false, false); // is_server == false, is_buffered == false
		instance->set_handler(&callback_data);
	} catch (...) {
		instance = NULL;
		return false;
	}

	return true;
}

int bindy_setkey(const char* name) {
	size_t len = strlen(name);
	if (keyfile != NULL)
		free(keyfile);
	keyfile = (char*)malloc(len+1);
	if (NULL == keyfile)
		return false;
	strncpy(keyfile, name, len);
	keyfile[len] = '\0';
	return true;
}

void sleep_until_recv(conn_id_t conn_id, int timeout)
{
	int time_elapsed = 0;
	bool flag;
	do {
		time_elapsed += SLEEP_WAIT_TIME_MS;
		sleep_ms(SLEEP_WAIT_TIME_MS);
		tlock lock(global_mutex);
		if (s_enum.count(conn_id) == 0)
			flag = false;
		else
			flag = s_enum[conn_id].recv;
	} while (!flag && (time_elapsed < timeout));
}

void sleep_until_open(uint32_t serial, int timeout)
{
	int time_elapsed = 0;
	bool flag;
	do {
		time_elapsed += SLEEP_WAIT_TIME_MS;
		sleep_ms(SLEEP_WAIT_TIME_MS);
		tlock lock(global_mutex);
		if (open_ok.count(serial) == 0)
			flag = false;
		else
			flag = open_ok[serial];
	} while ( (false == flag) && (time_elapsed < timeout) );
}

int bindy_enumerate(const char * addr, int enum_timeout, uint8_t ** ptr)
{
	return bindy_enumerate_specify_adapter(addr, "", enum_timeout, ptr);
}

int bindy_enumerate_specify_adapter(const char* addr, const char* adapter_addr, int enum_timeout, uint8_t** ptr)
{
	if (!bindy_init())
		return -1;

	uint32_t devices = 0;
	*ptr = nullptr;
	uint8_t* buf = NULL;
	conn_id_t enum_conn_id = conn_id_invalid;
	try {
		std::vector<uint8_t> s(7 * 4, 0);
		uint32_to_buf(CURRENT_PROTOCOL_VERSION, &s.at(0));
		uint32_to_buf(data_pkt::EnumerateRequest, &s.at(4));

		enum_conn_id = instance->connect(addr, adapter_addr);
		int initial_timeout = adaptive_wait_send(enum_conn_id, s, enum_timeout); // send enum request
		sleep_until_recv(enum_conn_id, enum_timeout - initial_timeout);

		tlock lock(global_mutex);
		if (!s_enum[enum_conn_id].recv) {
			s_enum.erase(enum_conn_id);
			return 0;
		}

		int recv_size = s_enum[enum_conn_id].size;
		// collect whatever we received into a temp buffer
		std::vector<uint8_t> vtmp_buf(recv_size);
		memcpy(&vtmp_buf.at(0), s_enum[enum_conn_id].ptr, recv_size);

		// crop first x bytes, because we want to return structs
		int head_len = 16; // according to exchange protocol v1
		if (recv_size >= head_len) {
			read_uint32(&devices, &vtmp_buf.at(12)); // according to exchange protocol v1

			buf = (uint8_t*)malloc(recv_size - head_len);
			std::copy(vtmp_buf.begin() + head_len, vtmp_buf.end(), buf);
		} else {
			buf = NULL;
		}

		// clean enum struct
		free(s_enum[enum_conn_id].ptr);
		s_enum[enum_conn_id].ptr = NULL;
		s_enum.erase(enum_conn_id);

		// return the data
		*ptr = buf;

		instance->disconnect(enum_conn_id);
	} catch (...) {
		// std::cerr << "Exception in network enumerate: " << std::endl;
	}

	return devices;
}

void bindy_free(uint8_t **ptr)
{
	free(*ptr);
	*ptr = nullptr;
}

uint32_t bindy_open(const char * addr, uint32_t serial, int open_timeout)
{
	uint32_t conn_id = conn_id_invalid;
	if (false == bindy_init())
		return conn_id;

	std::vector<uint8_t> request(sizeof(common_header), 0);
	uint32_to_buf(CURRENT_PROTOCOL_VERSION, &request.at(0));
	uint32_to_buf(data_pkt::OpenDeviceRequest, &request.at(4));
	uint32_to_buf(serial, &request.at(12));

	global_mutex.lock();
	open_ok[serial] = false;
	global_mutex.unlock();
	int initial_timeout;
	try {
		conn_id = instance->connect(addr);
		initial_timeout = adaptive_wait_send(conn_id, request, open_timeout);
	} catch (...) {
		instance->disconnect(conn_id);
		return conn_id_invalid;
	}

	sleep_until_open(serial, open_timeout-initial_timeout);

	tlock lock(global_mutex);
	bool ok = open_ok[serial];
	open_ok.erase(serial);

	if (!ok) {
		instance->disconnect(conn_id);
		return conn_id_invalid;
	}
	device_by_conn[conn_id] = new Device(serial);

	return conn_id;
}

int bindy_write(conn_id_t conn_id, const uint8_t* buf, size_t size)
{
	if (false == bindy_init())
		return ret_bindy_error;
	int retcode = ret_ok;

	uint32_t serial;
	global_mutex.lock();
	if (device_by_conn.count(conn_id) == 0) {
		retcode = ret_nodevice;
	} else {
		serial = device_by_conn[conn_id]->serial;
	}
	global_mutex.unlock();
	if (retcode != ret_ok)
		return retcode;

	std::vector<uint8_t> s(sizeof(common_header)+size, 0);
	uint32_to_buf(CURRENT_PROTOCOL_VERSION, &s.at(0));
	uint32_to_buf(data_pkt::RawData, &s.at(4));
	uint32_to_buf(serial, &s.at(12));
	std::copy(buf, buf + size, s.begin() + sizeof(common_header));

	bool is_ok = true;
	try {
		adaptive_wait_send(conn_id, s, SEND_WAIT_TIMEOUT_MS);
	} catch (...) {
		is_ok = false;
	}

	return is_ok;
}

size_t bindy_read(conn_id_t conn_id, uint8_t* buf, size_t size)
{
	if (false == bindy_init())
		return ret_bindy_error;

	int retcode = ret_ok;
	global_mutex.lock();
	Device *d;
	if (device_by_conn.count(conn_id) == 0)
		retcode = ret_nodevice;
	else {
		d = device_by_conn[conn_id];
		d->mutex->lock();
	}
	global_mutex.unlock();
	if (retcode != ret_ok)
		return retcode;

	if (d->buffer->size() < size)
		size = d->buffer->size();
	if (size > 0) {
		for (int i = 0; i < size; i++)
			buf[i] = d->buffer->at(i);
		d->buffer->erase(d->buffer->begin(), d->buffer->begin()+size);
	}
	d->mutex->unlock();
	return size;
}

void bindy_close(conn_id_t conn_id, int close_timeout)
{
	if (false == bindy_init())
		return;
	bool should_break = false;

	Device *d;
	global_mutex.lock();
	if (device_by_conn.count(conn_id) == 0) {
		should_break = true; // no such device, nothing to do -- return now
	} else {
		d = device_by_conn[conn_id];
	}
	global_mutex.unlock();
	if (should_break)
		return;

	std::vector<uint8_t> request(sizeof(common_header), 0);
	uint32_to_buf(CURRENT_PROTOCOL_VERSION, &request.at(0));
	uint32_to_buf(data_pkt::CloseDeviceRequest, &request.at(4));
	uint32_to_buf(device_by_conn[conn_id]->serial, &request.at(12));

	try {
		instance->send_data(conn_id, request); // send close request
		instance->disconnect(conn_id);
	} catch (...) {
		; // whatever; server will close the device when socket is closed
	}
	global_mutex.lock();
	device_by_conn.erase(conn_id);
	delete d;
	global_mutex.unlock();
}

/*
  Temporary wrapper as a solution for finding key in libximc using c++ functions.
*/
bool find_key(const char* hints, const char* key, char* buf, unsigned int length)
{
	const auto hinted_parameters = read_hints(hints);
	auto hint_addr = hinted_parameters.find(key);
	if (hint_addr == hinted_parameters.end())
		return false;
	strncpy(buf, hint_addr->second.c_str(), length);
	return true;
}


#if defined(__cplusplus)
};
#endif
