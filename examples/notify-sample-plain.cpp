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

#include "sample-ids.hpp"

class service_sample {
public:
    service_sample(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _use_tcp, uint32_t _cycle, bool _flip_flop) :
            app_(vsomeip::runtime::get()->create_application("notify-sample-plain")),
            is_registered_(false),
            service_(_service),
            instance_(_instance),
            use_tcp_(_use_tcp),
            cycle_(_cycle),
			flip_flop_(_flip_flop),
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
//		app_->set_client(SAMPLE_SERVICE_APP_ID);
		// not yet offered, won't work here :(
		/* app_->update_service_configuration(service_, instance_, */
		/* 								   30509, //SAMPLE_PORT, */
		/* 								   true, //reliable_ */
		/* 								   false, // magic_cookies_nebled_ */
		/* 								   true /\*offer*\/); */
        app_->register_state_handler(
                std::bind(&service_sample::on_state, this,
                        std::placeholders::_1));

        app_->register_message_handler(
			service_,
			instance_,
			SAMPLE_GET_METHOD_ID,
			std::bind(&service_sample::on_get, this,
					  std::placeholders::_1));

        app_->register_message_handler(
			service_,
			instance_,
			SAMPLE_SET_METHOD_ID,
			std::bind(&service_sample::on_set, this,
					  std::placeholders::_1));
		
        std::set<vsomeip::eventgroup_t> its_groups;
        its_groups.insert(SAMPLE_EVENTGROUP_ID);
        app_->offer_event(
			service_,
			instance_,
			SAMPLE_EVENT_ID,
			its_groups,
			vsomeip::event_type_e::ET_FIELD, std::chrono::milliseconds::zero(),
			false, true, nullptr, vsomeip::reliability_type_e::RT_UNKNOWN);
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
		//static bool first_time = true;
        std::lock_guard<std::mutex> its_lock(notify_mutex_);
        app_->offer_service(service_, instance_);
		/* if (first_time) */
		/* 			app_->update_service_configuration(service_, instance_, */
		/* 								   30509, //SAMPLE_PORT, */
		/* 								   true, //reliable_ */
		/* 								   false, // magic_cookies_nebled_ */
		/* 								   true /\*offer*\/); */
		//first_time = false;
        is_offered_ = true;
        notify_condition_.notify_one();
    }

    void stop_offer() {
        app_->stop_offer_service(service_, instance_);
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
        app_->notify(service_, instance_,
                     SAMPLE_EVENT_ID, payload_);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);
        while (running_) {
            if (is_offer)
                offer();
            else
                stop_offer();

            for (int i = 0; i < 10 && running_; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			if (flip_flop_)
				is_offer = !is_offer;
        }
    }

    void notify() {
        std::shared_ptr<vsomeip::message> its_message
            = vsomeip::runtime::get()->create_request(use_tcp_);

        its_message->set_service(service_);
        its_message->set_instance(instance_);
        its_message->set_method(SAMPLE_SET_METHOD_ID);

        vsomeip::byte_t its_data[10];
        uint32_t its_size = 1;

        while (running_) {
            std::unique_lock<std::mutex> its_lock(notify_mutex_);
            while (!is_offered_ && running_)
                notify_condition_.wait(its_lock);
            while (is_offered_ && running_) {
                if (its_size == sizeof(its_data))
                    its_size = 1;

                for (uint32_t i = 0; i < its_size; ++i)
                    its_data[i] = static_cast<uint8_t>(i);

                {
                    std::lock_guard<std::mutex> its_lock(payload_mutex_);
                    payload_->set_data(its_data, its_size);

                    std::cout << "Setting event (Length=" << std::dec << its_size << ")." << std::endl;
                    app_->notify(service_, instance_, SAMPLE_EVENT_ID, payload_);
                }

                its_size++;

                std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
            }
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
	vsomeip::service_t service_;
	vsomeip::instance_t instance_;
    bool use_tcp_;
    uint32_t cycle_;
	bool flip_flop_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    bool is_offered_;

    std::mutex payload_mutex_;
    std::shared_ptr<vsomeip::payload> payload_;

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
	bool flip_flop = false;
    uint32_t cycle = 1000; // default 1s
	vsomeip::service_t service = SAMPLE_SERVICE_ID;
	vsomeip::instance_t instance = SAMPLE_INSTANCE_ID;
	
    std::string tcp_enable("--tcp");
    std::string udp_enable("--udp");
    std::string cycle_arg("--cycle");
    std::string flip_flop_arg("--flip-flop");
    std::string service_arg("--service");
    std::string instance_arg("--instance");
// todo flip flop
    for (int i = 1; i < argc; i++) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
          }
        else if (udp_enable == argv[i]) {
            use_tcp = false;
        }
        else if (flip_flop_arg == argv[i]) {
            flip_flop = true;
        }
        else if (cycle_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
        else if (service_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            if (argv[i][1] > 1 && argv[i][0] == '0' && argv[i][1] == 'x') 
                converter << std::hex << argv[i];			
			else
                converter << std::hex << argv[i];							
            converter >> service;
        }
        else if (instance_arg == argv[i] && i + 1 < argc) {
            i++;
            std::stringstream converter;
            if (argv[i][1] > 1 && argv[i][0] == '0' && argv[i][1] == 'x') 
                converter << std::hex << argv[i];			
			else
                converter << std::hex << argv[i];							
            converter >> instance;
        }
		else
			std::cerr << "Unknown arg ignored! " << argv[i] << std::endl;

    }

    service_sample its_sample(service, instance, use_tcp, cycle, flip_flop);
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
