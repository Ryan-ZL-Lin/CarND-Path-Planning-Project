[//]: # (Image References)

[image1]: Driving_Records.png "result"

# Model Documentation
This file shows the detail of path generation.

## Trajectory generation (Path Planner) 
Starts from main.cpp line 209

1. Use the previous path data as the anchor points to come up with Spline function
    ptsx and ptsx are the anchor points that generated from previous path. Depneds on "previous path size", it mat take the current car position as part of it or directly take the last 2 points from previoud path.
2. In Frenet coordinate, add evenly 30m spaced points (3 points exactly) into anchor points.
3. Use the anchor points (5 points for each x and y) to come up with Spline function.
4. Based on the remaining previous path data, always come up with the trajectory plan with 50 points (1 sec) by using spline.
5. During the process of generating 50 points, the data transoformation between global corodinates and local coordinates makes the math calculation easier.
6. Once 50 points are calculated, send it to websocket package to simualator.


## Behavior planning - make decision whether stay in the current lane or shift left or right
Starts from main.cpp line 157

1. Identify whether there is a car ahead or behind in current lane, current left lane and current right lane (from sensor fusion).
2. Set up the parameters of safety distance (i.e. 40 meters for the car ahead, 10 meters of the car behind)
3. Come up with the state machine to make the decision in each states. 
4. To make the car have more flexibility to change lanes, the program tries its best to make the ego car always stay in the middle lane.


## Simulator driving result
1. 16.90 miles without any incident
2. Speed and jerkiness limit are not reached in the whole driving period. 
3. No collision. 

![Result][image1]
