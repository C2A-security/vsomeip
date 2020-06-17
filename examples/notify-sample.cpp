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
#include <mutex>

#include <vsomeip/vsomeip.hpp>
#include "implementation/configuration/include/configuration.hpp"
/* #ifdef VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS */
/* #include "implementation/configuration/include/configuration_impl.hpp" */
/* #else */
/* #include "implementation/configuration/include/configuration.hpp" */
/* #include "implementation/configuration/include/configuration_plugin.hpp" */
/* #endif // VSOMEIP_ENABLE_MULTIPLE_ROUTING_MANAGERS */
//#include <implementation/configuration/include/service.hpp>
#include <implementation/configuration/include/eventgroup.hpp>
#include <implementation/configuration/include/event.hpp>
#include <implementation/configuration/include/service.hpp>
#include "sample-ids.hpp"

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
    vsomeip::method_t shutdown_method_id;
    vsomeip::method_t notify_method_id;
};


class service_sample {
public:
    service_sample(bool _use_tcp, uint32_t _cycle) :
            app_(vsomeip::runtime::get()->create_application()),
            is_registered_(false),
            use_tcp_(_use_tcp),
            cycle_(_cycle),
            blocked_(false),
            running_(true),
            is_offered_(false),
            offer_thread_(std::bind(&service_sample::run, this)),
            notify_thread_(std::bind(&service_sample::notify, this)) {
    }

    bool init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
		configuration_ = app_->get_configuration();
		std::cout << "Got configuration "<<std::endl;
		for (auto i : configuration_->get_local_services()) {

			app_->register_message_handler(
                i.first,
                i.second, 
                SAMPLE_SET_METHOD_ID,
                std::bind(&service_sample::on_set, this,
                          std::placeholders::_1));
			
			app_->register_message_handler(
                i.first,
                i.second, 
                SAMPLE_GET_METHOD_ID,
                std::bind(&service_sample::on_get, this,
                          std::placeholders::_1));

			auto service = configuration_->find_service(i.first, i.second);
			service_map.insert(std::make_pair(i, service));
			std::cout << "Got service "<<std::hex<<i.first<<":"<<i.second<<std::endl;
			for (auto j : service->eventgroups_)
			{
				if (!j.first)
					continue;
				std::set<vsomeip::eventgroup_t> its_groups;
				its_groups.insert(j.first);
				for (auto k : j.second->events_)
				{
					std::cout << "Will offer event "<<std::hex<<
						i.first<<":"<<
						i.second<<":"<< 
						k->id_<<":"<<std::endl;
					
					app_->offer_event(
						i.first,
						i.second, 
						k->id_,
						its_groups,
						vsomeip::event_type_e::ET_FIELD,
						std::chrono::milliseconds::zero(), // cycle
						false, // change resets cycle
						true, // update on change
						nullptr, // epsilon change func
						k->reliability_);
					/* { */
					/* 	std::lock_guard<std::mutex> its_lock(payload_mutex_); */
					/* 	payload_ = vsomeip::runtime::get()->create_payload(); */
					/* } */
				}
			}
		}
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            payload_ = vsomeip::runtime::get()->create_payload();
        }

        blocked_ = true;
        condition_.notify_one();
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
        running_ = false;
        blocked_ = true;
        condition_.notify_one();
        notify_condition_.notify_one();
        app_->clear_all_handler();
        stop_offer();
        offer_thread_.join();
        notify_thread_.join();
        app_->stop();
    }
#endif

    void offer() {
        std::lock_guard<std::mutex> its_lock(notify_mutex_);
		for (auto i : service_map)
			app_->offer_service(i.first.first, i.first.second);
        is_offered_ = true;
        notify_condition_.notify_one();
    }

    void stop_offer() {
		for (auto i : service_map)		
				 app_->stop_offer_service(i.first.first, i.first.second);
        is_offered_ = false;
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.") << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_get(const std::shared_ptr<vsomeip::message> &_message) {
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            its_response->set_payload(payload_);
        }
		std::cout << "On get will respond for "<< std::hex << 
			_message->get_service() <<":"<< _message->get_instance() << std::endl;
        app_->send(its_response);
    }

    void on_set(const std::shared_ptr<vsomeip::message> &_message) {
        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_message);
        {
            std::lock_guard<std::mutex> its_lock(payload_mutex_);
            payload_ = _message->get_payload();
            its_response->set_payload(payload_);
        }

        app_->send(its_response);
		uint16_t event_ = SAMPLE_EVENT_ID;

		auto service_ = service_map.at(std::make_pair(_message->get_service(), _message->get_instance()));
		if (service_)
		{
			event_ = service_->eventgroups_.begin()->second->events_.begin()->get()->id_ ; // ilya come here
		}
		std::cout << "On set will notify event id "<< std::hex << event_ << std::endl;

        app_->notify(_message->get_service(), _message->get_instance(),
                     event_, payload_);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);
        while (running_) {
            if (is_offer)
            {
				offer();
				for (int i = 0; i < 10 && running_; i++)
					std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			}
            else
			{
                stop_offer();
				for (int i = 0; i < 10 && running_; i++)
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

            is_offer = !is_offer;
        }
    }

    void notify() {
		
        std::shared_ptr<vsomeip::message> its_message
            = vsomeip::runtime::get()->create_request(use_tcp_);
		vsomeip::byte_t its_data[10];
		uint32_t its_size = 1;
		for (uint32_t i = 0; i < its_size; ++i)
			its_data[i] = static_cast<uint8_t>(i);
		while (running_) {

			for (auto i : service_map)
			{			
				its_message->set_service(i.first.first);
				its_message->set_instance(i.first.second);
				its_message->set_method(SAMPLE_GET_METHOD_ID); // or SET ??
				uint16_t event_ = SAMPLE_EVENT_ID;
				auto service_ = service_map.at(i.first);
				
				if (service_)
				{
					for (auto j : service_->eventgroups_)
					{
						if (!j.first)
							continue;
						for (auto k : j.second->events_)
						{
							event_ = k->id_ ;
			
							std::unique_lock<std::mutex> its_lock(notify_mutex_);
							while (!is_offered_ && running_)
								notify_condition_.wait(its_lock);
							while (is_offered_ && running_) {
								if (its_size == sizeof(its_data))
								{
									its_size = 1;
									break;
								}
								{
									std::lock_guard<std::mutex> its_lock(payload_mutex_);
									payload_->set_data(its_data, its_size);
						
									std::cout << "Setting event "<<std::hex <<
										i.first.first<<":"<< i.first.second<<":"<<
										event_ <<
										"(Length=" << std::dec << its_size << ")." << std::endl;
									app_->notify(i.first.first, i.first.second, event_, payload_);
								}
								its_size++;
							}
						}
					}

				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));

		}
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip_v3::configuration> configuration_;
    bool is_registered_;
    bool use_tcp_;
    uint32_t cycle_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    bool is_offered_;

    std::mutex payload_mutex_;
    std::shared_ptr<vsomeip::payload> payload_;

	//serviceinfo service_info;  
	std::map<std::pair<vsomeip_v3::service_t, vsomeip_v3::instance_t>, std::shared_ptr<vsomeip_v3::cfg::service>> service_map;
    // blocked_ / is_offered_ must be initialized before starting the threads!
    std::thread offer_thread_;
    std::thread notify_thread_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    service_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_tcp = false;
    uint32_t cycle = 2000; // default 1/2 s

    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string cycle_arg("--cycle");

    for (int i = 1; i < argc; i++) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
            break;
        }
        if (udp_enable == argv[i]) {
            use_tcp = false;
            break;
        }

        if (cycle_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
    }

    service_sample its_sample(use_tcp, cycle);
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
