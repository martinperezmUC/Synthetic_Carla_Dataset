#include "throughputSubscriberApp.hpp"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/core/ReturnCode.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace eprosima::fastdds::dds;

throughputSubscriberApp::throughputSubscriberApp(const int& domain_id)
    : factory_(nullptr), participant_(nullptr), subscriber_(nullptr)
    , topic_(nullptr), reader_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false)
{
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("EdgeNode_Subscriber");
    factory_ = DomainParticipantFactory::get_shared_instance();
    participant_ = factory_->create_participant(domain_id, pqos, nullptr, StatusMask::none());

    type_.register_type(participant_);

    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
    topic_ = participant_->create_topic("VehicleDataTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    reader_ = subscriber_->create_datareader(topic_, rqos, this);
}

throughputSubscriberApp::~throughputSubscriberApp() {
    if (participant_) { participant_->delete_contained_entities(); factory_->delete_participant(participant_); }
}

void throughputSubscriberApp::on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) {
    if (info.current_count_change == 1) {
        std::cout << "[Edge Node] New vehicle connected. Total number: " << info.current_count << std::endl;
    } else if (info.current_count_change == -1) {
        std::cout << "[Edge Node] Vehicle disconnected. Total number: " << info.current_count << std::endl;
    }
}

void throughputSubscriberApp::on_data_available(DataReader* reader) {
    SyntheticData::Frame sample;
    SampleInfo info;
    while (!is_stopped() && (RETCODE_OK == reader->take_next_sample(&sample, &info))) {
        if (info.valid_data) {
            // Increment the total messages counter atomically
            samples_received_.fetch_add(1, std::memory_order_relaxed);
            
            // Query FastCDR to calculate the EXACT network byte size of this specific sample
            uint32_t exact_size = type_->calculate_serialized_size(&sample, eprosima::fastdds::dds::DEFAULT_DATA_REPRESENTATION);
            
            // Add the exact bytes to the bandwidth counter atomically
            bytes_received_.fetch_add(exact_size, std::memory_order_relaxed);
        }
    }
}

void throughputSubscriberApp::run() {
    std::cout << "--- Started edge node. Monitoring Throughput ---" << std::endl;
    
    while (!is_stopped()) {
        // Take a "snapshot" of the atomic counters at t=0
        uint64_t msgs_before = samples_received_.load(std::memory_order_relaxed);
        uint64_t bytes_before = bytes_received_.load(std::memory_order_relaxed);
        
        // Sleep for exactly one second to establish the measurement window
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Take a "snapshot" of the atomic counters at t=1
        uint64_t msgs_after = samples_received_.load(std::memory_order_relaxed);
        uint64_t bytes_after = bytes_received_.load(std::memory_order_relaxed);
        
        // Calculate the delta
        uint64_t msgs_per_sec = msgs_after - msgs_before;
        uint64_t bytes_per_sec = bytes_after - bytes_before;

        if (msgs_per_sec > 0) {
            double mbps = (bytes_per_sec * 8.0) / 1000000.0;
            double MBps = bytes_per_sec / 1000000.0;
            
            std::cout << "[Throughput] " << std::setw(6) << msgs_per_sec << " msg/s | " 
                      << std::fixed << std::setprecision(2) 
                      << mbps << " Mbps | " 
                      << MBps << " MB/s " << std::endl;
        }
    }
}

bool throughputSubscriberApp::is_stopped() { return stop_.load(); }
void throughputSubscriberApp::stop() { stop_.store(true); terminate_cv_.notify_all(); }
