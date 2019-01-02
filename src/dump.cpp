// this will have to be executed on a single thread!?

// hacky pass-through shader which evaluates the input shading network in a gridlike fashion.
// this allows me to write out e.g noise fields into a point cloud.

#include <ai.h>
#include <iostream>
#include <fstream>
#include <vector>
 
AI_SHADER_NODE_EXPORT_METHODS(DumpMethods);
 
enum DumpParams  { 
    p_emission,
    p_bbox_min,
    p_bbox_max,
    p_gridsize,
    p_outfile
};

struct Grid {
    std::vector<AtVector> gridpos;
    std::vector<AtRGB> gridvalues;
    float gridsize;
    int counter;
    std::string outfile;
};
 

node_parameters {
   AiParameterRGB("emission", 0.7f, 0.7f, 0.7f);
   AiParameterVec("bbox_min", 0.0f, 0.0f, 0.0f);
   AiParameterVec("bbox_max", 1.0f, 1.0f, 1.0f);
   AiParameterFlt("gridsize", 0.05f);
   AiParameterStr("outfile", "/home/cactus/shadersxyz_challenge_bread/objs/bread_low_values.txt");
}


node_initialize {
    AiNodeSetLocalData(node, new Grid());
}


node_update {
    Grid *grid = (Grid*)AiNodeGetLocalData(node);

    grid->gridpos.clear();
    grid->gridvalues.clear();
    grid->counter = 0;
    grid->outfile = AiNodeGetStr(node, "outfile");

    AtVector bbox_min = AiNodeGetVec(node, "bbox_min");
    AtVector bbox_max = AiNodeGetVec(node, "bbox_max");
    grid->gridsize = AiNodeGetFlt(node, "gridsize");

    int steps_x = (int)((bbox_max.x - bbox_min.x) / grid->gridsize);
    int steps_y = (int)((bbox_max.y - bbox_min.y) / grid->gridsize);
    int steps_z = (int)((bbox_max.z - bbox_min.z) / grid->gridsize);

    std::cout << "steps_x: " << steps_x << std::endl;
    std::cout << "steps_y: " << steps_y << std::endl;
    std::cout << "steps_z: " << steps_z << std::endl;

    for (int x = 0; x <= steps_x; x++){
    for (int y = 0; y <= steps_y; y++){
    for (int z = 0; z <= steps_z; z++){
        AtVector pos(bbox_min.x + x*grid->gridsize,
                     bbox_min.y + y*grid->gridsize,
                     bbox_min.z + z*grid->gridsize);

        grid->gridpos.push_back(pos);
    }
    }
    }
}


node_finish {
    Grid *grid = (Grid*)AiNodeGetLocalData(node);

    // write out the data
    std::ofstream myfile;
    myfile.open (grid->outfile);

    for (int i = 0; i < grid->gridpos.size(); i++){
        myfile << grid->gridpos[i].x << " "
               << grid->gridpos[i].y << " "
               << grid->gridpos[i].z << " "
               << grid->gridvalues[i].r << " "
               << grid->gridvalues[i].g << " "
               << grid->gridvalues[i].b << std::endl;
    }

    myfile.close();
}


shader_evaluate {
    Grid *grid = (Grid*)AiNodeGetLocalData(node);

    if (grid->counter < grid->gridpos.size()){
        
        AtShaderGlobals *sg2 = AiShaderGlobals();
        sg2->P = grid->gridpos[grid->counter];
        sg2->Po = grid->gridpos[grid->counter];
        grid->gridvalues.push_back(AtRGB(AiShaderEvalParamFuncFlt(sg2, node, p_emission)));

        ++grid->counter;
    }

}

 
node_loader
{
   if (i > 0) return false;
   node->methods     = DumpMethods;
   node->output_type = AI_TYPE_RGB;
   node->name        = "dump";
   node->node_type   = AI_NODE_SHADER;
   strcpy(node->version, AI_VERSION);
   return true;
}