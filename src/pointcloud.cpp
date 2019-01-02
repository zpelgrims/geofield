// arnold procedural which reads in a pointcloud and renders it as spheres

#include <ai.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>
#include <iterator>


#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include "tiny_obj_loader.h"

  
AI_PROCEDURAL_NODE_EXPORT_METHODS(PointCloudMtd);


bool intersectTriangle (AtVector &out, AtVector pt, AtVector dir, std::vector<AtVector> tri) {
    AtVector edge1 = tri[1] - tri[0];
    AtVector edge2 = tri[2] - tri[0];
    
    AtVector pvec = AiV3Cross(dir, edge2);
    float det = AiV3Dot(edge1, pvec);
    
    if (det < 0.00001) return false;
    AtVector tvec = pt - tri[0];
    float u = AiV3Dot(tvec, pvec);
    if (u < 0 || u > det) return false;
    AtVector qvec = AiV3Cross(tvec, edge1);
    float v = AiV3Dot(dir, qvec);
    if (v < 0 || u + v > det) return false;
    
    float t = AiV3Dot(edge2, qvec) / det;
    out[0] = pt[0] + t * dir[0];
    out[1] = pt[1] + t * dir[1];
    out[2] = pt[2] + t * dir[2];
    return true;
}


void load_obj(std::string inputfile, 
              tinyobj::attrib_t &attrib, 
              std::vector<tinyobj::shape_t> &shapes, 
              std::vector<tinyobj::material_t> &materials){

   std::string warn;
   std::string err;
   bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str());
   
   if (!err.empty()) std::cerr << err << std::endl;
   if (!ret) return;
}


// from point, cast ray and count intersections. If even number, point is outside the mesh.
// maybe? could use arnold api for this, cast a ray with a traceset?
bool is_in_mesh(const AtVector point, const tinyobj::attrib_t attrib, const std::vector<tinyobj::shape_t> shapes){
   
   int intersect_cnt = 0;

   // Loop over shapes
   //for (size_t s = 0; s < shapes.size(); s++) {
   // Loop over faces(polygon)
   size_t index_offset = 0;
   for (size_t f = 0; f < shapes[0].mesh.num_face_vertices.size(); f++) {
      int fv = shapes[0].mesh.num_face_vertices[f];

      tinyobj::index_t idx;
      idx = shapes[0].mesh.indices[index_offset + 0];
      AtVector v1(attrib.vertices[3*idx.vertex_index+0],
                  attrib.vertices[3*idx.vertex_index+1],
                  attrib.vertices[3*idx.vertex_index+2]);
      idx = shapes[0].mesh.indices[index_offset + 1];
      AtVector v2(attrib.vertices[3*idx.vertex_index+0],
                  attrib.vertices[3*idx.vertex_index+1],
                  attrib.vertices[3*idx.vertex_index+2]);
      idx = shapes[0].mesh.indices[index_offset + 2];
      AtVector v3(attrib.vertices[3*idx.vertex_index+0],
                  attrib.vertices[3*idx.vertex_index+1],
                  attrib.vertices[3*idx.vertex_index+2]);

      AtVector dir(0, 1, 0);
      AtVector out(0, 0, 0);
      std::vector<AtVector> vertex = {v1, v2, v3};
      if (intersectTriangle(out, point, dir, vertex)){
         ++intersect_cnt;
      }
      
      index_offset += fv;
   }
   //}

   if (intersect_cnt % 2 == 0){
      return false;
   } else {
      return true;
   }
}


struct Points {
   std::vector<AtVector> points;
   std::string file;
   std::string geo;
   float point_radius;
};

enum PointCloudParams {
   p_point_radius,
   p_seed,
   p_pointcloud,
   p_geo
};


static void create_points(Points *points, const tinyobj::attrib_t attrib, const std::vector<tinyobj::shape_t> shapes, const std::vector<AtRGB> valuelist, const std::vector<AtVector> poslist) {
   for (int i = 0; i < poslist.size(); i++){
      if (!is_in_mesh(poslist[i], attrib, shapes)){
         continue;
      }

      if(i < valuelist.size()) {
         if(valuelist[i].r > 0.01) {
            points->points.push_back(poslist[i]);
         }
      }      
   }
}


node_parameters
{
   AiParameterFlt("point_radius", 0.01f);
   AiParameterInt("seed", 0);
   AiParameterStr("pointcloud", "/home/cactus/shadersxyz_challenge_bread/objs/bread_low_values.txt");
   AiParameterStr("geo", "/home/cactus/shadersxyz_challenge_bread/objs/bread_low.obj");
}
  
procedural_init
{
   Points *flake = new Points();
   *user_ptr = flake;
  
   srand48(AiNodeGetInt(node, "seed"));
  
   return true;
}
  
procedural_cleanup
{
   Points *flake = (Points*)user_ptr;
   delete flake;

   return true;
}
  
procedural_num_nodes
{
   return 1;
}

  
procedural_get_node
{
   Points *pointcloud = (Points*)user_ptr;

   pointcloud->points.clear();
   pointcloud->file = AiNodeGetStr(node, "pointcloud"); // should i be using parameval?
   pointcloud->point_radius = AiNodeGetFlt(node, "point_radius"); // should i be using parameval?
   pointcloud->geometry = AiNodeGetStr(node, "geo");
   
   // load pointcloud from file
   std::string line;
   std::vector<AtRGB> valuelist;
   std::vector<AtVector> poslist;
   std::ifstream myfile (pointcloud->file);
   if (myfile.is_open()) {
      while ( getline (myfile,line) ) {
         std::string str = line;
         std::string buf;
         std::stringstream ss(str);
         std::vector<std::string> tokens;
         while (ss >> buf) tokens.push_back(buf);
         
         // std::cout << tokens[0] << " " << tokens[1] << " " << tokens[2] << " " << tokens[3] << " " << tokens[4] << " " << tokens[5] << std::endl;
         poslist.push_back(AtVector(std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2])));
         valuelist.push_back( AtRGB(std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5])));
      }

      myfile.close();
   }

   // load geo from file (triangulated obj)
   tinyobj::attrib_t attrib;
   std::vector<tinyobj::shape_t> shapes;
   std::vector<tinyobj::material_t> materials;
   load_obj(pointcloud->geometry, attrib, shapes, materials);


   // create points
   create_points(pointcloud, attrib, shapes, valuelist, poslist);
   AtArray *point_array = AiArrayConvert(pointcloud->points.size(), 1, AI_TYPE_VECTOR, &pointcloud->points[0]);
  

   // create node with procedural node as parent
   AtNode *points_node = AiNode("points", "flake", node);
   AiNodeSetArray(points_node, "points", point_array);
   AiNodeSetFlt(points_node, "radius", pointcloud->point_radius);
   AiNodeSetStr(points_node, "mode", "sphere");
  
   return points_node;
}


node_loader
{
   if (i>0) return false;
   node->methods      = PointCloudMtd;
   node->output_type  = AI_TYPE_NONE;
   node->name         = "pointcloud";
   node->node_type    = AI_NODE_SHAPE_PROCEDURAL;
   strcpy(node->version, AI_VERSION);
   return true;
}