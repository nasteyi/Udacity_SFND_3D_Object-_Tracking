
#include <fstream>
#include <sstream>
#include <iostream>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "objectDetection2D.hpp"


// detects objects in an image using the YOLO library and a set of pre-trained objects from the COCO database;
// a set of 80 classes is listed in "coco.names" and pre-trained weights are stored in "yolov3.weights"
void detectObjects(
    cv::Mat& img,
    std::vector<BoundingBox>& bBoxes,
    float confThreshold,
    float nmsThreshold,
    const std::string& basePath,
    const std::string& classesFile,
    const std::string& modelConfiguration,
    const std::string& modelWeights,
    bool bVis)
{
    // load class names from file
    std::vector<std::string> classes;
    std::ifstream ifs(classesFile.c_str());
    std::string line;
    while (std::getline(ifs, line))
    {
        classes.push_back(line);
    }

    // load neural network
    cv::dnn::Net net = cv::dnn::readNetFromDarknet(modelConfiguration, modelWeights);
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    // generate 4D blob from input image
    cv::Mat blob;
    std::vector<cv::Mat> netOutput;
    double scalefactor = 1/255.0;
    cv::Size size = cv::Size(416, 416);
    cv::Scalar mean = cv::Scalar(0,0,0);
    bool swapRB = false;
    bool crop = false;
    cv::dnn::blobFromImage(img, blob, scalefactor, size, mean, swapRB, crop);

    // Get names of output layers
    std::vector<cv::String> names;
    std::vector<int> outLayers = net.getUnconnectedOutLayers(); // get  indices of  output layers, i.e.  layers with unconnected outputs
    std::vector<cv::String> layersNames = net.getLayerNames(); // get  names of all layers in the network

    names.resize(outLayers.size());
    for(std::size_t i = 0; i < outLayers.size(); ++i) // Get the names of the output layers in names
        names[i] = layersNames[outLayers[i] - 1];

    // invoke forward propagation through network
    net.setInput(blob);
    net.forward(netOutput, names);

    // Scan through all bounding boxes and keep only the ones with high confidence
    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for(std::size_t i = 0; i < netOutput.size(); ++i)
    {
        float* data = (float*)netOutput[i].data;
        for(int j = 0; j < netOutput[i].rows; ++j, data += netOutput[i].cols)
        {
            cv::Mat scores = netOutput[i].row(j).colRange(5, netOutput[i].cols);
            cv::Point classId;
            double confidence;

            // Get the value and location of the maximum score
            cv::minMaxLoc(scores, 0, &confidence, 0, &classId);
            if (confidence > confThreshold)
            {
                cv::Rect box; int cx, cy;
                cx = (int)(data[0] * img.cols);
                cy = (int)(data[1] * img.rows);
                box.width = (int)(data[2] * img.cols);
                box.height = (int)(data[3] * img.rows);
                box.x = cx - box.width/2; // left
                box.y = cy - box.height/2; // top

                boxes.emplace_back(box);
                classIds.emplace_back(classId.x);
                confidences.push_back((float)confidence);
            }
        }
    }

    // perform non-maxima suppression
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
    for(auto it=indices.begin(); it!=indices.end(); ++it) {

        BoundingBox bBox;
        bBox.roi = boxes[*it];
        bBox.classID = classIds[*it];
        bBox.confidence = confidences[*it];
        bBox.boxID = (int)bBoxes.size(); // zero-based unique identifier for this bounding box

        bBoxes.emplace_back(bBox);
    }

    // show results
    if(bVis)
    {
        cv::Mat visImg = img.clone();
        for(auto it=bBoxes.begin(); it!=bBoxes.end(); ++it)
        {
            // Draw rectangle displaying the bounding box
            int top = it->roi.y;
            int left = it->roi.x;
            int width = it->roi.width;
            int height = it->roi.height;

            cv::rectangle(visImg, cv::Point(left, top), cv::Point(left+width, top+height),cv::Scalar(0, 255, 0), 2);

            std::string label = cv::format("%.2f", (*it).confidence);
            label = classes[((*it).classID)] + ":" + label;

            // Display label at the top of the bounding box
            int baseLine;
            cv::Size labelSize = getTextSize(label, cv::FONT_ITALIC, 0.5, 1, &baseLine);
            top = std::max(top, labelSize.height);

            cv::rectangle(
                visImg,
                cv::Point(left, top - static_cast<int>(std::round(1.5*labelSize.height))),
                cv::Point(left + static_cast<int>(std::round(1.5*labelSize.width)), top + baseLine),
                cv::Scalar(255, 255, 255),
                cv::FILLED);

            cv::putText(visImg, label, cv::Point(left, top), cv::FONT_ITALIC, 0.75, cv::Scalar(0,0,0),1);

        }

        std::string windowName = "Object classification";
        cv::namedWindow( windowName, 2);
        cv::imshow( windowName, visImg );
        cv::waitKey(0); // wait for key to be pressed
    }
}
