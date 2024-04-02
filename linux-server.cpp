#include "bindy.h"
#include "ximc.h"
#include "tinythread.h"

// common headers [inherited from libximc]
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
// some more
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <stddef.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_link.h>

#include <map>
#include <assert.h>
#include <exception>
// User defined types used both in server and client part of the wrapper
#include "structs.h"

typedef int handle_t;
#define handle_invalid ((handle_t)-1)

using std::cout;
using std::endl;
#define DEBUG(text) { ; }
//#define DEBUG(text) { cout << text << endl; }

bindy::Bindy * pb = NULL;
uint32_t ext_ip;
char host[NI_MAXHOST];
typedef uint8_t byte;

#define RW_THREAD_SLEEP_TIME 1000
#define READ_BUF_SIZE 1024
#define SEND_WAIT_TIMEOUT_MS 5000

inline void buf2str (std::vector<uint8_t> &buf, std::string &str) {
	for (int i=0; i<buf.size(); i++) {
		str.push_back((char)buf.at(i));
	}
}

inline void str2buf (std::string &str, std::vector<uint8_t> &buf) {
	for (int i=0; i<str.size(); i++) {
		buf.push_back(str.at(i));
	}
}

inline void void2str (void* v, unsigned int length, std::string &str) {
	for (int i=0; i<length; i++) {
		str.push_back(  *(char*)((uint8_t*)v+i)  );
	}
}

inline void void2buf (void* v, unsigned int length, std::vector<uint8_t> &buf) {
	for (int i=0; i<length; i++) {
		buf.push_back(  *(uint8_t*)((uint8_t*)v+i)  );
	}
}

inline void read_uint32(uint32_t * value, byte * p) {
	*value = ((uint32_t)(p[0]<<24)) | ((uint32_t)(p[1]<<16)) | ((uint32_t)(p[2]<<8)) | ((uint32_t)(p[3]<<0));
}

inline void read_uint16(uint16_t * value, byte * p) {
	*value = ((uint16_t)(p[0]<<8)) | ((uint16_t)(p[1]<<0));
}

inline void read_uint8(uint8_t * value, byte * p) {
	*value = (uint8_t)p[0];
}

inline void write_uint16(byte * p, uint16_t value) {
	*(p+0) = ((value & 0xFF00) >> 8);
	*(p+1) = ((value & 0x00FF) >> 0);
}

inline void write_uint32(byte * p, uint32_t value) {
	*(p+0) = ((value & 0xFF000000) >> 24);
	*(p+1) = ((value & 0x00FF0000) >> 16);
	*(p+2) = ((value & 0x0000FF00) >> 8);
	*(p+3) = ((value & 0x000000FF) >> 0);
}

inline void write_bytes(byte * p, byte * value, uint32_t size) { // does not check target length
	memcpy(p, value, size);
}

inline void write_bool(byte * p, bool value) {
	*p = value;
}

int adaptive_wait_send (bindy::Bindy* bindy, conn_id_t conn_id, std::vector<uint8_t> data, int timeout_ms)
{
	bool send_ok = false;
	int delay = 2;
	int total_delay = 0;
	while (!send_ok && (total_delay + delay < timeout_ms)) {
		try {
			DEBUG( "send_data attempt" );
			bindy->send_data(conn_id, data);
			DEBUG( "send_data OK" );
			send_ok = true;
		} catch (std::exception &e) {
			DEBUG( "send_data FAIL" );
			bindy::sleep_ms(delay);
			delay = delay*1.5;
			total_delay += delay;
		}
	}
	if (!send_ok)
		throw std::exception();
	return total_delay;
}

void dev_to_net_function(void *arg);
void buf_to_dev_function(void *arg);
void get_ext_ip();

class Device {
public:
	Device(conn_id_t conn_id, uint32_t serial);
	~Device();

	conn_id_t conn_id;
	uint32_t serial;
	bool should_exit;
	int handle;
	std::vector<uint8_t> * net_to_dev_buffer; // a buffer with the data read from the device and sent to the network
	tthread::mutex * net_to_dev_mutex; // mutex to sync buffer usage
	tthread::thread * buf_to_dev_thread; // a thread which gets data from the "net_to_dev" buffer and writes it into the controller
	tthread::thread * dev_to_net_thread; // a thread which reads data from the controller and sends it to the network
private:
	Device(const Device & other); // please do not copy Device class, it makes no sense
	Device & operator= (const Device & other); // do not assign it either
};

// All thread-safe locks happen inside Supermap; Device class doesn't know about it
Device::Device(conn_id_t _conn_id, uint32_t _serial) {
	this->conn_id = _conn_id;
	this->serial = _serial;
	char devstr[9]; // large enough to hold a uint32_t serial in hex + terminating null, so 9 bytes
	sprintf(devstr, "%08X", serial);

	std::string addr = std::string("/dev/ximc/").append(devstr);
	handle = open(addr.c_str(), O_RDWR | O_DSYNC | O_NOCTTY);
	if (handle == -1) {
		throw std::exception();
	}
	// Lock the device
	if (flock( handle, LOCK_EX|LOCK_NB ) == -1 && errno == EWOULDBLOCK) {
		close( handle );
		throw std::exception();
	}
	if (flock( handle, LOCK_EX ) == -1) {
		close( handle );
		throw std::exception();
	}

	// Set non-blocking read mode
	struct termios options;
	if (fcntl(handle, F_SETFL, 0) == -1) {
		close( handle );
		throw std::exception();
	}
	if (tcgetattr(handle, &options) == -1) {
		close( handle );
		throw std::exception();
	}
	// set port flags
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	options.c_cflag |= (CLOCAL | CREAD);
 	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag &= ~(PARENB | PARODD);
	options.c_cflag |= CSTOPB;
	options.c_cflag &= ~CRTSCTS;

	options.c_iflag &= ~(IXON | IXOFF | IXANY);
	options.c_iflag &= ~(INPCK | PARMRK | ISTRIP | IGNPAR);
	options.c_iflag &= ~(IGNBRK | BRKINT | INLCR | IGNCR | ICRNL | IMAXBEL);

	options.c_oflag &= ~OPOST;

	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = 0;
	if (tcsetattr(handle, TCSAFLUSH, &options) == -1) {
		close( handle );
		throw std::exception();
	}
	tcflush(handle, TCIOFLUSH);
	// Set non-blocking read mode end

	should_exit = false;
	dev_to_net_thread = new tthread::thread(dev_to_net_function, this);
	net_to_dev_mutex = new tthread::mutex();
	net_to_dev_mutex->lock();
	net_to_dev_buffer = new std::vector<uint8_t>;
	buf_to_dev_thread = new tthread::thread(buf_to_dev_function, this);
	net_to_dev_mutex->unlock();
}

Device::~Device() {
	// set exit bit
	should_exit = true;

	// wait for read-write threads
	dev_to_net_thread->join();
	buf_to_dev_thread->join();

	// close the device
	close(handle);

	// delete threads and mutexes
	delete dev_to_net_thread;
	delete net_to_dev_mutex;
	delete net_to_dev_buffer;
	delete buf_to_dev_thread;
}


class Supermap {
public:
	Device* findDevice(conn_id_t conn_id, uint32_t serial);
	bool addDevice(conn_id_t conn_id, uint32_t serial);
	void removeDevice(conn_id_t conn_id, uint32_t serial);
	void removeConnection(conn_id_t conn_id);

private:
	std::map<conn_id_t, std::map<uint32_t, Device*> > conndevmap;
	tthread::mutex map_mutex;
};

Device* Supermap::findDevice(conn_id_t conn_id, uint32_t serial) {
	map_mutex.lock();
	Device* ptr = NULL;
	if (conndevmap.count(conn_id) > 0)
		if (conndevmap[conn_id].count(serial) > 0)
			ptr = conndevmap[conn_id][serial];
	map_mutex.unlock();
	return ptr;
}

// returns true if addition was successful, or device already exists and available; otherwise returns false
bool Supermap::addDevice(conn_id_t conn_id, uint32_t serial) {
	map_mutex.lock();
	if (conndevmap.count(conn_id) > 0)
		if (conndevmap[conn_id].count(serial) > 0)
			return true;
	Device *d;
	try {
		d = new Device(conn_id, serial);
	} catch (...) {
		d = NULL;
	}
	if (d != NULL)
		conndevmap[conn_id][serial] = d;
	map_mutex.unlock();
	return (d != NULL);
}

void Supermap::removeDevice(conn_id_t conn_id, uint32_t serial) {
	map_mutex.lock();
	if (conndevmap.count(conn_id) > 0) {
		if (conndevmap[conn_id].count(serial) > 0) {
			delete conndevmap[conn_id][serial];
			conndevmap[conn_id].erase(serial);
		}
	}
	map_mutex.unlock();
}

void Supermap::removeConnection(conn_id_t conn_id) {
	map_mutex.lock();
	if (conndevmap.count(conn_id) > 0) {
		std::map<uint32_t, Device*> *map = &conndevmap[conn_id];
		std::map<uint32_t, Device*>::iterator it;
		for ( it = map->begin(); it != map->end(); ++it) {
			delete it->second;
			conndevmap[conn_id].erase(it->first);
		}
		conndevmap.erase(conn_id);
	}
	map_mutex.unlock();
}

Supermap supermap;
tthread::mutex global_mutex;

template <int P>
class CommonDataPacket {
public:
	bool send_data() {
		if (pb == NULL) {
			DEBUG ( "pb == NULL in send_data()" );
			return false;
		}
		try {
			adaptive_wait_send(pb, conn_id, reply, SEND_WAIT_TIMEOUT_MS);
		} catch (std::exception &e) {
			DEBUG ( "Exception in send_data" );
			; // what exactly? different for different packet types?
			return false;
		}
		return true;
	}; // true if send is successful

protected:
	std::vector<uint8_t> reply;
	conn_id_t conn_id;
};

template <int P>
class DataPacket : public CommonDataPacket <P> { }; // Still allows us to instantiate common packet, which is wrong


// RawData packet ctor
template <>
class DataPacket <data_pkt::RawData> : public CommonDataPacket <data_pkt::RawData> {
public:
	DataPacket(conn_id_t conn_id, uint32_t serial, uint8_t* ptr, uint32_t size) {
		this->conn_id = conn_id;

		int len = sizeof(common_header) + size;
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::RawData);
		write_uint32(&reply.at(12), serial);
		write_bytes(&reply.at(sizeof(common_header)), ptr, size);
	}
};

/*
// SERVER DOES NOT USE THIS (YET)
template <>
class DataPacket <data_pkt::OpenDeviceRequest> : public CommonDataPacket<data_pkt::OpenDeviceRequest> {
public:
	DataPacket(conn_id_t conn_id, bool opened_ok) {
		this->conn_id = conn_id;

		int len = sizeof(common_header);
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::OpenDeviceRequest);
		write_uint32(&reply.at(12), serial);
	}
};
*/

template <>
class DataPacket <data_pkt::OpenDeviceAnswer> : public CommonDataPacket<data_pkt::OpenDeviceAnswer> {
public:
	DataPacket(conn_id_t conn_id, uint32_t serial, bool opened_ok) {
		this->conn_id = conn_id;

		int len = sizeof(common_header) + sizeof(uint32_t);
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::OpenDeviceAnswer);
		write_uint32(&reply.at(12), serial);
		write_bool(&reply.at(len-1), opened_ok);
	}
};

/*
// SERVER DOES NOT USE THIS (YET)
template <>
class DataPacket <data_pkt::CloseDeviceRequest> : public CommonDataPacket<data_pkt::CloseDeviceRequest> {
public:
	DataPacket(conn_id_t conn_id, uint32_t serial) {
		this->conn_id = conn_id;

		int len = sizeof(common_header);
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::CloseDeviceRequest);
		write_uint32(&reply.at(12), serial);
	}
};
*/

template <>
class DataPacket <data_pkt::CloseDeviceAnswer> : public CommonDataPacket<data_pkt::CloseDeviceAnswer> {
public:
	DataPacket(conn_id_t conn_id, uint32_t serial) {
		this->conn_id = conn_id;

		int len = sizeof(common_header) + sizeof(uint32_t);
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::CloseDeviceAnswer);
		write_uint32(&reply.at(12), serial);

	}
};

/*
// SERVER DOES NOT USE THIS (YET)
template <>
class DataPacket <data_pkt::EnumerateRequest> : public CommonDataPacket<data_pkt::EnumerateRequest> {
public:
	DataPacket(conn_id_t conn_id) {
		this->conn_id = conn_id;

		int len = 7*4;
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::EnumerateRequest);
	}
};
*/


template <>
class DataPacket <data_pkt::EnumerateAnswer> : public CommonDataPacket<data_pkt::EnumerateAnswer> {
public:
	DataPacket(conn_id_t conn_id, uint32_t device_count, std::vector<device_description> descriptions) {
		this->conn_id = conn_id;

		int len = 16 + (sizeof (device_description)) * (descriptions.size());
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::EnumerateAnswer);
		write_uint32(&reply.at(12), device_count);

		// write all structs
		if (descriptions.size() > 0)
			write_bytes(&reply.at(16), (byte*)&descriptions.at(0), (sizeof (device_description))*descriptions.size() ); // this works, but looks wrong
	}
};


template <>
class DataPacket <data_pkt::DeviceReadWriteErrorNotification> : public CommonDataPacket<data_pkt::DeviceReadWriteErrorNotification> {
public:
	DataPacket(conn_id_t conn_id, uint32_t serial) {
		this->conn_id = conn_id;

		int len = sizeof(common_header);
		reply.resize(len);
		std::fill (reply.begin(), reply.end(), 0x00);

		write_uint32(&reply.at(0), CURRENT_PROTOCOL_VERSION);
		write_uint32(&reply.at(4), data_pkt::DeviceReadWriteErrorNotification);
		write_uint32(&reply.at(12), serial);
	}
};



// ========================================================
void buf_to_dev_function(void *arg) {
	int n, err;
	Device *p = (Device*) arg;

	while (!p->should_exit) {
		p->net_to_dev_mutex->lock();

		std::vector<uint8_t> * buf = p->net_to_dev_buffer;
		if (buf->size() > 0) {
			n = write( p->handle, &(buf->at(0)), buf->size() );
			err = errno;
			if (n > 0) {
				buf->erase(buf->begin(), buf->begin()+n); // erase from the beginning
			} else if (n == 0) { // "zero indicates nothing was written"
				;
			} else { // n == -1  means error
				DEBUG( "WRITE ERROR" );
				p->should_exit = true;
				DataPacket<data_pkt::DeviceReadWriteErrorNotification>(p->conn_id, p->serial).send_data();
			}
		}
		p->net_to_dev_mutex->unlock();
		usleep(RW_THREAD_SLEEP_TIME);
	}
}

// ========================================================
void dev_to_net_function(void *arg) {
	int n, err;
	Device *p = (Device*) arg;
	byte buf[READ_BUF_SIZE];

	while (!p->should_exit) {
		assert(READ_BUF_SIZE < SSIZE_MAX);
		n = read( p->handle, buf, READ_BUF_SIZE );
		err = errno;

		if (n > 0) {
			DataPacket<data_pkt::RawData>(p->conn_id, p->serial, buf, n).send_data();
		}
		usleep(RW_THREAD_SLEEP_TIME);
	}
}
// ========================================================
void callback_data(conn_id_t conn_id, std::vector<uint8_t> data) {
	DEBUG( "Got " << data.size() << " bytes; conn_id = " << conn_id );
//	DEBUG( "DATA: " << bindy::hex_encode(data) );

	assert(data.size() >= 16); // We need at least the protocol version and command code... and serial too
	uint32_t protocol_ver;
	uint32_t command_code;
	uint32_t serial;
	read_uint32(&protocol_ver, &data[0]);
	assert(CURRENT_PROTOCOL_VERSION == protocol_ver);
	/*if (CURRENT_PROTOCOL_VERSION > protocol_ver) {
		;
	}*/
	read_uint32(&command_code, &data[4]);
	read_uint32(&serial, &data[12]); // strictly speaking it might read junk in case of enumerate_reply or something else which does not have the serial... if someone sends us such packet
	switch (command_code) {
		case data_pkt::RawData: {
			Device * d = supermap.findDevice(conn_id, serial);
			if (d == NULL) {
				DEBUG( "conn_id = " << conn_id << ", serial = " << std::to_string(serial) );
				DEBUG( "Request for Rawdata to unopened serial, aborting" );
				throw std::exception();
			}
			d->net_to_dev_mutex->lock(); // effectively this is net_to_buf function part
			d->net_to_dev_buffer->insert( d->net_to_dev_buffer->end(), data.begin()+sizeof(common_header), data.end() );
			d->net_to_dev_mutex->unlock(); // "function" end
			break;
		}
		case data_pkt::OpenDeviceRequest: {
			DataPacket<data_pkt::OpenDeviceAnswer>(conn_id, serial, supermap.addDevice(conn_id, serial)).send_data();
			break;
		}
		case data_pkt::CloseDeviceRequest: {
			DataPacket<data_pkt::CloseDeviceAnswer>(conn_id, serial).send_data();
			supermap.removeDevice(conn_id, serial);
			break;
		}
		case data_pkt::EnumerateRequest: {
			global_mutex.lock();
			device_enumeration_t dev_enum = enumerate_devices(0, NULL);
			global_mutex.unlock();
			uint32_t devs = get_device_count(dev_enum);
			DEBUG( "found " << devs << " devices" );

			std::vector<device_description> descriptions;
			for (int i=0; i<devs; i++) {
				/*
				get_enumerate_device_serial         (dev_enum, i, &descriptions.at(i).serial);
				get_enumerate_device_information    (dev_enum, i, &descriptions.at(i).device_information);
				sprintf(reinterpret_cast<char*>(&descriptions.at(i).device_name), "\\\\net\\%s\\%08X", ext_ip, descriptions.at(i).serial);
				*/
				// TODO: check for libximc function result errors
				// if (result_ok != get_...) {

				device_description desc;

				device_information_t device_information;
				controller_name_t cname;
				stage_name_t sname;

				memset(&desc, 0x00, sizeof(desc));

				std::string name = get_device_name(dev_enum, i);
				device_t id = open_device(name.c_str());
				if (id == device_undefined) {
					continue;
				}
				//get_serial_number      (id, &desc.serial);
				// Do not read serial, trust udev instead
				DEBUG( "name = " << name );
				std::string xicom = "xi-com:///dev/ximc/";
				name.replace(name.begin(), name.begin()+xicom.length(), std::string());
				std::size_t pos;
				desc.serial = std::stoi(name, &pos, 16);
				DEBUG( "serial = " << desc.serial );
				result_t res;
				bool device_good = true;

				res = get_device_information (id, &device_information);
				device_good &= (res == result_ok);

				res = get_controller_name    (id, &cname);
				device_good &= (res == result_ok);

				res = get_stage_name         (id, &sname);
				device_good &= (res == result_ok);

				close_device(&id);
				if (device_good) {
					memcpy(&desc.my_device_information.Manufacturer, device_information.Manufacturer, sizeof(device_information.Manufacturer) );
					memcpy(&desc.my_device_information.ManufacturerId, device_information.ManufacturerId, sizeof(device_information.ManufacturerId) );
					memcpy(&desc.my_device_information.ProductDescription, device_information.ProductDescription, sizeof(device_information.ProductDescription) );
					desc.my_device_information.Major = device_information.Major;
					desc.my_device_information.Minor = device_information.Minor;
					desc.my_device_information.Release = device_information.Release;

					memcpy(&desc.my_cname.ControllerName, cname.ControllerName, sizeof(cname.ControllerName) );
					desc.my_cname.CtrlFlags = cname.CtrlFlags;

					memcpy(&desc.my_sname.PositionerName, sname.PositionerName, sizeof(sname.PositionerName) );
				} else {
					strcpy(&desc.my_cname.ControllerName[0], "NON-XIMC DEVICE");
				}

				get_ext_ip();
				DEBUG("ext ip = " << ext_ip);
				desc.ipv4 = ext_ip; // ext_ip is always in network-byte order, because we extract it that way
				// do not use write_uint32() with it, it will fail on little-endian systems

				descriptions.push_back(desc);
			}
			DataPacket<data_pkt::EnumerateAnswer>(conn_id, descriptions.size(), descriptions).send_data();
			break;
		}
		default: {
			DEBUG( "DBG> pkt code unknown" );
			break;
		}
	}
}
// ========================================================
void callback_disc(conn_id_t conn_id) {
	supermap.removeConnection(conn_id);
}

void get_ext_ip() {
   struct ifaddrs *ifaddr, *ifa;
   int family, s, n;
   sockaddr* addr;

   if (getifaddrs(&ifaddr) == -1) {
	   perror("getifaddrs");
	   exit(EXIT_FAILURE);
   }

   for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
	   addr = ifa->ifa_addr;
	   if (addr == NULL)
		   continue;

       family = ifa->ifa_addr->sa_family;
	   if (family == AF_INET /*|| family == AF_INET6*/) {
		   s = getnameinfo(ifa->ifa_addr,
				   (family == AF_INET) ? sizeof(struct sockaddr_in) :
										 sizeof(struct sockaddr_in6),
				   host, NI_MAXHOST,
				   NULL, 0, NI_NUMERICHOST);
		   if (s != 0) {
			   printf("getnameinfo() failed: %s\n", gai_strerror(s));
			   exit(EXIT_FAILURE);
		   }
			if (addr->sa_family == AF_INET) {
				struct sockaddr_in sa;
				inet_pton(AF_INET, host, &(sa.sin_addr));
				ext_ip = sa.sin_addr.s_addr;
				//printf("found ip %s \n", host);
			}
		}
   }
	freeifaddrs(ifaddr);
}

void ignore_sigpipe()
{
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPIPE, &act, NULL);
}

int main (int argc, char *argv[])
{
	ignore_sigpipe();
	get_ext_ip();
	if (argc != 2) {
		printf("Usage: %s keyfile\n", argv[0]);
		exit(0);
	} else {
		bindy::Bindy bindy(argv[1], true, false);
		bindy.connect();
		pb = &bindy;
		bindy.set_handler(&callback_data);
		bindy.set_discnotify(&callback_disc);
		cout << "SERVER started, using addr " << host << endl;
	}
	cout << "Exiting." << endl;
	return 0;
}
