// SPDX-License-Identifier: MIT OR GPL-2.0-or-later
#include "bmo_face_sdf.h"

namespace BmoFaceSdf {

const ExprTarget* targetForIndex(size_t index) {
  constexpr size_t kExprCount = sizeof(EXPR_TABLE) / sizeof(EXPR_TABLE[0]);
  return (index < kExprCount) ? &EXPR_TABLE[index] : nullptr;
}

} // namespace BmoFaceSdf
