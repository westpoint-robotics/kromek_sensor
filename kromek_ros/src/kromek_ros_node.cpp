#include "ros/ros.h"
#include <std_msgs/Int64.h>
#include <std_msgs/UInt32MultiArray.h>
#include <std_msgs/MultiArrayLayout.h>
#include <std_msgs/MultiArrayDimension.h>
#include <map>
#include <iostream>
#include "SpectrometerDriver.h"

/**
 * @brief Node converts occupancy grid map to jpg (rviz background color), or optionally png (transparent background)
 */

const int detChannels = 4096;
const int annChannels = 100;
const int integrationSeconds = 5;

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
        kromek_pub = nh_.advertise<std_msgs::Int64>("kromek/sum", 1);
        kromek_raw = nh_.advertise<std_msgs::UInt32MultiArray>("kromek/raw", 1);
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

        // uint32_t data[numDets * annChannels];
        std_msgs::MultiArrayLayout layout;
        std_msgs::MultiArrayDimension dimensions[2];

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

            // Start measurement on all detectors
            while ((detectorID = kr_GetNextDetector(detectorID)))
            {
                kr_StopDataAcquisition(detectorID);

                kr_GetAcquiredData(detectorID, histogram, &counts, &realtime, &livetime);

                sumMsg.data = sumMsg.data + counts;

                auto it = detMap.find(detectorID);

                int det = it->second;
                
                rebinHist(histogram, gridMsg.data, det, detChannels, annChannels);

                // for (int i = 0; i < annChannels; i++)
                // {
                //     data[det*annChannels + i] = smlhistogram[i];
                // }

                kr_ClearAcquiredData(detectorID);
            }

            kromek_pub.publish(sumMsg);

            kromek_raw.publish(gridMsg);

            // // std::cout << "total counts: " << totalCounts << std::endl;
            // for (int i = 0; i < numDets; i++)
            // {
            //     for (int j = 0; j < annChannels; j++)
            //     {
            //         // std::cout << imageResult[i][j] << " ";
            //     }
            // }
            // // std::cout << std::endl;
        }
    }
 
    void rebinHist(unsigned int *oldArr, std::vector<unsigned int> data, const int detectorIndex, const int oldLength, const int newLength)
    {
        int nBinsAccumulate = oldLength / newLength;

        for (int i = 0; i < newLength; i++)
        {
            int index = detectorIndex * newLength + i;

            data[index] = 0;

            for (int j = 0; j < nBinsAccumulate; j++)
            {
                data[index] += oldArr[i * nBinsAccumulate + j];
            }
        }
    }

    ros::Publisher kromek_pub, kromek_raw;
    ros::NodeHandle nh_;
    ros::NodeHandle pn_;
    std_msgs::Int64 sumMsg;
    std_msgs::UInt32MultiArray gridMsg;

    unsigned int detectorID = 0;
    unsigned int histogram[detChannels];
    unsigned int counts;
    unsigned int realtime;
    unsigned int livetime;
    std::map<int, int> detMap;
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "kromek_ros_node");

    KromekRosNode node;

    node.process();

    return 0;
}
