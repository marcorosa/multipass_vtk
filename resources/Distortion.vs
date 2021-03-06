//PASSTHROUGH

#version 440
in vec3 vertex;
layout( location = 1 ) uniform mat4 modelview;
layout( location = 2 ) uniform mat4 projection;


void main()
{

//    gl_Position = vec4(vertex, 1);
//    mat4 mvp = mat4(1.0) * projection;
//    gl_Position = mvp * vec4(vertex, 1.0);

    gl_Position = projection * modelview * vec4(vertex, 1);
    
}
