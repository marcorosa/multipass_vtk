#include "vtkRiftRenderPass.h"
#include "vtkObjectFactory.h"
#include <assert.h>
#include "vtkRenderState.h"
#include "vtkRenderer.h"
#include "vtkgl.h"
#include "vtkFrameBufferObject.h"
#include "vtkTextureObject.h"
#include "vtkShaderProgram2.h"
#include "vtkShader2.h"
#include "vtkShader2Collection.h"
#include "vtkUniformVariables.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkTextureUnitManager.h"

// to be able to dump intermediate passes into png files for debugging.
// only for vtkRiftRenderPass developers.
//#define VTK_GAUSSIAN_BLUR_PASS_DEBUG

#include "vtkPNGWriter.h"
#include "vtkImageImport.h"
#include "vtkPixelBufferObject.h"
#include "vtkPixelBufferObject.h"
#include "vtkImageExtractComponents.h"
#include "vtkCamera.h"
#include "vtkMath.h"

bool verbose = false;

void check_uniforms(bool enabled){
    // if(enabled){  
    // 	// check that uniform locations were set
    // 	for(int i = 0; i != 4; i++){
    // 	    if(-1 == texShaded->input[i]){
    // 		std::cout << "Failed to set uniform location for input[" 
    // 			  << i << "]. "<< std::endl;
    // 		exit(1);
    // 	    }
    // 	}
    // }
    return;
}

void checkerror(){
    GLenum err;
    if ((err = glGetError()) != GL_NO_ERROR) {
        cerr << "OpenGL error: " << err << endl;
	
	std::string error;
	switch(err) {
	case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
	case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
	case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
	case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
	}
 
	std::cout << "GL_" << error.c_str() << std::endl;
	exit(1);
    }
    return;
}

void printMatrix(float matrix[16], std::string name = "matrix"){
    std::cout << std::endl;
    std::cout << name;
    for(int i = 0; i != 16; i++){
	if(0 == (i%4) ) std::cout << std::endl;
	std::cout << matrix[i] << " ";
    }
    std::cout << std::endl;
}

vtkCxxRevisionMacro(vtkRiftRenderPass, "$Revision: 1.9 $");
vtkStandardNewMacro(vtkRiftRenderPass);

#include <stdlib.h>

using namespace std;
static char* readFile(const char *fileName) {
    char* text;

    if (fileName != NULL) {
	FILE *file = fopen(fileName, "rt");

	if (file != NULL) {
	    fseek(file, 0, SEEK_END);
	    int count = ftell(file);
	    rewind(file);

	    if (count > 0) {
		text = (char*)malloc(sizeof(char) * (count + 1));
		count = fread(text, sizeof(char), count, file);
		text[count] = '\0';
	    }
	    fclose(file);
	}
    }
    return text;
}

// ----------------------------------------------------------------------------
vtkRiftRenderPass::vtkRiftRenderPass()
{
    this->Supported=false;
    this->SupportProbed=false;
    this->isInit = false;
    computeAverages = false;
    actualWeights = new float[3];

}

// ----------------------------------------------------------------------------
vtkRiftRenderPass::~vtkRiftRenderPass(){}

void vtkRiftRenderPass::Resize(){


    int temp_m_width  = m_width;
    int temp_m_height = m_height;

    m_width = m_height = 10;

//    cout << "m_old_width was" << m_old_width << "m_width was" << m_width << endl;

    createAuxiliaryTexture(
	texRender, GENERATE_FBO, true );
    //createAuxiliaryTexture(
//	texShaded, GENERATE_FBO, true);

    glFlush();

    m_width  = temp_m_width;
    m_height = temp_m_height;

    createAuxiliaryTexture(
	texRender, /*GENERATE_MIPMAPS | INTERPOLATED |*/ GENERATE_FBO, true );
    // createAuxiliaryTexture(
//	texShaded, GENERATE_MIPMAPS | INTERPOLATED | GENERATE_FBO, true);

}

// ----------------------------------------------------------------------------
void vtkRiftRenderPass::init()
{
    if(this->isInit)
	return;

    const GLenum err = glewInit();
    if (GLEW_OK != err)
    {
	// Problem: glewInit failed, something is seriously wrong.
	printf("BufferedQmitkRenderWindow Error: glewInit failed with %s\n", glewGetErrorString(err));
    }

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    createAuxiliaryTexture(
	texRender, GENERATE_MIPMAPS | INTERPOLATED | GENERATE_FBO );
    createAuxiliaryTexture(
	texShaded, GENERATE_MIPMAPS | INTERPOLATED | GENERATE_FBO );

    // Gotta read and compile in order to set up the uniforms!
    texShaded->shader = shaderManager.loadfromFile((char*) "Distortion.vs", (char*) "Distortion.fs");

    texShaded->input[0]= glGetUniformLocation(
	texShaded->shader->GetProgramObject(),"Texture0");
    texShaded->input[1]= glGetUniformLocation(
	texShaded->shader->GetProgramObject(),"modelview");
    texShaded->input[2]= glGetUniformLocation(
	texShaded->shader->GetProgramObject(),"projection");
    texShaded->input[3]= glGetUniformLocation(
	texShaded->shader->GetProgramObject(),"offset");

    check_uniforms(false);

    //texShaded->shader= shaderManager.loadfromFile("simple.vs", "simple.fs");
    //texShaded->shader= shaderManager.loadfromMemory(0, "void main(void){ gl_FragColor = vec4(1,0,0,1);}");
  
    if (0 == texShaded->shader){
	std::cout << glGetString(GL_VERSION) << std::endl;
	std::cout << "Error Loading, compiling or linking shader" << std::endl;
	std::cout << "and that you're RUNNING in the right dir" << std::endl;
	exit(1);
    }
    FramebufferObject::Disable();

    this->isInit = true;

}


// ----------------------------------------------------------------------------
void vtkRiftRenderPass::PrintSelf(ostream& os, vtkIndent indent)
{
    this->Superclass::PrintSelf(os,indent);
}

void vtkRiftRenderPass::showRiftRender(const vtkRenderState *s)
{

    assert("pre: s_exists" && s!=0);

    int size[2];
    s->GetWindowSize(size);
    // this is the same as the viewport size (and I'm not exactly sure why...)

    this->NumberOfRenderedProps=0;

    vtkRenderer *r=s->GetRenderer();
    r->GetTiledSizeAndOrigin(
	&this->ViewportWidth,
	&this->ViewportHeight,
	&this->ViewportX,
	&this->ViewportY);

    if(verbose){
	GLint m_viewport[4];
	glGetIntegerv( GL_VIEWPORT, m_viewport );
	cout << "Screen size is " 
	     << m_viewport[0] << ", "  
	     << m_viewport[1] << ", "  
	     << m_viewport[2] << ", "  
	     << m_viewport[3] << ", "  << endl;


	cout << "  w: " << size[0] 
	     << ", h: " << size[1] 
	     << ", vw:" << this->ViewportWidth 
	     << ", vh:" << this->ViewportHeight
	     << ", vx:" << this->ViewportX
	     << ", vy:" << this->ViewportY
	     << endl;
    }
  
    if(this->DelegatePass!=0)
    {

	int width;
	int height;
	int size[2];
	s->GetWindowSize(size);
	width  = size[0];
	height = size[1];
	int w = width;
	int h = height;
	m_height = h;
	m_width = w;
	init();
        
	if(w != m_old_width && h != m_old_height )
	{
	    createAuxiliaryTexture(
		texRender, GENERATE_MIPMAPS | INTERPOLATED | GENERATE_FBO );
	    createAuxiliaryTexture(
		texShaded, GENERATE_MIPMAPS | INTERPOLATED | GENERATE_FBO, true);

	    texShaded->input[0]= glGetUniformLocation(
		texShaded->shader->GetProgramObject(),"Texture0");
	    texShaded->input[1]= glGetUniformLocation(
		texShaded->shader->GetProgramObject(),"modelview");
	    texShaded->input[2]= glGetUniformLocation(
		texShaded->shader->GetProgramObject(),"projection");
	    texShaded->input[3]= glGetUniformLocation(
		texShaded->shader->GetProgramObject(),"offset");

	    int texwidth;
	    glGetTexLevelParameteriv(
		GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texwidth);
	    cout << "GL_TEXTURE_2D Width: " << texwidth << endl;

	    // check that uniform locations are *still* okay (caught a bug with this)
	    check_uniforms(false);

	    FramebufferObject::Disable();
	    m_old_width = w;
	    m_old_height = h;
	}
///////////////////////////////////////////////////////////////////////////////////
// Test
///////////////////////////////////////////////////////////////////////////////////

	// glEnable(GL_DEPTH_TEST); // clears the depth buffer, will draw anything closer
	// FramebufferObject::Disable();
	// this->DelegatePass->Render(s);


////////////////////////////////////////////////////////////////////////////////////
// Custom rendering Shiz
///////////////////////////////////////////////////////////////////////////////////

	// // may need to clear the buffer
	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D,  texRender->id);
	// glScissor(ViewportX, ViewportY, ViewportWidth, ViewportHeight);
	// glEnable(GL_SCISSOR_TEST);
	// glClearColor(1,0,0,1);
	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// glDisable(GL_SCISSOR_TEST);
	// glBindTexture(GL_TEXTURE_2D, 0);

	//render to my fbo 
	texRender->fbo->Bind();

	glEnable(GL_DEPTH_TEST);
	this->DelegatePass->Render(s); // render scene to texRender texture
	glDisable(GL_DEPTH_TEST);

	FramebufferObject::Disable();

	// get matrices ready for vertex shader
	glMatrixMode(GL_MODELVIEW);  // switch to MODELVIEW
	glPushMatrix();              // push current to MODELVIEW stack
	glLoadIdentity();            // load something innocuous
	GLfloat modelview[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, modelview); 

	glMatrixMode(GL_PROJECTION); // switch to PROJECTION
	glPushMatrix();              // push current to PROJECTION stack
	glLoadIdentity();            // load something innocuous
	GLfloat projection[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection); 

	GLfloat offset[2] = {(GLfloat)ViewportX, (GLfloat) ViewportY};

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,  texRender->id);

	texShaded->shader->begin();    // does program->use, enables uniforms

	// set all uniforms
	glUniform1i(texShaded->input[0], 0); // and pass that through as a uniform
	glUniformMatrix4fv(texShaded->input[1], 1, GL_FALSE, modelview);
	glUniformMatrix4fv(texShaded->input[2], 1, GL_FALSE, projection);
	glUniform2fv(texShaded->input[3], 1, offset);

	glScissor(ViewportX, ViewportY, ViewportWidth, ViewportHeight);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_DEPTH_TEST);

	texRender->drawQuad();

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);

	// clean up matrix stack
	glMatrixMode(GL_PROJECTION);  glPopMatrix();                
	glMatrixMode(GL_MODELVIEW);   glPopMatrix();
	
	texShaded->shader->end();
    
////////////////////////////////////////////////////////////////////////////////

    }
    else
    {
	vtkWarningMacro(<<" no delegate.");
    }
}

// ----------------------------------------------------------------------------
// Description:
// Perform rendering according to a render state \p s.
// \pre s_exists: s!=0
void vtkRiftRenderPass::Render(const vtkRenderState *s)
{
    showRiftRender(s);
}

// ----------------------------------------------------------------------------
// Description:
// Release graphics resources and ask components to release their own
// resources.
// \pre w_exists: w!=0
void vtkRiftRenderPass::ReleaseGraphicsResources(vtkWindow *w)
{
    assert("pre: w_exists" && w!=0);

    this->Superclass::ReleaseGraphicsResources(w);

}

void vtkRiftRenderPass::createAuxiliaryTexture(TextureInfo *&texCurrent, unsigned char flags, bool resize)
{
    if(resize)
	glDeleteTextures(1,&texCurrent->id);

    if(!resize)
	texCurrent = new TextureInfo; // TODO: match this with a delete
    
    // glActiveTexture(GL_TEXTURE0 + 1);

    texCurrent->imgWidth = m_width *2;//+ ViewportX;
    texCurrent->imgHeight = m_height + ViewportY;
    texCurrent->texWidth = m_width *2;//+ ViewportX;
    texCurrent->texHeight = m_height + ViewportY;

    if(verbose){
    cout << " m_width:  " << m_width
	 << " m_height: " << m_height
	 << " texW:  " << texCurrent->texWidth
	 << " texH:  " << texCurrent->texHeight
	 << " imgW:  " << texCurrent->imgWidth
	 << " imgH:  " << texCurrent->imgHeight
	 << endl;
    }

    texCurrent->format = GL_RGB;
    texCurrent->internalFormat = GL_RGB32F_ARB;
    texCurrent->u0 = (0);
    texCurrent->u1 = (texCurrent->imgWidth / (float)texCurrent->texWidth);

    if (flags&FLIP_Y)
    {
	texCurrent->v1 = (texCurrent->imgHeight / (float)texCurrent->texHeight);
	texCurrent->v0 = (0);
    }
    else
    {
	texCurrent->v0 = (texCurrent->imgHeight / (float)texCurrent->texHeight);
	texCurrent->v1 = (0);
    }

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texCurrent->id);
    glBindTexture(GL_TEXTURE_2D, texCurrent->id);
    glTexImage2D(GL_TEXTURE_2D, 0, texCurrent->internalFormat, texCurrent->texWidth, texCurrent->texHeight, 0,  texCurrent->format, GL_FLOAT, NULL);
    if (flags&GENERATE_MIPMAPS)
    {
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glGenerateMipmapEXT(GL_TEXTURE_2D);
    }
    else if (flags&INTERPOLATED)
    {
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }
    else 
    {
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    if (flags&GENERATE_FBO)
    {
//	if(!resize)
	texCurrent->fbo = new FramebufferObject; // TODO: match this with a delete
	texCurrent->rbo = new Renderbuffer; // TODO: match this with a delete
	texCurrent->rbo->Set( 
	    GL_DEPTH_COMPONENT24, texCurrent->texWidth, texCurrent->texHeight );
	texCurrent->fbo->Bind();
	texCurrent->fbo->AttachTexture(
	    GL_TEXTURE_2D, texCurrent->id, GL_COLOR_ATTACHMENT0_EXT);
	texCurrent->fbo->AttachRenderBuffer(
	    texCurrent->rbo->GetId(), GL_DEPTH_ATTACHMENT_EXT);
	texCurrent->fbo->IsValid();
    }

}

