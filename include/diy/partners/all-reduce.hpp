#ifndef DIY_PARTNERS_ALL_REDUCE_HPP
#define DIY_PARTNERS_ALL_REDUCE_HPP

#include "merge.hpp"

namespace diy
{

//! Allreduce (reduction with results broadcasted to all blocks) is
//! implemented as two merge reductions, with incoming and outgoing items swapped in second one.
//! Ie, follows merge reduction up and down the merge tree
struct RegularAllReducePartners: public RegularMergePartners
{
  typedef       RegularMergePartners                            Parent; //!< base class merge reduction

                //! contiguous parameter indicates whether to match partners contiguously or in a round-robin fashion;
                //! contiguous is useful when data needs to be united;
                //! round-robin is useful for vector-"halving"
                RegularAllReducePartners(int dim, //!< dimensionality of regular block grid
                                         int nblocks, //!< total number of blocks
                                         int k, //!< target k value
                                         bool contiguous = true):
                  Parent(dim, nblocks, k, contiguous)         {}
                RegularAllReducePartners(const DivisionVector&   divs, //!< explicit division vector
                                         const KVSVector&        kvs, //!< explicit k vector
                                         bool  contiguous = true):
                  Parent(divs, kvs, contiguous)               {}

  //! returns total number of rounds
  size_t        rounds() const                                  { return 2*Parent::rounds(); }
  //! returns size of a group of partners in a given round
  int           size(int round) const                           { return Parent::size(parent_round(round)); }
  //! returns dimension (direction of partners in a regular grid) in a given round
  int           dim(int round) const                            { return Parent::dim(parent_round(round)); }
  //! returns whether a given block in a given round has dropped out of the merge yet or not
  inline bool   active(int round, int gid) const                { return Parent::active(parent_round(round), gid); }
  //! returns what the current round would be in the first or second parent merge reduction
  int           parent_round(int round) const                   { return round < Parent::rounds() ? round : rounds() - round; }

  // incoming is only valid for an active gid; it will only be called with an active gid
  inline void   incoming(int round, int gid, std::vector<int>& partners) const
  {
      if (round <= Parent::rounds())
          Parent::incoming(round, gid, partners);
      else
          Parent::outgoing(parent_round(round), gid, partners);
  }

  inline void   outgoing(int round, int gid, std::vector<int>& partners) const
  {
      if (round < Parent::rounds())
          Parent::outgoing(round, gid, partners);
      else
          Parent::incoming(parent_round(round), gid, partners);
  }
};

} // diy

#endif


