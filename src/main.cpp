#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);
  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }
  
  // 1: middle lane, 0: left lane, 2: right lane(far away from the yellow line)
  int lane = 1; 
  double ref_vel = 0.0;
  double ref_acc = 0.224;
  double speed_limit = 49.5;
  
  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy, &lane, &ref_vel, &ref_acc, &speed_limit]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {            
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];     
          int prev_path_size = previous_path_x.size();
          json msgJson;
          
          if (prev_path_size > 0){
            car_s = end_path_s;
          }

          /* 
          Check the cars within the all lanes of ego car
          */
          bool car_ahead = false; //current lane
          bool car_ahead_at_left = false;
          bool car_ahead_at_right = false;
          
          for(int i = 0; i < sensor_fusion.size(); i++){
            //car is in my lane
            float d = sensor_fusion[i][6];
            
            
            double vx = sensor_fusion[i][3];
            double vy = sensor_fusion[i][4];
            double check_speed = sqrt(vx*vx + vy*vy);
            double check_car_s = sensor_fusion[i][5];
            
            /*
            Prediction - predict the furture position of the cars on the highway (not ego car)
            */
            check_car_s += (double)(prev_path_size * 0.02 * check_speed);
            
            // check the car in front of ego car at current lane
            if ( d < (2 + 4*lane + 2) && d > (2 + 4*lane -2) ){
              //the car is in front of us and the distance is less than 30 meters
              if( (check_car_s > car_s) && ((check_car_s - car_s) < 30) ){
                car_ahead = true;
                //maybe change lane
                //if (lane > 0){
                //  lane = 0;
                //}   
              }    
            }
            
            // check the car in front of ego car at current left lane
            if(d < (4 + 4*(lane - 1)) && d > (4 * (lane - 1))){
              auto s_diff = check_car_s - car_s;
              if ((s_diff > -10) && (s_diff < 40))
              {
                car_ahead_at_left = true;
              }
            }
            
            // check the car in front of ego car at current right lane
            if(d < (4 + 4*(lane + 1)) && d > (4 * (lane + 1)))
            {
              auto s_diff = check_car_s - car_s;
              if ((s_diff > -10) && (s_diff < 40))
              {
                car_ahead_at_right = true;
              }
            }
          }
          /*
          Behavior planning - make decision whether stay in the current lane or shift left or right
          */
          if(car_ahead){
            if(lane == 0){
              if(!car_ahead_at_right){
                lane = 1;
              }
              else{
                ref_vel -= ref_acc; 
              }
            }
            else if(lane == 1){
              if(!car_ahead_at_left){
                lane = 0;
              }
              else if(!car_ahead_at_right){
                lane = 2;
              }
              else{
                ref_vel -= ref_acc; 
              } 
            }
            else if(lane == 2){
              if (!car_ahead_at_left){
                lane = 1;
              }
              else{
                ref_vel -= ref_acc; 
              }
            }
          }
          else{ //always try to stay in the middle to keep flexibility to change to both lanes
            if (lane == 0 and !car_ahead_at_right){
               lane = 1;
            }
            else if( lane == 2 and !car_ahead_at_left){
               lane = 1;
            }
            
            if (ref_vel < speed_limit){
              ref_vel += ref_acc; 
            }
          }
          
          std::cout << "Lane: " << lane << " car_ahead: " << car_ahead << " car_ahead_at_left: " << car_ahead_at_left << " car_ahead_at_right: " << car_ahead_at_right  << std::endl;
          
          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
          
          /*
          Trajectory generation (Path Planner)
          1. Use the previous path data as the anchor points to come up with Spline function
          2. Based on the remaining previous path data, always come up with the trajectory plan with 50 points (1 sec)  
          */
          vector<double> ptsx;
          vector<double> ptsy;
          
          double ref_x = car_x;
          double ref_y = car_y;
          double ref_yaw = deg2rad(car_yaw);
          
          //if previous path size is almost empty, use the current car state as the starting reference (anchor points)
          if(prev_path_size < 2){
            double prev_car_x = car_x - cos(car_yaw);
            double prev_car_y = car_y - sin(car_yaw);
            
            ptsx.push_back(prev_car_x);
            ptsx.push_back(car_x);
            
            ptsy.push_back(prev_car_y);
            ptsy.push_back(car_y);
          }
          //use the previous path as the starting reference (anchor points)
          else{
            ref_x = previous_path_x[prev_path_size-1];
            ref_y = previous_path_y[prev_path_size-1];
            
            double ref_x_prev = previous_path_x[prev_path_size-2];
            double ref_y_prev = previous_path_y[prev_path_size-2];
            ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);
            
            ptsx.push_back(ref_x_prev);
            ptsx.push_back(ref_x);
            ptsy.push_back(ref_y_prev);
            ptsy.push_back(ref_y);            
          }
          
          //In Frenet coordinate, add evenly 30m spaced points after the starting reference
          vector<double> next_XY0 = getXY(car_s+30, 2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);          
          vector<double> next_XY1 = getXY(car_s+60, 2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);          
          vector<double> next_XY2 = getXY(car_s+90, 2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);          
          
          ptsx.push_back(next_XY0[0]);
          ptsx.push_back(next_XY1[0]);
          ptsx.push_back(next_XY2[0]);
          
          ptsy.push_back(next_XY0[1]);
          ptsy.push_back(next_XY1[1]);
          ptsy.push_back(next_XY2[1]);
          
          //Shift rotation to make math calculatiom much easier
          for (int i = 0; i < ptsx.size(); i++){
            double shift_x = ptsx[i] - ref_x;
            double shift_y = ptsy[i] - ref_y;
            
            //shift car reference angles to 0 degrees
            ptsx[i] = shift_x*cos(0 - ref_yaw) - shift_y*sin(0 - ref_yaw);
            ptsy[i] = shift_x*sin(0 - ref_yaw) + shift_y*cos(0 - ref_yaw);
          }
          
          tk::spline s;
          s.set_points(ptsx, ptsy);
          
          //define the actual (x, y) points for the path planner to use
          vector<double> next_x_vals;
          vector<double> next_y_vals;
          
          for (int i = 0; i < prev_path_size; i++){
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }
          
          //calculate how to break up Spline points so that we travel at our desired velovity
          double target_x = 30.0;
          double target_y = s(target_x);
          double target_dist = sqrt( (target_x*target_x) + (target_y*target_y) );
          
          double x_add_on = 0;
          
          // Fill up the rest of our path planner after filling it with previous points, here we always output 50 points     
          for (int i = 1; i <= 50 - previous_path_x.size(); i++){
            double N = (target_dist / (0.02*ref_vel/2.24));
            double x_point = x_add_on + (target_x)/N;
            double y_point = s(x_point);
            
            x_add_on = x_point;
            
            double x_ref = x_point;
            double y_ref = y_point;
            
            //rotate back to the global corodinates from local coordinates
            x_point = x_ref*cos(ref_yaw) - y_ref*sin(ref_yaw);
            y_point = x_ref*sin(ref_yaw) + y_ref*cos(ref_yaw);
            
            x_point += ref_x;
            y_point += ref_y;
            
            next_x_vals.push_back(x_point);
            next_y_vals.push_back(y_point);
          }
          
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}