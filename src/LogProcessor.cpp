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

#include "LogProcessor.h"

//Qt
#include <QProcess>
#include <QTextCodec>

//Const
static const int CHANNEL_STDOUT = 1;
static const int CHANNEL_STDERR = 2;

//Helper
#define SAFE_DEL(X) do { if(X) { delete (X); X = NULL; } } while (0)

/*
 * Constructor
 */
CLogProcessor::CLogProcessor(void)
:
	m_logStdout(false),
	m_logStderr(false)
{
	m_process = new QProcess();
	
	//Setup process
	m_process->setProcessChannelMode(QProcess::SeparateChannels);
	connect(m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(readFromStdout()));
	connect(m_process, SIGNAL(readyReadStandardError()), this, SLOT(readFromStderr()));

	//Create decoder
	m_decoderStdout = QTextCodec::codecForName("UTF-8")->makeDecoder();
	m_decoderStderr = QTextCodec::codecForName("UTF-8")->makeDecoder();

	//Setup regular exporession
	QRegExp *m_regExp = new QRegExp("(\\f|\\n|\\r|\\v)");
}

/*
 * Destructor
 */
CLogProcessor::~CLogProcessor(void)
{
	SAFE_DEL(m_process);
	SAFE_DEL(m_decoderStdout);
	SAFE_DEL(m_decoderStderr);
	SAFE_DEL(m_regExp);
}

/*
 * Read from StdOut
 */
void CLogProcessor::readFromStdout(void)
{
	const QByteArray data = m_process->readAllStandardOutput();

	if(data.length() > 0)
	{
		fwrite(data.constData(), 1, data.length(), stdout);
		if(m_logStdout) processData(data, CHANNEL_STDOUT);
	}
}

/*
 * Read from StdErr
 */
void CLogProcessor::readFromStderr(void)
{
	const QByteArray data = m_process->readAllStandardOutput();

	if(data.length() > 0)
	{
		fwrite(data.constData(), 1, data.length(), stderr);
		if(m_logStderr) processData(data, CHANNEL_STDERR);
	}
}

/*
 * Process data (decode and tokenize)
 */
void CLogProcessor::processData(const QByteArray &data, const int channel)
{
	QString *buffer = NULL;
	QTextDecoder *decoder = NULL;
	
	switch(channel)
	{
	case CHANNEL_STDOUT:
		buffer = &m_bufferStdout;
		decoder = m_decoderStdout;
		break;
	case CHANNEL_STDERR:
		buffer = &m_bufferStderr;
		decoder = m_decoderStderr;
		break;
	default:
		throw "Bad selection!";
	}

	buffer->append(decoder->toUnicode(data).replace(QChar('\b'), QChar('\r')));
	
	int pos = m_regExp->indexIn(*buffer);
	while(pos >= 0)
	{
		if(pos > 0)
		{
			logString(buffer->left(pos), channel);
		}
		buffer->remove(0, pos + 1);
		pos = m_regExp->indexIn(*buffer);
	}
}

/*
 * Append string to log file
 */
void CLogProcessor::logString(const QString &data, const int channel)
{
}
