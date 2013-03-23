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

//Qt
#include <QCoreApplication>
#include <QStringList>
#include <QTimer>

//Internal
#include "LogProcessor.h"

//Version tags
static const int VERSION_MAJOR = 2;
static const int VERSION_MINOR = 0;

//Parameters
class parameters_t
{
public:
	QStringList childArgs;
};

//Forward declarations
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters);
static void printUsage(void);

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

	parameters_t parameters;
	if(!parseArguments(argc, argv, &parameters))
	{
		printUsage();
		return -1;
	}

	QCoreApplication application(dummy_argc, dummy_argv);

	QTimer timer;
	QObject::connect(&timer, SIGNAL(timeout()), &application, SLOT(quit()));
	timer.start(5000);

	return application.exec();
}

/*
 * Parse the CLI args
 */
static bool parseArguments(int argc, wchar_t* argv[], parameters_t *parameters)
{
	bool bChildAgrs = false;
	
	//Setup defaults
	parameters->childArgs.clear();

	for(int i = 0; i < argc; i++)
	{
		const QString current = QString::fromUtf16(reinterpret_cast<const ushort*>(argv[i])).trimmed();

		if(!bChildAgrs)
		{
			if(!current.compare(":", Qt::CaseInsensitive))
			{
				bChildAgrs = true;
			}
			else if((!current.compare("--help", Qt::CaseInsensitive)) || (!current.compare("--version", Qt::CaseInsensitive)) || (!current.compare("/?", Qt::CaseInsensitive)))
			{
				return false;
			}
		}
		else
		{
			parameters->childArgs.append(current);
		}
	}
	
	return !parameters->childArgs.isEmpty();
}

/*
 * Print the CLI help
 */
static void printUsage(void)
{
	fprintf(stderr, "\nLogging Utility v%d.%02d, built on %s at %s\n", VERSION_MAJOR, VERSION_MINOR, __DATE__, __TIME__);
	fprintf(stderr, "Copyright (c) 2010-2013 LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.\n");
	fprintf(stderr, "Please visit http://www.muldersoft.com/ for news and updates!\n\n");
	fprintf(stderr, "This program is free software: you can redistribute it and/or modify\n");
	fprintf(stderr, "it under the terms of the GNU General Public License <http://www.gnu.org/>.\n");
	fprintf(stderr, "Note that this program is distributed with ABSOLUTELY NO WARRANTY.\n\n");
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
	__except(1)
	{
		/*catch all exceptions*/
		fprintf(stderr, "\n\nFATAL ERROR: Opus, some slunks have sneaked into your system and now it broke :-(\n\n");
		fflush(stderr);
		return -1;
	}
}
