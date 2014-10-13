#version 420 core                          
#extension GL_NV_gpu_shader5 : enable 
                                                   
///////////////////////////////////////////////////////////////////////////////
// input
///////////////////////////////////////////////////////////////////////////////
layout (location = 0) in vec3  position;   
layout (location = 1) in uint  index;      
layout (location = 2) in vec4  tesscoord;  

///////////////////////////////////////////////////////////////////////////////                                         
// output
///////////////////////////////////////////////////////////////////////////////                      
flat out vec3  vPosition;                  
flat out uint  vIndex;                    
flat out vec2  vTessCoord;                 
            
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////                                                   
void main()                                
{                                          
  vPosition  = position;                   
  vIndex     = index;                      
  vTessCoord = tesscoord.xy;               
} 