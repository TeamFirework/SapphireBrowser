// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
#define URL_MOJO_ORIGIN_MOJOM_TRAITS_H_

#include "base/strings/string_piece.h"
#include "url/mojom/origin.mojom.h"
#include "url/origin.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::OriginDataView, url::Origin> {
  static const std::string& scheme(const url::Origin& r) { return r.scheme(); }
  static const std::string& host(const url::Origin& r) { return r.host(); }
  static uint16_t port(const url::Origin& r) { return r.port(); }
  static bool unique(const url::Origin& r) { return r.unique(); }
  static bool Read(url::mojom::OriginDataView data, url::Origin* out) {
    if (data.unique()) {
      *out = url::Origin();
    } else {
      base::StringPiece scheme, host;
      if (!data.ReadScheme(&scheme) || !data.ReadHost(&host))
        return false;

      base::Optional<url::Origin> origin =
          url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
              scheme, host, data.port());
      if (!origin.has_value())
        return false;

      *out = origin.value();
    }

    return true;
  }
};

}  // namespace mojo

#endif  // URL_MOJO_ORIGIN_MOJOM_TRAITS_H_
