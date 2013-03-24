///////////////////////////////////////////////////////////////////////////////
// Logging Utility
// Copyright (C) 2010-2013 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "InputReader.h"

//Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//Qt
#include <QMutex>
#include <QMutexLocker>

/*
 * Constructor
 */
CInputReader::CInputReader(void)
:
	m_aborted(false),
	m_threadHandle(INVALID_HANDLE_VALUE),
	m_cancelSyncIo(NULL)
{
	m_dataLock = new QMutex();
	m_data = new QByteArray();

	if(HMODULE krnl32 = GetModuleHandleA("Kernel32.dll"))
	{
		m_cancelSyncIo = (FunCancelSynchronousIo) GetProcAddress(krnl32, "CancelSynchronousIo");
	}
}

/*
 * Destructor
 */
CInputReader::~CInputReader(void)
{
	delete m_data;
	delete m_dataLock;

	if(m_threadHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_threadHandle);
	}
}

/*
 * Start thread
 */
void CInputReader::start(Priority priority)
{
	m_aborted = false;
	QThread::start(priority);
}

/*
 * Abort Thread
 */
void CInputReader::abort(void)
{
	m_aborted = true;
	if(m_cancelSyncIo && (m_threadHandle != INVALID_HANDLE_VALUE))
	{
		m_cancelSyncIo(m_threadHandle);
	}
}

/*
 * Thread entry point
 */
void CInputReader::run(void)
{
	//Setup thread handle
	if(!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_threadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
	{
		m_threadHandle = INVALID_HANDLE_VALUE;
	}
	
	//Setup local variables
	BYTE buffer[1024];
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	DWORD bytesRead = 0;
	
	//Main processing loop
	while(!m_aborted)
	{
		if(ReadFile(h, buffer, 1024, &bytesRead, NULL))
		{
			if(bytesRead > 0)
			{
				QMutexLocker lock(m_dataLock);
				m_data->append(reinterpret_cast<const char*>(buffer), bytesRead);
				lock.unlock();
				emit dataAvailable(bytesRead);
				continue;
			}
		}
		break;
	}
}

/*
 * Read all data currently available
 */
size_t CInputReader::readAllData(QByteArray &output)
{
	QMutexLocker lock(m_dataLock);
	const size_t bytes = m_data->size();
	output.append(m_data->constData(), bytes);
	m_data->clear();
	return bytes;
}
