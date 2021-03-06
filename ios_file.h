// ios_file.h
#ifndef IOS_FILE_H
#define IOS_FILE_H

#include "Zaimoni.STL/Pure.C/comptest.h"        /* for C library test results */

#ifdef ZAIMONI_HAS_MICROSOFT_IO_H
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif


#define OFSTREAM_ACID_CLOSE(F,NAME)	\
if (F) {	\
	F.close();	\
	unlink(NAME);	\
	rename(NAME ".tmp", NAME);	\
} else {	\
	F.close();	\
	unlink(NAME ".tmp");	\
	debugmsg("Failed to write to " NAME ".");	\
}

#define DECLARE_AND_ACID_OPEN(TYPE,F,NAME,ACTION)	\
TYPE F(NAME ".tmp");	\
if (!F) {	\
	debugmsg("Can't open " NAME ".");	\
	ACTION	\
}

#define DECLARE_AND_OPEN(TYPE,F,NAME,ACTION)	\
TYPE F(NAME);	\
if (!F) {	\
	debugmsg("Can't open " NAME ".");	\
	ACTION	\
}

#define DECLARE_AND_OPEN_SILENT(TYPE,F,NAME,ACTION)	\
TYPE F(NAME);	\
if (!F) {	\
	ACTION	\
}


#endif
