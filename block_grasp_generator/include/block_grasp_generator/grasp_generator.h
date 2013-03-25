/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, University of Colorado, Boulder
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Univ of CO, Boulder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

// Author: Dave Coleman
//   Desc:   Generates grasps for a cube

// ROS
#include <ros/ros.h>
#include <tf/tf.h>
#include <tf_conversions/tf_eigen.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/PoseStamped.h>
#include <manipulation_msgs/Grasp.h>
#include <Eigen/Core>
#include <Eigen/Geometry>

// Rviz
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include "robot_viz_tools.h"

// MoveIt
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <moveit/robot_state/robot_state.h>
#include <moveit/kinematics_plugin_loader/kinematics_plugin_loader.h>

// C++
#include <boost/thread.hpp>
#include <math.h>
#define _USE_MATH_DEFINES

namespace block_grasp_generator
{

static const double RAD2DEG = 57.2957795;
static const double BLOCK_SIZE = 0.04;

// Struct for passing parameters to threads, for cleaner code
struct IkThreadStruct
{
  IkThreadStruct(std::vector<manipulation_msgs::Grasp> &possible_grasps, // the input
                 std::vector<manipulation_msgs::Grasp> &filtered_grasps, // the result
                 int grasps_id_start,
                 int grasps_id_end,
                 kinematics::KinematicsBasePtr kin_solver,
                 double timeout,
                 boost::mutex *lock,
                 int thread_id)
    : possible_grasps_(possible_grasps),
      filtered_grasps_(filtered_grasps),
      grasps_id_start_(grasps_id_start),
      grasps_id_end_(grasps_id_end),
      kin_solver_(kin_solver),
      timeout_(timeout),
      lock_(lock),
      thread_id_(thread_id)
  {
  }
  std::vector<manipulation_msgs::Grasp> &possible_grasps_;
  std::vector<manipulation_msgs::Grasp> &filtered_grasps_;
  int grasps_id_start_;
  int grasps_id_end_;
  kinematics::KinematicsBasePtr kin_solver_;
  double timeout_;
  boost::mutex *lock_;
  int thread_id_;
};


// Class
class GraspGenerator
{
private:
  // A shared node handle
  ros::NodeHandle nh_;

  // ROS publishers
  ros::Publisher collision_obj_pub_;

  // Shared planning scene monitor from parent object
  planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor_;

  // Parameters from goal
  std::string base_link_;

  // TF Frame Transform stuff
  tf::Transform transform_;

  // Grasp axis orientation
  enum grasp_axis_t {X_AXIS, Y_AXIS, Z_AXIS};
  enum grasp_direction_t {UP, DOWN};

  // threaded kinematic solvers
  std::vector<kinematics::KinematicsBasePtr> kin_solvers_;

  // whether to publish grasp info to rviz
  bool rviz_verbose_;

  // class for publishing stuff to rviz
  block_grasp_generator::RobotVizToolsPtr rviz_tools_;

  // Planning group
  std::string planning_group_;

public:

  // Constructor
  GraspGenerator( planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor,
                  std::string base_link, bool rviz_verbose, RobotVizToolsPtr rviz_tools, 
                  const std::string planning_group );

  // Destructor
  ~GraspGenerator();

  // Create all possible grasp positions for a block
  bool generateGrasps(const geometry_msgs::Pose& block_pose,
                      std::vector<manipulation_msgs::Grasp>& possible_grasps);

  // Of an array of grasps, choose just one for use
  bool chooseBestGrasp( const std::vector<manipulation_msgs::Grasp>& possible_grasps,
                        manipulation_msgs::Grasp& chosen );

  // Choose the 1st grasp that is kinematically feasible
  bool filterGrasps(std::vector<manipulation_msgs::Grasp>& possible_grasps);

private:

  // Create grasp positions in one axis
  bool generateAxisGrasps(std::vector<manipulation_msgs::Grasp>& possible_grasps, grasp_axis_t axis,
                          grasp_direction_t direction );

  // Take the nth grasp from the array
  bool filterNthGrasp(std::vector<manipulation_msgs::Grasp>& possible_grasps, int n);

  // Thread for checking part of the possible grasps list
  void filterGraspThread(IkThreadStruct ik_thread_struct);

  // Show all grasps in Rviz
  void visualizeGrasps(const std::vector<manipulation_msgs::Grasp>& possible_grasps,
                       const geometry_msgs::Pose& block_pose);

}; // end of class

typedef boost::shared_ptr<GraspGenerator> GraspGeneratorPtr;
typedef boost::shared_ptr<const GraspGenerator> GraspGeneratorConstPtr;

} // namespace

