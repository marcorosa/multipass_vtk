#include "GL/glew.h"

#include "vtkConeSource.h"
#include "vtkPolyDataMapper.h"
#include "vtkRenderWindow.h"
#include "vtkCamera.h"
#include "vtkActor.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLProperty.h"

#include "vtkRiftRenderPass.h"
#include "vtkCameraPass.h"
#include "vtkSequencePass.h"
#include "vtkRenderPassCollection.h"
#include "vtkDefaultPass.h"
#include "vtkLightsPass.h"

#include "vtkGenericDataObjectReader.h"  // for reading btain.vtk

#include <vtkRenderWindowInteractor.h>   // for interactor
#include <vtkCallbackCommand.h>          // for callbacks
#include <time.h>

#include "riftclass.h"                    // for rift

#include <time.h>

timespec diff(timespec start, timespec end){
    timespec difference;
    if ((end.tv_nsec-start.tv_nsec)<0) {
	difference.tv_sec = end.tv_sec-start.tv_sec-1;
	difference.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
	difference.tv_sec = end.tv_sec-start.tv_sec;
	difference.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return difference;
}

bool print_times = false;


// used for keypress callback
struct ClientData{
    Rift* rift;
    vtkRenderer* renderer_l;
    vtkRenderer* renderer_r;
    vtkRiftRenderPass* pass_l;
    vtkRiftRenderPass* pass_r;
};

// static void CameraModifiedCallback ( vtkObject* caller, long unsigned int eventId,
//           void* clientData, void* callData );

// // used for mouse callback
// struct clientDataMouse{
//     vtkCamera* camera;
//     long unsigned int observerID;
//     vtkCallbackCommand* observer;
// };


void KeypressCallbackFunction (
    vtkObject* caller,
    long unsigned int vtkNotUsed(eventId),
    void* clientData,
    void* vtkNotUsed(callData) ){

    vtkRenderWindowInteractor *iren =
	static_cast<vtkRenderWindowInteractor*>(caller);

    char* key = iren->GetKeySym();
    // care! GetKeySym returns things like 'space'!
    cout << "Pressed: " << key << endl;

    ClientData* cd = (ClientData*) clientData;

    if( 0 == strcmp("space", (const char*) key)){
// RESET CAMERAS

	double eye_spacing = 0.4;

	cd->renderer_l->ResetCamera();
	cd->renderer_r->ResetCamera();
	// resets a bunch of things, inc. position

	// adjust camera positions for stereo
	double camera_position[3];
	cd->renderer_l->GetActiveCamera()->GetPosition(camera_position);
	camera_position[0] -= eye_spacing / 2;
	cd->renderer_l->GetActiveCamera()->SetPosition(camera_position);
	camera_position[0] += eye_spacing;
	cd->renderer_r->GetActiveCamera()->SetPosition(camera_position);

	// cd->renderer_l->GetActiveCamera()->SetRoll(0);
	// cd->renderer_r->GetActiveCamera()->SetRoll(0);

	//last_yaw = last_pitch = last_roll = 0; // doesn't work
	cd->rift->ResetSensor();
    }

    if('p' == key[0]){
	print_times = !print_times;
    }

    if('r' == key[0]){
	cd->pass_l->Resize();
	cd->pass_r->Resize();
    }

    if( 0 == strcmp("KP_Add", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Dolly(1.1);
	cd->renderer_l->GetActiveCamera()->Dolly(1.1);
    }
    if( 0 == strcmp("KP_Subtract", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Dolly(0.9);
	cd->renderer_l->GetActiveCamera()->Dolly(0.9);
    }

    if( 0 == strcmp("Left", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Azimuth(1);
	cd->renderer_l->GetActiveCamera()->Azimuth(1);
    }

    if( 0 == strcmp("Right", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Azimuth(-1);
	cd->renderer_l->GetActiveCamera()->Azimuth(-1);
    }

    if( 0 == strcmp("Up", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Elevation(1);
	cd->renderer_l->GetActiveCamera()->Elevation(1);
    }

    if( 0 == strcmp("Down", (const char*) key)){
	cd->renderer_r->GetActiveCamera()->Elevation(-1);
	cd->renderer_l->GetActiveCamera()->Elevation(-1);
    }

    if( 0 == strcmp("bracketleft", (const char*) key)){
	double camera_l_position[3];
	double camera_r_position[3];
	cd->renderer_l->GetActiveCamera()->GetPosition(camera_l_position);
	cd->renderer_r->GetActiveCamera()->GetPosition(camera_r_position);
	camera_l_position[0] -= 0.1;
	camera_r_position[0] += 0.1;
	cd->renderer_l->GetActiveCamera()->SetPosition(camera_l_position);
	cd->renderer_r->GetActiveCamera()->SetPosition(camera_r_position);

	cout << "Eye separation is "
	     << camera_l_position[0] - camera_r_position[0] << endl;
    }

    if( 0 == strcmp("bracketright", (const char*) key)){
	double camera_l_position[3];
	double camera_r_position[3];
	cd->renderer_l->GetActiveCamera()->GetPosition(camera_l_position);
	cd->renderer_r->GetActiveCamera()->GetPosition(camera_r_position);
//	if(camera_l_position[0] < camera_r_position[0]){ // don't invert eyes
	    camera_l_position[0] += 0.1;
	    camera_r_position[0] -= 0.1;
//	}
	cd->renderer_l->GetActiveCamera()->SetPosition(camera_l_position);
	cd->renderer_r->GetActiveCamera()->SetPosition(camera_r_position);

	cout << "Eye separation is "
	     << camera_l_position[0] - camera_r_position[0] << endl;
    }

}

class vtkTimerCallback : public vtkCommand{
public:
    static vtkTimerCallback *New(){
	vtkTimerCallback *cb = new vtkTimerCallback;
	cb->TimerCount = 0;
	return cb;
    }

    void Configure(
	vtkCamera* left,
	vtkCamera* right,
	vtkRenderWindow* window,
	Rift* rift_pointer
	){
	camera_l_ = left;
	camera_r_ = right;
	renderWindow_ = window;
	last_yaw = last_pitch = last_roll = 0.0;
	rift = rift_pointer;
	double camera_position[3];
	double eye_spacing = 0.8;
	camera_l_->GetPosition(camera_position);
	camera_position[0] += eye_spacing / 2;
	camera_r_->SetPosition(camera_position);
	camera_position[0] -= eye_spacing;
	camera_l_->SetPosition(camera_position);

	rift->ResetSensor();
//	double camera_focus[3];
//	// may need to Get, Set the FocalPoint(camera_focus)
    }

    virtual void Execute(vtkObject *vtkNotUsed(caller),
			 unsigned long eventId,
			 void *vtkNotUsed(callData)){

	if (vtkCommand::TimerEvent == eventId){
	    ++this->TimerCount;

	    timespec tick, tock;
	    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tick);

	    rift->HeadPosition(yaw, pitch, roll);
	    // cout << "  y"; cout.width(5); cout << (int)yaw;
	    // cout << "  p"; cout.width(5); cout << (int) pitch;
	    // cout << "  r"; cout.width(5); cout << (int) roll;
	    // cout.width(5); cout << (int) (last_pitch - pitch);
	    // cout.width(5); cout << (int) (last_yaw - yaw);
	    // cout << endl;

	    camera_r_->SetRoll( (double) - roll  );
	    camera_l_->SetRoll( (double) - roll  );

	    camera_r_->Yaw(  (double) yaw - last_yaw  );
	    camera_l_->Yaw(  (double) yaw - last_yaw  );

	    camera_r_->Pitch((double) pitch - last_pitch);
	    camera_l_->Pitch((double) pitch - last_pitch);

	    last_yaw = yaw; last_pitch = pitch; last_roll = roll;

	    // camera_r_->Azimuth(1);  camera_l_->Azimuth(1);


	    renderWindow_->Render();

	    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tock);
	    float miliseconds = 1000.0 * diff(tick,tock).tv_sec
		+ diff(tick,tock).tv_nsec / 1000000.0;
	    if(print_times){
		cout << "Render time: "
		     << setprecision(3) << miliseconds << endl;
	    }
	}
    }
private:
    int TimerCount;
    vtkSmartPointer<vtkCamera> camera_l_;
    vtkSmartPointer<vtkCamera> camera_r_;
    vtkSmartPointer<vtkRenderWindow> renderWindow_;
    Rift* rift;

    float yaw, pitch, roll;
    float last_yaw, last_pitch, last_roll;

};

int main(int argc, char *argv[] )
{
    bool use_cone = false; // let's be optimistic, eh?
    if ( argc == 1 ){
	cout<<"No .vtk input specified, use default cone source." << endl;
	use_cone = true;
    }
    else {
	ifstream the_file ( argv[1] );
	// Always check to see if file opening succeeded
	if ( !the_file.is_open() ){
	    cout<<"Could not open file, use default cone source." << endl;
	    use_cone = true;
	}
	// the_file is closed implicitly here
    }

    // mapper, defined outside if statement
    vtkSmartPointer<vtkPolyDataMapper> coneMapper = vtkPolyDataMapper::New();

    // get source data
    if(!use_cone){
	std::string inputFilename = argv[1]; 
        // candidates: CCtracts.vtk btain.vtk def1.bnd.vtk FullBrain.vtk

	// read data
	vtkSmartPointer<vtkGenericDataObjectReader> reader =
	    vtkSmartPointer<vtkGenericDataObjectReader>::New();
	reader->SetFileName(inputFilename.c_str());
	reader->Update();

	// polydata
	vtkSmartPointer<vtkPolyData> output = reader->GetPolyDataOutput();
	coneMapper->SetInputConnection(output->GetProducerPort());

    } else{
	vtkSmartPointer<vtkConeSource> cone = vtkConeSource::New();
	coneMapper->SetInputConnection( cone->GetOutputPort() );
    }



    // actor
    vtkSmartPointer<vtkActor> coneActor = vtkActor::New();
    coneActor->SetMapper( coneMapper );

    // renderer
    vtkSmartPointer<vtkRenderer> ren_l = vtkRenderer::New();
    ren_l->AddActor( coneActor );
    vtkSmartPointer<vtkRenderer> ren_r = vtkRenderer::New();
    ren_r->AddActor( coneActor );
    // renderer->SetBackground( 0.1, 0.2, 0.4 );

    // Nasty hack:
    // load the FullBrain2.vtk if a third parameter is present
    // an elegant way of *only* loading named content is needed...
    if(!use_cone && argc == 3){
	vtkSmartPointer<vtkPolyDataMapper> coneMapper2 = vtkPolyDataMapper::New();
	vtkSmartPointer<vtkGenericDataObjectReader> reader2 =
	    vtkSmartPointer<vtkGenericDataObjectReader>::New();
	reader2->SetFileName("FullBrain2.vtk");
	reader2->Update();
	// polydata
	vtkSmartPointer<vtkPolyData> output2 = reader2->GetPolyDataOutput();
	coneMapper2->SetInputConnection(output2->GetProducerPort());
	vtkSmartPointer<vtkActor> coneActor2 = vtkActor::New();
	coneActor2->SetMapper( coneMapper2 );

	ren_l->AddActor( coneActor2 );
	ren_r->AddActor( coneActor2 );
    }

    // render window
    double viewport_l[4] = {0.0, 0.0, 0.5, 1.0};
    double viewport_r[4] = {0.5, 0.0, 1.0, 1.0};
    vtkSmartPointer<vtkRenderWindow> renWin = vtkRenderWindow::New();

    // size and shift window
    renWin->SetSize(1280, 800);
    renWin->SetPosition(1680, 0);

    // alternative: full screen & mirror BUT sets to 1280 x 720!!
    // renWin->FullScreenOn();

    renWin->AddRenderer(ren_l); ren_l->SetViewport(viewport_l);
    renWin->AddRenderer(ren_r); ren_r->SetViewport(viewport_r);


    // left
    vtkDefaultPass* defal_l=vtkDefaultPass::New();
    vtkLightsPass* lights_l=vtkLightsPass::New();

    vtkRenderPassCollection *passes_l=vtkRenderPassCollection::New();
    passes_l->AddItem(defal_l);
    passes_l->AddItem(lights_l);
    vtkSequencePass *seq_l=vtkSequencePass::New();
    seq_l->SetPasses(passes_l);
    vtkCameraPass *cameraP_l=vtkCameraPass::New();
    cameraP_l->SetDelegatePass(seq_l);
    vtkRiftRenderPass* saliencyP_l = vtkRiftRenderPass::New();
    saliencyP_l->SetDelegatePass(cameraP_l);


    // right
    vtkDefaultPass* defal_r=vtkDefaultPass::New();
    vtkLightsPass* lights_r=vtkLightsPass::New();
    vtkRenderPassCollection *passes_r=vtkRenderPassCollection::New();
    passes_r->AddItem(defal_r);
    passes_r->AddItem(lights_r);
    vtkSequencePass *seq_r=vtkSequencePass::New();
    seq_r->SetPasses(passes_r);
    vtkCameraPass *cameraP_r=vtkCameraPass::New();
    cameraP_r->SetDelegatePass(seq_r);
    vtkRiftRenderPass* saliencyP_r = vtkRiftRenderPass::New();
    saliencyP_r->SetDelegatePass(cameraP_r);


    ren_l->SetPass(saliencyP_l);
    ren_r->SetPass(saliencyP_r);

    Rift rift;

    // Timed interactor for HMD
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor =
	vtkSmartPointer<vtkRenderWindowInteractor>::New();
    renderWindowInteractor->SetRenderWindow(renWin);
    renderWindowInteractor->Initialize();

    vtkSmartPointer<vtkTimerCallback> cb =
	vtkSmartPointer<vtkTimerCallback>::New();

    renderWindowInteractor->AddObserver(
	vtkCommand::TimerEvent,
	cb);
    int timerId = renderWindowInteractor->CreateRepeatingTimer(16);
    std::cout << "timerId: " << timerId << std::endl;

    renWin->Render();

    // touching the cameras before first render stops auto-scaling
    cb->Configure(ren_l->GetActiveCamera(),
		  ren_r->GetActiveCamera(),
		  renWin,
		  &rift);

    ClientData clientdata;
    clientdata.rift = &rift;
    clientdata.renderer_l = ren_l;
    clientdata.renderer_r = ren_r;
    clientdata.pass_l = saliencyP_l;
    clientdata.pass_r = saliencyP_r;


    // vtkSmartPointer<vtkCallbackCommand> observer1 = 
    // 	vtkSmartPointer<vtkCallbackCommand>::New();
    // observer1->SetCallback (CameraModifiedCallback);
    // clientDataMouse clientData1;

    // vtkSmartPointer<vtkCallbackCommand> observer2 = 
    // 	vtkSmartPointer<vtkCallbackCommand>::New();
    // observer2->SetCallback (CameraModifiedCallback);
    // clientDataMouse clientData2;

    // long unsigned int observerID;
    // observerID = ren_l->GetActiveCamera()->AddObserver(
    // 	vtkCommand::ModifiedEvent, observer1);
    // clientData1.observerID = observerID;

    // observerID = ren_r->GetActiveCamera()->AddObserver( // TODO: set observer ID
    // 	vtkCommand::ModifiedEvent, observer2);
    // clientData2.observerID = observerID;

    // clientData1.camera = ren_r->GetActiveCamera();
    // clientData1.observer = observer2;

    // clientData2.camera = ren_l->GetActiveCamera();
    // clientData2.observer = observer1;

    // observer1->SetClientData(&clientData1); // know what the other camera is!

    // observer2->SetClientData(&clientData2); // know what the other camera is!



    // // Keypress interactor
    vtkSmartPointer<vtkCallbackCommand> keypressCallback =
	vtkSmartPointer<vtkCallbackCommand>::New();
    keypressCallback->SetCallback ( KeypressCallbackFunction );
    keypressCallback->SetClientData( &clientdata);

    renderWindowInteractor->AddObserver (
	vtkCommand::KeyPressEvent,
	keypressCallback );

    renderWindowInteractor->Start();

    return 0;
}


// static void CameraModifiedCallback(vtkObject* caller,
//                                    long unsigned int observerID,
//                                    void* clientDataIn,
// 				   void* vtkNotUsed(callData) )
// {
//     long unsigned int tempObserverID; 
//     clientDataMouse* data = static_cast<clientDataMouse*>(clientDataIn);

//     vtkCamera* camera = static_cast<vtkCamera*>(caller);
//     vtkCamera* camera2 = data->camera;

//     camera2->RemoveObserver(data->observerID);
//     camera2->SetPosition(camera->GetPosition());
//     camera2->SetFocalPoint(camera->GetFocalPoint());
//     camera2->SetViewUp(camera->GetViewUp());
//     tempObserverID = camera2->AddObserver(vtkCommand::ModifiedEvent, data->observer);
//     data->observerID = tempObserverID;
// }
