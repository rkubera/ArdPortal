// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <app/clusters/fixed-label-server/fixed-label-cluster.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <cstring>

// Fixed Label values are limited to 16 UTF-8 bytes. Keep one bounded copy per
// endpoint; both the generic name and HA's DIY-device alias share that copy.
class ArdMatterLabel : public chip::app::Clusters::FixedLabelCluster {
public:
  chip::app::ServerClusterRegistration registration;
  char text[17]{};
  /** @brief Copy a non-null UTF-8 name into the endpoint label, truncating at 16 bytes on a character boundary. */
  ArdMatterLabel(uint16_t endpoint, const char* name)
    : FixedLabelCluster(endpoint), registration(*this) {
    size_t length = std::strlen(name);
    if (length > 16) {
      length = 16;
      while (length && (uint8_t(name[length]) & 0xC0) == 0x80) --length;
    }
    std::memcpy(text, name, length);
  }
  /** @brief Encode shared name/HA labels for LabelList; delegate other attribute reads to the SDK cluster. */
  chip::app::DataModel::ActionReturnStatus ReadAttribute(const chip::app::DataModel::ReadAttributeRequest& request, chip::app::AttributeValueEncoder& encoder) override {
    using namespace chip::app::Clusters;
    if (request.path.mAttributeId != FixedLabel::Attributes::LabelList::Id) return FixedLabelCluster::ReadAttribute(request, encoder);
    return encoder.EncodeList([this](const auto& list) -> CHIP_ERROR {
      FixedLabel::Structs::LabelStruct::Type label;
      label.label = chip::CharSpan::fromCharString("name");
      label.value = chip::CharSpan::fromCharString(text);
      ReturnErrorOnFailure(list.Encode(label));
      // HA recognizes this label for its documented test/DIY vendor IDs.
      label.label = chip::CharSpan::fromCharString("ha_entitylabel");
      return list.Encode(label);
    });
  }
};
