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
            writer_->write(&sample);
        }
    }
}

void latencySubscriberApp::run() {
    std::cout << "Ponger is ready and waiting for Pings..." << std::endl;
    std::unique_lock<std::mutex> lck(terminate_cv_mtx_);
    terminate_cv_.wait(lck, [this] { return is_stopped(); });
}

bool latencySubscriberApp::is_stopped() { return stop_.load(); }
void latencySubscriberApp::stop() { stop_.store(true); terminate_cv_.notify_all(); }
