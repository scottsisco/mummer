
//-- NOTE: this option will significantly hamper program performance,
//         mostly the alignment extension performance (sw_align.h)
//#define _DEBUG_ASSERT       // self testing assert functions

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>

#include "postnuc.hh"
#include "tigrinc.hh"
#include "sw_align.hh"

namespace mummer {
namespace postnuc {
// Read one sequence from fasta stream into T (starting at index 1, 0
// is unused), store its name and returns true if successful.
bool Read_Sequence(std::istream& is, std::string& T, std::string& name) {
  int c = is.peek();
  for( ; c != EOF && c != '>'; c = is.peek())
    ignore_line(is);
  if(c == EOF) return false;
  std::getline(is, name);
  name = name.substr(1, name.find_first_of(" \t\n") - 1);

  T = '\0';
  std::string line;
  for(c = is.peek(); c != EOF && c != '>'; c = is.peek()) {
    c = is.get();
    if(isspace(c)) continue;
    c = tolower(c);
    if(!isalpha(c) && c != '*')
      c = 'x';
    T += c;
  }
  return true;
}

bool Read_Sequence(std::istream& is, FastaRecord& record) {
  if(!Read_Sequence(is, record.m_seq, record.m_Id)) return false;
  record.m_len = record.m_seq.size() - 1;
  return true;
}

void addNewAlignment
(std::vector<Alignment> & Alignments, std::vector<Cluster>::iterator Cp,
 std::vector<Match>::iterator Mp)

//  Create and add a new alignment object based on the current match
//  and cluster information pointed to by Cp and Mp.

{
  Alignment Align;

  //-- Initialize the data
  Align.sA = Mp->sA;
  Align.sB = Mp->sB;
  Align.eA = Mp->sA + Mp->len - 1;
  Align.eB = Mp->sB + Mp->len - 1;
  Align.dirB = Cp->dirB;
  Align.delta.clear( );
  Align.deltaApos = 0;

  //-- Add the alignment object
  Alignments.push_back (Align);

  return;
}




bool merge_syntenys::extendBackward(std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator CurrAp,
                                    std::vector<Alignment>::iterator TargetAp, const char * A, const char * B)

//  Extend an alignment backwards off of the current alignment object.
//  The current alignment object must be freshly created and consist
//  only of an exact match (i.e. the delta vector MUST be empty).
//  If the TargetAp alignment object is reached by the extension, it will
//  be merged with CurrAp and CurrAp will be destroyed. If TargetAp is
//  NULL the function will extend as far as possible. It is a strange
//  and dangerous function because it can delete CurrAp, so edit with
//  caution. Returns true if TargetAp was reached and merged, else false
//  Designed only as a subroutine for extendClusters, should be used
//  nowhere else.

{
  bool target_reached = false;
  bool overflow_flag = false;
  bool double_flag = false;

  std::vector<long int>::iterator Dp;

  unsigned int m_o;
  long int targetA, targetB;

  m_o = BACKWARD_SEARCH;

  //-- Set the target coordinates
  if ( TargetAp != Alignments.end( ) )
    {
      targetA = TargetAp->eA;
      targetB = TargetAp->eB;
    }
  else
    {
      targetA = 1;
      targetB = 1;
      m_o |= OPTIMAL_BIT;
    }

  //-- If alignment is too long, bring with bounds and set overflow_flag true
  if ( CurrAp->sA - targetA + 1 > MAX_ALIGNMENT_LENGTH )
    {
      targetA = CurrAp->sA - MAX_ALIGNMENT_LENGTH + 1;
      overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }
  if ( CurrAp->sB - targetB + 1 > MAX_ALIGNMENT_LENGTH )
    {
      targetB = CurrAp->sB - MAX_ALIGNMENT_LENGTH + 1;
      if ( overflow_flag )
        double_flag = true;
      else
        overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }


  if ( TO_SEQEND && !double_flag )
    m_o |= SEQEND_BIT;


  //-- Attempt to reach the target
  target_reached = alignSearch (A, CurrAp->sA, targetA,
                                B, CurrAp->sB, targetB, m_o);

  if ( overflow_flag  ||  TargetAp == Alignments.end( ) )
    target_reached = false;

  if ( target_reached )
    {
      //-- Merge the two alignment objects
      extendForward (TargetAp, A, CurrAp->sA,
                     B, CurrAp->sB, FORCED_FORWARD_ALIGN);
      TargetAp->eA = CurrAp->eA;
      TargetAp->eB = CurrAp->eB;
      Alignments.pop_back( );
    }
  else
    {
      alignTarget (A, targetA, CurrAp->sA,
                   B, targetB, CurrAp->sB,
                   CurrAp->delta, FORCED_FORWARD_ALIGN);
      CurrAp->sA = targetA;
      CurrAp->sB = targetB;

      //-- Update the deltaApos value for the alignment object
      for ( Dp = CurrAp->delta.begin( ); Dp < CurrAp->delta.end( ); Dp ++ )
        CurrAp->deltaApos += *Dp > 0 ? *Dp : labs(*Dp) - 1;
    }

  return target_reached;
}




bool extendForward
(std::vector<Alignment>::iterator CurrAp, const char * A, long int targetA,
 const char * B, long int targetB, unsigned int m_o)

//  Extend an alignment forwards off the current alignment object until
//  target or end of sequence is reached, and merge the delta values of the
//  alignment object with the new delta values generated by the extension.
//  Return true if the target was reached, else false

{
  long int ValA;
  long int ValB;
  unsigned int Di;
  bool target_reached;
  bool overflow_flag = false;
  bool double_flag = false;
  std::vector<long int>::iterator Dp;

  //-- Set Di to the end of the delta vector
  Di = CurrAp->delta.size( );

  //-- Calculate the distance between the start and end positions
  ValA = targetA - CurrAp->eA + 1;
  ValB = targetB - CurrAp->eB + 1;

  //-- If the distance is too long, shrink it and set the overflow_flag
  if ( ValA > MAX_ALIGNMENT_LENGTH )
    {
      targetA = CurrAp->eA + MAX_ALIGNMENT_LENGTH - 1;
      overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }
  if ( ValB > MAX_ALIGNMENT_LENGTH )
    {
      targetB = CurrAp->eB + MAX_ALIGNMENT_LENGTH - 1;
      if ( overflow_flag )
        double_flag = true;
      else
        overflow_flag = true;
      m_o |= OPTIMAL_BIT;
    }

  if ( double_flag )
    m_o &= ~SEQEND_BIT;

  target_reached = alignTarget (A, CurrAp->eA, targetA,
                                B, CurrAp->eB, targetB,
                                CurrAp->delta, m_o);

  //-- Notify user if alignment was chopped short
  if ( target_reached  &&  overflow_flag )
    target_reached = false;

  //-- Pick up delta where left off (Di) and merge with new deltas
  if ( Di < CurrAp->delta.size( ) )
    {
      //-- Merge the deltas
      ValA = (CurrAp->eA - CurrAp->sA + 1) - CurrAp->deltaApos - 1;
      CurrAp->delta[Di] += CurrAp->delta[Di] > 0 ? ValA : -(ValA);
      if ( CurrAp->delta[Di] == 0  ||  ValA < 0 ) {
        cerr << "ERROR: failed to merge alignments at position " << CurrAp->eA << '\n'
             << "       Please file a bug report\n";
        exit (EXIT_FAILURE);
      }

      //-- Update the deltaApos
      for ( Dp = CurrAp->delta.begin( ) + Di; Dp < CurrAp->delta.end( ); Dp ++ )
        CurrAp->deltaApos += *Dp > 0 ? *Dp : labs(*Dp) - 1;
    }

  //-- Set the alignment coordinates
  CurrAp->eA = targetA;
  CurrAp->eB = targetB;

  return target_reached;
}




void merge_syntenys::extendClusters(std::vector<Cluster> & Clusters,
                                    const FastaRecord * Af, const FastaRecord * Bf, std::ostream& DeltaFile)

//  Connect all the matches in every cluster between sequences A and B.
//  Also, extend alignments off of the front and back of each cluster to
//  expand total alignment coverage. When these extensions encounter an
//  adjacent cluster, fuse the two regions to create one single
//  encompassing region. This routine will create alignment objects from
//  these extensions and output the resulting delta information to the
//  delta output file.

{
  //-- Sort the clusters (ascending) by their start coordinate in sequence A
  sort (Clusters.begin( ), Clusters.end( ), AscendingClusterSort( ));


  //-- If no delta file is requested
  if ( ! DO_DELTA )
    return;


  bool target_reached = false;         // reached the adjacent match or cluster

  const char * A, * B;                 // the sequences A and B
  char * Brev = NULL;                  // the reverse complement of B

  unsigned int m_o;
  long int targetA, targetB;           // alignment extension targets in A and B

  std::vector<Match>::iterator Mp;          // match pointer

  std::vector<Cluster>::iterator PrevCp;    // where the extensions last left off
  std::vector<Cluster>::iterator CurrCp;    // the current cluster being extended
  std::vector<Cluster>::iterator TargetCp = Clusters.end( );  // the target cluster

  std::vector<Alignment> Alignments;        // the vector of alignment objects
  std::vector<Alignment>::iterator CurrAp = Alignments.begin( );   // current align
  std::vector<Alignment>::iterator TargetAp;                // target align


  //-- Extend each cluster
  A = Af->seq();
  PrevCp = Clusters.begin( );
  CurrCp = Clusters.begin( );
  while ( CurrCp < Clusters.end( ) ) {
      if ( DO_EXTEND ) {
        if ( ! target_reached ) //-- Ignore if shadowed or already extended
          if ( CurrCp->wasFused ||
               (!DO_SHADOWS && isShadowedCluster (CurrCp, Alignments, CurrAp)) ) {
            CurrCp->wasFused = true;
            CurrCp = ++ PrevCp;
            continue;
          }
      }

      //-- Pick the right directional sequence for B
      if ( CurrCp->dirB == FORWARD_CHAR )
        B = Bf->seq();
      else if ( Brev != NULL )
        B = Brev;
      else {
        Brev = (char *) Safe_malloc ( sizeof(char) * (Bf->len() + 2) );
        strcpy ( Brev + 1, Bf->seq() + 1 );
        Brev[0] = '\0';
        Reverse_Complement (Brev, 1, Bf->len());
        B = Brev;
      }

      //-- Extend each match in the cluster
      for ( Mp = CurrCp->matches.begin( ); Mp < CurrCp->matches.end( ); Mp ++ ) {
        //-- Try to extend the current match backwards
        if ( target_reached ) {
          //-- Merge with the previous match
          if ( CurrAp->eA != Mp->sA  ||  CurrAp->eB != Mp->sB ) {
            if ( Mp >= CurrCp->matches.end( ) - 1 ) {
              cerr << "ERROR: Target match does not exist, please\n"
                   << "       file a bug report\n";
              exit (EXIT_FAILURE);
            }
            continue;
          }
          CurrAp->eA += Mp->len - 1;
          CurrAp->eB += Mp->len - 1;
        } else { //-- Create a new alignment object
          addNewAlignment (Alignments, CurrCp, Mp);
          CurrAp = Alignments.end( ) - 1;

          if ( DO_EXTEND  ||  Mp != CurrCp->matches.begin ( ) ) {
            //-- Target the closest/best alignment object
            TargetAp = getReverseTargetAlignment (Alignments, CurrAp);

            //-- Extend the new alignment object backwards
            if ( extendBackward (Alignments, CurrAp, TargetAp, A, B) )
              CurrAp = TargetAp;
          }
        }

          m_o = FORWARD_ALIGN;

          //-- Try to extend the current match forwards
          if ( Mp < CurrCp->matches.end( ) - 1 ) {
            //-- Target the next match in the cluster
            targetA = (Mp + 1)->sA;
            targetB = (Mp + 1)->sB;

            //-- Extend the current alignment object forward
            target_reached = extendForward (CurrAp, A, targetA, B, targetB, m_o);
          } else if ( DO_EXTEND ) {
            targetA = Af->len();
            targetB = Bf->len();

            //-- Target the closest/best match in a future cluster
            TargetCp = getForwardTargetCluster (Clusters, CurrCp, targetA, targetB);
            if ( TargetCp == Clusters.end( ) ) {
              m_o |= OPTIMAL_BIT;
              if ( TO_SEQEND )
                m_o |= SEQEND_BIT;
            }

            //-- Extend the current alignment object forward
            target_reached = extendForward (CurrAp, A, targetA, B, targetB, m_o);
          }
      }
      if ( TargetCp == Clusters.end( ) )
        target_reached = false;

      CurrCp->wasFused = true;

      if ( target_reached == false )
        CurrCp = ++ PrevCp;
      else
        CurrCp = TargetCp;
    }

#ifdef _DEBUG_ASSERT
  validateData (Alignments, Clusters, Af, Bf);
#endif

  //-- Output the alignment data to the delta file
  flushAlignments (Alignments, Af, Bf, DeltaFile);

  if ( Brev != NULL )
    free (Brev);

  return;
}




void flushAlignments
(std::vector<Alignment> & Alignments,
 const FastaRecord * Af, const FastaRecord * Bf,
 std::ostream& DeltaFile)

//  Simply output the delta information stored in Alignments to the
//  given delta file. Free the memory used by Alignments once the
//  data is successfully output to the file.

{
  std::vector<Alignment>::iterator Ap;       // alignment pointer
  std::vector<long int>::iterator Dp;             // delta pointer

  DeltaFile << '>' << Af->Id() << ' ' << Bf->Id() << ' ' << Af->len() << ' ' << Bf->len() << '\n';

  //-- Generate the error counts
  parseDelta (Alignments, Af, Bf);

  for ( Ap = Alignments.begin( ); Ap != Alignments.end( ); Ap ++ ) {
    if ( Ap->dirB == FORWARD_CHAR )
      DeltaFile << Ap->sA << ' ' << Ap->eA << ' '
                << Ap->sB << ' ' << Ap->eB << ' '
                << Ap->Errors << ' ' << Ap->SimErrors << ' ' << Ap->NonAlphas
                << '\n';
    else
      DeltaFile << Ap->sA << ' ' << Ap->eA << ' '
                << revC(Ap->sB, Bf->len()) << ' ' << revC(Ap->eB, Bf->len()) << ' '
                << Ap->Errors << ' ' << Ap->SimErrors << ' ' << Ap->NonAlphas
                << '\n';

      for ( Dp = Ap->delta.begin( ); Dp < Ap->delta.end( ); Dp ++ )
        DeltaFile << *Dp << '\n';
      DeltaFile << "0\n";

      Ap->delta.clear( );
    }

  Alignments.clear( );

  return;
}




void flushSyntenys
(std::vector<Synteny> & Syntenys, std::ostream& ClusterFile)

//  Simply output the synteny/cluster information generated by the mgaps
//  program. However, now the coordinates reference their appropriate
//  reference sequence, and the reference sequecne header is added to
//  the appropriate lines. Free the memory used by Syntenys once the
//  data is successfully output to the file.

{
  std::vector<Synteny>::iterator Sp; // synteny pointer
  std::vector<Cluster>::iterator Cp; // cluster pointer
  std::vector<Match>::iterator   Mp; // match pointer

  if ( ClusterFile ) {
    for ( Sp = Syntenys.begin( ); Sp < Syntenys.end( ); Sp ++ ) {
      ClusterFile << '>' << Sp->AfP->Id() << ' ' << Sp->Bf.Id() << ' '
                  << Sp->AfP->len() << ' ' << Sp->Bf.len() << '\n';
      for ( Cp = Sp->clusters.begin( ); Cp < Sp->clusters.end( ); Cp ++ ) {
        ClusterFile << setw(2) << FORWARD_CHAR << ' ' << setw(2) << Cp->dirB << '\n';
        for ( Mp = Cp->matches.begin( ); Mp < Cp->matches.end( ); Mp ++ ) {
          if ( Cp->dirB == FORWARD_CHAR )
            ClusterFile << setw(8) << Mp->sA << ' ' << setw(8) << Mp->sB << ' '
                        << setw(6) << Mp->len;
          else
            ClusterFile << setw(8) << Mp->sA << ' ' << setw(8) << revC(Mp->sB, Sp->Bf.len()) << ' '
                        << setw(6) << Mp->len;
          if ( Mp != Cp->matches.begin( ) )
            ClusterFile << setw(6) << (Mp->sA - (Mp - 1)->sA - (Mp - 1)->len) << ' '
                        << setw(6) << (Mp->sB - (Mp - 1)->sB - (Mp - 1)->len) << '\n';
          else
            ClusterFile << "     -      -\n";
        }
        Cp->matches.clear( );
      }
      Sp->clusters.clear( );
    }
  } else {
    for ( Sp = Syntenys.begin( ); Sp < Syntenys.end( ); Sp ++ ) {
      for ( Cp = Sp->clusters.begin( ); Cp < Sp->clusters.end( ); Cp ++ )
        Cp->matches.clear( );
      Sp->clusters.clear( );
    }
  }

  Syntenys.clear( );
}




std::vector<Cluster>::iterator getForwardTargetCluster
(std::vector<Cluster> & Clusters, std::vector<Cluster>::iterator CurrCp,
 long int & targetA, long int & targetB)

//  Return the cluster that is most likely to successfully join (in a
//  forward direction) with the current cluster. The returned cluster
//  must contain 1 or more matches that are strictly greater than the end
//  of the current cluster. The targeted cluster must also be on a
//  diagonal close enough to the current cluster, so that a connection
//  could possibly be made by the alignment extender. Assumes clusters
//  have been sorted via AscendingClusterSort. Returns targeted cluster
//  and stores the target coordinates in targetA and targetB. If no
//  suitable cluster was found, the function will return NULL and target
//  A and targetB will remain unchanged.

{
  std::vector<Match>::iterator Mip;               // match iteratrive pointer
  std::vector<Cluster>::iterator Cp;              // cluster pointer
  std::vector<Cluster>::iterator Cip;             // cluster iterative pointer
  long int eA, eB;                           // possible target
  long int greater, lesser;                  // gap sizes between two clusters
  long int sA = CurrCp->matches.rbegin( )->sA +
    CurrCp->matches.rbegin( )->len - 1;      // the endA of the current cluster 
  long int sB = CurrCp->matches.rbegin( )->sB +
    CurrCp->matches.rbegin( )->len - 1;      // the endB of the current cluster

  //-- End of sequences is the default target, set distance accordingly
  long int dist = (targetA - sA < targetB - sB ? targetA - sA : targetB - sB);

  //-- For all clusters greater than the current cluster (on sequence A)
  Cp = Clusters.end( );
  for ( Cip = CurrCp + 1; Cip < Clusters.end( ); Cip ++ )
    {
      //-- If the cluster is on the same direction
      if ( CurrCp->dirB == Cip->dirB )
        {
          eA = Cip->matches.begin( )->sA;
          eB = Cip->matches.begin( )->sB;

          //-- If the cluster overlaps the current cluster, strip some matches
          if ( ( eA < sA  ||  eB < sB )  &&
               Cip->matches.rbegin( )->sA >= sA  &&
               Cip->matches.rbegin( )->sB >= sB )
            {
              for ( Mip = Cip->matches.begin( );
                    Mip < Cip->matches.end( )  &&  ( eA < sA  ||  eB < sB );
                    Mip ++ )
                {
                  eA = Mip->sA;
                  eB = Mip->sB;
                }
            }

          //-- If the cluster is strictly greater than current cluster
          if ( eA >= sA  &&  eB >= sB )
            {
              if ( eA - sA > eB - sB )
                {
                  greater = eA - sA;
                  lesser = eB - sB;
                }
              else
                {
                  lesser = eA - sA;
                  greater = eB - sB;
                }

              //-- If the cluster is close enough
              if ( greater < getBreakLen( )  ||
                   (lesser) * GOOD_SCORE [getMatrixType( )] +
                   (greater - lesser) * CONT_GAP_SCORE [getMatrixType( )] >= 0 )
                {
                  Cp = Cip;
                  targetA = eA;
                  targetB = eB;
                  break;
                }
              else if ( (greater << 1) - lesser < dist )
                {
                  Cp = Cip;
                  targetA = eA;
                  targetB = eB;
                  dist = (greater << 1) - lesser;
                }
            }
        }
    }


  return Cp;
}




std::vector<Alignment>::iterator getReverseTargetAlignment
(std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator CurrAp)

//  Return the alignment that is most likely to successfully join (in a
//  reverse direction) with the current alignment. The returned alignment
//  must be strictly less than the current cluster and be on a diagonal
//  close enough to the current alignment, so that a connection
//  could possibly be made by the alignment extender. Assumes clusters
//  have been sorted via AscendingClusterSort and processed in order, so
//  therefore all alignments are in order by their start A coordinate.

{
  std::vector<Alignment>::iterator Ap;        // alignment pointer
  std::vector<Alignment>::iterator Aip;       // alignment iterative pointer
  long int eA, eB;                       // possible targets
  long int greater, lesser;              // gap sizes between the two alignments
  long int sA = CurrAp->sA;              // the startA of the current alignment
  long int sB = CurrAp->sB;              // the startB of the current alignment

  //-- Beginning of sequences is the default target, set distance accordingly
  long int dist = (sA < sB ? sA : sB);

  //-- For all alignments less than the current alignment (on sequence A)
  Ap = Alignments.end( );
  for ( Aip = CurrAp - 1; Aip >= Alignments.begin( ); Aip -- )
    {
      //-- If the alignment is on the same direction
      if ( CurrAp->dirB == Aip->dirB )
        {
          eA = Aip->eA;
          eB = Aip->eB;

          //-- If the alignment is strictly less than current cluster
          if ( eA <= sA  && eB <= sB )
            {
              if ( sA - eA > sB - eB )
                {
                  greater = sA - eA;
                  lesser = sB - eB;
                }
              else
                {
                  lesser = sA - eA;
                  greater = sB - eB;
                }

              //-- If the cluster is close enough
              if ( greater < getBreakLen( )  ||
                   (lesser) * GOOD_SCORE [getMatrixType( )] +
                   (greater - lesser) * CONT_GAP_SCORE [getMatrixType( )] >= 0 )
                {
                  Ap = Aip;
                  break;
                }
              else if ( (greater << 1) - lesser < dist )
                {
                  Ap = Aip;
                  dist = (greater << 1) - lesser;
                }
            }
        }
    }


  return Ap;
}




bool isShadowedCluster
(std::vector<Cluster>::iterator CurrCp,
 std::vector<Alignment> & Alignments, std::vector<Alignment>::iterator Ap)

//  Check if the current cluster is shadowed by a previously produced
//  alignment region. Return true if it is, else false.

{
  std::vector<Alignment>::iterator Aip;         // alignment pointer

  long int sA = CurrCp->matches.begin( )->sA;
  long int eA = CurrCp->matches.rbegin( )->sA +
    CurrCp->matches.rbegin( )->len - 1;
  long int sB = CurrCp->matches.begin( )->sB;
  long int eB = CurrCp->matches.rbegin( )->sB +
    CurrCp->matches.rbegin( )->len - 1;

  if ( ! Alignments.empty( ) )             // if there are alignments to use
    {
      //-- Look backwards in hope of finding a shadowing alignment
      for ( Aip = Ap; Aip >= Alignments.begin( ); Aip -- )
        {
          //-- If in the same direction and shadowing the current cluster, break
          if ( Aip->dirB == CurrCp->dirB )
            if ( Aip->eA >= eA  &&  Aip->eB >= eB )
              if ( Aip->sA <= sA  &&  Aip->sB <= sB )
                break;
        }

      //-- Return true if the loop was broken, i.e. shadow found
      if ( Aip >= Alignments.begin( ) )
        return true;
    }

  //-- Return false if Alignments was empty or loop was not broken
  return false;
}




void __parseAbort
(const char * s, const char* file, size_t line)

//  Abort the program if there was an error in parsing file 's'

{
  std::cerr << "ERROR: " << file << ':' << line
            << " Could not parse input from '" << s << "'. \n"
            << "Please check the filename and format, or file a bug report\n";
  exit (EXIT_FAILURE);
}

void parseDelta
(std::vector<Alignment> & Alignments,
 const FastaRecord * Af, const FastaRecord *Bf)

// Use the delta information to generate the error counts for each
// alignment, and fill this information into the data type

{
  const char * A, * B;
  char* Brev = NULL;
  char ch1, ch2;
  long int Delta;
  int Sign;
  long int i, Apos, Bpos;
  long int Remain, Total;
  long int Errors, SimErrors;
  long int NonAlphas;
  std::vector<Alignment>::iterator Ap;
  std::vector<long int>::iterator Dp;

  for ( Ap = Alignments.begin( ); Ap != Alignments.end( ); ++Ap) {
      A = Af->seq();
      B = Bf->seq();

      if ( Ap->dirB == REVERSE_CHAR ) {
        if ( Brev == NULL ) {
          Brev = (char *) Safe_malloc ( sizeof(char) * (Bf->len() + 2) );
          strcpy ( Brev + 1, Bf->seq() + 1 );
          Brev[0] = '\0';
          Reverse_Complement (Brev, 1, Bf->len());
        }
        B = Brev;
      }

      Apos = Ap->sA;
      Bpos = Ap->sB;

      Errors = 0;
      SimErrors = 0;
      NonAlphas = 0;
      Remain = Ap->eA - Ap->sA + 1;
      Total = Remain;

      //-- For all delta's in this alignment
      for ( Dp = Ap->delta.begin( ); Dp != Ap->delta.end( ); ++Dp) {
        Delta = *Dp;
        Sign = Delta > 0 ? 1 : -1;
        Delta = std::abs ( Delta );

        //-- For all the bases before the next indel
          for ( i = 1; i < Delta; i ++ ) {
            ch1 = A [Apos ++];
            ch2 = B [Bpos ++];

            if ( !isalpha (ch1) ) {
              ch1 = STOP_CHAR;
              NonAlphas ++;
            }
            if ( !isalpha (ch2) ) {
              ch2 = STOP_CHAR;
              NonAlphas ++;
            }

            ch1 = toupper(ch1);
            ch2 = toupper(ch2);
            if (1 > MATCH_SCORE[getMatrixType( )][ch1 - 'A'][ch2 - 'A'] )
              SimErrors ++;
            if ( ch1 != ch2 )
                Errors ++;
          }

          //-- Process the current indel
          Remain -= i - 1;
          Errors ++;
          SimErrors ++;

          if ( Sign == 1 ) {
            Apos ++;
            Remain --;
          } else {
            Bpos ++;
            Total ++;
          }
      }

      //-- For all the bases after the final indel
      for ( i = 0; i < Remain; i ++ ) {
        //-- Score character match and update error counters
        ch1 = A [Apos ++];
        ch2 = B [Bpos ++];

        if ( !isalpha (ch1) ) {
          ch1 = STOP_CHAR;
          NonAlphas ++;
        }
        if ( !isalpha (ch2) ) {
          ch2 = STOP_CHAR;
          NonAlphas ++;
        }

        ch1 = toupper(ch1);
        ch2 = toupper(ch2);
        if ( 1 > MATCH_SCORE[getMatrixType( )][ch1 - 'A'][ch2 - 'A'] )
          SimErrors ++;
        if ( ch1 != ch2 )
          Errors ++;
      }

      Ap->Errors = Errors;
      Ap->SimErrors = SimErrors;
      Ap->NonAlphas = NonAlphas;
  }

  if ( Brev != NULL )
    free ( Brev );
}


void merge_syntenys::processSyntenys(std::vector<Synteny> & Syntenys, FastaRecord * Af,
                                     std::istream& QryFile, std::ostream& ClusterFile, std::ostream& DeltaFile)

//  For each syntenic region with clusters, read in the B sequence and
//  extend the clusters to expand total alignment coverage. Only should
//  be called once all the clusters for the contained syntenic regions have
//  been stored in the data structure. Frees the memory used by the
//  the syntenic regions once the output of extendClusters and
//  flushSyntenys has been produced.

{
  FastaRecord Bf;                     // the current B sequence

  std::vector<Synteny>::iterator CurrSp;   // current synteny pointer

  //-- For all the contained syntenys
  for(CurrSp = Syntenys.begin(); CurrSp != Syntenys.end(); ++CurrSp) {
      //-- If no clusters, ignore
      if(CurrSp->clusters.empty())
        continue;

      //-- If a B sequence not seen yet, read it in
      //-- IMPORTANT: The B sequences in the synteny object are assumed to be
      //      ordered as output by mgaps, if they are not in order the program
      //      will fail. (All like tags must be adjacent and in the same order
      //      as the query file)
      if(CurrSp == Syntenys.begin( )  ||
         CurrSp->Bf.Id() != (CurrSp-1)->Bf.Id()) {
          //-- Read in the B sequence
        while(Read_Sequence(QryFile, Bf))
          if(CurrSp->Bf.Id() == Bf.Id())
              break;
        if(CurrSp->Bf.Id() != Bf.Id())
          parseAbort ( "Query File " + CurrSp->Bf.Id() + " != " + Bf.Id());
      }

      //-- Extend clusters and create the alignment information
      /// XXX: not setting CurrSp->Bf.len may not be correct XXX
      CurrSp->Bf.len_w() = Bf.len();
      assert(CurrSp->Bf.len() >= 0);
      extendClusters (CurrSp->clusters, CurrSp->AfP, &Bf, DeltaFile);
  }

  //-- Create the cluster information
  flushSyntenys (Syntenys, ClusterFile);
}




  inline long int revC
(long int Coord, long int Len)

//  Reverse complement the given coordinate for the given length.

{
  assert (Len - Coord + 1 > 0);
  return (Len - Coord + 1);
}

// always_assert: similar to assert macro, but not subject to NDEBUG
#define always_assert(x)                                                \
  if(!(x)) {                                                            \
    std::cerr << __FILE__ << ':' << __LINE__                            \
              << ": assertion failed " << #x << std::endl;              \
      abort();                                                          \
  }

void validateData
(std::vector<Alignment> Alignments, std::vector<Cluster> Clusters,
 const FastaRecord * Af, const FastaRecord * Bf)

//  Self test function to check that the cluster and alignment information
//  is valid

{
  char *                           Brev = NULL;
  std::vector<Cluster>::iterator   Cp;
  std::vector<Match>::iterator     Mp;
  std::vector<Alignment>::iterator Ap;
  const char *                     A    = Af->seq(), * B;

  for ( Cp = Clusters.begin( ); Cp < Clusters.end( ); Cp ++ ) {
    always_assert ( Cp->wasFused );

    //-- Pick the right directional sequence for B
    if ( Cp->dirB == FORWARD_CHAR ) {
      B = Bf->seq();
    } else if ( Brev != NULL ) {
      B = Brev;
    } else {
      Brev = (char *) Safe_malloc ( sizeof(char) * (Bf->len() + 2) );
      strcpy ( Brev + 1, Bf->seq() + 1 );
      Brev[0] = '\0';
      Reverse_Complement (Brev, 1, Bf->len());
      B = Brev;
    }

    for ( Mp = Cp->matches.begin( ); Mp < Cp->matches.end( ); ++Mp) {
      //-- always_assert for each match in cluster, it is indeed a match
      long int x = Mp->sA;
      long int y = Mp->sB;
      for (long int i = 0; i < Mp->len; i ++ )
        always_assert ( A[x ++] == B[y ++] );

      //-- always_assert for each match in cluster, it is contained in an alignment
      for ( Ap = Alignments.begin( ); Ap < Alignments.end( ); Ap ++ ) {
        if ( Ap->sA <= Mp->sA  &&  Ap->sB <= Mp->sB  &&
             Ap->eA >= Mp->sA + Mp->len - 1  &&
             Ap->eB >= Mp->sB + Mp->len - 1 )
          break;
      }
      always_assert ( Ap < Alignments.end( ) );
    }
  }

  //-- always_assert alignments are optimal (quick check if first and last chars equal)
  for ( Ap = Alignments.begin( ); Ap < Alignments.end( ); ++Ap) {
    if ( Ap->dirB == REVERSE_CHAR ) {
      always_assert (Brev != NULL);
      B = Brev;
    } else
      B = Bf->seq();
    always_assert ( Ap->sA <= Ap->eA );
    always_assert ( Ap->sB <= Ap->eB );

    always_assert ( Ap->sA >= 1 && Ap->sA <= Af->len() );
    always_assert ( Ap->eA >= 1 && Ap->eA <= Af->len() );
    always_assert ( Ap->sB >= 1 && Ap->sB <= Bf->len() );
    always_assert ( Ap->eB >= 1 && Ap->eB <= Bf->len() );

    char Xc = toupper(isalpha(A[Ap->sA]) ? A[Ap->sA] : STOP_CHAR);
    char Yc = toupper(isalpha(B[Ap->sB]) ? B[Ap->sB] : STOP_CHAR);
    always_assert ( 0 <= MATCH_SCORE [0] [Xc - 'A'] [Yc - 'A'] );

    Xc = toupper(isalpha(A[Ap->eA]) ? A[Ap->eA] : STOP_CHAR);
    Yc = toupper(isalpha(B[Ap->eB]) ? B[Ap->eB] : STOP_CHAR);
    always_assert ( 0 <= MATCH_SCORE [0] [Xc - 'A'] [Yc - 'A'] );
  }

  if ( Brev != NULL )
    free (Brev);
}

} // namespace postnuc
} // namespace mummer
