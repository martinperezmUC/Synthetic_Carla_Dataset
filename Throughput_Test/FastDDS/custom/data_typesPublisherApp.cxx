#include "data_typesPublisherApp.hpp"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>

using namespace eprosima::fastdds::dds;

data_typesPublisherApp::data_typesPublisherApp(const int& domain_id)
    : factory_(nullptr), participant_(nullptr), publisher_(nullptr)
    , topic_(nullptr), writer_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false), matched_(0)
{
    target_hz_ = 0; 
    if (const char* env_p = std::getenv("PUB_FREQ")) {
        target_hz_ = std::atoi(env_p);
    }

    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("Vehicle_Publisher");
    factory_ = DomainParticipantFactory::get_shared_instance();
    participant_ = factory_->create_participant(domain_id, pqos, nullptr, StatusMask::none());

    type_.register_type(participant_);

    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    topic_ = participant_->create_topic("VehicleDataTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);

    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writer_ = publisher_->create_datawriter(topic_, wqos, this);
}

data_typesPublisherApp::~data_typesPublisherApp() {
    if (participant_) { participant_->delete_contained_entities(); factory_->delete_participant(participant_); }
}

void data_typesPublisherApp::on_publication_matched(DataWriter*, const PublicationMatchedStatus& info) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    matched_ = info.current_count;
    match_cv_.notify_one();
}

void data_typesPublisherApp::run() {
    std::cout << "[Vehicle] Waiting to connect with Edge Node..." << std::endl;
    std::unique_lock<std::mutex> lock(match_mutex_);
    match_cv_.wait(lock, [this]() { return (matched_ > 0) || is_stopped(); });
    if (is_stopped()) return;

    if (target_hz_ > 0) {
        std::cout << "[Vehicle] Connected. Frequency: " << target_hz_ << " Hz" << std::endl;
    } else {
        std::cout << "[Vehicle] Connected. Frequency: MAX " << std::endl;
    }

    SyntheticData::Frame sample;
    int32_t frame_count = 0;

    // Pre-calculate the sleep duration based on the requested Hz
    auto sleep_interval = std::chrono::microseconds(target_hz_ > 0 ? 1000000 / target_hz_ : 0);
    auto next_wake = std::chrono::steady_clock::now();

    while (!is_stopped()) {
        sample.frame(frame_count++); // Simply increment an internal counter for tracking
        writer_->write(&sample);

        if (target_hz_ > 0) {
            // Throttled mode: Sleep until the next precise cycle tick
            next_wake += sleep_interval;
            std::this_thread::sleep_until(next_wake);
        } else {
            // Max speed mode: Yield the thread lightly to prevent complete OS freezing
            std::this_thread::yield();
        }
    }
}

bool data_typesPublisherApp::is_stopped() { return stop_.load(); }
void data_typesPublisherApp::stop() { stop_.store(true); match_cv_.notify_all(); }
