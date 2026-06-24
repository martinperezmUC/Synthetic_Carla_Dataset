#include "latencySubscriberApp.hpp"

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

#include <iostream>
#include <stdexcept>

#include <numeric>
#include <cmath>
#include <algorithm>
#include <iomanip>

using namespace eprosima::fastdds::dds;

latencySubscriberApp::latencySubscriberApp(const int& domain_id)
    : factory_(nullptr), participant_(nullptr), publisher_(nullptr), subscriber_(nullptr)
    , ping_topic_(nullptr), pong_topic_(nullptr), writer_(nullptr), reader_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false)
{
    DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
    pqos.name("SyntheticData_Ponger");
    factory_ = DomainParticipantFactory::get_shared_instance();
    participant_ = factory_->create_participant(domain_id, pqos, nullptr, StatusMask::none());

    type_.register_type(participant_);

    publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
    subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);

    ping_topic_ = participant_->create_topic("PingTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);
    pong_topic_ = participant_->create_topic("PongTopic", type_.get_type_name(), TOPIC_QOS_DEFAULT);

    DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
    wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    writer_ = publisher_->create_datawriter(pong_topic_, wqos, this);

    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    reader_ = subscriber_->create_datareader(ping_topic_, rqos, this);
}

latencySubscriberApp::~latencySubscriberApp() {
    if (participant_) { participant_->delete_contained_entities(); factory_->delete_participant(participant_); }
}

void latencySubscriberApp::on_publication_matched(DataWriter*, const PublicationMatchedStatus& info) {
    if (info.current_count_change == 1) std::cout << "Ponger: DataWriter matched (PongTopic)." << std::endl;
}

void latencySubscriberApp::on_subscription_matched(DataReader*, const SubscriptionMatchedStatus& info) {
    if (info.current_count_change == 1) std::cout << "Ponger: DataReader matched (PingTopic)." << std::endl;
}

void latencySubscriberApp::on_data_available(DataReader* reader) {
    SyntheticData::Frame sample;
    SampleInfo info;
    while (!is_stopped() && (RETCODE_OK == reader->take_next_sample(&sample, &info))) {
        if (info.valid_data) {
            // Destination timestamp
            auto now = std::chrono::system_clock::now();
            int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()
            ).count();

            // Source timestamp
            auto source_sec = info.source_timestamp.seconds();
            auto source_nsec = info.source_timestamp.nanosec();
            int64_t source_us = (static_cast<int64_t>(source_sec) * 1000000) + (source_nsec / 1000);
            // Calculate E2E latency
            double latency_us = static_cast<double>(now_us - source_us);
            
            // Save result
            if (latency_us > 0) {
                times_.push_back(latency_us);
            } else {
                std::cout << "Uncoherent latency calculated." << std::endl;
            }
        }
    }
}

void latencySubscriberApp::run() {
    std::cout << "Ponger is ready and waiting for Pings..." << std::endl;
    
    std::unique_lock<std::mutex> lck(terminate_cv_mtx_);
    terminate_cv_.wait(lck, [this] { return is_stopped(); });

    // Eliminar muestras de calentamiento
    if (times_.size() > 100) {
        times_.erase(times_.begin(), times_.begin() + 100);
    }

    if (!times_.empty()) {
        std::sort(times_.begin(), times_.end());
        double sum = std::accumulate(times_.begin(), times_.end(), 0.0);
        double mean = sum / times_.size();
        
        double variance = 0.0;
        for (double t : times_) variance += (t - mean) * (t - mean);
        double stdev = std::sqrt(variance / times_.size());

        std::cout << "\n--- ONE-WAY LATENCY RESULTS ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Samples : " << times_.size() << std::endl;
        std::cout << "Mean    : " << mean << " us" << std::endl;
        std::cout << "StDev   : " << stdev << " us" << std::endl;
        std::cout << "Min     : " << times_.front() << " us" << std::endl;
        std::cout << "50% (Med): " << times_[times_.size() * 0.50] << " us" << std::endl;
        std::cout << "90%     : " << times_[times_.size() * 0.90] << " us" << std::endl;
        std::cout << "99%     : " << times_[times_.size() * 0.99] << " us" << std::endl;
        std::cout << "Max     : " << times_.back() << " us" << std::endl;
    } else {
        std::cout << "\nNo samples were received." << std::endl;
    }
}

bool latencySubscriberApp::is_stopped() { return stop_.load(); }
void latencySubscriberApp::stop() { stop_.store(true); terminate_cv_.notify_all(); }
