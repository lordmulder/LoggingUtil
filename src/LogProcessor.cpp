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

//Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//Qt
#include <QProcess>
#include <QTextStream>
#include <QTextCodec>
#include <QFile>
#include <QDateTime>
#include <QCoreApplication>
#include <QTimer>

//Internal
#include "InputReader.h"

//Const
static const int CHANNEL_STDOUT = 1;
static const int CHANNEL_STDERR = 2;
static const int CHANNEL_STDINP = 4;
static const int CHANNEL_SYSMSG = 8;

//Helper
#define SAFE_DEL(X) do { if(X) { delete (X); X = NULL; } } while (0)

// ===================================================
// Constructor & Destructor
// ===================================================

/*
 * Constructor
 */
CLogProcessor::CLogProcessor(QFile &logFile)
:
	m_logStdout(true),
	m_logStderr(true),
	m_simplify(true),
	m_logFormat(LOG_FORMAT_VERBOSE),
	m_logInitialized(false),
	m_logFinished(false),
	m_logIsEmpty(logFile.size() == 0),
	m_exitCode(-1)
{
	//Sanity check
	if(!(logFile.isOpen() && logFile.isWritable()))
	{
		throw "Log file not open for writing!";
	}

	//Create process
	m_process = new QProcess();
	
	//Setup process
	m_process->setProcessChannelMode(QProcess::SeparateChannels);
	connect(m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(readFromStdout()));
	connect(m_process, SIGNAL(readyReadStandardError()), this, SLOT(readFromStderr()));
	connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int)));

	//Setup STDIN reader
	m_stdinReader = new CInputReader();
	connect(m_stdinReader, SIGNAL(dataAvailable(quint32)), this, SLOT(readFromStdinp(void)), Qt::QueuedConnection);
	connect(m_stdinReader, SIGNAL(finished()), this, SLOT(readerFinished(void)), Qt::QueuedConnection);

	//Create default decoder
	QTextCodec *codec = QTextCodec::codecForName("UTF-8");
	m_codecStdout = new QTextDecoder(codec);
	m_codecStderr = new QTextDecoder(codec);
	m_codecStdinp = new QTextDecoder(codec);

	//Setup regular exporession
	m_regExpEOL = new QRegExp("(\\f|\\n|\\r|\\v)");
	m_regExpKeep = m_regExpSkip = NULL;

	//Assign the log file
	m_logFile = new QTextStream(&logFile);
	m_logFile->setCodec(QTextCodec::codecForName("UTF-8"));
	m_logFile->setGenerateByteOrderMark(m_logIsEmpty);
	
	//Create event loop
	m_eventLoop = new QEventLoop();
}

/*
 * Destructor
 */
CLogProcessor::~CLogProcessor(void)
{
	//Make sure, we are not still running
	forceQuit(true);

	//Clean up all heap objects
	SAFE_DEL(m_process);
	SAFE_DEL(m_stdinReader);
	SAFE_DEL(m_regExpEOL);
	SAFE_DEL(m_regExpKeep);
	SAFE_DEL(m_regExpSkip);
	SAFE_DEL(m_eventLoop);
	SAFE_DEL(m_logFile);
	SAFE_DEL(m_codecStdout);
	SAFE_DEL(m_codecStderr);
	SAFE_DEL(m_codecStdinp);
}

// ===================================================
// Public Methods
// ===================================================

/*
 * Start the process
 */
bool CLogProcessor::startProcess(const QString &program, const QStringList &arguments)
{
	if(m_process->state() != QProcess::NotRunning)
	{
		return false;
	}

	initializeLog();
	logString(QString("Creating new process: %1 [%2]").arg(program, arguments.join("; ")), CHANNEL_SYSMSG);
	
	m_process->start(program, arguments);

	if(!m_process->waitForStarted())
	{
		logString(QString("Process creation failed: %1").arg(m_process->errorString()) , CHANNEL_SYSMSG);
		m_process->kill();
		return false;
	}

	logString(QString().sprintf("Process created successfully (PID: 0x%08X)", m_process->pid()->hProcess), CHANNEL_SYSMSG);
	return true;
}

/*
 * Start reading from Stdin
 */
bool  CLogProcessor::startStdinProcessing(void)
{
	if(m_stdinReader->isRunning())
	{
		return false;
	}

	initializeLog();
	logString("Started logging from STDIN stream...", CHANNEL_SYSMSG);

	m_stdinReader->start();
	return true;
}

/*
 * Event processing
 */
int CLogProcessor::exec(void)
{
	if(m_process->state() == QProcess::Running || m_stdinReader->isRunning())
	{
		//Make sure we will read immediately
		QTimer::singleShot(0, this, SLOT(readFromStdout()));
		QTimer::singleShot(0, this, SLOT(readFromStderr()));
		QTimer::singleShot(0, this, SLOT(readFromStdinp()));

		//Event processing
		return m_eventLoop->exec();
	}
	else
	{
		//Read any pending data (might be that we already finished!)
		readFromStdout();
		readFromStderr();
		readFromStdinp();

		//Flush buffer contents
		flushBuffers();

		//Return last exit code
		return m_exitCode;
	}
}

// ===================================================
// Slots
// ===================================================
/*
 * Force process to quit ASAP
 */
void CLogProcessor::forceQuit(const bool silent)
{
	if(!silent)
	{
		logString("Aborted by user! (Ctrl+C)", CHANNEL_SYSMSG);
	}
	
	if(m_process)
	{
		if(m_process->state() != QProcess::NotRunning)
		{
			if(m_process->state() == QProcess::Starting)
			{
				m_process->waitForStarted();
			}
			m_process->kill();
			m_process->waitForFinished();
		}
	}
	
	if(m_stdinReader)
	{
		if(m_stdinReader->isRunning())
		{
			m_stdinReader->abort();
			if(!m_stdinReader->wait(5000))
			{
				m_stdinReader->terminate();
				m_stdinReader->wait();
			}
		}
	}
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
		fflush(stdout);
		if(m_logStdout) processData(data, CHANNEL_STDOUT);
	}
}

/*
 * Read from StdErr
 */
void CLogProcessor::readFromStderr(void)
{
	const QByteArray data = m_process->readAllStandardError();

	if(data.length() > 0)
	{
		fwrite(data.constData(), 1, data.length(), stderr);
		fflush(stderr);
		if(m_logStderr) processData(data, CHANNEL_STDERR);
	}
}

/*
 * Read from StdIn
 */
void CLogProcessor::readFromStdinp(void)
{
	QByteArray data;
	m_stdinReader->readAllData(data);

	if(data.length() > 0)
	{
		fwrite(data.constData(), 1, data.length(), stderr);
		fflush(stderr);
		processData(data, CHANNEL_STDINP);
	}
}

/*
 * Process has finished
 */
void CLogProcessor::processFinished(int exitCode)
{
	//Just to be sure (?)
	m_process->waitForFinished();
	
	//Process pending outputs
	readFromStdout();
	readFromStderr();

	//Flush buffer contents
	flushBuffers();
	
	//Now return the exit code
	m_exitCode = exitCode;
	logString(QString().sprintf("Process has terminated (exit code: 0x%08X)", exitCode), CHANNEL_SYSMSG);
	finishLog();

	m_eventLoop->exit(m_exitCode);
}

/*
 * STDIN reader has finished
 */
void CLogProcessor::readerFinished(void)
{
	//Process pending outputs
	readFromStdinp();

	//Flush buffer contents
	flushBuffers();

	//Now return the exit code
	logString("No more data available from STDIN (process has terminated)", CHANNEL_SYSMSG);
	finishLog();

	m_eventLoop->exit(0);
}

// ===================================================
// Private Methods
// ===================================================

/*
 * FLush any pending data from buffer
 */
void CLogProcessor::flushBuffers(void)
{
	if(m_logStdout && (!m_bufferStdout.isEmpty()))
	{
		logString(m_simplify ? m_bufferStdout.simplified() : m_bufferStdout, CHANNEL_STDOUT);
		m_bufferStdout.clear();
	}

	if(m_logStderr && (!m_bufferStderr.isEmpty()))
	{
		logString(m_simplify ? m_bufferStderr.simplified() : m_bufferStderr, CHANNEL_STDERR);
		m_bufferStderr.clear();
	}

	if(!m_bufferStdinp.isEmpty())
	{
		logString(m_simplify ? m_bufferStdinp.simplified() : m_bufferStdinp, CHANNEL_STDOUT);
		m_bufferStdinp.clear();
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
		decoder = m_codecStdout;
		break;
	case CHANNEL_STDERR:
		buffer = &m_bufferStderr;
		decoder = m_codecStderr;
		break;
	case CHANNEL_STDINP:
		buffer = &m_bufferStdinp;
		decoder = m_codecStdinp;
		break;
	default:
		throw "Bad selection!";
	}

	buffer->append(decoder->toUnicode(data).replace(QChar('\b'), QChar('\r')));

	int pos = m_regExpEOL->indexIn(*buffer);
	while(pos >= 0)
	{
		if(pos > 0)
		{
			logString(m_simplify ? buffer->left(pos).simplified() : buffer->left(pos), channel);
		}
		buffer->remove(0, pos + 1);
		pos = m_regExpEOL->indexIn(*buffer);
	}
}

/*
 * Append string to log file
 */
void CLogProcessor::logString(const QString &data, const int channel)
{
	//No logging if not ready
	if((!m_logInitialized) || m_logFinished)
	{
		return;
	}

	//Do not log any empty strings!
	if(data.isEmpty() || ((m_logFormat == LOG_FORMAT_PLAIN) && (channel == CHANNEL_SYSMSG)))
	{
		return;
	}
	
	//Filter out strings
	if(channel != CHANNEL_SYSMSG)
	{
		if(m_regExpKeep)
		{
			if(m_regExpKeep->indexIn(data) < 0) return;
		}
		if(m_regExpSkip)
		{
			if(m_regExpSkip->indexIn(data) >= 0) return;
		}
	}

	QChar chanId;

	switch(channel)
	{
	case CHANNEL_STDOUT:
		chanId = 'O';
		break;
	case CHANNEL_STDERR:
		chanId = 'E';
		break;
	case CHANNEL_STDINP:
		chanId = 'I';
		break;
	case CHANNEL_SYSMSG:
		chanId = 'S';
		break;
	default:
		throw "Bad selection!";
	}

	static const QString format_date("yyyy-MM-dd"), format_time("hh:mm:ss");
	QDateTime time = (m_logFormat == LOG_FORMAT_PLAIN) ? QDateTime() : QDateTime::currentDateTime();

	switch(m_logFormat)
	{
	case LOG_FORMAT_VERBOSE:
		m_logFile->operator<<(QString("[%1] [%2] [%3] %4\r\n").arg(chanId, time.toString(format_date), time.toString(format_time), data));
		break;
	case LOG_FORMAT_PLAIN:
		m_logFile->operator<<(QString("%1\r\n").arg(data));
		break;
	case LOG_FORMAT_HTML:
		m_logFile->operator<<(QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>\r\n").arg(chanId, time.toString(format_date), time.toString(format_time), escape(data)));
		break;
	default:
		throw "Bad selection!";
	}
}

/*
 * Initialize log file
 */
void CLogProcessor::initializeLog(void)
{
	if(m_logInitialized)
	{
		return;
	}

	if((m_logFormat == LOG_FORMAT_HTML) && m_logIsEmpty)
	{
		m_logFile->operator<<("<!DOCTYPE html>\r\n");
		m_logFile->operator<<("<html><head><title>Log File</title></head><body><table style=\"font-family:monospace\" border>\r\n");
		m_logFile->operator<<("<tr><td>&nbsp;</td><td><b>Date</b></td><td><b>Time</b></td><td><b>Log Message</b></td></tr>\r\n");
	}
	if((m_logFormat == LOG_FORMAT_VERBOSE) && (!m_logIsEmpty))
	{
		m_logFile->operator<<("---------------------------\r\n");
	}

	m_logInitialized = true;
}

/*
 * Finish log file
 */
void CLogProcessor::finishLog(void)
{
	if((!m_logInitialized) || m_logFinished)
	{
		return;
	}

	if((m_logFormat == LOG_FORMAT_HTML) && m_logIsEmpty)
	{
		m_logFile->operator<<("</table></body></html>\r\n");
	}

	m_logFinished = true;
}

// ===================================================
// Setter methods
// ===================================================

/*
 * Set which streams to capture
 */
void CLogProcessor::setCaptureStreams(const bool captureStdout, const bool captureStderr)
{
	m_logStdout = captureStdout;
	m_logStderr = captureStderr;
}

/*
 * Set whether strings are simplified/trimmed
 */
void CLogProcessor::setSimplifyStrings(const bool simplify)
{
	m_simplify = simplify;
}

/*
 * Set verbose logging mode
 */
void CLogProcessor::setOutputFormat(const Format format)
{
	m_logFormat = format;
}

/*
 * Set regular expressions for filtering
 */
void CLogProcessor::setFilterStrings(const QString &regExpKeep, const QString &regExpSkip)
{
	if(!regExpKeep.isEmpty())
	{
		SAFE_DEL(m_regExpKeep);
		m_regExpKeep = new QRegExp(regExpKeep);
	}

	if(!regExpSkip.isEmpty())
	{
		SAFE_DEL(m_regExpSkip);
		m_regExpSkip = new QRegExp(regExpSkip);
	}
}

/*
 * Set text encodings
 */
bool CLogProcessor::setTextCodecs(const char *inputCodec, const char *outputCodec)
{
	if(inputCodec)
	{
		QTextCodec *codec = QTextCodec::codecForName(inputCodec);
		if(codec)
		{
			SAFE_DEL(m_codecStdout); m_codecStdout = new QTextDecoder(codec);
			SAFE_DEL(m_codecStderr); m_codecStderr = new QTextDecoder(codec);
		}
		else
		{
			return false;
		}
	}

	if(outputCodec)
	{
		QTextCodec *codec = QTextCodec::codecForName(outputCodec);
		if(codec)
		{
			m_logFile->setCodec(codec);
		}
		else
		{
			return false;
		}
	}

	return true;
}

// ===================================================
// Misc Stuff
// ===================================================

/*
 * Escape (some) HTML characters
 */
QString CLogProcessor::escape(const QString &text)
{
	QString escaped(text);
	
	escaped.replace('<', "&lt;");
	escaped.replace('>', "&gt;");
	escaped.replace('&', "&amp;");
	escaped.replace('"', "&quot;");
	escaped.replace(' ', "&nbsp;");

	return escaped;
}
