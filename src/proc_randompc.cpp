// much more ideal would be if i could feed this a shading network.. so I don't have to code it all lol
   // maybe through the parameter evaluation api?
      // nope don't think so 

   // could i initialize a shaderglobals at a certain position?

   // maybe could write out the data in a volume shader that raymarches? could align rays with the grid? maybe in that way i can sample the shader at arbitrary points? AiVolumeSampleFltFunc(channel, sg, AI_VOLUME_INTERP_*, value), AI_CLOSURE_EMISSION??

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

  
AI_PROCEDURAL_NODE_EXPORT_METHODS(RandomFlakeMtd);

enum RandomFlakeParams {
   p_density,
   p_seed,
   p_Kd,
};

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
   
   if (!err.empty()) { // `err` may contain warning message.
      std::cerr << err << std::endl;
   }

   if (!ret) {
      return;
   }
}

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

      // // Loop over vertices in the face.
      // for (size_t v = 0; v < fv; v++) {
      //    // access to vertex
      //    tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
      //    tinyobj::real_t vx = attrib.vertices[3*idx.vertex_index+0];
      //    tinyobj::real_t vy = attrib.vertices[3*idx.vertex_index+1];
      //    tinyobj::real_t vz = attrib.vertices[3*idx.vertex_index+2];
      //    tinyobj::real_t nx = attrib.normals[3*idx.normal_index+0];
      //    tinyobj::real_t ny = attrib.normals[3*idx.normal_index+1];
      //    tinyobj::real_t nz = attrib.normals[3*idx.normal_index+2];
      //    tinyobj::real_t tx = attrib.texcoords[2*idx.texcoord_index+0];
      //    tinyobj::real_t ty = attrib.texcoords[2*idx.texcoord_index+1];
      // }

      AtVector dir(0, 1, 0);
      AtVector out(0,0,0);
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


float smin2(float a, float b, float r)
{
    float f = std::max(0., 1. - std::abs(b - a)/r);
    return std::min(a, b) - r*.25*f*f;
}


AtVector hash(AtVector x)
{
    AtVector a( AiV3Dot(x, AtVector(127.1, 311.7, 74.7)),
                AiV3Dot(x, AtVector(269.5, 183.3, 246.1)),
                AiV3Dot(x, AtVector(113.5, 271.9, 124.6)) 
   );
   
   AtVector sin_a;
   for (int i = 0; i < 3; i++) sin_a[i] = std::sin(a[i]) * 43758.5453123;

   AtVector sin_a_floored;
   for (int i = 0; i < 3; i++) sin_a_floored[i] = std::floor(sin_a[i]);

   return sin_a - sin_a_floored;
}


float voronoi_smoothed(AtVector x, float smoothing){

   AtVector floor_x;
   for (int i = 0; i < 3; i++) floor_x[i] = std::floor(x[i]);
   AtVector p = floor_x;
   AtVector f = x - floor_x;

   AtVector h;
   for (int i = 0; i < 3; i++) h[i] = AiStep(f[i], 0.5f) - 2.0;
   p += h; f -= h;
    
   AtVector minCellID(0.0, 0.0, 0.0);
   AtVector mo;
   
   float md = 8.0, lMd = 8.0, lnDist, d;
   
   for( int k=0; k<=3; k++ )
   for( int j=0; j<=3; j++ )
   for( int i=0; i<=3; i++ )
   {
      AtVector b((i), (j), (k));
      AtVector r = b - f + hash( p + b );
      
      d = AiV3Dot(r, r);
      if( d<md ){   
         md = d;
         mo = r; 
         //cellID = h + p; // For cell coloring.
         minCellID = b; // Record the minimum distance cell ID. // zeno: how should mincellid be used??
      }
   }

   for( int k=0; k<=3; k++ )
   for( int j=0; j<=3; j++ )
   for( int i=0; i<=3; i++ )
   {
      AtVector b((i), (j), (k));
      AtVector r = b - f + hash( p + b ) - mo;
      
      if(AiV3Dot(r, r)>.00001){
         lnDist = AiV3Dot(mo + r*.5, AiV3Normalize(r));
         lMd = smin2(lMd, lnDist, (lnDist*.5 + .5)*smoothing);
      }
   }

   return std::max(lMd, 0.0f);    
}


float remap(AtVector p, float smoothing, float ew){    
    float c = voronoi_smoothed(p, smoothing);
    
    if(c < ew){ 
        c = std::abs(c - ew)/ew; // Normalize the domain to a range of zero to one.
        c = AiSmoothStep(0.03f, 0.1f, c);
    }
    else { // Over the threshold? Use the regular Voronoi cell value.
        c = (c - ew)/(1. - ew); // Normalize the domain to a range of zero to one.
        c = AiSmoothStep(1.0f, 1.0f, c);
    }
    
    return c;
}



struct Points
{
   std::vector<AtVector> points;
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
   AiParameterFlt("density", 1.0f);
   AiParameterInt("seed", 0);
   AiParameterRGB("Kd", 0.7f,0.7f,0.7f);
}
  
procedural_init
{
   Points *flake = new Points();
   *user_ptr = flake;
  
   srand48(AiNodeGetInt(node, "seed"));
  
   flake->points.clear();
  
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
   Points *flake = (Points*)user_ptr;
   
   // bbox testing, swap this for max and min vertices found through loading obj
   std::vector<AtVector> bbox;
   AtVector bboxmin(-6.28, -0.034, -5.2707);
   AtVector bboxmax(6.279, 1.0784, 4.0926);
   bbox.push_back(bboxmin);
   bbox.push_back(bboxmax);

   
   
   std::string line;
   std::vector<AtRGB> valuelist;
   std::vector<AtVector> poslist;
   std::ifstream myfile ("/home/cactus/shadersxyz_challenge_bread/objs/bread_low_values.txt");
   if (myfile.is_open())
   {
      while ( getline (myfile,line) )
      {
         std::string str = line;
         std::string buf;
         std::stringstream ss(str);
         std::vector<std::string> tokens;
         while (ss >> buf) tokens.push_back(buf);
         
         std::cout << tokens[0] << " " << tokens[1] << " " << tokens[2] << " " << tokens[3] << " " << tokens[4] << " " << tokens[5] << std::endl;
         poslist.push_back(AtVector(std::stof(tokens[0]), std::stof(tokens[1]), std::stof(tokens[2])));
         valuelist.push_back( AtRGB(std::stof(tokens[3]), std::stof(tokens[4]), std::stof(tokens[5])));
      }
      myfile.close();
   }


   tinyobj::attrib_t attrib;
   std::vector<tinyobj::shape_t> shapes;
   std::vector<tinyobj::material_t> materials;
   std::string inputfile = "/home/cactus/shadersxyz_challenge_bread/objs/bread_low.obj";
   load_obj(inputfile, attrib, shapes, materials);

   create_points(flake, attrib, shapes, valuelist, poslist);

   AtArray *point_array = AiArrayConvert(flake->points.size(), 1, AI_TYPE_VECTOR, &flake->points[0]);
  
   // create node with procedural node as parent
   AtNode *points_node = AiNode("points", "flake", node);
   AiNodeSetArray(points_node, "points", point_array);
   AiNodeSetFlt(points_node, "radius", 0.01f);
   AiNodeSetStr(points_node, "mode", "sphere");
  
   return points_node;
}
  
node_loader
{
   if (i>0)
      return false;
  
   node->methods      = RandomFlakeMtd;
   node->output_type  = AI_TYPE_NONE;
   node->name         = "random_flake";
   node->node_type    = AI_NODE_SHAPE_PROCEDURAL;
   strcpy(node->version, AI_VERSION);
  
   return true;
}