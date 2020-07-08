// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EXAMPLES_SAMPLE_IDS_HPP
#define VSOMEIP_EXAMPLES_SAMPLE_IDS_HPP

#define SAMPLE_SERVICE_APP_ID   0x1277
#define SAMPLE_CLIENT_APP_ID    0x1344


#define SAMPLE_SERVICE_ID       0x1234
#define SAMPLE_INSTANCE_ID      0x5678
#define SAMPLE_METHOD_ID        0x0421

#define SAMPLE_EVENT_ID         0x0777
#define SAMPLE_GET_METHOD_ID    0x0001
#define SAMPLE_SET_METHOD_ID    0x0002

#define SAMPLE_EVENTGROUP_ID    0x4465

#define OTHER_SAMPLE_SERVICE_ID 0x0248
#define OTHER_SAMPLE_INSTANCE_ID 0x5422
#define OTHER_SAMPLE_METHOD_ID  0x1421

static const vsomeip::byte_t set_data[]
= { 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x50, 0x51, 0x52 };

static const vsomeip::byte_t payload_data[]
= {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10};

#endif // VSOMEIP_EXAMPLES_SAMPLE_IDS_HPP
