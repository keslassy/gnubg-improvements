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

#if HAVE_SOCKETS
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#endif
#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif /* HAVE_SOCKETS */

#include "backgammon.h"
#include "drawboard.h"
#include "external.h"
#include "rollout.h"
#include "i18n.h"

#if defined(AF_UNIX) && !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#define PF_LOCAL PF_UNIX
#endif

#if HAVE_SOCKETS
extern int ExternalSocket( struct sockaddr **ppsa, int *pcb, char *sz ) {

    int h, f;
    struct sockaddr_un *psun;
    struct sockaddr_in *psin;
    struct hostent *phe;
    char *pch;
    
    if( ( pch = strchr( sz, ':' ) ) && !strchr( sz, '/' ) ) {
	/* Internet domain socket. */
	if( ( h = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
	    return -1;

	f = TRUE;
	if( setsockopt( h, SOL_SOCKET, SO_REUSEADDR, &f, sizeof f ) )
	    return -1;
	
	psin = malloc( *pcb = sizeof (struct sockaddr_in) );
	
	psin->sin_family = AF_INET;

	*pch = 0;
	
	if( !*sz )
	    /* no host specified */
	    psin->sin_addr.s_addr = htonl( INADDR_ANY );
	else if( !inet_aton( sz, &psin->sin_addr ) ) {
	    if( !( phe = gethostbyname( sz ) ) ) {
		*pch = ':';
		errno = EINVAL;
		free( psin );
		return -1;
	    }

	    psin->sin_addr = *(struct in_addr *) phe->h_addr;
	}

	*pch++ = ':';
	
	psin->sin_port = htons( atoi( pch ) );
	
	*ppsa = (struct sockaddr *) psin;
    } else {
	/* Local domain socket. */
	if( ( h = socket( PF_LOCAL, SOCK_STREAM, 0 ) ) < 0 )
	    return -1;

	/* yuck... there's no portable way to obtain the necessary
	   sockaddr_un size, but this is a conservative estimate */
	psun = malloc( *pcb = 16 + strlen( sz ) );
	
	psun->sun_family = AF_LOCAL;
	strcpy( psun->sun_path, sz );

	*ppsa = (struct sockaddr *) psun;
    }
    
    return h;
#if 0 
    assert( FALSE );
#endif
}
#endif

#if HAVE_SOCKETS
static void ExternalUnbind( char *sz ) {

    if( strchr( sz, ':' ) && !strchr( sz, '/' ) )
	/* it was a TCP socket; no cleanup necessary */
	return;

    unlink( sz );
}
#endif

#if HAVE_SOCKETS
extern int ExternalRead( int h, char *pch, int cch ) {

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
	    outputl( _("External connection closed.") );
	    return -1;
	} else if( n < 0 ) {
	    if( errno == EINTR )
		continue;

	    outputerr( _("external connection") );
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
#if 0
    assert( FALSE );
#endif
}
#endif

#if HAVE_SOCKETS
extern int ExternalWrite( int h, char *pch, int cch ) {

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

	    outputerr( _("external connection") );
	    return -1;
	}
	
	cch -= n;
	p += n;
    }

    return 0;
#if 0
    assert( FALSE );
#endif
}
#endif

extern void CommandExternal( char *sz ) {

#if !HAVE_SOCKETS
    outputl( _("This installation of GNU Backgammon was compiled without\n"
	     "socket support, and does not implement external controllers.") );
#else
    int h, hPeer, cb;
    struct sockaddr *psa;
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
	outputl( _("You must specify the name of the socket to the external\n"
		 "controller -- try `help external'.") );
	return;
    }

    if( ( h = ExternalSocket( &psa, &cb, sz ) ) < 0 ) {
	outputerr( sz );
	return;
    }

    if( bind( h, psa, cb ) < 0 ) {
	outputerr( sz );
	close( h );
	free( psa );
	return;
    }

    free( psa );
    
    if( listen( h, 1 ) < 0 ) {
	outputerr( _("listen") );
	close( h );
	ExternalUnbind( sz );
	return;
    }

    while( ( hPeer = accept( h, NULL, NULL ) ) < 0 ) {
	if( errno == EINTR ) {
	    if( fAction )
		fnAction();

	    if( fInterrupt ) {
		close( h );
		ExternalUnbind( sz );
		return;
	    }
	    
	    continue;
	}
	
	outputerr( _("accept") );
	close( h );
	ExternalUnbind( sz );
	return;
    }

    close( h );
    ExternalUnbind( sz );

    while( !ExternalRead( hPeer, szCommand, sizeof( szCommand ) ) )
	if( ParseFIBSBoard( szCommand, anBoard, szName, szOpp, &nMatchTo,
                            anScore, anScore + 1, anDice, &nCube,
			    &fCubeOwner, &fDoubled, &fTurn, &fCrawford ) )
          outputl( _("Warning: badly formed board from external controller.") );
	else {

            if ( ! fTurn )
              SwapSides( anBoard );

	    SetCubeInfo ( &ci, nCube, fCubeOwner, fTurn, nMatchTo, anScore,
			  fCrawford, fJacoby, nBeavers, bgvDefault ); 

	    memcpy( anBoardOrig, anBoard, sizeof( anBoard ) );

	    if ( fDoubled > 0 ) {

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

		case NODOUBLE_BEAVER:
		case DOUBLE_BEAVER:
		case NO_REDOUBLE_BEAVER:
		    strcpy( szResponse, "beaver" );
		    break;
		    
		default:
		    strcpy( szResponse, "take" );
		}

	    } else if ( fDoubled < 0 ) {
              
                /* if opp wants to resign (extension to FIBS board) */

		float arOutput[ NUM_ROLLOUT_OUTPUTS ];
		float rEqBefore, rEqAfter;
		const float epsilon = 1.0e-6;

		if ( GeneralEvaluationE( arOutput, anBoard, &ci,
		    &esEvalCube.ec ) )
		    break;

		rEqBefore = arOutput[ OUTPUT_CUBEFUL_EQUITY ];

		/* I win 100% if opponent resigns */
		arOutput[ 0 ] = 1.0; 
		arOutput[ 1 ] = arOutput[ 2 ] =
			arOutput[ 3 ] = arOutput[ 4 ] = 0.0;

		/* resigned at least a gammon */
		if( fDoubled <= -2 ) arOutput[ 1 ] = 1.0;

		/* resigned a backgammon */
		if( fDoubled == -3 ) arOutput[ 2 ] = 1.0;

		InvertEvaluation ( arOutput );
      		rEqAfter = Utility ( arOutput, &ci );
		if ( nMatchTo ) rEqAfter = eq2mwc( rEqAfter, &ci );

		/* comment this out when debugging is done 
		printf ("# equity before resignation: %.10f\n"
			"# equity after resignation : %.10f\n",
			rEqBefore, rEqAfter );*/

		/* if opponent gives up equity by resigning */
		if( ( rEqBefore - rEqAfter ) >= epsilon )
		    strcpy( szResponse, "accept" );
		else
		    strcpy( szResponse, "reject" );

	    } else if( anDice[ 0 ] ) {
		/* move */
		if( FindBestMove( anMove, anDice[ 0 ], anDice[ 1 ],
				  anBoard, &ci, &esEvalChequer.ec,
                                  aamfEval ) < 0 )
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
