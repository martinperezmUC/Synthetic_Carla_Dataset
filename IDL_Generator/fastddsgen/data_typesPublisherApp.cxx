#include "data_typesPublisherApp.hpp"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>

#include <stdexcept>
#include <iostream>
#include <chrono>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace eprosima::fastdds::dds;

data_typesPublisherApp::data_typesPublisherApp(const int& domain_id)
    : factory_(nullptr), participant_(nullptr), publisher_(nullptr), subscriber_(nullptr)
    , ping_topic_(nullptr), pong_topic_(nullptr), writer_(nullptr), reader_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false)
    , matched_pub_(0), matched_sub_(0), pong_received_(false)
{
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("SyntheticData_Pinger");
    factory_ = DomainParticipantFactory::get_shared_instance();
    participant_ = factory_->create_participant(domain_id, pqos, nullptr, StatusMask::none());

    type_.register_type(participant_);

    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);

    ping_topic_ = participant_->create_topic("PingTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);
    pong_topic_ = participant_->create_topic("PongTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);

    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writer_ = publisher_->create_datawriter(ping_topic_, wqos, this);

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    reader_ = subscriber_->create_datareader(pong_topic_, rqos, this);
}

data_typesPublisherApp::~data_typesPublisherApp() {
    if (participant_) { participant_->delete_contained_entities(); factory_->delete_participant(participant_); }
}

void data_typesPublisherApp::on_publication_matched(DataWriter*, const PublicationMatchedStatus& info) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    matched_pub_ = info.current_count;
    match_cv_.notify_one();
}

void data_typesPublisherApp::on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    matched_sub_ = info.current_count;
    match_cv_.notify_one();
}

void data_typesPublisherApp::on_data_available(DataReader* reader) {
    SyntheticData::Frame sample;
    SampleInfo info;
    while (!is_stopped() && (RETCODE_OK == reader->take_next_sample(&sample, &info))) {
        if (info.valid_data) {
            std::lock_guard<std::mutex> lock(pong_mutex_);
            pong_received_ = true; 
            pong_cv_.notify_one();
        }
    }
}

void data_typesPublisherApp::run() {
    std::cout << "Waiting for matching..." << std::endl;
    std::unique_lock<std::mutex> lock(match_mutex_);
    match_cv_.wait(lock, [this]() { return (matched_pub_ > 0 && matched_sub_ > 0) || is_stopped(); });
    if (is_stopped()) return;

    SyntheticData::Frame sample;

    std::cout << "Matching complete. Starting Warm-up (" << warmup_samples_ << " samples)..." << std::endl;
    for (int i = 0; i < warmup_samples_ && !is_stopped(); ++i) {
        sample.id_mensaje(i); 
        pong_received_ = false;
        writer_->write(&sample);
        
        std::unique_lock<std::mutex> pong_lock(pong_mutex_);
        pong_cv_.wait_for(pong_lock, std::chrono::seconds(1), [this]() { return pong_received_ || is_stopped(); });
    }

    std::cout << "Starting Latency Test (" << samples_to_send_ << " samples)..." << std::endl;
    times_.reserve(samples_to_send_);

    for (int i = 0; i < samples_to_send_ && !is_stopped(); ++i) {
        sample.id_mensaje(i);
        pong_received_ = false;

        auto start = std::chrono::high_resolution_clock::now();
        writer_->write(&sample);

        std::unique_lock<std::mutex> pong_lock(pong_mutex_);
        pong_cv_.wait(pong_lock, [this]() { return pong_received_ || is_stopped(); });
        auto end = std::chrono::high_resolution_clock::now();

        if (is_stopped()) break;

        double rtt_us = std::chrono::duration<double, std::micro>(end - start).count();
        times_.push_back(rtt_us / 2.0); // Dividimos entre 2 para el One-Way Latency
    }

    if (!times_.empty()) {
        std::sort(times_.begin(), times_.end());
        double sum = std::accumulate(times_.begin(), times_.end(), 0.0);
        double mean = sum / times_.size();
        
        double variance = 0.0;
        for (double t : times_) variance += (t - mean) * (t - mean);
        double stdev = std::sqrt(variance / times_.size());

        std::cout << "\n--- RESULTS (One-Way Latency in microseconds) ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Samples : " << times_.size() << std::endl;
        std::cout << "Mean    : " << mean << " us" << std::endl;
        std::cout << "StDev   : " << stdev << " us" << std::endl;
        std::cout << "Min     : " << times_.front() << " us" << std::endl;
        std::cout << "50%     : " << times_[times_.size() * 0.50] << " us" << std::endl;
        std::cout << "90%     : " << times_[times_.size() * 0.90] << " us" << std::endl;
        std::cout << "99%     : " << times_[times_.size() * 0.99] << " us" << std::endl;
        std::cout << "99.99%  : " << times_[times_.size() * 0.9999] << " us" << std::endl;
        std::cout << "Max     : " << times_.back() << " us" << std::endl;
    }
}

bool data_typesPublisherApp::is_stopped() { return stop_.load(); }
void data_typesPublisherApp::stop() { stop_.store(true); match_cv_.notify_all(); pong_cv_.notify_all(); }
