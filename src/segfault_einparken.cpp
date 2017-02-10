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


class Regelung {
public:
/*Control variables:
   yr        := current distance to right wall
   wr        := desired distance to right wall
   ur        := motor cmd for right controller
   states    := states are yr and yaw
   Kopt      := matrix of optimal values for k1, k2
   prefilter := necessary for error->0 for t->Inf
 */
float yr = 0;
float yl = 0;
float yf = 0;
float yrOld = 0;
float ylOld = 0;
//float y[] = {0,0,0,0,0};
float yaw = 0;
float yawOld = 0;
float angvZ = 0;
float angvZ_old = 0;
float angvZ_diff;
float wr = 0.4;
float wl = 0.25;
float wphi = 0.0;
float ur = 0;
float er, el, erI, elI;
float erDiff = 0;
float elDiff = 0;
float erOld = 0;
float elOld = 0;
float urBoundCheck = 0;
int count = 0;
float speedZero = 0;
float yawOffset = 0;
//altes Kopt: [0.8165, 0,3604]
const float Ki = 0.008;
const float Kd = 0.9;
//float Kp = 1.4142;
static const float Kopt[];
//float prefilter = 2.1623;
int idx = 0;
float yr_avg = 0;
float yr_hist[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
float yf_avg;
float yf_hist[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
float er_avg = 0;
float er_hist[10] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
int firsttime = 1;
int testflag;


const float PI = 3.14159265;

static const float polyL4[];
static const float polyR4[];

//TODO:dt has to be dependent on loop_rate, fix that
float dt = 1.f/40.f;

Regelung();
void setNewDistance(float distance);
float degreeToSteering(const float* poly, float ur);
void sonsorInput();
void control(const pses_basis::SensorData &sensors, pses_basis::Command &cmd);
float calcAverage(int iteration, const float* array);
private:



};

Regelung::Regelung(void) {
        //std::cout << "Object is being created" << std::endl;
};

const float Regelung::polyL4[] = {0.0010, -0.0355, 0.3800, 0.3266, 0.0071};
const float Regelung::polyR4[]= {-0.0003, 0.0102, -0.1148, -1.0710, -0.0381};
const float Regelung::Kopt[] = {1.4142, 0.6098};

void Regelung::setNewDistance(float distance){
        wr = distance;
}

float Regelung::degreeToSteering(const float* poly, float ur){
        return poly[0]*pow(fabs(ur),4)+poly[1]*pow(fabs(ur),3)+poly[2]*pow(fabs(ur),2)+poly[3]*fabs(ur)+poly[4];
}

float Regelung::calcAverage(int iteration, const float* array){
        float avgSum= 0;
        for (int i = 0; i < iteration; i++) {
                avgSum += array[i];
        }
        return avgSum/(float)iteration;
}

void Regelung::control(const pses_basis::SensorData &sensors, pses_basis::Command &cmd){


        /* In case carInfo.yaw is used:
            TODO check for jump in sensor data after turn w/ angle > pi*/
        testflag = 0;
        if(testflag) {
                cmd.steering_level = 50;
        }

        cmd.motor_level = 10;

        // Read sensor values
        yr = sensors.range_sensor_right;
        yl = sensors.range_sensor_left;
        yf = sensors.range_sensor_front;
        // compute control error
        el = wl - yl;
        er = wr - yr;
        if (firsttime) {
                for (int i = 0; i < 10; i++) {
                        yf_hist[i] = yf;
                        yr_hist[i] = yr;
                        er_hist[i] = er;
                }
        }
        firsttime = 0;

        // yaw computation
        angvZ = sensors.angular_velocity_z;
        angvZ_diff = angvZ - angvZ_old;
        (angvZ_diff > 0.05)  ?   (yaw += angvZ*dt) : (yaw = yaw);
        //yaw = carInfo.yaw;
        yaw += yawOffset;

        // In case of past left turn

        //if( ((yaw - yawOffset) > PI/2.f) && (fabs(yr_avg - wr) < 0.5) ) yawOffset -= PI/2.f;
        // In case of past right turn
        //else if(((yaw - yawOffset) < -PI/2.f) && (fabs(yr_avg - wr) < 0.5)) yawOffset += PI/2.f;
        /*########################################################################
        ############# Computing Averages #########################################
        ########################################################################*/
        // Compute average of distance to wall over last 10 timesteps

        // Compute average of distance to wall over last 10 timesteps

        if (idx > 9) {
                idx= 0;
        }else{
                yr_hist[idx] = yr;
                yf_hist[idx] = yf;
                er_hist[idx] = er;
        }
        idx++;

        yr_avg = Regelung::calcAverage(10,yr_hist);
        // Compute average of front sensor output over the last ten timesteps
        yf_avg = Regelung::calcAverage(10, yf_hist);
        //Compute average control error
        er_avg = Regelung::calcAverage(10,er_hist);

        if(er_avg < 0.1) {
                yaw = 0;
        }

        erDiff = er - erOld;
        // (expression 1) ? expression 2 : expression 3

        (erDiff > 0.05) ? (erI += er*dt) : (erI = erI);
        // &&(sensors.range_sensor_front!=0)
        if( (yf_avg <= 0.15)) {
                erI = 0;
                elI = 0;
                yaw = 0;
                yrOld = 0;
                angvZ_old = 0;
                cmd.motor_level = 0;
                cmd.steering_level = 0;
        }                 //&&(sensors.range_sensor_front != 0)
        else if( (yf_avg < 0.85)  && (yl > yr) ) {
                //cmd.motor_level -= 0.5;
                ur = 50;
                cmd.steering_level = ur;
        }                 //&&(sensors.range_sensor_front != 0)
        else if( (yf_avg < 0.85)  && (yr > yl) ) {
                //cmd.motor_level -= 0.5;
                ur = -50;
                //yawOffset -= PI/2.f;
                cmd.steering_level = ur;
        }
        // Calibration event: cover front sensor
        else{
                //TODO: Yaw completely useless after disturbances, how to fix?
                //motor command ur in rad
                //altes Regelgesetz
                ur = atan(Kopt[0]*er + Kopt[1]*0*(wphi-yaw) + Ki*erI + Kd*erDiff/dt);
                //convert ur to deg
                ur = ur * (180.f/PI);
                //steer right
                if( ur < 0 ) {
                        urBoundCheck = Regelung::degreeToSteering(polyR4,ur);
                        if(urBoundCheck <  -50) {
                                cmd.steering_level =  -50;
                        }else{
                                cmd.steering_level = urBoundCheck;
                        }
                }
                //steer left
                else if( ur > 0 ) {
                        urBoundCheck = Regelung::degreeToSteering(polyL4,ur);
                        if(urBoundCheck >  50) {
                                cmd.steering_level =  50;
                        }else{
                                cmd.steering_level = urBoundCheck;
                        }
                }
        }

        //  ROS_INFO( "PADDINGL; yaw: [%f]; yf_avg: [%f]; yr: [%f]; er: [%f]; DistFront: [%f]: ; Speed [%f]; steering_level: [%d]; yr_avg: [%f]", yaw, yf_avg,yr, er, sensors.range_sensor_front, carInfo.speed, cmd.steering_level, yr_avg );

        yrOld = yr;
        erOld = er;
        yawOld = yaw;
        angvZ_old = angvZ;
}


//yaw aus Car-info
void odometryCallback(const nav_msgs::Odometry::ConstPtr& msg, nav_msgs::Odometry *odom){
        *odom = *msg;
}

void sensorCallback(const pses_basis::SensorData::ConstPtr& sens_msg, pses_basis::SensorData *sensors){
        *sensors = *sens_msg;
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

        /*Control variables:
           yr        := current distance to right wall
           wr        := desired distance to right wall
           ur        := motor cmd for right controller
           states    := states are yr and yaw
           Kopt      := matrix of optimal values for k1, k2
           prefilter := necessary for error->0 for t->Inf
         */

        //Pubs and Subs
        ros::Publisher commands_pub = nctrl.advertise<pses_basis::Command>("pses_basis/command", 10);
        ros::Subscriber odom_sub = nctrl.subscribe<nav_msgs::Odometry>("odom",50, std::bind(odometryCallback, std::placeholders::_1, &odom));
	//TODO recently changed
        ros::Subscriber sensor_sub = nctrl.subscribe<pses_basis::SensorData>("pses_basis/sensor_data", 10, std::bind(sensorCallback, std::placeholders::_1, &sensors));
        ros::Subscriber carInfo_sub = nctrl.subscribe<pses_basis::CarInfo>("pses_basis/car_info",10, std::bind(carInfoCallback, std::placeholders::_1, &carInfo));
        //ros::Subscriber camera_info = nctrl.subscribe<pses_

        ros::Time::init();
        Regelung regc;
        //regc.setNewDistance(0.25f);

        float d1= 0;
        float d2= 0;

        float avgSensR[3] = {0.0,0.0,0.0};
        float avgSensRight=0;
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
//--------

        float factor;
        float speed;
        int idx = 0;
        bool schleife=false;

        while(ros::ok()) {

		if(sensors.range_sensor_front < 0.1f && sensors.range_sensor_front != 0){
			cmd.motor_level = 0;
} else {
		//cmd.motor_level = 5;		

                yawHandler(oldYaw,carInfo.yaw, currentYaw);

                if (idx > 3) {
                        idx= 0;
                }else{
                        avgSensR[idx] =  sensors.range_sensor_right;
                }
                idx++;
                avgSensRight = regc.calcAverage(3,avgSensR);

//Lückenerkennung
if (luecke==false){
                if (schleife==false){
                cmd.motor_level = 5;
                speed = 0.2f;


                if (avgSensRight < distanceToWall + epsilon) {
                        distanceToWall = avgSensRight;
                }
                  if (avgSensRight > distanceToWall + epsilon) {
                    schleife=true;
                  }
}else{
                if (avgSensRight > distanceToWall + epsilon) {

                        if ((schleife)&&(time2-time1 > parkluecke/speed)) {
                                if(firstLoop) {
                                        breiteLuecke = avgSensRight;
                                        time1 = ros::Time::now().toSec();
                                }

                                time2= ros::Time::now().toSec();

                                if(avgSensRight <= distanceToWall+epsilon) {
                                        firstLoop= false;
                                        break;
                                }
                        }
                        if (time2-time1 >= parkluecke/speed) {
                                luecke = true;
                                currentYaw=0;
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
      } else if ((wendepunkt==true)&&(fabs(currentYaw)<=0)){
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
                ROS_INFO( "PADDINGL; yaw: [%f]; yf_avg: [%f]; yr: [%f]; er: [%f]; DistFront: [%f]: ; Speed [%f]; steering: [%d]; yr_avg: [%f]", regc.yaw, regc.yf_avg,regc.yr, regc.er, sensors.range_sensor_front, carInfo.speed, cmd.steering_level, regc.yr_avg );

                // publish
                commands_pub.publish(cmd);
                ros::spinOnce();
                loop_rate.sleep();
        }
        ros::spin();
}
/*
   PsesHeader header
   float32 accelerometer_x
   float32 accelerometer_y
   float32 accelerometer_z
   float32 angurar_velocity_x
   float32 angurar_velocity_y
   float32 angurar_velocity_z
   float32 hall_sensor_dt
   float32 hall_sensor_dt_furl
   uint32 hall_sensor_count
   float32 range_sensor_front
   float32 range_sensor_left
   float32 range_sensor_right
   float32 system_battery_voltage
   float32 motor_battery_voltage
 */