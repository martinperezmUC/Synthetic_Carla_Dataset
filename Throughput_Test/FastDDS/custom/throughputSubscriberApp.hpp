#ifndef DATA_TYPES_SUBSCRIBER_APP_HPP
#define DATA_TYPES_SUBSCRIBER_APP_HPP

#include "throughputApplication.hpp"
#include "data_typesPubSubTypes.hpp"

#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <memory>

namespace eprosima {
namespace fastdds {
namespace dds {
    class DomainParticipantFactory;
    class DomainParticipant;
    class Subscriber;
    class Topic;
    class DataReader;
}
}
}

class throughputSubscriberApp : public throughputApplication,
                                public eprosima::fastdds::dds::DataReaderListener
{
public:
    throughputSubscriberApp(const int& domain_id);
    ~throughputSubscriberApp();

    void on_subscription_matched(eprosima::fastdds::dds::DataReader* reader, const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;
    void on_data_available(eprosima::fastdds::dds::DataReader* reader) override;

    void run() override;
    void stop() override;

private:
    std::shared_ptr<eprosima::fastdds::dds::DomainParticipantFactory> factory_;
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Subscriber* subscriber_;
    eprosima::fastdds::dds::Topic* topic_;
    eprosima::fastdds::dds::DataReader* reader_;
    eprosima::fastdds::dds::TypeSupport type_;

    std::atomic<bool> stop_;
    std::atomic<bool> has_matched_{false};
    std::mutex terminate_cv_mtx_;
    std::condition_variable terminate_cv_;
    
    // Contadores para el Throughput
    std::atomic<uint64_t> samples_received_{0};
    std::atomic<uint64_t> bytes_received_{0};
    
    bool is_stopped();
};

#endif // DATA_TYPES_SUBSCRIBER_APP_HPP
