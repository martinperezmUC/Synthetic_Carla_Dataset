#ifndef DATA_TYPES_SUBSCRIBER_APP_HPP
#define DATA_TYPES_SUBSCRIBER_APP_HPP

#include "latencyApplication.hpp"
#include "data_typesPubSubTypes.hpp"

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <memory>
#include <vector>

namespace eprosima {
namespace fastdds {
namespace dds {
    class DomainParticipantFactory;
    class DomainParticipant;
    class Publisher;
    class Subscriber;
    class Topic;
    class DataWriter;
    class DataReader;
}
}
}

class latencySubscriberApp : public latencyApplication,
                                public eprosima::fastdds::dds::DataWriterListener,
                                public eprosima::fastdds::dds::DataReaderListener
{
public:
    latencySubscriberApp(const int& domain_id);
    ~latencySubscriberApp();

    void on_publication_matched(eprosima::fastdds::dds::DataWriter* writer, const eprosima::fastdds::dds::PublicationMatchedStatus& info) override;
    void on_subscription_matched(eprosima::fastdds::dds::DataReader* reader, const eprosima::fastdds::dds::SubscriptionMatchedStatus& info) override;
    void on_data_available(eprosima::fastdds::dds::DataReader* reader) override;

    void run() override;
    void stop() override;

private:
    std::shared_ptr<eprosima::fastdds::dds::DomainParticipantFactory> factory_;
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Publisher* publisher_;
    eprosima::fastdds::dds::Subscriber* subscriber_;
    eprosima::fastdds::dds::Topic* ping_topic_;
    eprosima::fastdds::dds::Topic* pong_topic_;
    eprosima::fastdds::dds::DataWriter* writer_;
    eprosima::fastdds::dds::DataReader* reader_;
    eprosima::fastdds::dds::TypeSupport type_;

    std::atomic<bool> stop_;
    std::mutex terminate_cv_mtx_;
    std::condition_variable terminate_cv_;
    std::vector<double> times_;
    bool is_stopped();
};

#endif // DATA_TYPES_SUBSCRIBER_APP_HPP
