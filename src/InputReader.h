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

#pragma once

#include <QThread>

//Forward declartion
class QMutex;

//Typedef
typedef int (__stdcall *FunCancelSynchronousIo)(void *hThread);

//Class CInputReader
class CInputReader : public QThread
{
	Q_OBJECT;

public:
	CInputReader(void);
	~CInputReader(void);

	size_t readAllData(QByteArray &output);
	void abort(void);

signals:
	void dataAvailable(quint32 newBytes);

public slots:
	void start(Priority priority = InheritPriority);

protected:
	virtual void run(void);

	volatile bool m_aborted;
	QByteArray *m_data;
	QMutex *m_dataLock;

	FunCancelSynchronousIo m_cancelSyncIo;
	void *m_threadHandle;
};
