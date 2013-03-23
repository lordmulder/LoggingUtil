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

//CRT
#include <tchar.h>
#include <fcntl.h>
#include <io.h>

//Stdlib
#include <cstdio>

//Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//Qt
#include <QCoreApplication>
#include <QStringList>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QLibraryInfo>

//Internal
#include "Version.h"
#include "LogProcessor.h"

//Version tags
static const int VERSION_MAJOR = VER_LOGGER_MAJOR;
static const int VERSION_MINOR = (10 * VER_LOGGER_MINOR_HI) + VER_LOGGER_MINOR_LO;

//Parameters
class parameters_t
{
public:
	QString childProgram;
	QStringList childArgs;
	QString logFile;
	bool captureStdout;
	bool captureStderr;
	bool enableSimplify;
	bool verboseMode;
	QString regExpKeep;
	QString regExpSkip;
};

//Forward declarations
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters);
static void printUsage(void);
static void printHeader(void);

/*
 * The Main function
 */
static int logging_util_main(int argc, wchar_t* argv[])
{
	int dummy_argc = 1;
	char *dummy_argv[] = { "program.exe", NULL };

	_setmode(_fileno(stdin ), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);

	//Check the Qt version
	if(_stricmp(qVersion(), QT_VERSION_STR))
	{
		printHeader();
		fprintf(stderr, "FATAL: Compiled with Qt v%s, but running on Qt v%s!\n\n", QT_VERSION_STR, qVersion());
		return -1;
	}

	//Parse the CLI parameters
	parameters_t parameters;
	if(!parseArguments(argc, argv, &parameters))
	{
		printUsage();
		return -1;
	}

	//Open the log file
	QFile logFile(parameters.logFile);
	if(!logFile.open(QIODevice::Append))
	{
		printHeader();
		fprintf(stderr, "Error: Failed to open log file for writing!\n\n");
		fprintf(stderr, "Path that failed to open is:\n%s\n\n", logFile.fileName().toUtf8().constData());
		return -1;
	}

	//Create application
	QCoreApplication *application = new QCoreApplication(dummy_argc, dummy_argv);

	//Create the logger
	CLogProcessor *processor = new CLogProcessor(logFile);

	//Setup parameters
	processor->setCaptureStreams(parameters.captureStdout, parameters.captureStderr);
	processor->setSimplifyStrings(parameters.enableSimplify);
	processor->setVerboseOutput(parameters.verboseMode);
	processor->setFilterStrings(parameters.regExpKeep, parameters.regExpSkip);

	//Try to start the process
	if(!processor->startProcess(parameters.childProgram, parameters.childArgs))
	{
		printHeader();
		fprintf(stderr, "Error: The process failed to start!\n\n");
		fprintf(stderr, "Command that failed is:\n%s\n\n", parameters.childProgram.toUtf8().constData());
		delete processor;
		delete application;
		return -1;
	}
	
	int retval = processor->exec();
	
	delete processor;
	delete application;

	return retval;
}

#define CHECK_ARGUMENT (((i+1) < argc) && (_wcsicmp(argv[i+1], L":") != 0))

/*
 * Parse the CLI args
 */
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters)
{
	bool bChildAgrs = false;

	//Setup defaults
	parameters->childProgram.clear();
	parameters->childArgs.clear();
	parameters->logFile.clear();
	parameters->captureStdout = true;
	parameters->captureStderr = true;
	parameters->enableSimplify = true;
	parameters->verboseMode = true;
	parameters->regExpKeep.clear();
	parameters->regExpSkip.clear();

	//Make sure user has set parameters
	if(argc < 2)
	{
		return false;
	}

	//Parse all parameters
	for(int i = 1; i < argc; i++)
	{
		const QString current = QString::fromUtf16(reinterpret_cast<const ushort*>(argv[i])).trimmed();

		if(!bChildAgrs)
		{
			if(!current.compare(":", Qt::CaseInsensitive))
			{
				bChildAgrs = true;
			}
			else if(!current.compare("--logfile", Qt::CaseInsensitive))
			{
				if(!CHECK_ARGUMENT) return false;
				parameters->logFile = QString::fromUtf16(reinterpret_cast<const ushort*>(argv[++i])).trimmed();
			}
			else if(!current.compare("--only-stdout", Qt::CaseInsensitive))
			{
				parameters->captureStdout = true;
				parameters->captureStderr = false;
			}
			else if(!current.compare("--only-stderr", Qt::CaseInsensitive))
			{
				parameters->captureStdout = false;
				parameters->captureStderr = true;
			}
			else if(!current.compare("--no-simplify", Qt::CaseInsensitive))
			{
				parameters->enableSimplify = false;
			}
			else if(!current.compare("--no-verbose", Qt::CaseInsensitive))
			{
				parameters->verboseMode = false;
			}
			else if(!current.compare("--regexp-keep", Qt::CaseInsensitive))
			{
				if(!CHECK_ARGUMENT) return false;
				parameters->regExpKeep = QString::fromUtf16(reinterpret_cast<const ushort*>(argv[++i])).trimmed();
			}
			else if(!current.compare("--regexp-skip", Qt::CaseInsensitive))
			{
				if(!CHECK_ARGUMENT) return false;
				parameters->regExpSkip = QString::fromUtf16(reinterpret_cast<const ushort*>(argv[++i])).trimmed();
			}
			else
			{
				return false;
			}
		}
		else
		{
			if(parameters->childProgram.isEmpty())
			{
				parameters->childProgram = current;
				continue;
			}
			parameters->childArgs.append(current);
		}
	}
	
	//Generate log file name
	if(parameters->logFile.isEmpty() && (!parameters->childProgram.isEmpty()))
	{
		QFileInfo info(parameters->childProgram);
		parameters->logFile = QString("%1.%2.log").arg(info.completeBaseName(), QDateTime::currentDateTime().toString("yyyy-MM-dd"));
	}

	return !parameters->childProgram.isEmpty();
}

/*
 * Print the CLI header
 */
static void printHeader(void)
{
	fprintf(stderr, "\nLogging Utility v%d.%02d, built %s %s (using Qt v%s)\n", VERSION_MAJOR, VERSION_MINOR, __DATE__, __TIME__, QT_VERSION_STR);
	fprintf(stderr, "Copyright (c) 2010-2013 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n");
	fprintf(stderr, "Please visit http://www.muldersoft.com/ for news and updates!\n\n");
	fprintf(stderr, "This program is free software: you can redistribute it and/or modify\n");
	fprintf(stderr, "it under the terms of the GNU General Public License <http://www.gnu.org/>.\n");
	fprintf(stderr, "Note that this program is distributed with ABSOLUTELY NO WARRANTY.\n\n");
}

/*
 * Print the CLI help
 */
static void printUsage(void)
{
	printHeader();
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  LoggingUtil.exe [logging options] : SomeProgram.exe [program parameters]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --logfile <logfile>  Specifies the output log file (appends if file exists)\n");
	fprintf(stderr, "  --only-stdout        Capture only output from STDOUT, ignores STDERR\n");
	fprintf(stderr, "  --only-stderr        Capture only output from STDERR, ignores STDOUT\n");
	fprintf(stderr, "  --no-simplify        Do NOT simplify/trimm the logged strings (default: on)\n");
	fprintf(stderr, "  --no-verbose         Do NOT write verbose output to log file (default: on)\n");
	fprintf(stderr, "  --regexp-keep <exp>  Keep ONLY strings that match the given RegExp\n");
	fprintf(stderr, "  --regexp-skip <exp>  Skip all the strings that match the given RegExp\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "  LoggingUtil.exe --logfile x264_log.txt : x264.exe -o output.mkv input.avs\n");
	fprintf(stderr, "\n");
}

/*
 * Application entry point
 */
int wmain(int argc, wchar_t* argv[])
{
	__try
	{
		logging_util_main(argc, argv);
	}
	__except(0)
	{
		/*catch all exceptions*/
		fprintf(stderr, "\n\nFATAL ERROR: Oups, some slunks have sneaked into your system and borke it :-(\n\n");
		fflush(stderr);
		return -1;
	}
}
