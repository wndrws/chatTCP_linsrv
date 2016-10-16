//
// Created by user on 15.10.16.
//

#ifndef SKEL_H
#define SKEL_H

/* Версия для UNIX */
char* program_name;

#define INIT()          ( program_name = strrchr( argv[ 0 ], '/' ) ) ? \
                        program_name++ : ( program_name = argv[ 0 ] )

#define EXIT(s)         exit( s )
#define CLOSE(s)        if( close( s ) ) error( 1, errno, "Error at close()" )
#define set_errno(e)    errno = ( e )
#define isvalidsock(s)  ( ( s ) >= 0 )

typedef int SOCKET;

#endif //SKEL_H
