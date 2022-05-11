#include "ros/ros.h"
#include <std_msgs/Int64.h>
#include <std_msgs/UInt32MultiArray.h>
#include <std_msgs/MultiArrayLayout.h>
#include <std_msgs/MultiArrayDimension.h>
#include <map>
#include <iostream>
#include "SpectrometerDriver.h"

/**
 * @brief Node publishes topics from kromek radiation sensor
 */

const int detChannels = 4096;
const int annChannels = 100;

void stdcall errorCallback(void *pCallbackObject, unsigned int deviceID, int errorCode, const char *pMessage)
{
    if (errorCode != ERROR_ACQUISITION_COMPLETE)
        printf("%s", pMessage);
}

class KromekRosNode
{

public:
    KromekRosNode()
        : nh_(), pn_("~")
    {
        sum_pub = nh_.advertise<std_msgs::Int64>("kromek/sum", 1);
        info_pub = nh_.advertise<std_msgs::UInt32MultiArray>("kromek/info", 1);
        raw_pub = nh_.advertise<std_msgs::UInt32MultiArray>("kromek/raw", 1);
        pn_.param("integration_seconds", integrationSeconds, 1);
    }

    ~KromekRosNode()
    {
        kr_Destruct();
    }

    void process()
    {
        _process();
    }

private:
    void _process()
    {
        kr_Initialise(errorCallback, NULL);
        
        int integrationMilliseconds = integrationSeconds * 1000;
        int numDets = 0;

        while ((detectorID = kr_GetNextDetector(detectorID)))
        {
            detMap.insert(std::pair<int, int>(detectorID, numDets));
            numDets += 1;
        }

        std_msgs::MultiArrayLayout infoLayout, layout;
        std_msgs::MultiArrayDimension infodim[1], dimensions[2];

        infodim[0].label = "detectors";
        infodim[0].size = numDets;
        infodim[0].stride = numDets;
        infoLayout.dim.push_back(infodim[0]);
        infoMsg.layout = infoLayout;
        infoMsg.data.assign(numDets, 0);
        for (auto iter = detMap.begin(); iter != detMap.end(); ++iter){
            infoMsg.data[iter->second] = iter->first;
            std::cout << "[" << iter->first << ","
                        << iter->second << "]\n";
        }

        dimensions[0].label = "detectors";
        dimensions[0].size = numDets;
        dimensions[0].stride = numDets * annChannels;
        layout.dim.push_back(dimensions[0]);

        dimensions[1].label = "channels";
        dimensions[1].size = annChannels;
        dimensions[1].stride = annChannels;
        layout.dim.push_back(dimensions[1]);

        gridMsg.layout = layout;
        gridMsg.data.assign(numDets * annChannels, 0);

        while (nh_.ok())
        {
            sumMsg.data = 0;

            // Start measurement on all detectors
            while ((detectorID = kr_GetNextDetector(detectorID)))
            {
                kr_BeginDataAcquisition(detectorID, integrationMilliseconds, 0);
            }

            sleep(integrationSeconds);

            // Collect measurement on all detectors
            while ((detectorID = kr_GetNextDetector(detectorID)))
            {
                kr_StopDataAcquisition(detectorID);

                kr_GetAcquiredData(detectorID, histogram, &counts, &realtime, &livetime);

                sumMsg.data = sumMsg.data + counts;

                auto detector = detMap.find(detectorID);

                int detIndex = detector->second;
                
                rebinHist(detector->second, histogram);

                kr_ClearAcquiredData(detectorID);
            }

            sum_pub.publish(sumMsg);

            raw_pub.publish(gridMsg);

            info_pub.publish(infoMsg);

            // std::cout << "total counts: " << sumMsg.data << std::endl;
        }
    }
 
    void rebinHist(unsigned int detIndex, unsigned int * oldArr)
    {
        int nBinsAccumulate = detChannels / annChannels;
        
        for (int i = 0; i < annChannels; i++)
        {
            uint32_t index = detIndex * annChannels + i;
            
            gridMsg.data[index] = 0;

            for (int j = 0; j < nBinsAccumulate; j++)
            {
                gridMsg.data[index] += oldArr[i * nBinsAccumulate + j];
            }
        }
    }

    ros::Publisher sum_pub, raw_pub, info_pub;
    ros::NodeHandle nh_;
    ros::NodeHandle pn_;

    std_msgs::Int64 sumMsg;
    std_msgs::UInt32MultiArray gridMsg;
    std_msgs::UInt32MultiArray infoMsg;

    unsigned int histogram[detChannels];
    unsigned int detectorID = 0;
    unsigned int counts;
    unsigned int realtime;
    unsigned int livetime;
    int integrationSeconds;
    std::map<int, int> detMap;
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "kromek_ros_node");

    KromekRosNode node;

    node.process();

    return 0;
}
