/*
 * rollout.c
 *
 * by Gary Wong <gary@cs.arizona.edu>, 1999.
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

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#if USE_GTK
#include "gtkgame.h"
#endif
#include "positionid.h"
#include "rollout.h"

static int QuasiRandomDice( int iTurn, int iGame, int cGames,
                            int anDice[ 2 ] ) {
  if( !iTurn && !( cGames % 36 ) ) {
    anDice[ 0 ] = ( iGame % 6 ) + 1;
    anDice[ 1 ] = ( ( iGame / 6 ) % 6 ) + 1;
    return 0;
  } else if( iTurn == 1 && !( cGames % 1296 ) ) {
    anDice[ 0 ] = ( ( iGame / 36 ) % 6 ) + 1;
    anDice[ 1 ] = ( ( iGame / 216 ) % 6 ) + 1;
    return 0;
  } else
    return RollDice( anDice );
}

/* Upon return, anBoard contains the board position after making the best
   play considering BOTH players' positions.  The evaluation of the play
   considering ONLY the position of the player in roll is stored in ar.
   The move used for anBoard and ar will usually (but not always) be the
   same.  Returning both values is necessary for performing variance
   reduction. */

static int 
FindBestBearoff( int anBoard[ 2 ][ 25 ], int nDice0, int nDice1,
                 float ar[ NUM_OUTPUTS ] ) {

  int i, j, anBoardTemp[ 2 ][ 25 ], iMinRolls;
  unsigned long cBestRolls;
  movelist ml;

  GenerateMoves( &ml, anBoard, nDice0, nDice1, FALSE );
    
  ml.rBestScore = -99999.9;
  cBestRolls = -1;
    
  for( i = 0; i < ml.cMoves; i++ ) {
    PositionFromKey( anBoardTemp, ml.amMoves[ i ].auch );

    SwapSides( anBoardTemp );

    if( ( j = EvalBearoff1Full( anBoardTemp, ar ) ) < cBestRolls ) {
	    iMinRolls = i;
	    cBestRolls = j;
    }
	
    if( ( ml.amMoves[ i ].rScore = -Utility( ar, &ciCubeless ) ) >
        ml.rBestScore ) {
	    ml.iMoveBest = i;
	    ml.rBestScore = ml.amMoves[ i ].rScore;
    }
  }

  PositionFromKey( anBoard, ml.amMoves[ ml.iMoveBest ].auch );

  PositionFromKey( anBoardTemp, ml.amMoves[ iMinRolls ].auch );

  SwapSides( anBoardTemp );
    
  EvalBearoff1( anBoardTemp, ar );
    
  return 0;
}

/* Rollouts of bearoff positions, with race database variance reduction and no
   lookahead. */
static int 
BearoffRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
                int nTruncate, int iTurn, int iGame, int cGames ) {

  int anDice[ 2 ], i;
  float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ], arMin[ NUM_OUTPUTS ];

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] = 0.0f;

  while( iTurn < nTruncate && ClassifyPosition( anBoard ) > CLASS_PERFECT ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
	    return -1;
	
    if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

    if( EvaluatePosition( anBoard, arMean, &ciCubeless, 0 ) < 0 )
	    return -1;
	
    if( iTurn & 1 )
	    InvertEvaluation( arMean );
	    
    FindBestBearoff( anBoard, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
                     arMin );

    if( !( iTurn & 1 ) )
	    InvertEvaluation( arMin );
	
    SwapSides( anBoard );

    for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] - arMin[ i ];

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    iTurn++;
  }

  if( EvaluatePosition( anBoard, ar, &ciCubeless, 0 ) )
    return -1;

  if( iTurn & 1 )
    InvertEvaluation( ar );

  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] += ar[ i ];

  return 0;
}

/* Rollout with no variance reduction, until a bearoff position is reached. */
static int 
BasicRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
              int nTruncate, int iTurn, int iGame, int cGames,
              cubeinfo *pci, evalcontext *pec ) {
  
  positionclass pc;
  int anDice[ 2 ];
    
  while( iTurn < nTruncate && ( pc = ClassifyPosition( anBoard ) ) >
         CLASS_BEAROFF1 ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
	    return -1;

    FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ], anBoard, pci, pec );

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    SwapSides( anBoard );

    iTurn++;
  }

  if( iTurn < nTruncate && pc == CLASS_BEAROFF1 )
    return BearoffRollout( anBoard, arOutput, nTruncate, iTurn, iGame,
                           cGames );

  if( EvaluatePosition( anBoard, arOutput, pci, pec ) )
    return -1;

  if( iTurn & 1 )
    InvertEvaluation( arOutput );

  return 0;
}

/* Variance reduction rollout with lookahead. */
static int VarRednRollout( int anBoard[ 2 ][ 25 ], float arOutput[],
                           int nTruncate, int iTurn, int iGame, int cGames,
                           cubeinfo *pci, evalcontext *pec ) {

  int aaanBoard[ 6 ][ 6 ][ 2 ][ 25 ],	anDice[ 2 ], i, j, n;
  float ar[ NUM_OUTPUTS ], arMean[ NUM_OUTPUTS ],
    aaar[ 6 ][ 6 ][ NUM_OUTPUTS ];
  positionclass pc;
  evalcontext ec;

  /* Create an evaluation context for evaluating positions one ply deep. */
  memcpy( &ec, pec, sizeof( ec ) );
  if( ec.nPlies ) {
    ec.nPlies--;
    ec.nSearchCandidates >>= 1;
    ec.rSearchTolerance /= 2.0;
  }
    
  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] = 0.0f;

  while( iTurn < nTruncate && ( pc = ClassifyPosition( anBoard ) ) >
         CLASS_BEAROFF1 ) {
    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 )
	    return -1;
	
    if( anDice[ 0 ]-- < anDice[ 1 ]-- )
	    swap( anDice, anDice + 1 );

    for( n = 0; n < NUM_OUTPUTS; n++ )
	    arMean[ n ] = 0.0f;
		
    for( i = 0; i < 6; i++ )
	    for( j = 0; j <= i; j++ ) {
        memcpy( &aaanBoard[ i ][ j ][ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
                2 * 25 * sizeof( int ) );

        /* Find the best move for each roll at ply 0 only. */
        if( FindBestMove( NULL, i + 1, j + 1, aaanBoard[ i ][ j ],
                          pci, NULL ) < 0 )
          return -1;
		
        SwapSides( aaanBoard[ i ][ j ] );

        /* Re-evaluate the chosen move at ply n-1. */
        if( EvaluatePosition( aaanBoard[ i ][ j ], aaar[ i ][ j ],
                              pci, &ec ) )
          return -1;
			
        if( !( iTurn & 1 ) )
          InvertEvaluation( aaar[ i ][ j ] );

        /* Calculate arMean; it will be the evaluation of the
           current position at ply n. */
        for( n = 0; n < NUM_OUTPUTS; n++ )
          arMean[ n ] += ( ( i == j ) ? aaar[ i ][ j ][ n ] :
                           ( aaar[ i ][ j ][ n ] * 2.0 ) );
	    }
		
    for( n = 0; n < NUM_OUTPUTS; n++ )
	    arMean[ n ] /= 36.0;

    /* Chose which move to play for the given roll. */
    if( pec->nPlies ) {
	    /* User requested n-ply play.  Another call to FindBestMove
	       is required. */
	    if( FindBestMove( NULL, anDice[ 0 ] + 1, anDice[ 1 ] + 1,
                        anBoard, pci, pec ) < 0 )
        return -1;
	    
	    SwapSides( anBoard );
    } else
	    /* 0-ply play; best move is already recorded. */
	    memcpy( &anBoard[ 0 ][ 0 ], &aaanBoard[ anDice[ 0 ] ]
              [ anDice[ 1 ]  ][ 0 ][ 0 ], 2 * 25 * sizeof( int ) );

    /* Accumulate variance reduction term. */
    for( i = 0; i < NUM_OUTPUTS; i++ )
	    arOutput[ i ] += arMean[ i ] -
        aaar[ anDice[ 0 ] ][ anDice[ 1 ] ][ i ];

    if( fAction )
	fnAction();
    
    if( fInterrupt )
	return -1;
	
    iTurn++;
  }

  /* Evaluate the resulting position accordingly. */
  if( iTurn < nTruncate && pc == CLASS_BEAROFF1 ) {
    if( BearoffRollout( anBoard, ar, nTruncate, iTurn,
                        iGame, cGames ) )
	    return -1;
  } else {
    if( EvaluatePosition( anBoard, ar, pci, pec ) )
	    return -1;

    if( iTurn & 1 )
	    InvertEvaluation( ar );
  }
    
  /* The final output is the sum of the resulting evaluation and all
     variance reduction terms. */
  for( i = 0; i < NUM_OUTPUTS; i++ )
    arOutput[ i ] += ar[ i ];
    
  return 0;
}

extern int Rollout( int anBoard[ 2 ][ 25 ], char *sz, float arOutput[],
		    float arStdDev[], int nTruncate, int cGames, int fVarRedn,
                    cubeinfo *pci, evalcontext *pec, int fInvert ) {

  int i, j, anBoardEval[ 2 ][ 25 ], anBoardOrig[ 2 ][ 25 ];
  float ar[ NUM_ROLLOUT_OUTPUTS ];
  double arResult[ NUM_ROLLOUT_OUTPUTS ], arVariance[ NUM_ROLLOUT_OUTPUTS ];
  float arMu[ NUM_ROLLOUT_OUTPUTS ], arSigma[ NUM_ROLLOUT_OUTPUTS ];
  enum _rollouttype { BEAROFF, BASIC, VARREDN } rt;
    
  if( cGames < 1 ) {
    errno = EINVAL;
    return -1;
  }

  if( ClassifyPosition( anBoard ) == CLASS_BEAROFF1 )
    rt = BEAROFF;
  else if( fVarRedn )
    rt = VARREDN;
  else
    rt = BASIC;
    
  for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
    arResult[ i ] = arVariance[ i ] = arMu[ i ] = 0.0f;

  memcpy( &anBoardOrig[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	  sizeof( anBoardOrig ) );

  if( fInvert )
      SwapSides( anBoardOrig );
  
  for( i = 0; i < cGames; i++ ) {
      if( rngCurrent != RNG_MANUAL )
	  InitRNGSeed( nRolloutSeed + ( i << 8 ) );
      
      memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	      sizeof( anBoardEval ) );

      switch( rt ) {
      case BEAROFF:
	  BearoffRollout( anBoardEval, ar, nTruncate, 0, i, cGames );
	  break;
      case BASIC:
	  BasicRollout( anBoardEval, ar, nTruncate, 0, i, cGames, pci, pec ); 
	  break;
      case VARREDN:
	  VarRednRollout( anBoardEval, ar, nTruncate, 0, i, cGames, pci, pec );
	  break;
      }
      
      if( fInterrupt )
	  break;
      
      if( fInvert )
	  InvertEvaluation( ar );
      
      ar[ OUTPUT_EQUITY ] = ar[ OUTPUT_WIN ] * 2.0 - 1.0 +
	  ar[ OUTPUT_WINGAMMON ] +
	  ar[ OUTPUT_WINBACKGAMMON ] -
	  ar[ OUTPUT_LOSEGAMMON ] -
	  ar[ OUTPUT_LOSEBACKGAMMON ];
      
      for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	  float rMuNew, rDelta;
	  
	  arResult[ j ] += ar[ j ];
	  rMuNew = arResult[ j ] / ( i + 1 );
	  
	  rDelta = rMuNew - arMu[ j ];
	  
	  arVariance[ j ] = arVariance[ j ] * ( 1.0 - 1.0 / ( i + 1 ) ) +
	      ( i + 2 ) * rDelta * rDelta;
	  
	  arMu[ j ] = rMuNew;
	  
	  if( j < OUTPUT_EQUITY ) {
	      if( arMu[ j ] < 0.0f )
		  arMu[ j ] = 0.0f;
	      else if( arMu[ j ] > 1.0f )
		  arMu[ j ] = 1.0f;
	  }
	  
	  arSigma[ j ] = sqrt( arVariance[ j ] / ( i + 1 ) );
      }

      SanityCheck( anBoardOrig, arMu );
      
      if( fShowProgress ) {
#if USE_GTK
	  if( fX )
	      GTKRolloutUpdate( arMu, arSigma, i, cGames );
	  else
#endif
	      {
		  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
			   "%5d\r", sz, arMu[ 0 ], arMu[ 1 ], arMu[ 2 ],
			   arMu[ 3 ], arMu[ 4 ], arMu[ 5 ], arSigma[ 5 ],
			   i + 1 );
		  fflush( stdout );
	      }
      }
  }

  if( !( cGames = i ) )
    return -1;

  if( arOutput )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	    arOutput[ i ] = arMu[ i ];

  if( arStdDev )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
	    arStdDev[ i ] = arSigma[ i ];	

  if( fShowProgress
#if USE_GTK
      && !fX
#endif
      ) {
      for( i = 0; i < 79; i++ )
	  outputc( ' ' );

      outputc( '\r' );
      fflush( stdout );
  }
    
  return cGames;
}


static int
BasicCubefulRollout ( int aanBoard[][ 2 ][ 25 ],
                      float aarOutput[][ NUM_ROLLOUT_OUTPUTS ], 
                      int iTurn, int iGame,
                      cubeinfo aci[], int cci, rolloutcontext *prc ) {
  int anDice [ 2 ];
  cubeinfo *pciLocal, *pci;
  cubedecision cd;
  int *pfFinished, *pf, ici;

  float arCf[ NUM_CUBEFUL_OUTPUTS ];

  int nTruncate = prc->nTruncate;
  int cGames = prc->nTrials;

  /* Make local copy of cubeinfo struct, since it
     may be modified */

  (cubeinfo *) pciLocal = malloc ( cci * sizeof ( cubeinfo ) );
  (int *) pfFinished = malloc ( cci * sizeof ( int ) );

  for ( ici = 0; ici < cci; ici++ )
    pfFinished[ ici ] = TRUE;

  memcpy ( pciLocal, aci, cci * sizeof (cubeinfo) );

  while ( iTurn < nTruncate ) {

    /* Cube decision */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        if ( prc->fCubeful && GetDPEq ( NULL, NULL, pci ) ) {

          if ( FindCubeDecision ( &cd, arCf,
                                  aarOutput[ ici ],
                                  aanBoard[ ici ],
                                  pci, &prc->aecCube[ pci->fMove ] ) )
            return -1;


          switch ( cd ) {

          case DOUBLE_TAKE:

            SetCubeInfo ( pci, 2 * pci->nCube, ! fMove, fMove, pci->nMatchTo,
                          pci->anScore, pci->fCrawford, pci->fJacoby,
                          pci->fBeavers);
            break;
        
          case DOUBLE_PASS:

            /* FIXME: we may check if all pfFinished are false,
               in which case we are finished */

            *pf = FALSE;

            /* assign cubeful output */

            aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
              arCf[ OUTPUT_OPTIMAL ];

            /* invert evaluations if required */

            if ( iTurn & 1 ) {

              InvertEvaluation ( aarOutput[ ici ] );
              aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= -1.0f;

            }

            break;

          case NODOUBLE_TAKE:
          case TOOGOOD_TAKE:
          case TOOGOOD_PASS:
          default:

            /* no op */
            break;
            
          }
        } /* cube */
      }
    } /* loop over ci */

    /* Chequer play */

    if( QuasiRandomDice( iTurn, iGame, cGames, anDice ) < 0 ) {
      free ( pciLocal );
      return -1;
    }


    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {

        FindBestMove( NULL, anDice[ 0 ], anDice[ 1 ],
                      aanBoard[ ici ], pci,
                      &prc->aecChequer [ pci->fMove ] );

        if( fAction )
          fnAction();
    
        if( fInterrupt ) {
          free ( pciLocal );
          return -1;
        }


        /* Invert board and more */
	
        SwapSides( aanBoard[ ici ] );

        SetCubeInfo ( pci, pci->nCube, pci->fCubeOwner,
                      ! pci->fMove, pci->nMatchTo,
                      pci->anScore, pci->fCrawford,
                      pci->fJacoby, pci->fBeavers );

      }
    }

    iTurn++;

  } /* loop truncate */


  /* evaluation at truncation */

    for ( ici = 0, pci = pciLocal, pf = pfFinished;
          ici < cci; ici++, pci++, pf++ ) {

      if ( *pf ) {
        
        if ( prc->fCubeful ) {
          if ( EvaluatePositionCubeful ( aanBoard[ ici ], arCf,
                                         aarOutput[ ici ],
                                         pci,
                                         &prc->aecCube[ pci->fMove ], 
                                         prc->aecCube[ pci->fMove ].nPlies ) )
            return -1;

          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
            arCf[ OUTPUT_OPTIMAL ];

        } 
        else {
          if ( EvaluatePosition ( aanBoard[ ici ], aarOutput[ ici ],
                                  pci,
                                  &prc->aecChequer[ pci->fMove ] ) )
            return -1;

          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] = 0.0f;
          
        }


        if ( iTurn & 1 ) {
          
          InvertEvaluation ( aarOutput[ ici ] );
          aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= -1.0f;
          
        }
      }

      if ( pci->nMatchTo )
        aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
          eq2mwc ( aarOutput[ ici ][ OUTPUT_CUBEFUL_EQUITY ], pci );
      
    }

    free ( pciLocal );
    free ( pfFinished );
    
    return 0;
}


extern int
RolloutGeneral( int anBoard[ 2 ][ 25 ], char *sz,
                   float aarOutput[][ NUM_ROLLOUT_OUTPUTS ],
                   float aarStdDev[][ NUM_ROLLOUT_OUTPUTS ],
                   rolloutcontext *prc,
                   cubeinfo aci[], int cci, int fInvert ) {

  int (*aanBoardEval)[ 2 ][ 25 ];
  float (*aar)[ NUM_ROLLOUT_OUTPUTS ];
  float (*aarMu)[ NUM_ROLLOUT_OUTPUTS ];
  float (*aarSigma)[ NUM_ROLLOUT_OUTPUTS ];
  double (*aarResult)[ NUM_ROLLOUT_OUTPUTS ];
  double (*aarVariance)[ NUM_ROLLOUT_OUTPUTS ];

  int i, j, ici;
  int anBoardOrig[ 2 ][ 25 ];

  enum _rollouttype { BEAROFF, BASIC, VARREDN } rt;

  int cGames = prc->nTrials;
    
  /* FIXME: use alloca! */

  aanBoardEval = malloc ( cci * 50 * sizeof ( int ) );

  aar =
    malloc ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  aarMu =
    malloc ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );
  aarSigma =
    malloc ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( float ) );

  aarResult =
    malloc ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( double ) );
  aarVariance =
    malloc ( cci * NUM_ROLLOUT_OUTPUTS * sizeof ( double ) );

  if( cGames < 1 ) {
    errno = EINVAL;
    return -1;
  }


  if( cci < 1 ) {
    errno = EINVAL;
    return -1;
  }


  if( ClassifyPosition( anBoard ) == CLASS_BEAROFF1 ) 
    rt = BEAROFF;
  else if ( prc->fVarRedn )
    rt = VARREDN;
  else
    rt = BASIC;
    
  for ( ici = 0; ici < cci; ici++ )
    for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
      aarResult[ ici ][ i ] =
        aarVariance[ ici ][ i ] =
        aarMu[ ici ][ i ] = 0.0f;


  memcpy( &anBoardOrig[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
	  sizeof( anBoardOrig ) );


  if( fInvert )
      SwapSides( anBoardOrig );

  
  for( i = 0; i < cGames; i++ ) {
      if( rngCurrent != RNG_MANUAL )
	  InitRNGSeed( nRolloutSeed + ( i << 8 ) );
      
      for ( ici = 0; ici < cci; ici++ )
        memcpy ( &aanBoardEval[ ici ][ 0 ][ 0 ], &anBoardOrig[ 0 ][ 0 ],
                 sizeof ( anBoardOrig ) );

      /* FIXME: old code was:
         
         memcpy( &anBoardEval[ 0 ][ 0 ], &anBoard[ 0 ][ 0 ],
         sizeof( anBoardEval ) );

         should it be anBoardOrig? */

      switch( rt ) {
      case BEAROFF:
        if ( ! prc->fCubeful )
	  BearoffRollout( *aanBoardEval, *aar, prc->nTruncate,
                          0, i, prc->nTrials );
        else
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aci, cci, prc );
        break;
      case BASIC:
	  BasicCubefulRollout( aanBoardEval, aar, 
                               0, i, aci, cci, prc );
	  break;
      case VARREDN:

        if ( ! prc->fCubeful )
	  VarRednRollout( *aanBoardEval, *aar, prc->nTruncate,
                          0, i, cGames, &aci[ 0 ],
                          &prc->aecChequer[ 0 ] );
        else
        /* FIXME: write me
           VarRednCubefulRollout( aanBoardEval, aar, 
                                  0, i, pci, prc ); */
          printf ( "not implemented yet...\n" );
        break;
      }
      
      if( fInterrupt )
	  break;
      
      for ( ici = 0; ici < cci ; ici++ ) {

        if( fInvert ) {

	  InvertEvaluation( aar[ ici ] );
          if ( aci[ ici ].nMatchTo )
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] =
              1.0 - aar [ ici ][ OUTPUT_CUBEFUL_EQUITY ];
          else
            aar[ ici ][ OUTPUT_CUBEFUL_EQUITY ] *= -1.0f;

        }

        aar[ ici ][ OUTPUT_EQUITY ] = Utility ( aar[ ici ], &aci [ ici ]);
      
        for( j = 0; j < NUM_ROLLOUT_OUTPUTS; j++ ) {
	  float rMuNew, rDelta;
	  
	  aarResult[ ici ][ j ] += aar[ ici ][ j ];
	  rMuNew = aarResult[ ici ][ j ] / ( i + 1 );
	  
	  rDelta = rMuNew - aarMu[ ici ][ j ];
	  
	  aarVariance[ ici ][ j ] =
            aarVariance[ ici ][ j ] * ( 1.0 - 1.0 / ( i + 1 ) ) +
	      ( i + 2 ) * rDelta * rDelta;
	  
	  aarMu[ ici ][ j ] = rMuNew;
	  
	  if( j < OUTPUT_EQUITY ) {
	      if( aarMu[ ici ][ j ] < 0.0f )
		  aarMu[ ici ][ j ] = 0.0f;
	      else if( aarMu[ ici ][ j ] > 1.0f )
		  aarMu[ ici ][ j ] = 1.0f;
	  }
	  
	  aarSigma[ ici ][ j ] = sqrt( aarVariance[ ici ][ j ] / ( i + 1 ) );
        }

        SanityCheck( anBoardOrig, aarMu[ ici ] );

      }
      
      if( fShowProgress ) {
#if USE_GTK
	  if( fX )
	      GTKRolloutUpdate( *aarMu, *aarSigma, i, cGames );
	  else
#endif
	      {
		  outputf( "%28s %5.3f %5.3f %5.3f %5.3f %5.3f (%6.3f) %5.3f "
			   "Cubeful: %6.3f %5.3f %5d\r", sz,
                           aarMu[ 0 ][ 0 ], aarMu[ 0 ][ 1 ], aarMu[ 0 ][ 2 ],
			   aarMu[ 0 ][ 3 ], aarMu[ 0 ][ 4 ], aarMu[ 0 ][ 5 ],
                           aarSigma[ 0 ][ 5 ],
                           aarMu[ 0 ][ 6 ], aarSigma[ 0 ][ 6 ],
			   i + 1 );
		  fflush( stdout );
	      }
      }
  }

  if( !( cGames = i ) )
    return -1;

  if( aarOutput )
    for ( ici = 0; ici < cci; ici++ )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarOutput[ ici ][ i ] = aarMu[ ici ][ i ];

  if( aarStdDev )
    for ( ici = 0; ici < cci; ici++ )
      for( i = 0; i < NUM_ROLLOUT_OUTPUTS; i++ )
        aarStdDev[ ici ][ i ] = aarSigma[ ici ][ i ];	

  if( fShowProgress
#if USE_GTK
      && !fX
#endif
      ) {
      for( i = 0; i < 79; i++ )
	  outputc( ' ' );

      outputc( '\r' );
      fflush( stdout );
  }

  free ( aanBoardEval );

  free ( aar );
  free ( aarMu );
  free ( aarSigma );

  free ( aarResult );
  free ( aarVariance );

  return cGames;
}
