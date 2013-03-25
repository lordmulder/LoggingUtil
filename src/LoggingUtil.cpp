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
#include <QTextCodec>
#include <QMutex>
#include <QMutexLocker>

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
	bool printHelp;
	QString childProgram;
	QStringList childArgs;
	QString logFile;
	bool captureStdout;
	bool captureStderr;
	bool enableSimplify;
	bool verboseMode;
	bool appendLogFile;
	bool htmlOutput;
	QString regExpKeep;
	QString regExpSkip;
	QString codecInp;
	QString codecOut;
};

//Helper
#define SAFE_DEL(X) do { if(X) { delete (X); X = NULL; } } while (0)
#define QSTR2STR(X) ((X).isEmpty() ? NULL : (X).toLatin1().constData())

//Forward declarations
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters);
static void printUsage(void);
static void printHeader(void);
static QByteArray supportedCodecs(void);

//Global variables
QMutex giantLock;
QCoreApplication *application = NULL;
CLogProcessor *processor = NULL;

//Const
const char *STDIN_MARKER = "#STDIN#";

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
		return -1;
	}

	//Print help screen
	if(parameters.printHelp)
	{
		printUsage();
		return 0;
	}

	//Does program file exist?
	if(parameters.childProgram.compare(STDIN_MARKER, Qt::CaseInsensitive))
	{
		QFileInfo program(parameters.childProgram);

		//Check for existence
		if(!(program.exists() && program.isFile()))
		{
			printHeader();
			fprintf(stderr, "ERROR: The specified program file does not exist!\n\n");
			fprintf(stderr, "Path that could not be found:\n%s\n\n", QFileInfo(parameters.childProgram).absoluteFilePath().toUtf8().constData());
			return -1;
		}

		//Make absoloute path
		parameters.childProgram = program.canonicalFilePath();
	}

	//Open the log file
	QFile logFile(parameters.logFile);
	QIODevice::OpenMode openFlags = (parameters.appendLogFile) ? QIODevice::Append : (QIODevice::WriteOnly | QIODevice::Truncate);
	if(!logFile.open(openFlags))
	{
		printHeader();
		fprintf(stderr, "ERROR: Failed to open log file for writing!\n\n");
		fprintf(stderr, "Path that failed to open is:\n%s\n\n", logFile.fileName().toUtf8().constData());
		return -1;
	}

	//Create application
	application = new QCoreApplication(dummy_argc, dummy_argv);

	//Create processor
	QMutexLocker lock(&giantLock);
	processor = new CLogProcessor(logFile);
	lock.unlock();

	//Setup parameters
	processor->setCaptureStreams(parameters.captureStdout, parameters.captureStderr);
	processor->setSimplifyStrings(parameters.enableSimplify);
	processor->setVerboseOutput(parameters.verboseMode);
	processor->setFilterStrings(parameters.regExpKeep, parameters.regExpSkip);
	processor->setHtmlOutput(parameters.htmlOutput);
	
	//Setup text encoding
	if(!processor->setTextCodecs(QSTR2STR(parameters.codecInp), QSTR2STR(parameters.codecOut)))
	{
		printHeader();
		fprintf(stderr, "ERROR: The selected text Codec is invalid!\n\n");
		fprintf(stderr, "Supported text codecs:\n%s\n\n", supportedCodecs().constData());
		logFile.close();
		delete processor;
		delete application;
		return -1;
	}

	//Try to start the child process (or STDIN reader)
	if(parameters.childProgram.compare(STDIN_MARKER, Qt::CaseInsensitive))
	{
		if(!processor->startProcess(parameters.childProgram, parameters.childArgs))
		{
			printHeader();
			fprintf(stderr, "ERROR: The process failed to start!\n\n");
			fprintf(stderr, "Command that failed is:\n%s\n\n", parameters.childProgram.toUtf8().constData());
			logFile.close();
			delete processor;
			delete application;
			return -1;
		}
	}
	else
	{
		if(!processor->startStdinProcessing())
		{
			printHeader();
			fprintf(stderr, "ERROR: The process failed to start!\n\n");
			fprintf(stderr, "Command that failed is:\n%s\n\n", parameters.childProgram.toUtf8().constData());
			logFile.close();
			delete processor;
			delete application;
			return -1;
		}
	}
	
	//Now run event loop
	int retval = processor->exec();

	//Clean up
	lock.relock();
	SAFE_DEL(processor);
	SAFE_DEL(application);
	lock.unlock();

	//Close log
	logFile.close();

	return retval;
}

/*
 * Make sure there is one more argument
 */
#define CHECK_NEXT_ARGUMENT(LST, ARG) do \
{ \
	if((LST).isEmpty() || (!(LST).first().compare(OPTION_MARKER, Qt::CaseInsensitive))) \
	{ \
		printHeader(); \
		fprintf(stderr, "ERROR: Argument for option '%s' is missing!\n\n", (ARG)); \
		fprintf(stderr, "Please type \"LoggingUtil.exe --help :\" for details...\n\n"); \
		return false; \
	} \
} \
while(0)

/*
 * Parse the CLI args
 */
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters)
{
	//Setup defaults
	parameters->printHelp = false;
	parameters->childProgram.clear();
	parameters->childArgs.clear();
	parameters->logFile.clear();
	parameters->captureStdout = true;
	parameters->captureStderr = true;
	parameters->enableSimplify = true;
	parameters->verboseMode = true;
	parameters->appendLogFile = true;
	parameters->htmlOutput = false;
	parameters->regExpKeep.clear();
	parameters->regExpSkip.clear();
	parameters->codecInp.clear();
	parameters->codecOut.clear();

	//Make sure user has set parameters
	if(argc < 2)
	{
		parameters->printHelp = true;
		return true;
	}

	//Convert all parameters to QString's
	QStringList list;
	for(int i = 1; i < argc; i++)
	{
		list << QString::fromUtf16(reinterpret_cast<const ushort*>(argv[i])).trimmed();
	}

	const QString OPTION_MARKER = ":";

	//Have logger options?
	bool bHaveOptions = false;
	for(QStringList::ConstIterator iter = list.constBegin(); iter != list.constEnd(); iter++)
	{
		if(!(*iter).compare(OPTION_MARKER, Qt::CaseInsensitive))
		{
			bHaveOptions = true;
			break;
		}
	}

	//Help screen requested?
	if(bHaveOptions)
	{
		static const char *help[] = { "--help", "-help", "/?" };
		
		for(QStringList::ConstIterator iter = list.constBegin(); iter != list.constEnd(); iter++)
		{
			if(!(*iter).compare(OPTION_MARKER, Qt::CaseInsensitive))
			{
				break;
			}
			
			for(int i = 0; i < 3; i++)
			{
				if(!(*iter).compare(QString::fromLatin1(help[i]), Qt::CaseInsensitive))
				{
					parameters->printHelp = true;
					return true;
				}
			}
		}
	}

	//Parse logger options
	while(bHaveOptions && (!list.isEmpty()))
	{
		const QString current = list.takeFirst().simplified();

		//End of logger options?
		if(!current.compare(OPTION_MARKER, Qt::CaseInsensitive))
		{
			break;
		}

		//Ignore alle empty strings
		if(current.isEmpty())
		{
			continue;
		}

		//Parse parameters
		if(!current.compare("--logfile", Qt::CaseInsensitive))
		{
			CHECK_NEXT_ARGUMENT(list, "--logfile");
			parameters->logFile = list.takeFirst();
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
		else if(!current.compare("--no-append", Qt::CaseInsensitive))
		{
			parameters->appendLogFile = false;
		}
		else if(!current.compare("--regexp-keep", Qt::CaseInsensitive))
		{
			CHECK_NEXT_ARGUMENT(list, "--regexp-keep");
			parameters->regExpKeep = list.takeFirst();
		}
		else if(!current.compare("--regexp-skip", Qt::CaseInsensitive))
		{
			CHECK_NEXT_ARGUMENT(list, "--regexp-skip");
			parameters->regExpSkip = list.takeFirst();
		}
		else if(!current.compare("--codec-in", Qt::CaseInsensitive))
		{
			CHECK_NEXT_ARGUMENT(list, "--codec-in");
			parameters->codecInp = list.takeFirst();
		}
		else if(!current.compare("--codec-out", Qt::CaseInsensitive))
		{
			CHECK_NEXT_ARGUMENT(list, "--codec-out");
			parameters->codecOut = list.takeFirst();
		}
		else if(!current.compare("--html-output", Qt::CaseInsensitive))
		{
			parameters->htmlOutput = true;
			parameters->appendLogFile = false;
		}
		else
		{
			printHeader();
			fprintf(stderr, "ERROR: Option '%s' is unknown!\n\n", current.toLatin1().constData());
			fprintf(stderr, "Please type \"LoggingUtil.exe --help :\" for details...\n\n"); \
			return false;
		}
	}

	//HTML implies verbose!
	if(parameters->htmlOutput && (!parameters->verboseMode))
	{
		printHeader();
		fprintf(stderr, "ERROR: Cannot use '--html-output' and '--no-verbose' at the same time!\n\n");
		return false;
	}

	//Check child process program name
	if(list.isEmpty() || list.first().isEmpty())
	{
		printHeader();
		fprintf(stderr, "ERROR: Program to execute has not been specified!\n\n");
		fprintf(stderr, "Please type \"LoggingUtil.exe --help :\" for details...\n\n"); \
		return false;
	}
	
	//Set child process program name and parameters
	parameters->childProgram = list.takeFirst();
	while(!list.isEmpty())
	{
		parameters->childArgs << list.takeFirst();
	}

	//Generate log file name
	if(parameters->logFile.isEmpty())
	{
		const QString ext = (parameters->htmlOutput) ? "htm" : "log";
		if(parameters->childProgram.compare(STDIN_MARKER, Qt::CaseInsensitive))
		{
			QFileInfo info(parameters->childProgram);
			QRegExp rx("[^a-zA-Z0-9_]");
			parameters->logFile = QString("%1.%2.%3").arg(info.completeBaseName().replace(rx, "_"), QDateTime::currentDateTime().toString("yyyy-MM-dd"), ext);
		}
		else
		{
			parameters->logFile = QString("STDIN.%1.%2").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"), ext);
		}
	}

	return true;
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
	fprintf(stderr, "Usage Mode #1:\n");
	fprintf(stderr, "  LoggingUtil.exe SomeProgram.exe [program parameters]\n");
	fprintf(stderr, "  LoggingUtil.exe [logging options] : SomeProgram.exe [program parameters]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage Mode #2:\n");
	fprintf(stderr, "  SomeProgram.exe [parameters] | LoggingUtil.exe [options] : #STDIN#\n");
	fprintf(stderr, "  SomeProgram.exe [parameters] 2>&1 | LoggingUtil.exe [options] : #STDIN#\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Logging Options:\n");
	fprintf(stderr, "  --logfile <logfile>  Specifies the output log file (appends if file exists)\n");
	fprintf(stderr, "  --only-stdout        Capture only output from STDOUT, ignores STDERR\n");
	fprintf(stderr, "  --only-stderr        Capture only output from STDERR, ignores STDOUT\n");
	fprintf(stderr, "  --no-simplify        Do NOT simplify/trimm the logged strings (default: on)\n");
	fprintf(stderr, "  --no-verbose         Do NOT write verbose output to log file (default: on)\n");
	fprintf(stderr, "  --no-append          Do NOT append, i.e. any existing log content is lost\n");
	fprintf(stderr, "  --regexp-keep <exp>  Keep ONLY strings that match the given RegExp\n");
	fprintf(stderr, "  --regexp-skip <exp>  Skip all the strings that match the given RegExp\n");
	fprintf(stderr, "  --codec-in <name>    Setup the input text encoding (default: \"UTF-8\")\n");
	fprintf(stderr, "  --codec-out <name>   Setup the output text encoding (default: \"UTF-8\")\n");
	fprintf(stderr, "  --html-output        Output log as HTML code, implies NO append\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  LoggingUtil.exe --logfile x264_log.txt : x264.exe -o output.mkv input.avs\n");
	fprintf(stderr, "  x264.exe -o output.mkv input.avs 2>&1 | LoggingUtil.exe : #STDIN#\n");
	fprintf(stderr, "\n");
}

/*
 * Get list of all supported Codec names
 */
static QByteArray supportedCodecs(void)
{
	QStringList list;
	QList<QByteArray> codecs = QTextCodec::availableCodecs();
	foreach(const QByteArray &c, codecs) list << QString::fromLatin1(c);
	return list.join(", ").toLatin1();
}

/*
 * Ctrl+C handler routine
 */
static BOOL WINAPI ctrlHandlerRoutine(DWORD dwCtrlType)
{
	QMutexLocker lock(&giantLock);
	
	if(processor)
	{
		QTimer::singleShot(0, processor, SLOT(forceQuit()));
	}

	return TRUE;
}

/*
 * Crash handler routine
 */
#pragma intrinsic(_InterlockedExchange)
static LONG WINAPI exceptionHandlerRoutine(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	static volatile long bFatalFlag = 0L;

	if(_InterlockedExchange(&bFatalFlag, 1L) == 0L)
	{
		__try
		{
			fprintf(stderr, "\n\nFATAL ERROR: Oups, some slunks have sneaked into your system and broke it :-(\n\n");
			fflush(stderr);
		}
		__except(1)
		{
			TerminateProcess(GetCurrentProcess(), DWORD(-1));
			return EXCEPTION_EXECUTE_HANDLER;
		}
	}

	TerminateProcess(GetCurrentProcess(), DWORD(-1));
	return EXCEPTION_EXECUTE_HANDLER;
}

/*
 * Application entry point
 */
int wmain(int argc, wchar_t* argv[])
{
	__try
	{
		SetUnhandledExceptionFilter(exceptionHandlerRoutine);
		SetConsoleCtrlHandler(ctrlHandlerRoutine, TRUE);
		logging_util_main(argc, argv);
	}
	__except(1)
	{
		exceptionHandlerRoutine(NULL);
		return -1;
	}
}
