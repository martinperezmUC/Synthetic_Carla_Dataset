#ifndef DATA_TYPES_PUBLISHER_APP_HPP
#define DATA_TYPES_PUBLISHER_APP_HPP

#include "throughputApplication.hpp"
#include "data_typesPubSubTypes.hpp"

#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>
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
    class Publisher;
    class Topic;
    class DataWriter;
}
}
}

class throughputPublisherApp : public throughputApplication,
                               public eprosima::fastdds::dds::DataWriterListener
{
public:
    throughputPublisherApp(const int& domain_id, std::string json_file_path);
    ~throughputPublisherApp();

    void on_publication_matched(eprosima::fastdds::dds::DataWriter* writer, const eprosima::fastdds::dds::PublicationMatchedStatus& info) override;

    void run() override;
    void stop() override;

private:
    std::shared_ptr<eprosima::fastdds::dds::DomainParticipantFactory> factory_;
    eprosima::fastdds::dds::DomainParticipant* participant_;
    eprosima::fastdds::dds::Publisher* publisher_;
    eprosima::fastdds::dds::Topic* topic_;
    eprosima::fastdds::dds::DataWriter* writer_;
    eprosima::fastdds::dds::TypeSupport type_;

    std::atomic<bool> stop_;
    std::mutex match_mutex_;
    std::condition_variable match_cv_;
    int matched_;
    
    int target_hz_;
    std::string json_file_path_;
    std::vector<SyntheticData::Frame> preloaded_frames_;

    bool is_stopped();
};

#endif // DATA_TYPES_PUBLISHER_APP_HPP
