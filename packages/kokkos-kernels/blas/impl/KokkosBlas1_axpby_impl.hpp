//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER
#ifndef KOKKOSBLAS1_AXPBY_IMPL_HPP_
#define KOKKOSBLAS1_AXPBY_IMPL_HPP_

#include "KokkosKernels_config.h"
#include "Kokkos_Core.hpp"
#include "Kokkos_InnerProductSpaceTraits.hpp"

#ifndef KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY
#define KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY 2
#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY

namespace KokkosBlas {
namespace Impl {

//
// axpby
//

// Single-vector version of Axpby_MV_Functor.  The definition
// immediately below lets a and b both be 1-D Views (and only requires
// that each have one entry).  Following this is a partial
// specialization that lets both of them be scalars.  This functor
// computes any of the following:
//
// 1. Y(i) = alpha*X(i) + beta*Y(i) for alpha,beta in -1,0,1
// 2. Y(i) = a(0)*X(i) + beta*Y(i) for beta in -1,0,1
// 3. Y(i) = alpha*X(i) + b(0)*Y(i) for alpha in -1,0,1
// 4. Y(i) = a(0)*X(i) + b(0)*Y(i)
//
// The template parameters scalar_x and scalar_y correspond to alpha
// resp. beta in the operation y = alpha*x + beta*y.  The values -1,
// 0, and -1 correspond to literal values of those coefficients.  The
// value 2 tells the functor to use the corresponding vector of
// coefficients.  Any literal coefficient of zero has BLAS semantics
// of ignoring the corresponding (multi)vector entry.  This does not
// apply to coefficients in the a and b vectors, if they are used.
template <class AV, class XV, class BV, class YV, int scalar_x, int scalar_y,
          class SizeType>
struct Axpby_Functor {
  typedef typename YV::execution_space execution_space;
  typedef SizeType size_type;
  typedef Kokkos::Details::ArithTraits<typename YV::non_const_value_type> ATS;

  XV m_x;
  YV m_y;
  AV m_a;
  BV m_b;

  Axpby_Functor(const XV& x, const YV& y, const AV& a, const BV& b,
                const SizeType startingColumn)
      : m_x(x), m_y(y), m_a(a), m_b(b) {
    static_assert(Kokkos::is_view<XV>::value,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: X is not a Kokkos::View.");
    static_assert(Kokkos::is_view<YV>::value,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: Y is not a Kokkos::View.");
    static_assert(std::is_same<typename YV::value_type,
                               typename YV::non_const_value_type>::value,
                  "KokkosBlas::Impl::Axpby_Functor: Y is const.  "
                  "It must be nonconst, because it is an output argument "
                  "(we have to be able to write to its entries).");
    static_assert((int)YV::Rank == (int)XV::Rank,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: X and Y must have the same rank.");
    static_assert(YV::Rank == 1,
                  "KokkosBlas::Impl::Axpby_Functor: "
                  "XV and YV must have rank 1.");

    if (startingColumn != 0) {
      m_a = Kokkos::subview(
          a, std::make_pair(startingColumn, SizeType(a.extent(0))));
      m_b = Kokkos::subview(
          b, std::make_pair(startingColumn, SizeType(b.extent(0))));
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const size_type& i) const {
    // scalar_x and scalar_y are compile-time constants (since they
    // are template parameters), so the compiler should evaluate these
    // branches at compile time.

#if KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY <= 2

    if (scalar_x == 0 && scalar_y == 0) {
      m_y(i) = ATS::zero();
    }
    if (scalar_x == 0 && scalar_y == 2) {
      m_y(i) = m_b(0) * m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 0) {
      m_y(i) = m_a(0) * m_x(i);
    }
    if (scalar_x == 2 && scalar_y == 2) {
      m_y(i) = m_a(0) * m_x(i) + m_b(0) * m_y(i);
    }

#else  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2

    if (scalar_x == 0 && scalar_y == 0) {
      m_y(i) = ATS::zero();
    }
    if (scalar_x == 0 && scalar_y == -1) {
      m_y(i) = -m_y(i);
    }
    if (scalar_x == 0 && scalar_y == 1) {
      return;  // m_y(i) = m_y(i);
    }
    if (scalar_x == 0 && scalar_y == 2) {
      m_y(i) = m_b(0) * m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 0) {
      m_y(i) = -m_x(i);
    }
    if (scalar_x == -1 && scalar_y == -1) {
      m_y(i) = -m_x(i) - m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 1) {
      m_y(i) = -m_x(i) + m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 2) {
      m_y(i) = -m_x(i) + m_b(0) * m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 0) {
      m_y(i) = m_x(i);
    }
    if (scalar_x == 1 && scalar_y == -1) {
      m_y(i) = m_x(i) - m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 1) {
      m_y(i) = m_x(i) + m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 2) {
      m_y(i) = m_x(i) + m_b(0) * m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 0) {
      m_y(i) = m_a(0) * m_x(i);
    }
    if (scalar_x == 2 && scalar_y == -1) {
      m_y(i) = m_a(0) * m_x(i) - m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 1) {
      m_y(i) = m_a(0) * m_x(i) + m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 2) {
      m_y(i) = m_a(0) * m_x(i) + m_b(0) * m_y(i);
    }

#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY
  }
};

// Partial specialization of Axpby_Functor that lets a and b be
// scalars (rather than 1-D Views, as in the most general version
// above).  This functor computes any of the following:
//
// 1. Y(i) = alpha*X(i) + beta*Y(i) for alpha,beta in -1,0,1
// 2. Y(i) = a*X(i) + beta*Y(i) for beta in -1,0,1
// 3. Y(i) = alpha*X(i) + b*Y(i) for alpha in -1,0,1
// 4. Y(i) = a*X(i) + b*Y(i)
//
// The template parameters scalar_x and scalar_y correspond to alpha
// resp. beta in the operation y = alpha*x + beta*y.  The values -1,
// 0, and -1 correspond to literal values of those coefficients.  The
// value 2 tells the functor to use the corresponding vector of
// coefficients.  Any literal coefficient of zero has BLAS semantics
// of ignoring the corresponding (multi)vector entry.  This does not
// apply to coefficients in the a and b vectors, if they are used.
template <class XV, class YV, int scalar_x, int scalar_y, class SizeType>
struct Axpby_Functor<typename XV::non_const_value_type, XV,
                     typename YV::non_const_value_type, YV, scalar_x, scalar_y,
                     SizeType> {
  typedef typename YV::execution_space execution_space;
  typedef SizeType size_type;
  typedef Kokkos::Details::ArithTraits<typename YV::non_const_value_type> ATS;

  XV m_x;
  YV m_y;
  const typename XV::non_const_value_type m_a;
  const typename YV::non_const_value_type m_b;

  Axpby_Functor(const XV& x, const YV& y,
                const typename XV::non_const_value_type& a,
                const typename YV::non_const_value_type& b,
                const SizeType /* startingColumn */)
      : m_x(x), m_y(y), m_a(a), m_b(b) {
    static_assert(Kokkos::is_view<XV>::value,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: X is not a Kokkos::View.");
    static_assert(Kokkos::is_view<YV>::value,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: Y is not a Kokkos::View.");
    static_assert(std::is_same<typename YV::value_type,
                               typename YV::non_const_value_type>::value,
                  "KokkosBlas::Impl::Axpby_Functor: R is const.  "
                  "It must be nonconst, because it is an output argument "
                  "(we have to be able to write to its entries).");
    static_assert((int)YV::Rank == (int)XV::Rank,
                  "KokkosBlas::Impl::"
                  "Axpby_Functor: X and Y must have the same rank.");
    static_assert(YV::Rank == 1,
                  "KokkosBlas::Impl::Axpby_Functor: "
                  "XV and YV must have rank 1.");
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const size_type& i) const {
    // scalar_x and scalar_y are compile-time constants (since they
    // are template parameters), so the compiler should evaluate these
    // branches at compile time.

#if KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY <= 2

    if (scalar_x == 0 && scalar_y == 0) {
      m_y(i) = static_cast<typename YV::non_const_value_type>(ATS::zero());
    }
    if (scalar_x == 0 && scalar_y == 2) {
      m_y(i) = static_cast<typename YV::non_const_value_type>(m_b * m_y(i));
    }
    if (scalar_x == 2 && scalar_y == 0) {
      m_y(i) = static_cast<typename YV::non_const_value_type>(m_a * m_x(i));
    }
    if (scalar_x == 2 && scalar_y == 2) {
      m_y(i) = static_cast<typename YV::non_const_value_type>(m_a * m_x(i) +
                                                              m_b * m_y(i));
    }

#else  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2

    if (scalar_x == 0 && scalar_y == 0) {
      m_y(i) = ATS::zero();
    }
    if (scalar_x == 0 && scalar_y == -1) {
      m_y(i) = -m_y(i);
    }
    if (scalar_x == 0 && scalar_y == 1) {
      return;  // m_y(i) = m_y(i);
    }
    if (scalar_x == 0 && scalar_y == 2) {
      m_y(i) = m_b * m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 0) {
      m_y(i) = -m_x(i);
    }
    if (scalar_x == -1 && scalar_y == -1) {
      m_y(i) = -m_x(i) - m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 1) {
      m_y(i) = -m_x(i) + m_y(i);
    }
    if (scalar_x == -1 && scalar_y == 2) {
      m_y(i) = -m_x(i) + m_b * m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 0) {
      m_y(i) = m_x(i);
    }
    if (scalar_x == 1 && scalar_y == -1) {
      m_y(i) = m_x(i) - m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 1) {
      m_y(i) = m_x(i) + m_y(i);
    }
    if (scalar_x == 1 && scalar_y == 2) {
      m_y(i) = m_x(i) + m_b * m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 0) {
      m_y(i) = m_a * m_x(i);
    }
    if (scalar_x == 2 && scalar_y == -1) {
      m_y(i) = m_a * m_x(i) - m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 1) {
      m_y(i) = m_a * m_x(i) + m_y(i);
    }
    if (scalar_x == 2 && scalar_y == 2) {
      m_y(i) = m_a * m_x(i) + m_b * m_y(i);
    }

#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY
  }
};

// Variant of Axpby_MV_Generic for single vectors (1-D Views) x and y.
// As above, either av and bv are both 1-D Views (and only the first
// entry of each will be read), or both av and bv are scalars.
//
// This takes the starting column, so that if av and bv are both 1-D
// Views, then the functor can take a subview if appropriate.
template <class AV, class XV, class BV, class YV, class SizeType>
void Axpby_Generic(const AV& av, const XV& x, const BV& bv, const YV& y,
                   const SizeType startingColumn, int a = 2, int b = 2) {
  static_assert(Kokkos::is_view<XV>::value,
                "KokkosBlas::Impl::"
                "Axpby_Generic: X is not a Kokkos::View.");
  static_assert(Kokkos::is_view<YV>::value,
                "KokkosBlas::Impl::"
                "Axpby_Generic: Y is not a Kokkos::View.");
  static_assert(std::is_same<typename YV::value_type,
                             typename YV::non_const_value_type>::value,
                "KokkosBlas::Impl::Axpby_Generic: Y is const.  "
                "It must be nonconst, because it is an output argument "
                "(we have to be able to write to its entries).");
  static_assert((int)YV::Rank == (int)XV::Rank,
                "KokkosBlas::Impl::"
                "Axpby_Generic: X and Y must have the same rank.");
  static_assert(YV::Rank == 1,
                "KokkosBlas::Impl::Axpby_Generic: "
                "XV and YV must have rank 1.");

  typedef typename YV::execution_space execution_space;
  const SizeType numRows = x.extent(0);
  Kokkos::RangePolicy<execution_space, SizeType> policy(0, numRows);

  if (a == 0 && b == 0) {
    Axpby_Functor<AV, XV, BV, YV, 0, 0, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S0", policy, op);
    return;
  }

#if KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2
  if (a == 0 && b == -1) {
    Axpby_Functor<AV, XV, BV, YV, 0, -1, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S1", policy, op);
    return;
  }
  if (a == 0 && b == 1) {
    Axpby_Functor<AV, XV, BV, YV, 0, 1, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S2", policy, op);
    return;
  }
#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2

  if (a == 0 && b == 2) {
    Axpby_Functor<AV, XV, BV, YV, 0, 2, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S3", policy, op);
    return;
  }

#if KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2
  // a == -1
  if (a == -1 && b == 0) {
    Axpby_Functor<AV, XV, BV, YV, -1, 0, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S4", policy, op);
    return;
  }
  if (a == -1 && b == -1) {
    Axpby_Functor<AV, XV, BV, YV, -1, -1, SizeType> op(x, y, av, bv,
                                                       startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S5", policy, op);
    return;
  }
  if (a == -1 && b == 1) {
    Axpby_Functor<AV, XV, BV, YV, -1, 1, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S6", policy, op);
    return;
  }
  if (a == -1 && b == 2) {
    Axpby_Functor<AV, XV, BV, YV, -1, 2, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S7", policy, op);
    return;
  }
  // a == 1
  if (a == 1 && b == 0) {
    Axpby_Functor<AV, XV, BV, YV, 1, 0, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S8", policy, op);
    return;
  }
  if (a == 1 && b == -1) {
    Axpby_Functor<AV, XV, BV, YV, 1, -1, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S9", policy, op);
    return;
  }
  if (a == 1 && b == 1) {
    Axpby_Functor<AV, XV, BV, YV, 1, 1, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S10", policy, op);
    return;
  }
  if (a == 1 && b == 2) {
    Axpby_Functor<AV, XV, BV, YV, 1, 2, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S11", policy, op);
    return;
  }
#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2

  // a == 2
  if (a == 2 && b == 0) {
    Axpby_Functor<AV, XV, BV, YV, 2, 0, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S12", policy, op);
    return;
  }

#if KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2
  if (a == 2 && b == -1) {
    Axpby_Functor<AV, XV, BV, YV, 2, -1, SizeType> op(x, y, av, bv,
                                                      startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S13", policy, op);
    return;
  }
  if (a == 2 && b == 1) {
    Axpby_Functor<AV, XV, BV, YV, 2, 1, SizeType> op(x, y, av, bv,
                                                     startingColumn);
    Kokkos::parallel_for("KokkosBlas::Axpby::S14", policy, op);
    return;
  }
#endif  // KOKKOSBLAS_OPTIMIZATION_LEVEL_AXPBY > 2

  // a and b arbitrary (not -1, 0, or 1)
  Axpby_Functor<AV, XV, BV, YV, 2, 2, SizeType> op(x, y, av, bv,
                                                   startingColumn);
  Kokkos::parallel_for("KokkosBlas::Axpby::S15", policy, op);
}

}  // namespace Impl
}  // namespace KokkosBlas
#endif  // KOKKOSBLAS1_AXPBY_IMPL_HPP_
