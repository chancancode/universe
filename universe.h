/****
	  universe.h
		Clone this package from git://github.com/rtv/universe.git
	  version 2
	  Richard Vaughan  
****/

#include <vector>
#include <math.h> 
#include <stdio.h>
#include <stdlib.h>

#define GRAPHICS 1

// handy STL iterator macro pair. Use FOR_EACH(I,C){ } to get an iterator I to
// each item in a collection C.
#define VAR(V,init) __typeof(init) V=(init)
#define FOR_EACH(I,C) for(VAR(I,(C).begin());I!=(C).end();I++)

namespace Uni
{
  /** Convert radians to degrees */
  inline double rtod( double r ){ return( r * 180.0 / M_PI ); }
  /** Convert degrees to radians */
  inline double dtor( double d){ return( d * M_PI / 180.0 ); }
  
  class Robot
  {
  public:
   class Speed
   {    
   public:
    double v; // forward speed
    double w; // turn speed
   
    // constructor sets speeds to zero
    Speed() : v(0.0), w(0.0) {}
   };
   
	 // each robot has a vector of these to store its observations
	 class Pixel
	 {
	 public:
		double range;
		Robot* robot;

	 Pixel() : range(0.0), robot(NULL) {}
	 };  
	 
	 // STATIC DATA AND METHODS ------------------------------------------
	 
	 /** initialization: call this before using any other calls. */	
	 static void Init( int argc, char** argv );

	 /** update all robots */
	 static void UpdateAll();

	 /** Normalize a length to within 0 to worldsize. */
	 static double DistanceNormalize( double d );

	 /** Normalize an angle to within +/_ M_PI. */
	 static double AngleNormalize( double a );

	 /** Swap the buffers **/
   static void SwapBuffers();

	 /** Start running the simulation. Does not return. */
	 static void Run();

	 static uint64_t updates; // number of simulation steps so far	 
	 static uint64_t updates_max; // number of simulation steps to run before quitting (0 means infinity)
	 static unsigned int sleep_msec; // number of milliseconds to sleep at each update
	 static double worldsize; // side length of the toroidal world
	 static double range;    // sensor detects objects up tp this maximum distance
	 static double fov;      // sensor detects objects within this angular field-of-view about the current heading
	 static unsigned int pixel_count; // number of pixels in sensor
	 static std::vector<Robot*> population; // a list of all robots
	 static unsigned int population_size; // number of robots
   static std::vector< std::vector< std::vector<Robot*> > > sectors; // 3D vector of robots (ouch brain hurts)
   static int num_of_sectors; // number of sectors per row/col
   static double sector_width; // width of each sector
	 static bool paused; // runs only when this is false
	 static bool show_data; // controls visualization of pixel data
	 static int winsize; // initial size of the window in pixels
	 static int displaylist; // robot body macro

   /** Double Buffer **/
   
   /* Read Only */
   static double **pose;
   static double **color;

   /* Write Only */
   static double **pose_next;
   static double **color_next;

#if GRAPHICS
	 /** render all robots in OpenGL */
	 static void DrawAll();
#endif
	 
	 // NON-STATIC DATA AND METHODS ------------------------------------------
	 
   int id;
	 std::vector<Pixel> pixels; // robot's sensor data vector
   Speed speed;
   int grid_location[2]; // which sector am I in
   
	 // create a new robot with these parameters
	 Robot(double x, double y, double a, double r, double g, double b);
	 
	 virtual ~Robot() {}
	 
	 // pure virtual - subclasses must implement this method	 
	 virtual void Controller() = 0;
	 
	 // render the robot in OpenGL
	 void Draw();
	 
	 // move the robot
	 void UpdatePose();

	 // update
	 void UpdatePixels();
  };	
}; // namespace Uni
