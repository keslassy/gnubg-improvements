/*
 * external.c
 *
 * by Gary Wong <gtw@gnu.org>, 2001, 2002.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "backgammon.h"
#include "drawboard.h"
#include "external.h"
#include "rollout.h"

#if defined(AF_UNIX) && !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#define PF_LOCAL PF_UNIX
#endif
#ifndef SUN_LEN
#define SUN_LEN(x) (sizeof *(x))
#endif

extern int ExternalRead( int h, char *pch, int cch ) {
#if HAVE_SOCKETS
    char *p = pch, *pEnd;
    int n;
    psighandler sh;
    
    while( cch ) {
	if( fAction )
	    fnAction();

	if( fInterrupt )
	    return -1;

	PortableSignal( SIGPIPE, SIG_IGN, &sh, FALSE );
	n = read( h, p, cch );
	PortableSignalRestore( SIGPIPE, &sh );
	
	if( !n ) {
	    outputl( "External connection closed." );
	    return -1;
	} else if( n < 0 ) {
	    if( errno == EINTR )
		continue;

	    perror( "external connection" );
	    return -1;
	}
	
	if( ( pEnd = memchr( p, '\n', n ) ) ) {
	    *pEnd = 0;
	    return 0;
	}
	
	cch -= n;
	p += n;
	
    }

    p[ cch - 1 ] = 0;
    return 0;
#else
    assert( FALSE );
#endif
}

extern int ExternalWrite( int h, char *pch, int cch ) {
#if HAVE_SOCKETS
    char *p = pch;
    int n;
    psighandler sh;

    while( cch ) {
	if( fAction )
	    fnAction();

	if( fInterrupt )
	    return -1;

	PortableSignal( SIGPIPE, SIG_IGN, &sh, FALSE );
	n = write( h, p, cch );
	PortableSignalRestore( SIGPIPE, &sh );
	
	if( !n )
	    return 0;
	else if( n < 0 ) {
	    if( errno == EINTR )
		continue;

	    perror( "external connection" );
	    return -1;
	}
	
	cch -= n;
	p += n;
    }

    return 0;
#else
    assert( FALSE );
#endif
}

extern void CommandExternal( char *sz ) {

#if !HAVE_SOCKETS
    outputl( "This installation of GNU Backgammon was compiled without\n"
	     "socket support, and does not implement external controllers." );
#else
    int h, hPeer;
    struct sockaddr_un saun;
    char szCommand[ 256 ], szResponse[ 32 ];
    char szName[ 32 ], szOpp[ 32 ];
    int anBoard[ 2 ][ 25 ], anBoardOrig[ 2 ][ 25 ], nMatchTo, anScore[ 2 ],
	anDice[ 2 ], nCube, fCubeOwner, fDoubled, fTurn, fCrawford,
	anMove[ 8 ];
    float arDouble[ NUM_CUBEFUL_OUTPUTS ],
	aarOutput[ 2 ][ NUM_ROLLOUT_OUTPUTS ],
	aarStdDev[ 2 ][ NUM_ROLLOUT_OUTPUTS ];
    rolloutstat aarsStatistics[ 2 ][ 2 ];
    cubeinfo ci;
    
    sz = NextToken( &sz );
    
    if( !sz || !*sz ) {
	outputl( "You must specify the name of the socket to the external\n"
		 "controller -- try `help external'." );
	return;
    }

    if( ( h = socket( PF_LOCAL, SOCK_STREAM, 0 ) ) < 0 ) {
	perror( "socket" );
	return;
    }

    saun.sun_family = AF_LOCAL;
    strcpy( saun.sun_path, sz ); /* yuck!  opportunities for buffer overflow
				    here... but we didn't write the broken
				    interface */

    if( bind( h, (struct sockaddr *) &saun, SUN_LEN( &saun ) ) < 0 ) {
	perror( sz );
	close( h );
	return;
    }
    
    if( listen( h, 1 ) < 0 ) {
	perror( "listen" );
	close( h );
	unlink( sz );
	return;
    }

    while( ( hPeer = accept( h, NULL, NULL ) ) < 0 ) {
	if( errno == EINTR ) {
	    if( fAction )
		fnAction();

	    if( fInterrupt ) {
		close( h );
		unlink( sz );
		return;
	    }
	    
	    continue;
	}
	
	perror( "accept" );
	close( h );
	unlink( sz );
	return;
    }

    close( h );
    unlink( sz );

    while( !ExternalRead( hPeer, szCommand, sizeof( szCommand ) ) )
	if( ParseFIBSBoard( szCommand, anBoard, szName, szOpp, &nMatchTo,
			     anScore + 1, anScore, anDice, &nCube,
			    &fCubeOwner, &fDoubled, &fTurn, &fCrawford ) )
	    outputl( "Warning: badly formed board from external controller." );
	else {
	    /* FIXME could SwapSides( anBoard ) be necessary? */
	    
	    SetCubeInfo ( &ci, nCube, fCubeOwner, fTurn, nMatchTo, anScore,
			  fCrawford, fJacoby, nBeavers );

	    memcpy( anBoardOrig, anBoard, sizeof( anBoard ) );

	    if( fDoubled ) {
		/* take decision */
		if( GeneralCubeDecision( "", aarOutput, aarStdDev,
					 aarsStatistics, anBoard, &ci,
					 &esEvalCube ) < 0 )
		    break;
	  
		switch( FindCubeDecision( arDouble, aarOutput, &ci ) ) {
		case DOUBLE_PASS:
		case TOOGOOD_PASS:
		case REDOUBLE_PASS:
		case TOOGOODRE_PASS:
		    strcpy( szResponse, "drop" );
		    break;

		default:
		    /* we don't consider beavers */
		    strcpy( szResponse, "take" );
		}
	    } else if( anDice[ 0 ] ) {
		/* move */
		if( FindBestMove( anMove, anDice[ 0 ], anDice[ 1 ],
				  anBoard, &ci, &esEvalChequer.ec ) < 0 )
		    break;

		FormatMovePlain( szResponse, anBoardOrig, anMove );
	    } else {
		/* double decision */
		if( GeneralCubeDecision( "", aarOutput, aarStdDev,
					 aarsStatistics, anBoard, &ci,
					 &esEvalCube ) < 0 )
		    break;
		
		switch( FindCubeDecision( arDouble, aarOutput, &ci ) ) {
		case DOUBLE_TAKE:
		case DOUBLE_PASS:
		case DOUBLE_BEAVER:
		case REDOUBLE_TAKE:
		case REDOUBLE_PASS:
		    strcpy( szResponse, "double" );
		    break;
		    
		default:
		    strcpy( szResponse, "roll" );
		}
	    }
	    
	    strcat( szResponse, "\n" );
	    
	    if( ExternalWrite( hPeer, szResponse, strlen( szResponse ) ) )
		break;
	}

    close( hPeer );
#endif
}
