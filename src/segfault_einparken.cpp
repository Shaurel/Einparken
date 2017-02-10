//#include "stdio.h"
#include "std_msgs/String.h"
#include <math.h>
#include "sstream"
#include <iostream>
#include <utility>
#include <ros/ros.h>
#include <pses_basis/SensorData.h>
#include <pses_basis/Command.h>
#include <pses_basis/CarInfo.h>
#include <sensor_msgs/Range.h>
#include <sensor_msgs/Imu.h>
#include <nav_msgs/Odometry.h>
#include <functional>

//yaw aus Car-info
void odometryCallback(const nav_msgs::Odometry::ConstPtr& msg, nav_msgs::Odometry *odom){
        *odom = *msg;
}

void sensorCallback(const pses_basis::SensorData::ConstPtr& sens_msg, pses_basis::SensorData *sensors){
        *sensors = *sens_msg;
        //ROS_INFO("front: [%f]", sensors->range_sensor_front);
}

void carInfoCallback(const pses_basis::CarInfo::ConstPtr& carInfo_msg, pses_basis::CarInfo *carInfo){
        *carInfo = *carInfo_msg;
}

void  yawHandler (float &oldYaw, float newYaw, float &relAngle){

        // Abfangen des Sprungs von -pi auf +pi
        // links drehend
        if (newYaw - oldYaw > 5.0f) {
                relAngle += fabs(newYaw + oldYaw);
        }
        // rechts drehend
        else if (newYaw - oldYaw < -5.0f) {
                relAngle -= fabs(newYaw + oldYaw);

        } else {
                // positiv values turn left, negativ values turn right
                relAngle += newYaw - oldYaw;
        }
        oldYaw = newYaw;
}

// Custom funs

float calcAverage(const float* array){
    int size = 3;
    float avgSum= 0;
    for (int i = 0; i < size; i++) {
        avgSum += array[i];
    }
    return avgSum/(float)size;
}

int main(int argc, char **argv)
{
        ros::init(argc, argv, "segfault_einparken");
        ros::NodeHandle nctrl;
        ros::Rate loop_rate(40);

        /*Subscriptions from other ROS Nodes*/
        //Initialize variables
        pses_basis::Command cmd;
        pses_basis::SensorData sensors;
        sensor_msgs::Imu imu;
        nav_msgs::Odometry odom;
        pses_basis::CarInfo carInfo;

        //Publisher
        ros::Publisher commands_pub = nctrl.advertise<pses_basis::Command>("pses_basis/command", 10);
        //Subscriber
        ros::Subscriber odom_sub = nctrl.subscribe<nav_msgs::Odometry>("odom",50, std::bind(odometryCallback, std::placeholders::_1, &odom));
	    ros::Subscriber sensor_sub = nctrl.subscribe<pses_basis::SensorData>("pses_basis/sensor_data", 10, std::bind(sensorCallback, std::placeholders::_1, &sensors));
        ros::Subscriber carInfo_sub = nctrl.subscribe<pses_basis::CarInfo>("pses_basis/car_info",10, std::bind(carInfoCallback, std::placeholders::_1, &carInfo));

        ros::Time::init();

        float d1= 0;
        float d2= 0;

        float avgSensR[3] = {0.0,0.0,0.0};
        float avgSensRight=1000;
        float distanceToWall= 6;
        float epsilon = 0.01;

        bool firstLoop = false;
        bool luecke=false;

        float breiteLuecke;

        double time1 = ros::Time::now().toSec();
        double time2 = ros::Time::now().toSec();
        double time3 = ros::Time::now().toSec();
        double time4 = ros::Time::now().toSec();

	    const float PI = 3.14159265;


        //---  Einparken  ---
        //Breite vom Auto
        float a=0.25;
        //von Hinterachse bis vorne
        float b=0.34;
        //Radabstand
        float radabstand=0.155;
        //maximaler Lenkwinkel
        float phi_lenk=24;
        //Radius des Autos bei max Einlenkung
        float R=(2*radabstand)/sin(phi_lenk);
        float h=sqrt(b*b-a*a+2*a*R);
        //luecke ca 70 cm
        float parkluecke =2*h-b;
        //Rotationsradius
        float R_0=(b*b)/(2*a);
        //Winkel bis Wendepunkt erreicht in RAD
        float theta=acos(1-(a/(R+R_0)))*180/PI;
        //Wendepunkt bereits erreicht?
        bool wendepunkt=false;

        float oldYaw;
        float currentYaw=0;
        //gesamtstrecke start und ende der parkluecke
        float startPL;
        float endPL = 0;
        //--------

        float factor;
        float speed;
        int idx = 0;
        bool schleife=false;
        int itr = 2;
        while(ros::ok()) {
            avgSensR[itr] = sensors.range_sensor_right;
            itr--;
            if(itr<0)itr=2;
            avgSensRight = calcAverage(avgSensR);
            if(sensors.range_sensor_front < 0.1f && sensors.range_sensor_front != 0){
                    cmd.motor_level = 0;
                } else {
                //cmd.motor_level = 5;

                        yawHandler(oldYaw,carInfo.yaw, currentYaw);


                        //Lückenerkennung
                        if (luecke==false){
                                if (schleife==false){
                                        cmd.motor_level = 5;
                                        speed = 0.2f;


                                        if (avgSensRight < distanceToWall - epsilon) {
                                                distanceToWall = avgSensRight;
                                        }
                                        if (avgSensRight > distanceToWall + epsilon) {
                                                schleife=true;
                                                startPL = carInfo.driven_distance;
                                        }
                                }else{
                                    if (schleife) {
                                            endPL = carInfo.driven_distance;
                                            if(endPL-startPL >= parkluecke ){
                                                    luecke = true;
                                                    currentYaw=0;
                                            }else if(avgSensRight < distanceToWall - epsilon){
                                                    schleife = false;
                                            }
                                    }
                                }
                        }

                        //Einparken
                        if(luecke){
                                cmd.motor_level=-5;

                                if ((wendepunkt==false)&&(fabs(currentYaw)<fabs(theta))){
                                        cmd.steering_level=50;
                                } else if ((wendepunkt==true)&&(fabs(currentYaw)>0)){
                                        cmd.steering_level=-50;
                                } else if ((wendepunkt==true)&&(fabs(currentYaw)<=0.01)){
                                        cmd.steering_level=0;
                                        cmd.motor_level=0;
                                }
                                if (fabs(currentYaw)>=fabs(theta)){
                                        wendepunkt=true;
                                }
                        }

                }

                        // compute control error
                        //    regc.control(sensors,cmd);
                        //tt.con(cmd);
                        //ROS_INFO( "PADDINGL; yaw: [%f]; yf_avg: [%f]; yr: [%f]; er: [%f]; DistFront: [%f]: ; Speed [%f]; steering: [%d]; yr_avg: [%f]", regc.yaw, regc.yf_avg,regc.yr, regc.er, sensors.range_sensor_front, carInfo.speed, cmd.steering_level, regc.yr_avg );

                        // publish
                        commands_pub.publish(cmd);
                        ros::spinOnce();
                        loop_rate.sleep();
        }
        ros::spin();
}

