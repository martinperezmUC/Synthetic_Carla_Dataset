#include "latencyPublisherApp.hpp"

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
#include <thread>

#include <chrono>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace eprosima::fastdds::dds;

latencyPublisherApp::latencyPublisherApp(const int& domain_id, int samples_to_send, int warmup_samples, int interval_ms)
    : factory_(nullptr), participant_(nullptr), publisher_(nullptr), subscriber_(nullptr)
    , ping_topic_(nullptr), pong_topic_(nullptr), writer_(nullptr), reader_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false)
    , matched_pub_(0), matched_sub_(0)
    , samples_to_send_(samples_to_send)
    , warmup_samples_(warmup_samples)
    , interval_ms_(interval_ms)
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

latencyPublisherApp::~latencyPublisherApp() {
    if (participant_) { participant_->delete_contained_entities(); factory_->delete_participant(participant_); }
}

void latencyPublisherApp::on_publication_matched(DataWriter*, const PublicationMatchedStatus& info) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    matched_pub_ = info.current_count;
    match_cv_.notify_one();
}

void latencyPublisherApp::on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) {
    std::lock_guard<std::mutex> lock(match_mutex_);
    matched_sub_ = info.current_count;
    match_cv_.notify_one();
}

void latencyPublisherApp::on_data_available(DataReader* reader) {
    SyntheticData::Frame sample;
    SampleInfo info;
    while (!is_stopped() && (RETCODE_OK == reader->take_next_sample(&sample, &info))) {
        if (info.valid_data) {
            
        }
    }
}

double search_json(const std::string& json, const std::string& clave) {
    size_t pos = json.find("\"" + clave + "\"");
    if (pos == std::string::npos) return 0.0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0.0;
    return std::stod(json.substr(pos + 1));
}

void latencyPublisherApp::run() {
    std::cout << "Waiting for matching..." << std::endl;
    std::unique_lock<std::mutex> lock(match_mutex_);
    // Wait until at least 1 publisher and 1 subscriber are matched
    match_cv_.wait(lock, [this]() { return matched_pub_ > 0 || is_stopped(); });
    if (is_stopped()) return;

    SyntheticData::Frame sample;

    std::cout << "Matching complete. Starting Warm-up (" << warmup_samples_ << " samples)..." << std::endl;
    for (int i = 0; i < warmup_samples_ && !is_stopped(); ++i) {
        writer_->write(&sample); // Send dummy data
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Starting Latency Test (" << samples_to_send_ << " samples)..." << std::endl;

    for (int i = 0; i < samples_to_send_ && !is_stopped(); ++i) {
        writer_->write(&sample);


        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }

    std::cout << "All samples sent. Keep the subscriber running to view results, or press Ctrl+C to terminate." << std::endl;

    while(!is_stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool latencyPublisherApp::is_stopped() { return stop_.load(); }
void latencyPublisherApp::stop() { stop_.store(true); match_cv_.notify_all(); }
