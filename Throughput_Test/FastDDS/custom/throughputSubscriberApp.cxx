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

#include <vector>
#include <numeric>
#include <cmath>
#include <algorithm>

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
        has_matched_.store(true, std::memory_order_relaxed);
    } else if (info.current_count_change == -1) {
        std::cout << "[Edge Node] Vehicle disconnected. Total number: " << info.current_count << std::endl;

        if (has_matched_.load(std::memory_order_relaxed) && info.current_count == 0) {
            std::cout << "[Edge Node] All vehicles disconnected." << std::endl;
            stop();
        }
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
    
    std::vector<double> msg_history;
    std::vector<double> mbps_history;

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

            msg_history.push_back(msgs_per_sec);
            mbps_history.push_back(mbps);
        }
    }

    if (!mbps_history.empty()) {
        // Sort vectors to calculate min, max, and percentiles
        std::sort(msg_history.begin(), msg_history.end());
        std::sort(mbps_history.begin(), mbps_history.end());

        // Message Rate (msg/s)
        size_t msg_samples = msg_history.size();
        double msg_sum = std::accumulate(msg_history.begin(), msg_history.end(), 0.0);
        double msg_mean = msg_sum / msg_samples;
        
        double msg_variance = 0.0;
        for (double m : msg_history) msg_variance += (m - msg_mean) * (m - msg_mean);
        double msg_stdev = std::sqrt(msg_variance / msg_samples);

        // Bandwidth (Mbps)
        size_t mbps_samples = mbps_history.size();
        double mbps_sum = std::accumulate(mbps_history.begin(), mbps_history.end(), 0.0);
        double mbps_mean = mbps_sum / mbps_samples;
        
        double mbps_variance = 0.0;
        for (double b : mbps_history) mbps_variance += (b - mbps_mean) * (b - mbps_mean);
        double mbps_stdev = std::sqrt(mbps_variance / mbps_samples);

        // --- PRINT MESSAGE RATE ---
        std::cout << "\n--- RESULTS (Throughput - Message Rate in msg/s) ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Samples : " << msg_samples << std::endl;
        std::cout << "Mean    : " << msg_mean << " msg/s" << std::endl;
        std::cout << "StDev   : " << msg_stdev << " msg/s" << std::endl;
        std::cout << "Min     : " << msg_history.front() << " msg/s" << std::endl;
        std::cout << "50%     : " << msg_history[msg_samples * 0.50] << " msg/s" << std::endl;
        std::cout << "90%     : " << msg_history[msg_samples * 0.90] << " msg/s" << std::endl;
        std::cout << "99%     : " << msg_history[msg_samples * 0.99] << " msg/s" << std::endl;
        std::cout << "99.99%  : " << msg_history[msg_samples * 0.9999] << " msg/s" << std::endl;
        std::cout << "Max     : " << msg_history.back() << " msg/s" << std::endl;

        // --- PRINT BANDWIDTH ---
        std::cout << "\n--- RESULTS (Throughput - Bandwidth in Mbps) ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Samples : " << mbps_samples << std::endl;
        std::cout << "Mean    : " << mbps_mean << " Mbps" << std::endl;
        std::cout << "StDev   : " << mbps_stdev << " Mbps" << std::endl;
        std::cout << "Min     : " << mbps_history.front() << " Mbps" << std::endl;
        std::cout << "50%     : " << mbps_history[mbps_samples * 0.50] << " Mbps" << std::endl;
        std::cout << "90%     : " << mbps_history[mbps_samples * 0.90] << " Mbps" << std::endl;
        std::cout << "99%     : " << mbps_history[mbps_samples * 0.99] << " Mbps" << std::endl;
        std::cout << "99.99%  : " << mbps_history[mbps_samples * 0.9999] << " Mbps" << std::endl;
        std::cout << "Max     : " << mbps_history.back() << " Mbps" << std::endl;
    } else {
        std::cout << "\n[-] No data available to calculate throughput statistics." << std::endl;
    }
}

bool throughputSubscriberApp::is_stopped() { return stop_.load(); }
void throughputSubscriberApp::stop() { stop_.store(true); terminate_cv_.notify_all(); }
