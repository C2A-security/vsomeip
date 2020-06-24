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
#include "configuration/include/configuration.hpp"
#include <configuration/include/eventgroup.hpp>
#include <configuration/include/event.hpp>
#include <configuration/include/service.hpp>
#include "sample-ids.hpp"

/* struct service_info { */
/*     vsomeip::service_t service_id; */
/*     vsomeip::instance_t instance_id; */
/*     vsomeip::method_t method_id; */
/*     vsomeip::event_t event_id; */
/*     vsomeip::eventgroup_t eventgroup_id; */
/*     vsomeip::method_t shutdown_method_id; */
/*     vsomeip::method_t notify_method_id; */
/* }; */


class service_sample {
public:
    service_sample(bool _use_tcp, uint32_t _cycle, uint32_t _size, bool _use_dynamic_routing) :
            app_(vsomeip::runtime::get()->create_application()),
            is_registered_(false),
            use_tcp_(_use_tcp),
            cycle_(_cycle),
            size_(_size),
			payload_data_(nullptr),
            use_dynamic_routing_(_use_dynamic_routing),			
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

		payload_data_ = new uint8_t[size_];
		for (uint8_t i = 0 ; i < size_; ++i)
			payload_data_[i] = (uint8_t)(i%10);
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
			{
				std::lock_guard<std::mutex> its_lock(payload_mutex_);
				payload_ = vsomeip::runtime::get()->create_payload();
				payload_->set_data(payload_data_, sizeof(payload_data_));
			}
			auto service = configuration_->find_service(i.first, i.second);
			service_map.insert(std::make_pair(i, service));
			std::cout << "Got service "<<std::hex<<i.first<<":"<<i.second<<std::endl;
			for (auto j : service->events_)
			{
				std::set<vsomeip::eventgroup_t> its_groups;
				for (auto k : j.second->groups_) // weak_ptr 
				{
					if (auto eg = k.lock())
					{
						std::cout << "                  In group " << std::hex << eg->id_ << std::endl;
						its_groups.insert(eg->id_);
					}
					//todo else error
				}
				std::cout << "                  Will offer event "<<std::hex<<
					i.first<<":"<<
					i.second<<":"<< 
					j.second->id_<<":"<<
					"("<<j.second->update_cycle_.count()<<")"<<
					std::endl;
					
				app_->offer_event(
					i.first,
					i.second, 
					j.second->id_,
					its_groups,
					vsomeip::event_type_e::ET_FIELD,
					j.second->update_cycle_, //std::chrono::milliseconds::zero(), // cycle
					false, // change resets cycle
					true, // update on change
					nullptr, // epsilon change func
					j.second->reliability_);
				/* { */
				/* 	std::lock_guard<std::mutex> its_lock(payload_mutex_); */
				/* 	payload_ = vsomeip::runtime::get()->create_payload(); */
				/* } */				
			}
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
		delete payload_data_;
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
        std::lock_guard<std::mutex> its_lock(notify_mutex_);
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
		/* std::cout << "On get will respond for "<< std::hex <<  */
		/* 	_message->get_service() <<":"<< _message->get_instance() << std::endl; */
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
		uint16_t event_ = 0;//SAMPLE_EVENT_ID;

		auto service_ = service_map.at(std::make_pair(_message->get_service(), _message->get_instance()));
		if (service_)
		{
			event_ = service_->eventgroups_.begin()->second->events_.begin()->get()->id_ ; // ilya come here
		}
//		std::cout << "On set will notify event id "<< std::hex << event_ << std::endl;

        app_->notify(_message->get_service(), _message->get_instance(),
                     event_, payload_);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);
		if (!use_dynamic_routing_) {
            offer();
            while (running_);
        } else {
            while (running_) {
                if (is_offer)
                    offer();
                else
                    stop_offer();

                for (int i = 0; i < 10 && running_; i++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                is_offer = !is_offer;
            }
        }
    }

    void notify() {
        std::shared_ptr<vsomeip::message> its_message
            = vsomeip::runtime::get()->create_request(use_tcp_);
		static uint32_t count = 0;
		std::unique_lock<std::mutex> its_lock(notify_mutex_);
		while (!is_offered_ && running_)
			notify_condition_.wait(its_lock);
		while (is_offered_ && running_)
		{
			for (auto i : service_map)
			{			
				its_message->set_service(i.first.first);
				its_message->set_instance(i.first.second);
				its_message->set_method(SAMPLE_GET_METHOD_ID); // or SET ??
				uint16_t event_ = SAMPLE_EVENT_ID;
				auto service_ = service_map.at(i.first);
				
				if (service_) // otherwise throw?
				{
					for (auto j : service_->eventgroups_)
					{
						for (auto k : j.second->events_)
						{
							event_ = k->id_ ;
							*(uint32_t*)payload_data_ =  count++;
							{								
								{
									std::lock_guard<std::mutex> its_lock(payload_mutex_);
									payload_->set_data(payload_data_, size_);
									/* std::cout << "Setting event "<<std::hex << */
									/* 	i.first.first<<":"<< i.first.second<<":"<< */
									/* 	event_ << */
									/* 	"(Length=" << std::dec << its_size << ")." << std::endl; */
									VSOMEIP_DEBUG << "Notify by app cycle # " << std::hex << count << " : " << event_ ;
									/* std::cout << "Notify by app cycle " << std::hex << event_ << std::endl; */
									app_->notify(i.first.first, i.first.second, event_, payload_);
								}
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
    uint32_t size_;
	uint8_t *payload_data_;

    bool use_dynamic_routing_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    bool is_offered_;

    std::mutex payload_mutex_;
    std::shared_ptr<vsomeip::payload> payload_;

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
    bool use_dynamic_routing(false);
    bool use_tcp = false;
    uint32_t cycle = 1000; // default 1s
	uint32_t size = 10; // bytes
	
    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string cycle_arg("--cycle");
    std::string size_arg("--size");
    std::string dynamic_routing_enable("--dynamic-routing");

    for (int i = 1; i < argc; i++) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
			std::cout << "Will use tcp for requests" << std::endl;
            break;
        }
        else if (udp_enable == argv[i]) {
            use_tcp = false;
            break;
        }
        else if (dynamic_routing_enable == argv[i]) {
            use_dynamic_routing = true;
			std::cout << "Will use dynamic routing (un/re-offering service[s])" << std::endl;
        }

        else if (cycle_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
			std::cout << "Will cycle for "<<cycle<< "for events update" << std::endl;

        }
        else if (size_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> size;
        }
		else
			std::cerr << "Unknown arg ignored! " << argv[i] << std::endl;
    }

    service_sample its_sample(use_tcp, cycle, size, use_dynamic_routing);
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
