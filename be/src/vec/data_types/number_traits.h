// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <type_traits>

#include "vec/common/uint128.h"
#include "vec/core/types.h"

namespace doris::vectorized {

/** Allows get the result type of the functions +, -, *, /, %, intDiv (integer division).
  * The rules are different from those used in C++.
  */

namespace NumberTraits {

struct Error {};

constexpr size_t max(size_t x, size_t y) {
    return x > y ? x : y;
}

constexpr size_t min(size_t x, size_t y) {
    return x < y ? x : y;
}

constexpr size_t next_size(size_t size) {
    return min(size * 2, 8);
}

template <bool is_signed, bool is_floating, size_t size>
struct Construct {
    using Type = Error;
};

template <>
struct Construct<false, false, 1> {
    using Type = Int16;
};
template <>
struct Construct<false, false, 2> {
    using Type = Int32;
};
template <>
struct Construct<false, false, 4> {
    using Type = Int64;
};
template <>
struct Construct<false, false, 8> {
    using Type = UInt64;
};
template <>
struct Construct<false, false, 16> {
    using Type = Int128;
};
template <>
struct Construct<false, true, 1> {
    using Type = Float32;
};
template <>
struct Construct<false, true, 2> {
    using Type = Float32;
};
template <>
struct Construct<false, true, 4> {
    using Type = Float32;
};
template <>
struct Construct<false, true, 8> {
    using Type = Float64;
};
template <>
struct Construct<true, false, 1> {
    using Type = Int8;
};
template <>
struct Construct<true, false, 2> {
    using Type = Int16;
};
template <>
struct Construct<true, false, 4> {
    using Type = Int32;
};
template <>
struct Construct<true, false, 8> {
    using Type = Int64;
};
template <>
struct Construct<true, false, 16> {
    using Type = Int128;
};
template <>
struct Construct<true, true, 1> {
    using Type = Float32;
};
template <>
struct Construct<true, true, 2> {
    using Type = Float32;
};
template <>
struct Construct<true, true, 4> {
    using Type = Float32;
};
template <>
struct Construct<true, true, 8> {
    using Type = Float64;
};

/** The result of addition or multiplication is calculated according to the following rules:
    * - if one of the arguments is floating-point, the result is a floating point, otherwise - the whole;
    * - if one of the arguments is signed, the result is signed, otherwise it is unsigned;
    * - the result contains more bits (not only meaningful) than the maximum in the arguments
    *   (for example, UInt8 + Int32 = Int64).
    */
template <typename A, typename B>
struct ResultOfAdditionMultiplication {
    using Type = typename Construct<std::is_signed_v<A> || std::is_signed_v<B>,
                                    std::is_floating_point_v<A> || std::is_floating_point_v<B>,
                                    next_size(max(sizeof(A), sizeof(B)))>::Type;
};

template <typename A, typename B>
struct ResultOfSubtraction {
    using Type =
            typename Construct<true, std::is_floating_point_v<A> || std::is_floating_point_v<B>,
                               next_size(max(sizeof(A), sizeof(B)))>::Type;
};

/** When dividing, you always get a floating-point number.
    */
template <typename A, typename B>
struct ResultOfFloatingPointDivision {
    using Type = Float64;
};

/** For integer division, we get a number with the same number of bits as in divisible.
    */
template <typename A, typename B>
struct ResultOfIntegerDivision {
    using Type =
            typename Construct<std::is_signed_v<A> || std::is_signed_v<B>, false, sizeof(A)>::Type;
};

/** Division with remainder you get a number with the same number of bits as in divisor.
    */
template <typename A, typename B>
struct ResultOfModulo {
    using Type =
            typename Construct<std::is_signed_v<A> || std::is_signed_v<B>, false, sizeof(B)>::Type;
};

template <typename A>
struct ResultOfNegate {
    using Type = typename Construct<true, std::is_floating_point_v<A>,
                                    std::is_signed_v<A> ? sizeof(A) : next_size(sizeof(A))>::Type;
};

template <typename A>
struct ResultOfAbs {
    using Type = typename Construct<false, std::is_floating_point_v<A>, sizeof(A)>::Type;
};

/** For bitwise operations, an integer is obtained with number of bits is equal to the maximum of the arguments.
    */
template <typename A, typename B>
struct ResultOfBit {
    using Type = typename Construct<std::is_signed_v<A> || std::is_signed_v<B>, false,
                                    std::is_floating_point_v<A> || std::is_floating_point_v<B>
                                            ? 8
                                            : max(sizeof(A), sizeof(B))>::Type;
};

template <typename A>
struct ResultOfBitNot {
    using Type = typename Construct<std::is_signed_v<A>, false, sizeof(A)>::Type;
};

/** Type casting for `if` function:
  * UInt<x>,  UInt<y>   ->  UInt<max(x,y)>
  * Int<x>,   Int<y>    ->   Int<max(x,y)>
  * Float<x>, Float<y>  -> Float<max(x, y)>
  * UInt<x>,  Int<y>    ->   Int<max(x*2, y)>
  * Float<x>, [U]Int<y> -> Float<max(x, y*2)>
  * Decimal<x>, Decimal<y> -> Decimal<max(x,y)>
  * UUID, UUID          -> UUID
  * UInt64 ,  Int<x>    -> Error
  * Float<x>, [U]Int64  -> Error
  */
template <typename A, typename B>
struct ResultOfIf {
    static constexpr bool has_float = std::is_floating_point_v<A> || std::is_floating_point_v<B>;
    static constexpr bool has_integer = std::is_integral_v<A> || std::is_integral_v<B>;
    static constexpr bool has_signed = std::is_signed_v<A> || std::is_signed_v<B>;
    static constexpr bool has_unsigned = !std::is_signed_v<A> || !std::is_signed_v<B>;

    static constexpr size_t max_size_of_unsigned_integer =
            max(std::is_signed_v<A> ? 0 : sizeof(A), std::is_signed_v<B> ? 0 : sizeof(B));
    static constexpr size_t max_size_of_signed_integer =
            max(std::is_signed_v<A> ? sizeof(A) : 0, std::is_signed_v<B> ? sizeof(B) : 0);
    static constexpr size_t max_size_of_integer =
            max(std::is_integral_v<A> ? sizeof(A) : 0, std::is_integral_v<B> ? sizeof(B) : 0);
    static constexpr size_t max_size_of_float = max(std::is_floating_point_v<A> ? sizeof(A) : 0,
                                                    std::is_floating_point_v<B> ? sizeof(B) : 0);

    using ConstructedType =
            typename Construct<has_signed, has_float,
                               ((has_float && has_integer &&
                                 max_size_of_integer >= max_size_of_float) ||
                                (has_signed && has_unsigned &&
                                 max_size_of_unsigned_integer >= max_size_of_signed_integer))
                                       ? max(sizeof(A), sizeof(B)) * 2
                                       : max(sizeof(A), sizeof(B))>::Type;

    using ConstructedWithUUID =
            std::conditional_t<std::is_same_v<A, UInt128> && std::is_same_v<B, UInt128>, A,
                               ConstructedType>;

    using Type = std::conditional_t<
            !IsDecimalNumber<A> && !IsDecimalNumber<B>, ConstructedWithUUID,
            std::conditional_t<IsDecimalNumber<A> && IsDecimalNumber<B>,
                               std::conditional_t<(sizeof(A) > sizeof(B)), A, B>, Error>>;
};

/** Before applying operator `%` and bitwise operations, operands are casted to whole numbers. */
template <typename A>
struct ToInteger {
    using Type = typename Construct<std::is_signed_v<A>, false,
                                    std::is_floating_point_v<A> ? 8 : sizeof(A)>::Type;
};

// CLICKHOUSE-29. The same depth, different signs
// NOTE: This case is applied for 64-bit integers only (for backward compatibility), but could be used for any-bit integers
template <typename A, typename B>
constexpr bool LeastGreatestSpecialCase = std::is_integral_v<A>&& std::is_integral_v<B> &&
                                          (8 == sizeof(A) && sizeof(A) == sizeof(B)) &&
                                          (std::is_signed_v<A> ^ std::is_signed_v<B>);

template <typename A, typename B>
using ResultOfLeast = std::conditional_t<LeastGreatestSpecialCase<A, B>,
                                         typename Construct<true, false, sizeof(A)>::Type,
                                         typename ResultOfIf<A, B>::Type>;

template <typename A, typename B>
using ResultOfGreatest = std::conditional_t<LeastGreatestSpecialCase<A, B>,
                                            typename Construct<false, false, sizeof(A)>::Type,
                                            typename ResultOfIf<A, B>::Type>;

} // namespace NumberTraits

} // namespace doris::vectorized
