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

#include <QObject>

//Forward declaration
class QProcess;
class QTextDecoder;
class QStringList;
class QTextStream;
class QFile;
class QEventLoop;
class CInputReader;

//Class CLogProcessor
class CLogProcessor : public QObject
{
	Q_OBJECT

public:
	CLogProcessor(QFile &logFile);
	~CLogProcessor(void);

	bool startProcess(const QString &program, const QStringList &arguments);
	bool startStdinProcessing(void);
	
	int exec(void);

	void setCaptureStreams(const bool captureStdout, const bool captureStderr);
	void setSimplifyStrings(const bool simplify);
	void setVerboseOutput(const bool verbose);
	void setFilterStrings(const QString &regExpKeep, const QString &regExpSkip);
	bool setTextCodecs(const char *inputCodec, const char *outputCodec);

public slots:
	void forceQuit(const bool silent = false);


private slots:
	void readFromStdout(void);
	void readFromStderr(void);
	void readFromStdinp(void);

	void processFinished(int exitCode);
	void readerFinished(void);

private:
	void flushBuffers(void);
	void processData(const QByteArray &data, const int channel);
	void logString(const QString &data, const int channel);

	QProcess *m_process;
	CInputReader *m_stdinReader;
	
	bool m_logStdout;
	bool m_logStderr;
	bool m_simplify;
	bool m_logIsEmpty;
	bool m_verbose;
	
	QTextDecoder *m_codecStdout;
	QTextDecoder *m_codecStderr;
	QTextDecoder *m_codecStdinp;

	QString m_bufferStdout;
	QString m_bufferStderr;
	QString m_bufferStdinp;

	QRegExp *m_regExpEOL;
	QRegExp *m_regExpSkip;
	QRegExp *m_regExpKeep;

	QTextStream *m_logFile;
	QEventLoop *m_eventLoop;

	int m_exitCode;
};
