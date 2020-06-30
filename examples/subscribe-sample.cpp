// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/enumeration_types.hpp>
//#include <vsomeip/primitive_types.hpp>
//#ifdef VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS
//#include "implementation/configuration/include/configuration_impl.hpp"
//#else
#include "configuration/include/configuration.hpp"
#include "configuration/include/configuration_plugin.hpp"
//#endif // VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS

#include <configuration/include/service.hpp>
#include <configuration/include/eventgroup.hpp>
#include <configuration/include/event.hpp>

#include "sample-ids.hpp"

class client_sample {
public:
    client_sample(bool _use_tcp, uint32_t _do_methods) :
            app_(vsomeip::runtime::get()->create_application()), use_tcp_(
				_use_tcp), do_methods_(_do_methods) {
    }

	void subscribe_one(vsomeip::service_t service, vsomeip::instance_t instance)
		{
			auto i = service_map.find(std::make_pair(service, instance));
			if (i!=service_map.end())
			{
				for (auto j : i->second->eventgroups_)
				{
					std::set<vsomeip::eventgroup_t> its_groups;
					its_groups.insert(j.first);
					for (auto k : j.second->events_)
					{
						/* std::cout << "Will request event "<< std::hex << "(" << j.first << ")" << */
						/* 	i.first<<":"<< */
						/* 	i.second<<":"<<  */
						/* 	k->id_<<":"<<std::endl; */

						app_->request_event(
							service, instance,
							k->id_,
							its_groups,
							(k->is_field_) ?
							vsomeip_v3::event_type_e::ET_FIELD : vsomeip_v3::event_type_e::ET_EVENT);
					}
					app_->subscribe(service, instance, j.first);
				}
			}
		}
	
	void subscribe(bool first_time)
		{
//			static bool first_time = true;
			
			for (auto i : service_map) {
				if (first_time)
				{
					VSOMEIP_INFO << "Registering message handler for instance "<<std::hex <<i.first.second;

				app_->register_message_handler(
					vsomeip::ANY_SERVICE, //i.first,
					i.first.second, 
					vsomeip::ANY_METHOD, //SAMPLE_GET_METHOD_ID,
					std::bind(&client_sample::on_message, this,
							  std::placeholders::_1));
				}
				if (first_time)
					app_->register_availability_handler(i.first.first, i.first.second,
														std::bind(&client_sample::on_availability,
																  this,
																  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

				subscribe_one(i.first.first, i.first.second);
			}
			first_time = false;
		}
	void unsubscribe_one(vsomeip::service_t service, vsomeip::instance_t instance)
		{
			auto i = service_map.find(std::make_pair(service, instance));
			if (i!=service_map.end())
			{
				for (auto j : i->second->events_)
				{
					app_->release_event(service, instance, j.first);
				}
				for (auto j : i->second->eventgroups_)
				{
					app_->unsubscribe(service, instance, j.first);
				}
			}
		}
	void unsubscribe()
		{			
			for (auto i : service_map) {
				unsubscribe_one(i.first.first, i.first.second);
			}
		}
    bool init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
		configuration_ = app_->get_configuration();
		std::cout << "Got configuration "<<std::endl;

		for (auto i : configuration_->get_local_services()) {
			auto service = configuration_->find_service(i.first, i.second);
			service_map.insert(std::make_pair(i, service));
			std::cout << "Got service "<<std::hex<<i.first<<":"<<i.second<<std::endl;
		}
        std::cout << "Client settings [protocol="
				  << (use_tcp_ ? "TCP" : "UDP")
				  << (do_methods_ ? ", set/get once in " : ", use of methods: ")
				  << (do_methods_ ? do_methods_ : 0 )
				  << "]"
                << std::endl;

        app_->register_state_handler(
                std::bind(&client_sample::on_state, this,
                        std::placeholders::_1));
		subscribe(true);
        return true;
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    /*
     * Handle signal to shutdown
     */
    void stop() {
        app_->clear_all_handler();
        app_->unsubscribe(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID);
        app_->release_event(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENT_ID);
        app_->release_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->stop();
    }
#endif

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.") << std::endl;
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
			for (auto i : service_map)
			{
				std::cout << "Will request service " <<
					std::hex << i.first.first<<":"<< i.first.second << std::endl;
				app_->request_service(i.first.first, i.first.second);
			}
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available)
		{
			//	static bool last_a = false;
			std::cout << "Service ["
					  << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
					  << "] is "
					  << (_is_available ? "available." : "NOT available.")
					  << std::endl;
			if (_is_available)
				subscribe_one(_service, _instance);
			else 
				unsubscribe_one(_service, _instance);
			//last_a = _is_available;
		}

    void on_message(const std::shared_ptr<vsomeip::message> &_response) {
        std::stringstream its_message;
        its_message << "Received a notification for Event ["
                << std::setw(4)    << std::setfill('0') << std::hex
                << _response->get_service() << "."
                << std::setw(4) << std::setfill('0') << std::hex
                << _response->get_instance() << "."
                << std::setw(4) << std::setfill('0') << std::hex
                << _response->get_method() << "] to Client/Session ["
                << std::setw(4) << std::setfill('0') << std::hex
                << _response->get_client() << "/"
                << std::setw(4) << std::setfill('0') << std::hex
                << _response->get_session()
                << "] = ";
        std::shared_ptr<vsomeip::payload> its_payload =
                _response->get_payload();
        its_message << "(" << std::dec << its_payload->get_length() << ") ";
        for (uint32_t i = 0; i < its_payload->get_length()%10; ++i)
            its_message << std::hex << std::setw(2) << std::setfill('0')
                << (int) its_payload->get_data()[i] << " ";
        VSOMEIP_DEBUG << its_message.str();

        if (do_methods_ && _response->get_client() == 0) {
            if (((*(uint32_t*) its_payload->get_data()) % do_methods_) == 0) {
                std::shared_ptr<vsomeip::message> its_get
                    = vsomeip::runtime::get()->create_request();
                its_get->set_service(SAMPLE_SERVICE_ID);
                its_get->set_instance(SAMPLE_INSTANCE_ID);
                its_get->set_method(SAMPLE_GET_METHOD_ID);
                its_get->set_reliable(use_tcp_);
				VSOMEIP_DEBUG << "Will send get" << std::endl;
                app_->send(its_get);
            }

			if (((*(uint32_t*) its_payload->get_data()) % (do_methods_+1)) == 0) {
                std::shared_ptr<vsomeip::message> its_set
                    = vsomeip::runtime::get()->create_request();
                its_set->set_service(SAMPLE_SERVICE_ID);
                its_set->set_instance(SAMPLE_INSTANCE_ID);
                its_set->set_method(SAMPLE_SET_METHOD_ID);
                its_set->set_reliable(use_tcp_);

                const vsomeip::byte_t set_data[]
                    = { 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
                        0x48, 0x49, 0x50, 0x51, 0x52 };
                std::shared_ptr<vsomeip::payload> its_set_payload
                    = vsomeip::runtime::get()->create_payload();
                its_set_payload->set_data(set_data, sizeof(set_data));
                its_set->set_payload(its_set_payload);
				VSOMEIP_DEBUG << "Will send set";
                app_->send(its_set);
            }
        }
    }

private:
    std::shared_ptr< vsomeip::application > app_;
    std::shared_ptr<vsomeip_v3::configuration> configuration_;
	std::map<std::pair<vsomeip_v3::service_t, vsomeip_v3::instance_t>, std::shared_ptr<vsomeip_v3::cfg::service>> service_map;

    bool use_tcp_;
    uint32_t do_methods_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    client_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_tcp = false;
	uint32_t do_methods = 0;
    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string methods_once_in("--methods");

    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (udp_enable == argv[i]) {
            use_tcp = false;
        }
		else if (methods_once_in == argv[i]) {
			std::stringstream converter;
			converter << argv[++i];
			converter >> do_methods;
		}
		else
			std::cerr << "Unknown arg ignored! " << argv[i] << std::endl;
		i++;
    }

client_sample its_sample(use_tcp, do_methods);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_sample.init()) {
        its_sample.start();
        return 0;
    } else {
        return 1;
    }
}
