#pragma once
#pragma warning(disable: 4996)
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fstream>
#include <vector>


#define TAG_AMF				"AMF"
#define TAG_RTMP			"RTMP"

#define LOGGER_FIEL			"./rtmp.log"


/**
* @brief:
* Logger for console and file
*/
class CLogger
{
public:
	typedef enum _log_level
	{
		LOG_LEVEL_DEBUG = 0x01,
		LOG_LEVEL_WARNING = 0x02,
		LOG_LEVEL_ERROR = 0x04
	} log_level_t;

public:
	CLogger();
	virtual ~CLogger();

	static CLogger* get_instance();

	void add_level(log_level_t level);
	void remove_level(log_level_t level);
	bool has_level(log_level_t level);

	void add_tag(const char *tag);
	void remove_tag(const char *tag);

	void log(log_level_t level, const char *tag, const char *fmt, ...);

	// Console
	bool open_console();
	void close_console();

	// File
	bool open_file(const char *path_ptr);
	void close_file();

protected:
	bool _found_tag(const char *tag, bool remove = false);

protected:
	static CLogger *_instance_ptr;

	HANDLE _console;
	std::ofstream _file;
	uint32_t _levels;
	std::vector<std::string> _tags;
};


#define LogD(tag, fmt, ...)		CLogger::get_instance()->log(CLogger::LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LogW(tag, fmt, ...)		CLogger::get_instance()->log(CLogger::LOG_LEVEL_WARNING, tag, fmt, ##__VA_ARGS__)
#define LogE(tag, fmt, ...)		CLogger::get_instance()->log(CLogger::LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)


