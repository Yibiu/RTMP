#pragma once
//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


typedef long long tick_t;

#define GET_TIME(T,S,F) ((double)((T)-(S))/(double)(F/1000))


class CTimeStatistics
{
public:
	CTimeStatistics()
	{
		m_llStart = 0;
		m_llStop = 0;
	}
	virtual ~CTimeStatistics() {};

	inline void StartTimeMeasurement()
	{
		m_llStart = vr_time_get_tick();
	}

	inline double GetDeltaTime()
	{
		return GET_TIME(vr_time_get_tick(), m_llStart, vr_time_get_frequency());
	}

	inline void StopTimeMeasurement()
	{
		m_llStop = vr_time_get_tick();
	}

	inline double GetMesurementTime()
	{
		return GET_TIME(m_llStop, m_llStart, vr_time_get_frequency());
	}

	inline void ResetTimeStatistics()
	{
		m_llStart = 0;
		m_llStop = 0;
	}

protected:
	tick_t vr_time_get_tick()
	{
		LARGE_INTEGER t1;

		QueryPerformanceCounter(&t1);
		return t1.QuadPart;
	}

	tick_t vr_time_get_frequency()
	{
		LARGE_INTEGER t1;

		QueryPerformanceFrequency(&t1);
		return t1.QuadPart;
	}

private:
	tick_t		m_llStart;
	tick_t		m_llStop;
};
