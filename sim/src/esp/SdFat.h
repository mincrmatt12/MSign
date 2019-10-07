#ifndef MSGN_SDFAT_SHIM
#define MSGN_SDFAT_SHIM

#define O_WRITE 0x01
#define O_READ 0x02
#define O_TRUNC 0x04
#define O_CREAT 0x00

#define FILE_READ O_READ
#define FILE_WRITE O_WRITE | O_TRUNC

#include <fstream>
#include <memory>
#include <Arduino.h>
#include <sys/types.h>
#include <sys/stat.h>
#define File SdFile

static inline uint32_t secureRandom(uint32_t x) {
	return x;
}

struct SdFile : public Stream {
	SdFile() {

	}
	SdFile(const char * name, int mode) {
		std::ios::openmode m;
		this->fst.reset(new std::fstream());
		this->_name = name;

		if (mode & O_READ) m |= std::ios::in;
		if (mode & O_WRITE) m |= std::ios::out;
		if (mode & O_TRUNC) m |= std::ios::trunc;
		if (name[0] == '/') {
			char buf[strlen(name) + 3];
			buf[0] = 's'; buf[1] = 'd';
			strcpy(&buf[2], name);

			fst->open(buf, m);
		}
		else {
			char buf[strlen(name) + 4];
			buf[0] = 's'; buf[1] = 'd'; buf[2] = '/';
			strcpy(&buf[3], name);

			fst->open(buf, m);
		}

		// get size
		fst->seekg (0, std::ios::end);
		bytes = fst->tellg();
		fst->seekg(std::ios::beg);
	}
	SdFile(const SdFile& other) : fst{other.fst}, bytes{other.bytes}, _name{other._name} {
	}
	~SdFile() {close();}

	void truncate(int) {;} // no-op
	int read() override {
		char out = fst->get();
		if (fst->eof()) return -1;
		return (int)out;
	}
	size_t write(uint8_t c) override {
		fst->put(c);
		if (fst->fail()) return 0;
		return 1;
	}
	void write(const char * buf, size_t amt) {
		fst->write(buf, amt);
	}
	int available() override {
		return (int)fst->tellg() - bytes;
	}
	int peek() override {return fst->peek();};
	void seek(int pos) {
		fst->seekg(pos);
	}
	size_t write(const uint8_t * buf, size_t amt) override {
		fst->write((char *)buf, amt);
		return fst->gcount();
	}
	void read(void * buf, size_t amt) {
		fst->read((char *)buf, amt);
	}

	void close() {fst->close();};
	size_t size() {return bytes;}
	const char * name() {return _name.c_str();}

	bool isOpen() {
		if (!fst.get()) return false;
		return fst->is_open();
	}

	void remove() {
		if (isOpen()) fst->close();
		std::string c = _name;
		if (c[0] == '/') c = "sd" + c;
		else c = "sd/" + c;
		std::remove(c.c_str());
	}
private:
	std::shared_ptr<std::fstream> fst;
	size_t bytes;
	std::string _name;
};

template<int A, int B, int C>
struct SdFatSoftSpi {
	bool exists(const char * place) {
		std::string c = place;
		if (c[0] == '/') c = "sd" + c;
		else c = "sd/" + c;

		struct stat info;
		return stat(c.c_str(), &info) == 0;
	}
	bool chdir(const char * place="") {
		_chdir = place;
	}
	void begin(int pin) {;}
	void remove(const char * name) {
		std::string c = name;
		if (c[0] == '/') c = "sd" + c;
		else c = "sd/" + c;
		std::remove(c.c_str());
	}

	void mkdir(const char * dir) {
		std::string c = dir;
		if (c[0] == '/') c = "sd" + c;
		else c = "sd/" + c;

		mkdir(c.c_str());
	}

	SdFile open(const char * name, int mode=FILE_READ) {
		if (_chdir.size() == 0 || name[0] == '/') {
			return SdFile(name, mode);
		}
		if (_chdir[_chdir.size() - 1] == '/') {
			return SdFile((_chdir + name).c_str(), mode);
		}
		else {
			return SdFile((_chdir + '/' + name).c_str(), mode);
		}
	}

private:
	std::string _chdir;
};

#endif
