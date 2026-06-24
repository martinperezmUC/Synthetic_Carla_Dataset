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

latencyPublisherApp::latencyPublisherApp(const int& domain_id, int samples_to_send, int warmup_samples, int interval_ms, std::string json_file_path)
    : factory_(nullptr), participant_(nullptr), publisher_(nullptr), subscriber_(nullptr)
    , ping_topic_(nullptr), pong_topic_(nullptr), writer_(nullptr), reader_(nullptr)
    , type_(new SyntheticData::FramePubSubType()), stop_(false)
    , matched_pub_(0), matched_sub_(0)
    , samples_to_send_(samples_to_send)
    , warmup_samples_(warmup_samples)
    , interval_ms_(interval_ms)
    , json_file_path_(json_file_path)
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

std::string find_json_value(const std::string& block, const std::string& key) {
    size_t pos = block.find("\"" + key + "\"");
    if (pos == std::string::npos) {
        return "";
    }

    pos = block.find(":", pos);
    if (pos == std::string::npos) {
        return "";
    }
    pos++;

    while (pos < block.length() &&(block[pos] == ' ' || block[pos] == '"' || block[pos] == '\t' || block[pos] == '\n' || block[pos] == '\r')) {
        pos++;
    }

    size_t end_pos = block.find_first_of("\",]}", pos);
    if (end_pos == std::string::npos) {
        end_pos = block.length();
    }
    return block.substr(pos, end_pos - pos);
}

std::string get_sub_block(const std::string& block, const std::string& key) {
    size_t pos = block.find("\"" + key + "\"");
    if (pos == std::string::npos) {
        return "";
    }

    size_t start_brace = block.find("{", pos);
    if (start_brace == std::string::npos) {
        return "";
    }

    int brace_count = 1;
    size_t i = start_brace + 1;
    for (; i < block.length() && brace_count > 0; i++) {
        if (block[i] == '{') {
            brace_count++;
        } else if (block[i] == '}') {
            brace_count--;
            
        }
    }

    return block.substr(start_brace, i - start_brace);
}

void latencyPublisherApp::run() {
    // ==========================================
    // PHASE 1: PRELOAD JSON ON MEMORY
    // ==========================================
    std::cout << "Pre-loading and parsing JSON file..." << std::endl;

    std::ifstream file(json_file_path_);
    if (!file.is_open()) {
        std::cerr << "Error: JSON file couldn't be opened in: " << json_file_path_ << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();
    file.close();

    size_t preload_pos = 0;
    int samples_loaded = 0;

    while (samples_loaded < samples_to_send_ && !is_stopped()) {
        size_t frame_key_pos =  json_content.find("\"frame\"", preload_pos);
        if (frame_key_pos == std::string::npos) {
            break; // File ending
        }

        size_t start_frame = json_content.rfind("{", frame_key_pos);
        int brace_count = 0;
        size_t end_frame = start_frame;
        for (; end_frame < json_content.length(); ++end_frame) {
            if (json_content[end_frame] == '{') brace_count++;
            else if (json_content[end_frame] == '}') {
                brace_count--;
                if (brace_count == 0) {
                    break;
                }
            }
        }

        // Extract string with desired frame
        std::string frame_block = json_content.substr(start_frame, end_frame - start_frame + 1);
        preload_pos = end_frame + 1;

        // Parse frame
        try {
            // Clean last Frame sample
            SyntheticData::Frame sample; 

            // Frame ID

            sample.frame(std::stoi(find_json_value(frame_block, "frame")));

            // ¿GNSS?
            std::string gnss_block = get_sub_block(frame_block, "gnss");
            if (!gnss_block.empty()) {
                sample.gnss_1().latitude(std::stod(find_json_value(gnss_block, "latitude")));
                sample.gnss_1().longitude(std::stod(find_json_value(gnss_block, "longitude")));
                sample.gnss_1().altitude(std::stod(find_json_value(gnss_block, "altitude")));
            }

            // ¿IMU?
            std::string imu_block = get_sub_block(frame_block, "imu");
            if (!imu_block.empty()) {
                std::string accel_block = get_sub_block(imu_block, "accelerometer");
                if (!accel_block.empty()) {
                    sample.imu_1().accel().x(std::stod(find_json_value(accel_block, "x")));
                    sample.imu_1().accel().y(std::stod(find_json_value(accel_block, "y")));
                    sample.imu_1().accel().z(std::stod(find_json_value(accel_block, "z")));
                }
                std::string gyro_block = get_sub_block(imu_block, "gyroscope");
                if (!gyro_block.empty()) {
                    sample.imu_1().gyro().x(std::stod(find_json_value(gyro_block, "x")));
                    sample.imu_1().gyro().y(std::stod(find_json_value(gyro_block, "y")));
                    sample.imu_1().gyro().z(std::stod(find_json_value(gyro_block, "z")));
                }
                std::string compass_val = find_json_value(imu_block, "compass");
                if (!compass_val.empty()) {
                    sample.imu_1().compass(std::stod(compass_val));
                }
            }

            // ¿CameraRGB?
            std::string camera_block = get_sub_block(frame_block, "CameraRGB");
            if (!camera_block.empty()) {
                sample.CameraRGB_1().x(std::stoi(find_json_value(camera_block, "x")));
                sample.CameraRGB_1().y(std::stoi(find_json_value(camera_block, "y")));
                sample.CameraRGB_1().fov(std::stoi(find_json_value(camera_block, "fov")));
                sample.CameraRGB_1().frameURI(find_json_value(camera_block, "frameURI"));
            }

            // ¿Radar?
            std::string radar_block = get_sub_block(frame_block, "Radar");
            if (!radar_block.empty()) {
                sample.Radar_1().num_detections(std::stoi(find_json_value(radar_block, "num_detections")));
                sample.Radar_1().cloudUri(find_json_value(radar_block, "cloudUri"));
            }

            // ¿LiDAR?
            std::string lidar_block = get_sub_block(frame_block, "LiDAR");
            if (!lidar_block.empty()) {
                sample.LiDAR_1().num_points(std::stoi(find_json_value(lidar_block, "num_points")));
                sample.LiDAR_1().horizontal_angle(std::stoi(find_json_value(lidar_block, "horizontal_angle")));
                sample.LiDAR_1().cloudUri(find_json_value(lidar_block, "cloudUri"));
            }

            preloaded_frames_.push_back(sample);
            samples_loaded++;

            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));

        } catch (const std::exception& e) {
            std::cerr << "Error pre-parsing frame: " << samples_loaded << ": " << e.what() << std::endl;
        }
    }

    if (preloaded_frames_.empty()) {
        std::cerr << "Error: No valid frames found in JSON." << std::endl;
        return;
    }

    size_t total_json_frames = preloaded_frames_.size();
    std::cout << "Pre-load complete! " << total_json_frames << " frames stored in RAM." << std::endl;

    // ==========================================
    // PHASE 2: LATENCY TEST
    // ==========================================
    std::cout << "Waiting for matching..." << std::endl;
    std::unique_lock<std::mutex> lock(match_mutex_);
    match_cv_.wait(lock, [this]() { return matched_pub_ > 0 || is_stopped(); });
    if (is_stopped()) return;

    SyntheticData::Frame warm_sample;
    std::cout << "Matching complete. Starting Warm-up (" << warmup_samples_ << " samples)..." << std::endl;
    for (int i = 0; i < warmup_samples_ && !is_stopped(); ++i) {
        writer_->write(&warm_sample); 
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Starting Latency Test (" << samples_to_send_ << " samples)..." << std::endl;
    int samples_sent = 0;

    while (samples_sent < samples_to_send_ && !is_stopped()) {
        // Search frame on ram
        SyntheticData::Frame sample = preloaded_frames_[samples_sent % total_json_frames];

        writer_->write(&sample);
        samples_sent++;

        /**
         * Progress bar
         */
        if (samples_sent % 50 == 0 || samples_sent == samples_to_send_) {
            float progress = static_cast<float>(samples_sent) / samples_to_send_;
            int bar_width = 30; // Bar visual width [========>    ]

            std::cout << "\r[";
            int pos = static_cast<int>(bar_width * progress);
            for (int i = 0; i < bar_width; ++i) {
                if (i < pos) std::cout << "=";
                else if (i == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << static_cast<int>(progress * 100.0) << "% (" 
                      << samples_sent << "/" << samples_to_send_ << ")" << std::flush;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
    }

    std::cout << "All samples sent. Terminate the subscriber to view results." << std::endl;
    
    while(!is_stopped()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool latencyPublisherApp::is_stopped() { return stop_.load(); }
void latencyPublisherApp::stop() { stop_.store(true); match_cv_.notify_all(); }
