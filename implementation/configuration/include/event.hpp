// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CFG_EVENT_HPP
#define VSOMEIP_V3_CFG_EVENT_HPP

#include <memory>
#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {
namespace cfg {

struct eventgroup;

struct event {
    event(event_t _id, bool _is_field, reliability_type_e _reliability, unsigned int _update_cycle)
        : id_(_id),
          is_field_(_is_field),
          reliability_(_reliability),
		  update_cycle_(_update_cycle){
    }

    event_t id_;
    bool is_field_;
    reliability_type_e reliability_;
	std::chrono::milliseconds update_cycle_;
    std::vector<std::weak_ptr<eventgroup> > groups_;
};

} // namespace cfg
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CFG_EVENT_HPP
